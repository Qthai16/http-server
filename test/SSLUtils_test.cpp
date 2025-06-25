#include <gtest/gtest.h>
#include <iostream>
#include <string>
#include <string_view>

#include "libs/SSLUtils.h"
#include "libs/StrUtils.h"

using namespace libs;

TEST(ssl_utils, base64_test) {
    std::string plain("this is a text message");
    auto base64val = base64::encode(plain.data(), plain.size());
    ASSERT_EQ(base64val, std::string{"dGhpcyBpcyBhIHRleHQgbWVzc2FnZQ==\n"});
    ASSERT_EQ(base64::decode(base64val.data(), base64val.size()), plain);
}

TEST(ssl_utils, hash_test) {
    std::string plain("this is test data");
    auto sha256 = hash_utils::sha256(plain.data(), plain.size());
    auto expectSHA256 = libs::fromHexStr("c893900c1106f13d55648a425f502dd6518e40eaa3ac7d24023d4c73d55dc12e");
    ASSERT_EQ(sha256, expectSHA256);

    auto md5sum = hash_utils::md5(plain.data(), plain.size());
    auto expectMD5 = libs::fromHexStr("4491147927626d42bec20f372abfcb0d");
    ASSERT_EQ(md5sum, expectMD5);

    auto sha1 = hash_utils::sha1(plain.data(), plain.size());
    auto expectSHA1 = libs::fromHexStr("f3c351aca845352503cc693e50ccbcab83609175");
    ASSERT_EQ(sha1, expectSHA1);

    // auto val = hash_utils::sha256_file("/home/thaipq/proj/commondb/custom/cmake-build-debug/server");
    // std::cout << libs::toHexStr((const char*) val.data(), val.size()) << std::endl;
}

// todo: gcm mode and ccm mode (with authenticate message tags)
// EVP_aes_256_gcm(), EVP_aes_256_ccm()

TEST(ssl_utils, encrypt_decrypt) {
    // openssl command
    // key=`echo -n "my_sensitive_encryption_key" | sha256sum | cut -c1-64`
    // iv=`printf "%08x%08x%08x%08x" 123 456 1000999 999888`
    // echo -n "this is a secret data" | openssl enc -nosalt -aes-256-cbc -e -K $key -iv $iv -base64
    // echo "<base64_encrypted>" | base64 --decode | openssl enc -nosalt -aes-256-cbc -d -K $key -iv $iv
    std::string rawdata("this is a secret data");
    auto testVals = std::map<const EVP_CIPHER *, std::string>{
            {EVP_aes_256_cbc(), "dAST2VJ4xs0O6jGq4Gj5jEjZcsMF2xiNk3CZFKHLfs4=\n"},
            {EVP_aes_128_ctr(), "mwJg9wqmx8kNy9FM+YTlzexcVeDb\n"}};
    std::vector<int> intvals{123, 456, 1000999, 999888};
    
    for (const auto &pair: testVals) {
        const auto &cipher = pair.first;
        const auto &encryptedText = pair.second;
        auto key = key_from_str(cipher, "my_sensitive_encryption_key");
        auto iv = iv_from_int(cipher, intvals);
        printf("key: %s\n", libs::toHexStr(key.data(), key.size()).c_str());
        printf("iv: %s\n", libs::toHexStr(iv.data(), iv.size()).c_str());
        // encrypt
        auto encCipher = ssl::encrypt(cipher, rawdata, key, iv);
        ASSERT_TRUE(encCipher.has_value());
        std::string encValue = encCipher.value();
        ASSERT_EQ(base64::encode(encValue), encryptedText);
        // decrypt
        auto decData = ssl::decrypt(cipher, encValue, key, iv);
        ASSERT_TRUE(decData.has_value());
        ASSERT_EQ(decData.value(), rawdata);
    }
}
