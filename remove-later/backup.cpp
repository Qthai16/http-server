  auto handler1 = [](int fd) {
    std::stringstream ss;
    time_t now = time(0);
    tm* localtm = localtime(&now);
    ss << "The local date and time is: " << asctime(localtm) << '\n';
    // snprintf(buffer, sizeof(buffer), "%.24s\r\n", ctime(&ticks));
    auto sendText = ss.rdbuf()->str();
    write(fd, sendText.data(), sendText.length());

    close(fd);
  };

  auto handler2 = [](int fd) {
    std::stringstream ss;
    ss << "Hello from server" << '\n';
    auto sendText = ss.rdbuf()->str();
    write(fd, sendText.data(), sendText.length());

    close(fd);
  };


// below is logic to handle http request, send it to worker thread

memset(_buffer, 0, sizeof(_buffer));
ssize_t byte_count = recv(clientfd, _buffer, sizeof(_buffer), 0);
if(byte_count > 0) {
  std::stringstream ss;
  ss << _buffer;
  HTTPRequest req(ss);
  // req.to_string(); for debug
  // matching using regex
  if(_handlersMap.count(req._path) && _handlersMap.at(req._path).first == req._method) {
    // call registered handler
    _handlersMap.at(req._path).second(clientfd);
  }
}
else if(byte_count == 0) {
  close(clientfd);
  continue;
}