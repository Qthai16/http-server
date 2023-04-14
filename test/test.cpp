#include <iostream>
#include <string>
#include <thread>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdio.h>
#include <cstring>
#include <sstream>

#include <chrono>

#define BUFFER_SIZE 4096
#define QUEUEBACKLOG    100

using namespace std::chrono;
using namespace std::chrono_literals;

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: [executable] <address> <port> \n";
        return -1;
    }
    std::string address(argv[1]);
    auto port = stoi(std::string{argv[2]});

    std::cout << address << ", " << port << std::endl;
    std::thread t1([](){
        std::cout << "Hello world" << std::endl;
    });
    std::thread t2([](){
        std::cout << "Hello world" << std::endl;
    });
    t1.join();
    t2.join();

    // int listenfd = 0, connfd = 0;
    auto timeNow = system_clock::now();
    sockaddr_in serv_addr;
    memset(&serv_addr, '0', sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    inet_pton(AF_INET, address.c_str(), &(serv_addr.sin_addr.s_addr));
    serv_addr.sin_port = htons(port);

    auto listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd < 0) {
        throw std::runtime_error("Failed to open socket");
        return -1;
    }

    if (bind(listenfd, (sockaddr*)&serv_addr, sizeof(serv_addr))) {
        throw std::runtime_error("Failed to bind socket");
        return -1;
    }

    if (listen(listenfd, QUEUEBACKLOG) < 0) {
        throw std::runtime_error("Failed to listen socket");
        return -1;
    }

    char buffer[BUFFER_SIZE];
    memset(buffer, '0', sizeof(buffer));
    // time_t ticks;

    while(1) {
        auto connfd = accept(listenfd, (struct sockaddr*)NULL, NULL);

        // ticks = time(NULL);
        std::stringstream ss;
        time_t now = time(0);
        tm* localtm = localtime(&now);
        ss << "The local date and time is: " << asctime(localtm) << '\n';
        // snprintf(buffer, sizeof(buffer), "%.24s\r\n", ctime(&ticks));
        auto sendText = ss.rdbuf()->str();
        write(connfd, sendText.data(), sendText.length());

        close(connfd);
        std::this_thread::sleep_for(100ms);
    }
    return 0;
}