#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/ssl.h>

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace SSLUtils {
  using String = std::string;
  using std::optional, std::string_view;
  using namespace std::string_literals;
  using EVPCipherCtxPtr = std::unique_ptr<EVP_CIPHER_CTX, decltype(&EVP_CIPHER_CTX_free)>;
  using CharArray = std::vector<char>;

  size_t CalcDecodeLen(std::string_view base64Input);
  String Base64Encode(const unsigned char* buffer, size_t length);
  CharArray Base64Decode(std::string_view msg);

  struct CipherActions {
    //   int (*initialize)(EVP_CIPHER_CTX* ctx, const EVP_CIPHER* cipher, ENGINE* impl,
    //                     const unsigned char* key, const unsigned char* iv);
    //   int (*update)(EVP_CIPHER_CTX* ctx, unsigned char* out, int* outl,
    //                 const unsigned char* in, int inl);

    //   int (*finalize)(EVP_CIPHER_CTX* ctx, unsigned char* out, int* outl);
    decltype(&EVP_EncryptInit_ex) initialize;
    decltype(&EVP_EncryptUpdate) update;
    decltype(&EVP_EncryptFinal_ex) finalize;
  };

  optional<String> _doCipher(const EVP_CIPHER* cipher,
                             string_view input,
                             string_view secretKey,
                             string_view iv,
                             const CipherActions& cipherAct,
                             size_t outputSizeHint = 0);

  optional<String> encrypt(const EVP_CIPHER* cipher,
                           const string_view& text,
                           const string_view& secretKey,
                           const string_view& iv);

  optional<String> decrypt(const EVP_CIPHER* cipher,
                           const string_view& encryptedText,
                           const string_view& secretKey,
                           const string_view& iv);
} // namespace SSLUtils
