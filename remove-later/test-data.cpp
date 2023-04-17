// std::ifstream ifs("../request-chrome.txt");
// HTTPRequest request(ifs);
// std::cout << std::boolalpha << (HttpMessage::trim_str("      fsdflakjdfalkd   \r\n   \r\r   ") == std::string{"fsdflakjdfalkd"}) << std::endl;
// std::cout << std::boolalpha << (HttpMessage::trim_str("\r\n").empty()) << std::endl;
// std::cout << std::boolalpha << (HttpMessage::trim_str("123456789       \r\n") == std::string{"123456789"}) << std::endl;
// std::cout << std::boolalpha << (HttpMessage::trim_str("\r\n         123456789") == std::string{"123456789"}) << std::endl;