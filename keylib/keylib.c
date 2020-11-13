#include "keylib.h"

#include "iit_asn1.h"
#include "attrcurvespec_asn1.h"

#include "key.h"
#include "compress.h"
#include "params.h"
#include "gost/gost89.h"
#include "gost/gosthash.h"

#include <openssl/x509.h>
#include <openssl/bn.h>

#include <string.h>

static const char iitStoreOID[] = "1.3.6.1.4.1.19398.1.1.1.2";
static const char dstu4145CurveOID[] = "1.3.6.1.4.1.19398.1.1.2.2";
static const char dstu4145KeyOID[] = "1.3.6.1.4.1.19398.1.1.2.3";

static void hash(const void* data, size_t size, unsigned char* dest)
{
    gost_ctx cctx;
    gost_hash_ctx ctx;
    gost_subst_block sbox;

    unpack_sbox(default_sbox, &sbox);
    gost_init(&cctx, &sbox);
    memset(&ctx, 0, sizeof(ctx));
    ctx.cipher_ctx = &cctx;
    hash_block(&ctx, data, size);
    finish_hash(&ctx, dest);
}

static void pkdf(const char* password, size_t passSize, unsigned char* key)
{
    int i = 0;
    hash(password, passSize, key);
    for (i = 0; i < 9999; ++i)
        hash(key, 32, key);
}

static BIGNUM* getPrivateKeyNum(BN_CTX* ctx, X509_ATTRIBUTE* attr)
{
    int count = X509_ATTRIBUTE_count(attr);
    int i = 0;
    int type = 0;
    int length;
    ASN1_STRING* str = NULL;
    unsigned char b = 0;
    const unsigned char* data = NULL;
    unsigned char* buf = NULL;
    BIGNUM* res = NULL;

    if (count < 1)
        return NULL;
    for (i = 0; i < count; ++i)
    {
        type = ASN1_TYPE_get(X509_ATTRIBUTE_get0_type(attr, i));
        if (type != V_ASN1_OCTET_STRING && type != V_ASN1_BIT_STRING)
            continue;
        str = X509_ATTRIBUTE_get0_data(attr, i, type, NULL);
        if (str != NULL)
            break;
    }
    if (str == NULL)
        return NULL;
    length = ASN1_STRING_length(str);
    data = ASN1_STRING_get0_data(str);
    buf = OPENSSL_malloc(length);
    for (i = 0; i < length; ++i)
    {
        // Swap bits
        char b = data[i];
        b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
        b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
        b = (b & 0xAA) >> 1 | (b & 0x55) << 1;
        // Swap bytes
        buf[length - i - 1] = b;
    }

    res = BN_bin2bn(buf,length, BN_CTX_get(ctx));

    OPENSSL_clear_free(buf, length);

    return res;
}

static BIGNUM* makePoly(BN_CTX* ctx, const DSTU_BinaryField* field)
{
    BIGNUM* res = BN_CTX_get(ctx);
    int poly[6];
    poly[0] = ASN1_INTEGER_get(field->m);
    if (field->poly->type == 0)
    {
        // Trinominal
        poly[1] = ASN1_INTEGER_get(field->poly->poly.k);
        poly[2] = 0;
        poly[3] = -1;
    }
    else
    {
        // Pentanominal
        poly[1] = ASN1_INTEGER_get(field->poly->poly.pentanomial->k);
        poly[2] = ASN1_INTEGER_get(field->poly->poly.pentanomial->j);
        poly[3] = ASN1_INTEGER_get(field->poly->poly.pentanomial->l);
        poly[4] = 0;
        poly[5] = -1;
    }
    if (BN_GF2m_arr2poly(poly, res) == 0)
        return NULL;
    return res;
}

static EC_POINT* makePoint(BN_CTX* ctx, const EC_GROUP* group, const ASN1_OCTET_STRING* p)
{
    EC_POINT* res = EC_POINT_new(group);
    if (dstu_point_expand(ASN1_STRING_get0_data(p), ASN1_STRING_length(p), group, res) == 0)
    {
        EC_POINT_free(res);
        return NULL;
    }
    return res;
}

static EC_GROUP* makeECGROUPFromSpec(BN_CTX* ctx, const DSTU_CustomCurveSpec* spec)
{
    BIGNUM* poly = makePoly(ctx, spec->field);
    BIGNUM* a = BN_CTX_get(ctx);
    BIGNUM* b = BN_bin2bn(ASN1_STRING_get0_data(spec->b), ASN1_STRING_length(spec->b), BN_CTX_get(ctx));
    BIGNUM* n = BN_CTX_get(ctx);
    EC_POINT* point = NULL;
    EC_GROUP* res = EC_GROUP_new_curve_GF2m(poly, a, b, ctx);

    if (res == NULL)
        return NULL;

    ASN1_INTEGER_to_BN(spec->a, a);

    point = makePoint(ctx, res, spec->bp);
    if (point == NULL)
    {
        EC_GROUP_free(res);
        return NULL;
    }
    if (EC_POINT_is_on_curve(res, point, ctx) == 0)
    {
        EC_POINT_free(point);
        EC_GROUP_free(res);
        return NULL;
    }
    ASN1_INTEGER_to_BN(spec->n, n);
    if (EC_GROUP_set_generator(res, point, n, BN_value_one()) == 0)
    {
        EC_GROUP_free(res);
        res = NULL;
    }
    EC_POINT_free(point);
    return res;
}

static EC_GROUP* makeECGROUP(BN_CTX* ctx, X509_ATTRIBUTE* attr)
{
    DSTU_AttrCurveSpec* spec = NULL;
    int count = X509_ATTRIBUTE_count(attr);
    int i = 0;
    EC_GROUP* res = NULL;

    if (count < 1)
        return NULL;

    for (int i = 0; i < count; ++i)
    {
        spec = ASN1_TYPE_unpack_sequence(ASN1_ITEM_rptr(DSTU_AttrCurveSpec), X509_ATTRIBUTE_get0_type(attr, i));
        if (spec != NULL)
            break;
    }

    if (spec == NULL)
        return NULL;

    res = makeECGROUPFromSpec(ctx, spec->spec);
    DSTU_AttrCurveSpec_free(spec);
    return res;
}

static EVP_PKEY* makePKey(X509_ATTRIBUTE* curveAttr, X509_ATTRIBUTE* keyAttr)
{
    BN_CTX* ctx = BN_CTX_new();
    EC_GROUP* group = NULL;
    BIGNUM* pkNum = NULL;
    DSTU_KEY* key = NULL;
    EVP_PKEY* res = NULL;

    BN_CTX_start(ctx);

    pkNum = getPrivateKeyNum(ctx, keyAttr);
    group = makeECGROUP(ctx, curveAttr);
    if (pkNum == NULL || group == NULL)
    {
        BN_CTX_end(ctx);
        BN_CTX_free(ctx);
        return NULL;
    }
    key = DSTU_KEY_new();
    if (key == NULL)
    {
        EC_GROUP_free(group);
        BN_CTX_end(ctx);
        BN_CTX_free(ctx);
        return NULL;
    }

    res = EVP_PKEY_new();

    if (res == NULL ||
        EC_KEY_set_group(key->ec, group) == 0 ||
        EC_KEY_set_private_key(key->ec, pkNum) == 0 ||
        dstu_add_public_key(key->ec) == 0)
    {
        DSTU_KEY_free(key);
        EC_GROUP_free(group);
        BN_CTX_end(ctx);
        BN_CTX_free(ctx);
        return NULL;
    }

    if (EVP_PKEY_set_type(res, NID_dstu4145le) == 0 ||
        EVP_PKEY_assign(res, EVP_PKEY_id(res), key) == 0)
    {
        DSTU_KEY_free(key);
        EVP_PKEY_free(res);
        res = NULL;
    }

    EC_GROUP_free(group);
    BN_CTX_end(ctx);
    BN_CTX_free(ctx);
    return res;
}

static EVP_PKEY* pkeyFromAttributes(const void* data, size_t size)
{
    const unsigned char* ptr = data;
    PKCS8_PRIV_KEY_INFO* pkcs8 = d2i_PKCS8_PRIV_KEY_INFO(NULL, &ptr, size);
    const STACK_OF(X509_ATTRIBUTE)* attributes = NULL;
    X509_ATTRIBUTE* curveAttr = NULL;
    X509_ATTRIBUTE* keyAttr = NULL;
    X509_ATTRIBUTE* attr = NULL;
    ASN1_OBJECT* dstu4145Curve = OBJ_txt2obj(dstu4145CurveOID, 1);
    ASN1_OBJECT* dstu4145Key = OBJ_txt2obj(dstu4145KeyOID, 1);
    ASN1_OBJECT* attrObject = NULL;
    EVP_PKEY* res = NULL;
    int nattr = 0;
    int i = 0;

    if (pkcs8 == NULL)
        return NULL;

    attributes = PKCS8_pkey_get0_attrs(pkcs8);

    nattr = sk_X509_ATTRIBUTE_num(attributes);
    for (i = 0; i < nattr; ++i)
    {
        attr = sk_X509_ATTRIBUTE_value(attributes, i);
        if (attr == NULL)
            continue;
        attrObject = X509_ATTRIBUTE_get0_object(attr);
        if (attrObject == NULL)
            continue;
        if (OBJ_cmp(attrObject, dstu4145Curve) == 0)
            curveAttr = attr;
        else if (OBJ_cmp(attrObject, dstu4145Key) == 0)
            keyAttr = attr;
    }

    ASN1_OBJECT_free(dstu4145Key);
    ASN1_OBJECT_free(dstu4145Curve);

    if (curveAttr == NULL || keyAttr == NULL)
        return NULL;

    res = makePKey(curveAttr, keyAttr);
    PKCS8_PRIV_KEY_INFO_free(pkcs8);
    return res;
}

static int decryptKey6(const void* data, size_t size, const void* pad, size_t padSize, const char* password, size_t passSize, EVP_PKEY*** keys, size_t* numKeys)
{
    gost_ctx ctx;
    gost_subst_block sbox;
    unsigned char key[32];
    const size_t sourceSize = size + padSize;
    const size_t resSize = sourceSize + 8;
    unsigned char* source = OPENSSL_malloc(sourceSize);
    unsigned char* res = OPENSSL_malloc(resSize);
    const unsigned char* ptr = res;
    EVP_PKEY* pkey1 = NULL;
    EVP_PKEY* pkey2 = NULL;

    unpack_sbox(default_sbox, &sbox);
    gost_init(&ctx, &sbox);
    pkdf(password, passSize, key);
    gost_key(&ctx, key);
    memcpy(source, data, size);
    if (padSize > 0)
        memcpy(source + size, pad, padSize);
    gost_dec(&ctx, source, res, sourceSize / 8);
    OPENSSL_clear_free(source, sourceSize);

    pkey1 = d2i_AutoPrivateKey(NULL, &ptr, resSize);
    if (pkey1 == NULL)
    {
        OPENSSL_clear_free(res, resSize);
        return 0;
    }

    pkey2 = pkeyFromAttributes(res, resSize);
    OPENSSL_clear_free(res, resSize);

    if (pkey2 == NULL)
    {
        *keys = OPENSSL_malloc(sizeof(EVP_PKEY*));
        (*keys)[0] = pkey1;
        *numKeys = 1;
    }
    else
    {
        *keys = OPENSSL_malloc(sizeof(EVP_PKEY*) * 2);
        (*keys)[0] = pkey1;
        (*keys)[1] = pkey2;
        *numKeys = 2;
    }

    return 1;
}

int parseKey6(const void* data, size_t size, const char* password, size_t passSize, EVP_PKEY*** keys, size_t* numKeys)
{
    int res = 0;
    ASN1_OBJECT* correctType = NULL;
    const unsigned char* ptr = data;
    IITStore* store = d2i_IITStore(NULL, &ptr, size);
    if (store == NULL)
        return 0;
    // Sanity checks
    if (store->header == NULL || store->data == NULL ||
        store->header->type == NULL || store->header->params == NULL)
    {
        IITStore_free(store);
        return 0;
    }
    // Type check
    correctType = OBJ_txt2obj(iitStoreOID, 1);
    if (OBJ_cmp(store->header->type, correctType) != 0)
    {
        ASN1_OBJECT_free(correctType);
        IITStore_free(store);
        return 0;
    }
    ASN1_OBJECT_free(correctType);
    if (store->header->params->pad == NULL)
        res = decryptKey6(ASN1_STRING_get0_data(store->data), ASN1_STRING_length(store->data),
                          NULL, 0,
                          password, passSize,
                          keys, numKeys);
    else
        res = decryptKey6(ASN1_STRING_get0_data(store->data), ASN1_STRING_length(store->data),
                          ASN1_STRING_get0_data(store->header->params->pad), ASN1_STRING_length(store->header->params->pad),
                          password, passSize,
                          keys, numKeys);
    IITStore_free(store);
    return res;
}

int readKey6(FILE* fp, const char* password, size_t passSize, EVP_PKEY*** keys, size_t* numKeys)
{
    int res = 0;
    BIO* bio = BIO_new_fp(fp, 0);
    if (!bio)
        return 0;
    res = readKey6_bio(bio, password, passSize, keys, numKeys);
    BIO_free(bio);
    return res;
}

int readKey6_bio(BIO* bio, const char* password, size_t passSize, EVP_PKEY*** keys, size_t* numKeys)
{
    unsigned char buf[1024];
    BIO *mem = BIO_new(BIO_s_mem());
    size_t total = 0;
    size_t bytes = 0;
    size_t written = 0;
    char* ptr = NULL;
    int res = 0;
    for (;;)
    {
        if (!BIO_read_ex(bio, buf, sizeof(buf), &bytes))
        {
            if (total > 0)
                break;
            BIO_free(mem);
            return 0;
        }
        if (bytes == 0)
            break;
        if (!BIO_write_ex(mem, buf, bytes, &written))
        {
            BIO_free(mem);
            return 0;
        }
        total += bytes;
    }
    bytes = BIO_get_mem_data(mem, &ptr);
    if (bytes == 0 || ptr == NULL)
    {
        BIO_free(mem);
        return 0;
    }
    res = parseKey6(ptr, bytes, password, passSize, keys, numKeys);
    BIO_free(mem);
    return res;
}