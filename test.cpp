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

#include "src/HttpMessage.h"

#define BUFFER_SIZE 4096
#define QUEUEBACKLOG 100

using namespace std::chrono;
using namespace std::chrono_literals;

using URLFormat = std::regex;
using HandlerFunction = std::function<void(int)>;
using HeadersMap = std::map<std::string, std::string>;

using namespace HttpMessage;

// std::initializer_list<std::string> requiredHeaders = {"Content-Type", "Content-Length", "Connection"};

// enum class HTTPMethod {
//   GET,
//   HEAD,
//   POST,
//   PUT,
//   DELETE,
//   CONNECT,
//   OPTIONS,
//   TRACE,
//   PATCH
// };

// enum class HTTPVersion {
//   HTTP_1_1 = 11,
//   HTTP_2_0 = 20
// };

// enum class HTTPStatusCode {
//   Continue = 100,
//   OK = 200,
//   Created = 201,
//   Accepted = 202,
//   NonAuthoritativeInformation = 203,
//   NoContent = 204,
//   ResetContent = 205,
//   PartialContent = 206,
//   MultipleChoices = 300,
//   MovedPermanently = 301,
//   Found = 302,
//   NotModified = 304,
//   BadRequest = 400,
//   Unauthorized = 401,
//   Forbidden = 403,
//   NotFound = 404,
//   MethodNotAllowed = 405,
//   RequestTimeout = 408,
//   ImATeapot = 418,
//   InternalServerError = 500,
//   NotImplemented = 501,
//   BadGateway = 502,
//   ServiceUnvailable = 503,
//   GatewayTimeout = 504,
//   HttpVersionNotSupported = 505
// };

// std::size_t content_length(std::istream& iss) {
//   if(iss.bad())
//     return 0;
//   iss.seekg(0, std::ios_base::end);
//   auto size = iss.tellg();
//   iss.seekg(0, std::ios_base::beg);
//   return size;
// }

// struct HTTPResponse {
//   HTTPVersion _version;
//   HTTPMethod _method;
//   HTTPStatusCode _statusCode;
//   HeadersMap _headers;
//   std::istream& _content;

//   HTTPResponse(std::istream& content) : _version(HTTPVersion::HTTP_1_1),
//                                         _method(HTTPMethod::GET),
//                                         _statusCode(HTTPStatusCode::OK),
//                                         _headers(),
//                                         _content(content) {}

//   std::string method_str(const HTTPMethod& method) {
//     switch(method) {
//     case HTTPMethod::GET:
//       return "GET";
//     case HTTPMethod::HEAD:
//       return "HEAD";
//     case HTTPMethod::POST:
//       return "POST";
//     case HTTPMethod::PUT:
//       return "PUT";
//     case HTTPMethod::DELETE:
//       return "DELETE";
//     case HTTPMethod::CONNECT:
//       return "CONNECT";
//     case HTTPMethod::OPTIONS:
//       return "OPTIONS";
//     case HTTPMethod::TRACE:
//       return "TRACE";
//     case HTTPMethod::PATCH:
//       return "PATCH";
//     default:
//       return "";
//     }
//   }

//   std::string version_str(const HTTPVersion& version) {
//     switch(version) {
//     case HTTPVersion::HTTP_1_1:
//       return "HTTP/1.1";
//     case HTTPVersion::HTTP_2_0:
//       return "HTTP/2.0";
//     default:
//       return "";
//     }
//   }

//   friend std::ostream& operator<<(std::ostream& oss, const HTTPResponse& response) {
//     // serialize to stream
//     oss << "HTTP/1.1 200 OK\r\n"; // for testing only
//     for(const auto& [key, value] : response._headers) {
//       oss << key << ": " << value << "\r\n";
//     }
//     oss << "\r\n";
//     oss << response._content.rdbuf();
//     return oss;
//   }
// };

void* get_ip_address(struct sockaddr* sa) {
  if(sa->sa_family == AF_INET) {
    return &(((struct sockaddr_in*)sa)->sin_addr);
  }
  return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(int argc, char* argv[]) {
  // std::map<URLFormat, HandlerFunction> handlerMaps;
  if(argc != 3) {
    std::cerr << "Usage: [executable] <address> <port> \n";
    return -1;
  }
  std::string address(argv[1]);
  auto port = stoi(std::string{argv[2]});

  std::cout << "Simple server start listening on " << address << ":" << port << std::endl;
  // int listenfd = 0, connfd = 0;
  // auto timeNow = system_clock::now();
  sockaddr_in serv_addr;
  memset(&serv_addr, '0', sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = INADDR_ANY;
  inet_pton(AF_INET, address.c_str(), &(serv_addr.sin_addr.s_addr));
  serv_addr.sin_port = htons(port);

  auto listenfd = socket(AF_INET, SOCK_STREAM, 0);
  if(listenfd < 0) {
    throw std::runtime_error("Failed to open socket");
    return -1;
  }

  if(bind(listenfd, (sockaddr*)&serv_addr, sizeof(serv_addr))) {
    throw std::runtime_error("Failed to bind socket");
    return -1;
  }

  if(listen(listenfd, QUEUEBACKLOG) < 0) {
    throw std::runtime_error("Failed to listen socket");
    return -1;
  }

  char buffer[BUFFER_SIZE];
  memset(buffer, '0', sizeof(buffer));

  while(1) {
    sockaddr_storage connAddr;
    char s[INET6_ADDRSTRLEN];
    socklen_t connAddrSize = sizeof(connAddr);
    auto fd = accept(listenfd, (struct sockaddr*)&connAddr, &connAddrSize);
    if(fd == -1) {
      // something went wrong
      std::cerr << "Failed to accept incomming connection\n";
      continue;
    }

    inet_ntop(connAddr.ss_family, get_ip_address((struct sockaddr*)&connAddr), s, sizeof(s));
    std::cout << "Got connection from " << s << std::endl;

    // std::string requestData;
    // recv(fd, requestData.data(), sendText.length(), 0);

    std::stringstream ss;
    std::ifstream index("../index.html");
    HTTPResponse response(index);
    response._headers = {{{"Content-Type", "text/html"},
                          {"Content-Length", std::to_string(content_length(index))},
                          {"Connection", "keep-alive"}}};
    ss << response;
    auto sendText = ss.rdbuf()->str();
    send(fd, sendText.data(), sendText.length(), 0);
    close(fd);
    std::this_thread::sleep_for(10ms);
  }
  return 0;
}