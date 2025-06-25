#pragma once

#include <atomic>
#include <chrono>
#include <cstring>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <regex>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <memory>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

#include "HttpMessage.h"

#include "libs/WorkerPool.h"

#define BUFFER_SIZE      8192
#define QUEUEBACKLOG     100
#define MAX_EPOLL_EVENTS 100

namespace simple_http {

    extern std::map<std::string, std::string> _mimeTypes;
    enum class EventType {
        UNINIT = 0,
        CONN_IO,
        PAIR_IO,
        ACCEPTOR,
    };
    struct EventBase {
        int _fd;
        EventType _type;
        char _eventBuffer[BUFFER_SIZE];
        std::size_t _bytesInBuffer;
        std::size_t bufferCap_;

        EventBase() : _fd(-1), _type(EventType::UNINIT), _eventBuffer(), _bytesInBuffer(0), bufferCap_(0) {}
        EventBase(int fd, EventType type) : _fd(fd), _type(type), _eventBuffer(), _bytesInBuffer(0), bufferCap_(BUFFER_SIZE) {
            // _eventBuffer = (char *) malloc(BUFFER_SIZE);
        }
        virtual ~EventBase() {
            // if (_eventBuffer) {
            //     free(_eventBuffer);
            //     _eventBuffer = nullptr;
            // }
            // ::close(_fd); // this fd should be close in io thread
        }

        void resetBuffer() {
            memset(_eventBuffer, 0, bufferCap_);
        }
    };
    struct PairEventData : public EventBase {
        PairEventData(int fd) : EventBase(fd, EventType::PAIR_IO) {}
    };

    struct ConnData : public EventBase {
        HTTPRequest *req;
        HTTPResponse *res;
        std::pair<std::string, int> addr;

        ConnData() : EventBase(-1, EventType::CONN_IO), req(nullptr), res(nullptr) {}
        ConnData(int fd) : EventBase(fd, EventType::CONN_IO), req(new HTTPRequest()), res(new HTTPResponse()) {}

        // todo: add method init(int clientfd) {
        // set fd
        // reset http req
        // reset http res
        // }

        ~ConnData() {
            cleanupReq();
            cleanupRes();
        }

        void resetData() {
            resetBuffer();
            req->resetData();
            res->resetData();
        }

        void cleanup() {
            cleanupReq();
            cleanupRes();
        }

        void cleanupReq() {
            if (req == nullptr)
                return;
            delete req;
            req = nullptr;
        }

        void cleanupRes() {
            if (res == nullptr)
                return;
            delete res;
            res = nullptr;
        }

        void initResponse() {
            cleanupRes();
            res = new HTTPResponse();
        }

        void initRequest() {
            cleanupReq();
            req = new HTTPRequest();
        }
    };
    // using EventHandler = std::function<void(ConnData *)>;

    struct EpollHandle {
        EpollHandle() : _epollFd(-1), events() {}
        ~EpollHandle() {
            ::close(_epollFd);
        }

        void init() {
            _epollFd = epoll_create1(0);
            if (_epollFd == -1) {
                std::cerr << "failed to create epoll" << std::endl;
                throw std::runtime_error("failed to create epoll");
            }
        }

        void add_or_modify_fd(int clientFd, int eventType, int opt, EventBase *eventData) {
            assert(eventData);
            struct epoll_event event;
            event.data.fd = clientFd;
            event.events = eventType;
            event.data.ptr = eventData;
            if (epoll_ctl(_epollFd, opt, clientFd, &event) == -1) {
                std::cout << "failed to add/modify fd: " << strerror(errno) << std::endl;
                throw std::runtime_error("failed to add/modify fd");
            }
        }

        void delete_fd(int clientFd) {
            if (epoll_ctl(_epollFd, EPOLL_CTL_DEL, clientFd, nullptr) == -1) {
                throw std::runtime_error("failed to delete fd");
            }
            // close(clientFd);
        }

        int _epollFd;
        epoll_event events[MAX_EPOLL_EVENTS];
    };
    class SimpleServer;
    class Acceptor {
    public:
        Acceptor(SimpleServer *server, EpollHandle *epollHandle) : th_(nullptr), server_(server), socketFd_(-1), handle_(epollHandle) {}
        ~Acceptor();

        void start();
        void stop();

    private:
        void initSocket();
        void eventLoop();

    private:
        std::unique_ptr<std::thread> th_;
        SimpleServer *server_;
        int socketFd_;
        EpollHandle *handle_;
        int pair_[2];
    };

    class IOWorker {
    public:
        explicit IOWorker(SimpleServer *server, EpollHandle *epollHandle) : handle_(epollHandle), server_(server), th_(nullptr) {}
        ~IOWorker();
        void start();
        void stop();

    protected:
        void eventLoop();
        void handleRead(ConnData *eventDataPtr);
        void handleWrite(ConnData *eventDataPtr);
        void cleanupEvent(int fd, EventBase *event);

    private:
        static std::atomic_int id_;
        EpollHandle *handle_;
        SimpleServer *server_;
        std::unique_ptr<std::thread> th_;
        int pair_[2];
    };

    class SimpleServer {
    public:
        using URLFormat = std::string;
        using HandlerFunction = std::function<void(HTTPRequest *, HTTPResponse *)>;
        using HandlersMap = std::unordered_map<URLFormat, std::pair<HTTPMethod, HandlerFunction>>;// todo: this should be vector<std::pair<method, handler>>
        using RegexHandlerMap = std::unordered_map<std::regex, std::pair<HTTPMethod, HandlerFunction>>;
        using DefaultHandlersMap = std::map<HTTPMethod, HandlerFunction>;

        struct HandlerTask {
            ConnData *data;
            IOWorker *ioWorker;
            SimpleServer *server;
        };
        using TaskPool = libs::NotifyQueueWorker<HandlerTask>;

    public:
        // SimpleServer() = default;
        SimpleServer(std::string address, unsigned int port, std::size_t poolSize = 1);
        ~SimpleServer();

        void start();
        void stop();
        bool isRunning() const;
        void addHandlers(const HandlersMap &handlers);
        void addHandlers(URLFormat path, HTTPMethod method, HandlerFunction fn);
        void printConnStat() const;

    private:
        void workerFn(HandlerTask &&task);
        std::pair<std::string, int> addrParse(struct sockaddr *sa);
        std::pair<bool, HandlerFunction> getHandler(HTTPMethod method, const std::string &path);
        void addActiveConn(ConnData* conn);
        void removeActiveConn(ConnData* conn);
        static void onExpectContinue(HTTPRequest *req, HTTPResponse *res);
        static void onDefaultGet(HTTPRequest *req, HTTPResponse *res);

    private:
        friend class Acceptor;
        friend class IOWorker;

    private:
        std::string _address;
        unsigned int _port;
        std::size_t _poolSize;
        HandlersMap handlers_;
        DefaultHandlersMap defaultHandlers_;
        std::atomic_bool _stop;
        std::vector<EpollHandle> _epollHandles;// for io workers
        EpollHandle accEpoll_;                 // for acceptor
        std::unique_ptr<Acceptor> acceptor_;
        std::vector<IOWorker *> ioWorkers_;
        mutable std::mutex mtx_;
        std::vector<ConnData* > activeConn_;
        std::stack<ConnData*> cacheConn_;
    };

}// namespace simple_http
