#include <iostream>
#include <string>
#include <thread>

int main(int argc, char* argv[]) {
    std::thread t1([](){
        std::cout << "Hello world" << std::endl;
    });
    std::thread t2([](){
        std::cout << "Hello world" << std::endl;
    });
    t1.join();
    t2.join();
    return 0;
}