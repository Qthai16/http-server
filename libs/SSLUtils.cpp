#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include <fstream>
#include <cassert>

#include "SSLUtils.h"
#include "Defines.h"
#include "RandomUtils.h"

namespace libs {
    namespace ssl {

#define buffer_cast(p)       ((unsigned char *) p)
#define const_buffer_cast(p) ((const unsigned char *) p)

#define EVP_PRINT_ERR        ERR_print_errors_fp(stderr);

        ResultPair doCipher(const EVP_CIPHER *cipher,
                            const char *input, size_t inputLen,
                            const char *secret, size_t secretLen,
                            const char *iv, size_t ivLen,
                            const CipherActions &cipherAct,
                            size_t blocksize) {

            if (!input || !inputLen || !secret || !secretLen || !iv || !ivLen) {
                return {false, {}};
            }
            EVPCipherCtxPtr evpCtxPtr(EVP_CIPHER_CTX_new());
            if (!evpCtxPtr) {
                return {false, {}};
            }

            auto cipherLen = 0;
            // do {
            // init
            if (!cipherAct.init(evpCtxPtr.get(), cipher, nullptr, const_buffer_cast(secret), const_buffer_cast(iv))) {
                return {false, {}};
            }
            int cLen = 0;
            std::string output;
            output.resize(inputLen + blocksize);
            // update
            if (!cipherAct.update(evpCtxPtr.get(), buffer_cast(output.data()), &cLen, const_buffer_cast(input), static_cast<int>(inputLen))) {
                return {false, {}};
                // break;
            }

            // finalize
            cipherLen = cLen;
            if (!cipherAct.finalize(evpCtxPtr.get(), buffer_cast(output.data() + cLen), &cLen)) {
                cipherLen = 0;
                return {false, {}};
            }
            cipherLen += cLen;
            output.resize(cipherLen);
            return {true, std::move(output)};
            // } while (false);
        }

        ResultPair encrypt(const EVP_CIPHER *cipher,
                           const char *plain, size_t plainLen,
                           const char *key, size_t keyLen,
                           const char *iv, size_t ivLen) {
            assert(cipher);
            if (!plain || !plainLen || !key || keyLen != EVP_CIPHER_key_length(cipher) || !iv || ivLen != EVP_CIPHER_iv_length(cipher)) {
                // printf("invalid param\n");
                return {false, {}};
            }
            CipherActions actions{EVP_EncryptInit_ex, EVP_EncryptUpdate, EVP_EncryptFinal_ex};
            return doCipher(cipher, plain, plainLen, key, keyLen, iv, ivLen, actions, 16);
        }

        ResultPair decrypt(const EVP_CIPHER *cipher,
                           const char *encrypted, size_t encryptedLen,
                           const char *key, size_t keyLen,
                           const char *iv, size_t ivLen) {
            assert(cipher);
            if (!encrypted || !encryptedLen || !key || keyLen != EVP_CIPHER_key_length(cipher) || !iv || ivLen != EVP_CIPHER_iv_length(cipher)) {
                // printf("invalid param\n");
                return {false, {}};
            }
            CipherActions actions{EVP_DecryptInit_ex, EVP_DecryptUpdate, EVP_DecryptFinal_ex};
            return doCipher(cipher, encrypted, encryptedLen, key, keyLen, iv, ivLen, actions);
        }

#if __cplusplus >= 201703L

        std::optional<std::string> encrypt(const EVP_CIPHER *cipher,
                                           std::string_view text,
                                           std::string_view key,
                                           std::string_view iv) {
            auto [b, ret] = encrypt(cipher, text.data(), text.size(), key.data(), key.size(), iv.data(), iv.size());
            return b ? std::make_optional<std::string>(std::move(ret)) : std::nullopt;
        }

        std::optional<std::string> decrypt(const EVP_CIPHER *cipher,
                                           std::string_view text,
                                           std::string_view key,
                                           std::string_view iv) {
            auto [b, ret] = decrypt(cipher, text.data(), text.size(), key.data(), key.size(), iv.data(), iv.size());
            return b ? std::make_optional<std::string>(std::move(ret)) : std::nullopt;
        }

#endif
        // void deriveFromPassword(key, iv, password);

    }// namespace ssl

    namespace hash_utils {
        HashVal hash(const EVP_MD *method, const char *data, size_t dataLen, size_t blocksize) {
            assert(method);
            EVPMDCtxPtr ctx(EVP_MD_CTX_new());
            // unsigned char hashVal[EVP_MAX_MD_SIZE];
            std::vector<unsigned char> hashVal(EVP_MAX_MD_SIZE, '\0');
            unsigned int hashLen;
            if (!ctx) {
                return {};
            }
            if (!EVP_DigestInit_ex(ctx.get(), method, nullptr)) {
                return {};
            }
            if (expr_likely(blocksize == 0))
                blocksize = dataLen;
            assert(blocksize > 0);
            const char *buf = data;
            size_t bufLen = dataLen;
            do {
                auto size = std::min(bufLen, blocksize);
                if (!EVP_DigestUpdate(ctx.get(), buf, size))
                    return {};
                buf += size;
                bufLen -= size;
            } while (bufLen > 0);
            if (!EVP_DigestFinal_ex(ctx.get(), hashVal.data(), &hashLen)) {
                return {};
            }
            hashVal.resize(hashLen);
            return std::move(hashVal);
        }

        HashVal hash(const EVP_MD *method, std::string_view data) {
            return hash(method, data.data(), data.size());
        }

        HashVal hash_file(const EVP_MD *method, const char *filepath) {
            assert(method);
            std::ifstream f(filepath, std::ios::binary);
            if (!f.is_open()) {
                printf("failed open\n");
                return {};
            }
            EVPMDCtxPtr ctx(EVP_MD_CTX_new());
            // unsigned char hashVal[EVP_MAX_MD_SIZE];
            std::vector<unsigned char> hashVal(EVP_MAX_MD_SIZE, '\0');
            unsigned int hashLen;
            if (!ctx) {
                return {};
            }
            if (!EVP_DigestInit_ex(ctx.get(), method, nullptr)) {
                return {};
            }
            const size_t blocksize = 4096;
            std::string buffer(blocksize, '\0');
            f.seekg(0, std::ios::end);
            size_t filesize = f.tellg();
            f.seekg(0, std::ios::beg);
            std::size_t size;
            do {
                // todo: check eof error
                size = std::min(filesize, blocksize);
                f.read(&buffer[0], size);
                if (!EVP_DigestUpdate(ctx.get(), buffer.data(), size))
                    return {};
                filesize -= size;
            } while (filesize > 0 && f.good() && !f.eof());
            if (!EVP_DigestFinal_ex(ctx.get(), hashVal.data(), &hashLen)) {
                return {};
            }
            hashVal.resize(hashLen);
            return std::move(hashVal);
        }

        HashVal sha256(const char *data, size_t len, size_t blocksize) {
            return hash(EVP_sha256(), data, len, blocksize);
        }

        HashVal sha256_file(const char *filepath) {
            return hash_file(EVP_sha256(), filepath);
        }

        HashVal sha1(const char *data, size_t len, size_t blocksize) {
            return hash(EVP_sha1(), data, len, blocksize);
        }

        HashVal md5(const char *data, size_t len, size_t blocksize) {
            return hash(EVP_md5(), data, len, blocksize);
        }
    }// namespace hash_utils

    namespace base64 {
        std::string encode(const char *buffer, size_t length) {
            // todo: check error code
            BIO *bio, *b64;
            b64 = BIO_new(BIO_f_base64());
            bio = BIO_new(BIO_s_mem());
            // BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
            bio = BIO_push(b64, bio);
            BIO_write(bio, buffer, length);
            BIO_flush(bio);
            BUF_MEM *bufferPtr;
            BIO_get_mem_ptr(bio, &bufferPtr);
            std::string ret(bufferPtr->data, bufferPtr->length);
            BIO_free_all(bio);
            return ret;
        }

        std::string decode(const char *msg, size_t msgLen) {
            // todo: check error code
            if (!msg || !msgLen) return {};
            std::string ret(msgLen, '\0');
            BIO *bio = BIO_new_mem_buf(msg, msgLen);
            BIO *base64 = BIO_new(BIO_f_base64());
            // BIO_set_flags(base64, BIO_FLAGS_BASE64_NO_NL);
            bio = BIO_push(base64, bio);
            auto outLen = BIO_read(bio, ret.data(), msgLen);
            BIO_free_all(bio);
            ret.resize(outLen);
            return ret;
        }

#if __cplusplus >= 201703L
        std::string encode(std::string_view buffer) {
            return encode(buffer.data(), buffer.size());
        }

        std::string decode(std::string_view msg) {
            return decode(msg.data(), msg.size());
        }
#endif
    };// namespace base64


    std::string key_from_str(const EVP_CIPHER *cipher, const std::string& str) {
        // key len can be 128, 192, or 256 bits (16, 24 or 32 bytes)
        std::string key(EVP_CIPHER_key_length(cipher), '\0');
        auto hashStr = hash_utils::sha256(str.data(), str.size());
        for (auto i = 0; i < key.size(); i++) {
            key[i] = hashStr[i];
        }
        return std::move(key);
    }

    std::string rand_iv(const EVP_CIPHER *cipher) {
        // iv must be 128 bits (16 bytes)
        std::string iv(EVP_CIPHER_iv_length(cipher), '\0');
        for (auto i = 0; i < iv.size() / sizeof(int); i++) {
            auto num = libs::random_num(0, std::numeric_limits<int>::max());
            // printf("gen num: %x\n", num);
            for (auto j = 0; j < sizeof(int) / sizeof(char); j++) {
                iv[j + i * sizeof(int)] = 0xff & (num >> (32 - 8 * (j + 1)));
            }
        }
        return std::move(iv);
    }

    std::string iv_from_int(const EVP_CIPHER *cipher, const std::vector<int>& vals) {
        std::string iv(EVP_CIPHER_iv_length(cipher), '\0');
        int num;
        for (auto i = 0; i < iv.size() / sizeof(int); i++) {
            num = i < vals.size() ? vals[i] : 0; // padding
            for (auto j = 0; j < sizeof(int) / sizeof(char); j++) {
                iv[j + i * sizeof(int)] = 0xff & (num >> (32 - 8 * (j + 1)));
            }
        }
        return std::move(iv);
    }
}// namespace libs