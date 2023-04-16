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