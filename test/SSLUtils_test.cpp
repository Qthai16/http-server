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
