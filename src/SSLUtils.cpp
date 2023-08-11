#include "SSLUtils.h"

#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/ssl.h>

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

using namespace SSLUtils;

  size_t SSLUtils::CalcDecodeLen(std::string_view base64Input) {
    size_t padding = 0;
    auto len = base64Input.length();
    if(base64Input[len - 1] == '=' && base64Input[len - 2] == '=') {
      padding = 2;
    }
    else if(base64Input[len - 1] == '=') {
      padding = 1;
    }

    return len * 3 / 4 - padding;
  }

  String SSLUtils::Base64Encode(const unsigned char* buffer, size_t length) {
    // should check for memory free after use
    BIO *bio, *b64;
    BUF_MEM* bufferPtr;
    b64 = BIO_new(BIO_f_base64());
    bio = BIO_new(BIO_s_mem());
    bio = BIO_push(b64, bio);
    BIO_write(bio, buffer, length);
    BIO_flush(bio);
    BIO_get_mem_ptr(bio, &bufferPtr);
    BIO_set_close(bio, BIO_NOCLOSE);
    BIO_free_all(bio);
    return String(bufferPtr->data, bufferPtr->length);
  }

  CharArray SSLUtils::Base64Decode(std::string_view msg) {
    auto decodeLen = CalcDecodeLen(msg);
    char decodeData[decodeLen] = {0};
    BIO* bio = BIO_new_mem_buf(msg.data(), -1);
    BIO* base64 = BIO_new(BIO_f_base64());
    bio = BIO_push(base64, bio);
    BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL); // Do not use newlines to flush buffer
    auto len = BIO_read(bio, decodeData, msg.length());
    if(len != decodeLen) {
      // logError("Invalid base64 decode length", errno);
      BIO_free_all(bio);
      return {};
    }
    BIO_free_all(bio);
    return CharArray(decodeData, decodeData + len / sizeof(char));
  }

  optional<String> SSLUtils::_doCipher(const EVP_CIPHER* cipher,
                                    string_view input,
                                    string_view secretKey,
                                    string_view iv,
                                    const CipherActions& cipherAct,
                                    size_t outputSizeHint) {
    auto evpCtxPtr = EVPCipherCtxPtr(EVP_CIPHER_CTX_new(), EVP_CIPHER_CTX_free);
    using buffer = unsigned char*;
    using cbuffer = const buffer;
    optional<String> ret;

    auto cipherLen = 0;
    do {
      if(!evpCtxPtr) {
        break;
      }
      // initialize
      if(!cipherAct.initialize(evpCtxPtr.get(), cipher, NULL, (cbuffer)secretKey.data(), (cbuffer)iv.data())) {
        break;
      }

      int cLen = 0;
      String output;
      output.resize(input.size() + outputSizeHint);
      // update
      if(!cipherAct.update(evpCtxPtr.get(), (buffer)output.data(), &cLen, (cbuffer)input.data(), input.size())) {
        break;
      }

      // finalize
      cipherLen = cLen;
      if(cipherAct.finalize(evpCtxPtr.get(), (buffer)output.data() + cLen, &cLen)) {
        cipherLen += cLen;
        output.resize(cipherLen);
        ret = std::move(output);
      }
      else {
        cipherLen = 0;
      }
    } while(false);
    return ret;
  }

  optional<String> SSLUtils::encrypt(const EVP_CIPHER* cipher,
                                  const string_view& text,
                                  const string_view& secretKey,
                                  const string_view& iv) {
    auto evpEncryptActions = CipherActions{EVP_EncryptInit_ex, EVP_EncryptUpdate, EVP_EncryptFinal_ex};
    return _doCipher(cipher, text, secretKey, iv, evpEncryptActions, 16);
  }

  optional<String> SSLUtils::decrypt(const EVP_CIPHER* cipher,
                                  const string_view& encryptedText,
                                  const string_view& secretKey,
                                  const string_view& iv) {
    auto evpDecryptActions = CipherActions{EVP_DecryptInit_ex, EVP_DecryptUpdate, EVP_DecryptFinal_ex};
    return _doCipher(cipher, encryptedText, secretKey, iv, evpDecryptActions);
  }
