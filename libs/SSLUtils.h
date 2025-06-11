/*
 * File:   SSLUtils.h
 * Author: thaipq
 *
 * Created on Thu May 08 2025 2:35:19 PM
 */

#ifndef LIBS_SSLUTILS_H
#define LIBS_SSLUTILS_H

#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/ssl.h>
#include <openssl/sha.h>

#include <memory>
#include <optional>
#include <string>
#include <vector>

#if __cplusplus >= 201703L
#include <string_view>
#endif

// todo: sha256 using openssl

namespace libs {
    namespace ssl {
        using ResultPair = std::pair<bool, std::string>;
        using namespace std::string_literals;

        template<typename _T>
        struct evp_cipher_deletor {
            void operator()(_T *ptr) const {
                EVP_CIPHER_CTX_free(ptr);
            }
        };

        template<typename _T>
        struct bio_uniq_deletor {
            void operator()(_T *ptr) const {
                BIO_free_all(ptr);
            }
        };

        // using EVPCipherCtxPtr = std::unique_ptr<EVP_CIPHER_CTX, decltype(&EVP_CIPHER_CTX_free)>;
        using EVPCipherCtxPtr = std::unique_ptr<EVP_CIPHER_CTX, evp_cipher_deletor<EVP_CIPHER_CTX>>;

        struct CipherActions {
            decltype(&EVP_EncryptInit_ex) init;
            decltype(&EVP_EncryptUpdate) update;
            decltype(&EVP_EncryptFinal_ex) finalize;
        };

        ResultPair doCipher(const EVP_CIPHER *cipher,
                            const char *input, size_t inputLen,
                            const char *secret, size_t secretLen,
                            const char *iv, size_t ivLen,
                            const CipherActions &cipherAct,
                            size_t outputSizeHint = 0);

        ResultPair encrypt(const EVP_CIPHER *cipher,
                           const char *plain, size_t plainLen,
                           const char *secret, size_t secretLen,
                           const char *iv, size_t ivLen);

        ResultPair decrypt(const EVP_CIPHER *cipher,
                           const char *encrypted, size_t encryptedLen,
                           const char *secret, size_t secretLen,
                           const char *iv, size_t ivLen);
#if __cplusplus >= 201703L

        std::optional<std::string> encrypt(const EVP_CIPHER *cipher,
                                           std::string_view text,
                                           std::string_view key,
                                           std::string_view iv);

        std::optional<std::string> decrypt(const EVP_CIPHER *cipher,
                                           std::string_view text,
                                           std::string_view key,
                                           std::string_view iv);
#endif
    }// namespace ssl

    namespace hash_utils {
        template<typename _T>
        struct evp_md_deletor {
            void operator()(_T *ptr) const {
                EVP_MD_CTX_free(ptr);
            }
        };
        using EVPMDCtxPtr = std::unique_ptr<EVP_MD_CTX, evp_md_deletor<EVP_MD_CTX>>;
        using HashVal = std::vector<unsigned char>;

        HashVal hash(const EVP_MD *method, const char *data, size_t len, size_t blocksize = 0);
#if __cplusplus >= 201703L
        HashVal hash(const EVP_MD *method, std::string_view data);
#endif
        HashVal hash_file(const EVP_MD *method, const char *filepath);

        HashVal sha256(const char *data, size_t len, size_t blocksize = 0);
        HashVal sha256_file(const char *filepath);

        HashVal sha1(const char *data, size_t len, size_t blocksize = 0);
        HashVal md5(const char *data, size_t len, size_t blocksize = 0);
    };// namespace hash_utils

    namespace base64 {
        std::string encode(const char *buffer, size_t length);
        std::string decode(const char *msg, size_t len);

#if __cplusplus >= 201703L
        std::string encode(std::string_view buffer);
        std::string decode(std::string_view msg);
#endif
    };// namespace base64
}// namespace libs

#endif// LIBS_SSLUTILS_H