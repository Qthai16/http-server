#include <gtest/gtest.h>
#include <thread>
#include "libs/http/HttpMessage.h"
#include "libs/http/SimpleServer.h"

using namespace libs::http;

TEST(http_server, start_stop) {
    std::shared_ptr<SimpleServer> server;
    server.reset(new libs::http::SimpleServer("0.0.0.0", 11237, 4));
    server->start();
    server->stop(); // stop async
    server.reset();
    EXPECT_TRUE(true);
}
