#include <array>
#include <memory>
#include <stdexcept>

#include <openssl/engine.h>
#include <openssl/err.h>
#include <openssl/conf.h>

#include <cstring>

namespace
{

const std::array<unsigned char, 32> hash = {
    0x10, 0xe6, 0x5a, 0x11, 0xac, 0xcc, 0x36, 0x5e,
    0x18, 0x46, 0x67, 0x02, 0xb0, 0x64, 0x6d, 0x74,
    0x92, 0x41, 0x9a, 0x9d, 0xa6, 0x57, 0xcf, 0x03,
    0x64, 0x52, 0x7c, 0x33, 0x65, 0x55, 0x5b, 0xa7
};

const auto c163 = "uacurve0";
const unsigned char e163[] = {0x01, 0x02, 0x5e, 0x40, 0xbd, 0x97, 0xdb, 0x01, 0x2b, 0x7a, 0x1d, 0x79, 0xde, 0x8e, 0x12, 0x93, 0x2d, 0x24, 0x7f, 0x61, 0xc6};
const unsigned char d163[] = {0x03, 0xe0, 0x74, 0x8d, 0x62, 0x9e, 0xb5, 0x4a, 0x1d, 0x8f, 0x9a, 0x87, 0xeb, 0xde, 0x12, 0xca, 0x0e, 0xed, 0x48, 0x54, 0xa5};
const unsigned char s163[] = {0x04, 0x2a, 0x66, 0xa7, 0x40, 0x07, 0x84, 0xa7, 0x4a, 0x72, 0xcc, 0xa7, 0x5b, 0x0c, 0x35, 0x8b, 0x4c, 0xdd, 0x6c, 0x2a, 0xa6, 0x36, 0x00, 0x00, 0xc4, 0x74, 0x66, 0xda, 0xd4, 0xe1, 0x01, 0x1b, 0x3e, 0x18, 0x95, 0x27, 0x72, 0xc0, 0x80, 0xa9, 0xa3, 0x1b, 0x31, 0x01};

const auto c257 = "uacurve6";
const unsigned char e257[] = {0x00, 0x43, 0x94, 0xe1, 0x9d, 0xcc, 0x1d, 0xef, 0x57, 0xdd, 0xbe, 0x6f, 0xb7, 0x05, 0x65, 0xc7, 0x49, 0xf2, 0x21, 0x44, 0x62, 0x6f, 0x05, 0x0a, 0x6d, 0xaa, 0xb4, 0xaf, 0x00, 0x71, 0xe6, 0xfe, 0xb9};
const unsigned char d257[] = {0x00, 0x6f, 0x31, 0xc4, 0x44, 0x59, 0x14, 0xfb, 0x34, 0x7b, 0xa9, 0xbd, 0x27, 0x2b, 0xb5, 0xb7, 0xf5, 0x13, 0x42, 0x2f, 0xe1, 0x16, 0xdf, 0xa4, 0xf0, 0xdf, 0x34, 0xb1, 0xce, 0xb1, 0x1a, 0x64, 0x5c};
const unsigned char s257[] = {0x04, 0x40, 0x6f, 0x77, 0x37, 0xe8, 0x65, 0xd2, 0x50, 0xd7, 0x0a, 0x29, 0xea, 0xdf, 0xe3, 0xc2, 0x3d, 0x37, 0x0c, 0x3f, 0xa7, 0xb5, 0xc9, 0x43, 0xcd, 0xed, 0x1b, 0x72, 0xf3, 0xe1, 0x53, 0x2d, 0x2c, 0x09, 0xe1, 0x8b, 0x17, 0x72, 0x1d, 0xab, 0xeb, 0xec, 0x39, 0xbd, 0x4a, 0x87, 0x9a, 0xb0, 0x50, 0x7d, 0x80, 0xe9, 0x99, 0xe1, 0xea, 0x5d, 0x65, 0xb5, 0x56, 0xd6, 0xf1, 0x66, 0x44, 0x8b, 0xee, 0x26};

const auto c431 = "uacurve9";
const unsigned char e431[] = {0x04, 0x57, 0xce, 0x62, 0x0e, 0x2b, 0x71, 0x10, 0x0d, 0x87, 0xf7, 0xe1, 0xc2, 0x32, 0x38, 0xaa, 0x36, 0x29, 0xba, 0x52, 0xc6, 0xba, 0xb4, 0x2c, 0x6a, 0x22, 0xdb, 0x43, 0x78, 0x15, 0xcc, 0xfe, 0xb9, 0xa4, 0x47, 0xb0, 0x2a, 0x82, 0x98, 0xe5, 0x26, 0x0a, 0x7f, 0xa9, 0x4a, 0x13, 0xc1, 0x82, 0x2d, 0x88, 0x5e, 0x8c, 0x5c, 0x69};
const unsigned char d431[] = {0x31, 0x13, 0x1b, 0xec, 0x94, 0x8d, 0xac, 0xbb, 0xaf, 0xad, 0xbf, 0x9b, 0xb1, 0x50, 0xb0, 0xb7, 0x92, 0xdf, 0x32, 0x24, 0x79, 0x2a, 0x80, 0xf6, 0x76, 0x1a, 0x0b, 0x03, 0xb3, 0xb8, 0x2d, 0x11, 0x8b, 0xc3, 0xc2, 0x27, 0x1c, 0x04, 0xa0, 0x4c, 0x2d, 0x9b, 0x38, 0x76, 0xc2, 0x33, 0x86, 0x0b, 0x19, 0x0d, 0xcc, 0xea, 0x01, 0x77};
const unsigned char s431[] = {0x04, 0x6c, 0xc4, 0xe4, 0xf0, 0xb5, 0xea, 0xb8, 0xe7, 0xaa, 0xf4, 0x17, 0xa6, 0xb5, 0xd1, 0xd7, 0x4f, 0xe5, 0x34, 0x81, 0x64, 0x50, 0xdf, 0x6d, 0xff, 0xd9, 0x37, 0x7d, 0xfc, 0xac, 0x5f, 0x25, 0x67, 0x6a, 0x11, 0x9e, 0x5f, 0x70, 0x0f, 0x92, 0xc2, 0x7c, 0x80, 0x17, 0xb6, 0xb3, 0xe3, 0xc2, 0x05, 0xf2, 0x74, 0xd5, 0x4c, 0xd3, 0x33, 0x00, 0xdd, 0x1e, 0xe8, 0x43, 0x63, 0x85, 0x69, 0xe1, 0x10, 0xe4, 0x15, 0xc4, 0x46, 0x0e, 0x4e, 0xb3, 0x25, 0x8f, 0xcf, 0xa7, 0x50, 0x98, 0xa5, 0xce, 0xb5, 0xad, 0x35, 0xb8, 0xfd, 0x28, 0xd1, 0x21, 0x06, 0xb5, 0x51, 0x14, 0x87, 0x43, 0x45, 0xef, 0x7d, 0xc1, 0xa1, 0x45, 0xa5, 0xbb, 0x1c, 0xcf, 0x27, 0x4b, 0x5d, 0x64, 0x80, 0x11};

struct TestVector
{
    std::string curve;
    int fieldByteSize;
    const unsigned char* e;
    const unsigned char* d;
    const unsigned char* s;
};

const std::array<TestVector, 3> vectors{
{
    {c163, 21, e163, d163, s163},
    {c257, 33, e257, d257, s257},
    {c431, 54, e431, d431, s431}
}};

std::string OPENSSLError() noexcept
{
    std::array<char, 256> buf{};
    ERR_error_string_n(ERR_get_error(), buf.data(), buf.size());
    return buf.data();
}

int fbytes(unsigned char *buf, int num);

struct ChangeRand
{
    ChangeRand()
    {
        if ((oldRand = RAND_get_rand_method()) == NULL)
            throw std::runtime_error("ChangeRand: failed to get rand method. " + OPENSSLError());

        rand.seed = oldRand->seed;
        rand.cleanup = oldRand->cleanup;
        rand.add = oldRand->add;
        rand.status = oldRand->status;
        /* use own random function */
        rand.bytes = fbytes;
        rand.pseudorand = oldRand->bytes;
        /* set new RAND_METHOD */
        if (!RAND_set_rand_method(&rand))
            throw std::runtime_error("ChangeRand: failed to set rand method. " + OPENSSLError());
    }

    ~ChangeRand()
    {
        RAND_set_rand_method(oldRand);
    }

    void set(const unsigned char* source, int size)
    {
        randSource = source;
        randSize = size;
    }

    const unsigned char* randSource;
    int randSize;
    RAND_METHOD rand;
    const RAND_METHOD *oldRand;
};

std::unique_ptr<ChangeRand> changeRand;

int fbytes(unsigned char *buf, int num)
{
    int to_copy = num;

    while (to_copy)
    {
        memcpy(buf, changeRand->randSource, (to_copy > changeRand->randSize) ? changeRand->randSize : to_copy);
        buf += (to_copy > changeRand->randSize) ? changeRand->randSize : to_copy;
        to_copy -= (to_copy > changeRand->randSize) ? changeRand->randSize : to_copy;
    }

    return num;
}

void test(ENGINE* engine)
{
    auto* pkey = EVP_PKEY_new();
    if (pkey == nullptr)
        throw std::runtime_error("test: failed to create new key. " + OPENSSLError());
    if (EVP_PKEY_set_type(pkey, NID_dstu4145le) == 0)
    {
        EVP_PKEY_free(pkey);
        throw std::runtime_error("test: failed to set key type. " + OPENSSLError());
    }
    auto* ctx = EVP_PKEY_CTX_new(pkey, NULL);
    if (ctx == nullptr)
    {
        EVP_PKEY_free(pkey);
        throw std::runtime_error("test: failed to create key context. " + OPENSSLError());
    }
    for (const auto& vector : vectors)
    {
        if (EVP_PKEY_CTX_ctrl_str(ctx, "curve", vector.curve.c_str()) == 0)
        {
            EVP_PKEY_CTX_free(ctx);
            EVP_PKEY_free(pkey);
            throw std::runtime_error("test: failed to set curve '" + vector.curve + "'. " + OPENSSLError());
        }

        changeRand->set(vector.d, vector.fieldByteSize);

        if (EVP_PKEY_keygen_init(ctx) == 0)
        {
            EVP_PKEY_CTX_free(ctx);
            EVP_PKEY_free(pkey);
            throw std::runtime_error("test: failed to init keygen for curve '" + vector.curve + "'. " + OPENSSLError());
        }

        if (EVP_PKEY_keygen(ctx, &pkey) == 0)
        {
            EVP_PKEY_CTX_free(ctx);
            EVP_PKEY_free(pkey);
            throw std::runtime_error("test: failed to run keygen for curve '" + vector.curve + "'. " + OPENSSLError());
        }

        changeRand->set(vector.e, vector.fieldByteSize);

        if (EVP_PKEY_sign_init(ctx) == 0)
        {
            EVP_PKEY_CTX_free(ctx);
            EVP_PKEY_free(pkey);
            throw std::runtime_error("test: failed to init signature for curve '" + vector.curve + "'. " + OPENSSLError());
        }

        std::array<unsigned char, 256> sig;
        size_t siglen = sig.size();
        if (EVP_PKEY_sign(ctx, sig.data(), &siglen, hash.data(), hash.size()) == 0)
        {
            EVP_PKEY_CTX_free(ctx);
            EVP_PKEY_free(pkey);
            throw std::runtime_error("test: failed to create signature for curve '" + vector.curve + "'. " + OPENSSLError());
        }

        if (memcmp(sig.data(), vector.s, siglen) != 0)
        {
            EVP_PKEY_CTX_free(ctx);
            EVP_PKEY_free(pkey);
            throw std::runtime_error("test: signature mismatch for curve '" + vector.curve + "'. " + OPENSSLError());
        }

        if (EVP_PKEY_verify_init(ctx) == 0)
        {
            EVP_PKEY_CTX_free(ctx);
            EVP_PKEY_free(pkey);
            throw std::runtime_error("test: failed to initialize signature verification for curve '" + vector.curve + "'. " + OPENSSLError());
        }

        if (EVP_PKEY_verify(ctx, sig.data(), siglen, hash.data(), hash.size()) != 1)
        {
            EVP_PKEY_CTX_free(ctx);
            EVP_PKEY_free(pkey);
            throw std::runtime_error("test: failed to verify signature for curve '" + vector.curve + "'. " + OPENSSLError());
        }
    }
    EVP_PKEY_CTX_free(ctx);
    EVP_PKEY_free(pkey);
}

}

int main()
{
    ERR_load_crypto_strings(); OpenSSL_add_all_algorithms();

    OPENSSL_init_crypto(OPENSSL_INIT_ENGINE_ALL_BUILTIN | OPENSSL_INIT_LOAD_CONFIG, nullptr);

    if (CONF_modules_load_file("openssl.cnf", nullptr, 0) <= 0)
        throw std::runtime_error("main: failed to load config file. " + OPENSSLError());

    auto* engine = ENGINE_by_id("dstu");
    if (engine == nullptr)
        throw std::runtime_error("main: failed to load engine. " + OPENSSLError());
    if (ENGINE_init(engine) == 0)
        throw std::runtime_error("main: failed to initialize engine. " + OPENSSLError());

    changeRand = std::make_unique<ChangeRand>();

    test(engine);

    changeRand.reset();

    ENGINE_finish(engine);
    ENGINE_free(engine);

    EVP_cleanup(); ERR_free_strings(); CRYPTO_cleanup_all_ex_data();

    return 0;
}