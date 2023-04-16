#pragma once

#include <iostream>
#include <string>
#include <thread>

#include <arpa/inet.h>
#include <cstring>
#include <fstream>
#include <functional>
#include <map>
#include <netinet/in.h>
#include <regex>
#include <sstream>
#include <stdio.h>
#include <sys/socket.h>
#include <unistd.h>
#include <unordered_map>

#include <chrono>

#include "HttpMessage.h"

#define BUFFER_SIZE 4096
#define QUEUEBACKLOG 100

using namespace std::chrono;
using namespace std::chrono_literals;

using URLFormat = std::regex;
using HandlerFunction = std::function<void(int)>;

class Server {
    std::unordered_map<std::regex, 
}