#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define MAX_EVENTS 10

int main(int argc, char* argv[]) {
  int server_sockfd, client_sockfd;
  struct sockaddr_in server_addr, client_addr;
  socklen_t client_len = sizeof(client_addr);
  struct epoll_event event, events[MAX_EVENTS];
  int epollfd, nfds, n;
  char buffer[256];

  // Create a socket
  server_sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if(server_sockfd < 0) {
    perror("socket");
    exit(EXIT_FAILURE);
  }

  // Bind the socket to an IP address and port
  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  server_addr.sin_port = htons(8080);
  if(bind(server_sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
    perror("bind");
    exit(EXIT_FAILURE);
  }

  // Listen for incoming connections
  if(listen(server_sockfd, 5) < 0) {
    perror("listen");
    exit(EXIT_FAILURE);
  }

  // Create an epoll instance
  epollfd = epoll_create1(0);
  if(epollfd == -1) {
    perror("epoll_create1");
    exit(EXIT_FAILURE);
  }

  // Register the listening socket with epoll
  event.data.fd = server_sockfd;
  event.events = EPOLLIN;
  if(epoll_ctl(epollfd, EPOLL_CTL_ADD, server_sockfd, &event) == -1) {
    perror("epoll_ctl: listen_sockfd");
    exit(EXIT_FAILURE);
  }

  while(1) {
    // Wait for events
    nfds = epoll_wait(epollfd, events, MAX_EVENTS, -1);
    if(nfds == -1) {
      perror("epoll_wait");
      exit(EXIT_FAILURE);
    }

    // Handle events
    for(n = 0; n < nfds; ++n) {
      if(events[n].data.fd == server_sockfd) {
        // Accept new connections
        client_sockfd = accept(server_sockfd, (struct sockaddr*)&client_addr, &client_len);
        if(client_sockfd == -1) {
          perror("accept");
          exit(EXIT_FAILURE);
        }

        // Set the client socket to non-blocking mode
        int flags = fcntl(client_sockfd, F_GETFL, 0);
        if(flags == -1) {
          perror("fcntl: F_GETFL");
          exit(EXIT_FAILURE);
        }
        flags |= O_NONBLOCK;
        if(fcntl(client_sockfd, F_SETFL, flags) == -1) {
          perror("fcntl: F_SETFL");
          exit(EXIT_FAILURE);
        }

        // Register the new client socket with epoll
        event.data.fd = client_sockfd;
        event.events = EPOLLIN | EPOLLET;
        if(epoll_ctl(epollfd, EPOLL_CTL_ADD, client_sockfd, &event) == -1) {
          perror("epoll_ctl: client_sockfd");
          exit(EXIT_FAILURE);
        }
      }
      else {
        // Read incoming data from the client socket
        int fd = events[n].data.fd;
        int len = recv(fd, buffer, sizeof(buffer), 0);
        if(len == -1) {
          if(errno != EAGAIN && errno != EWOULDBLOCK) {
            perror("recv");
            exit(EXIT_FAILURE);
          }
        }
        else if(len == 0) {
          // Connection closed by client
          close(fd);
          epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, NULL);
        }
        else {
          // Handle the HTTP request
          // ...

          // Write the HTTP response to the client socket
          const char* response = "HTTP/1.1 200 OK\r\nContent-Length: 12\r\n\r\nHello World!";
          if(send(fd, response, strlen(response), 0) == -1) {
            perror("send");
            exit(EXIT_FAILURE);
          }
        }
      }
    }
  }

  return 0;
}