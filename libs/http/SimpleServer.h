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

#include "WorkerPool.h"

#define BUFFER_SIZE      8192
#define QUEUEBACKLOG     1024
#define MAX_EPOLL_EVENTS 512
#define MAX_CACHE_CONN   128
// #define RES_BUF_SIZE     8192
#define CACHE_CONN

namespace libs::http {

    extern std::map<std::string, std::string> _mimeTypes;
    enum class EventType {
        UNINIT = 0,
        CONN_IO,
        PAIR_IO,
        ACCEPTOR,
    };
    using AddrPair = std::pair<std::string, int>;
    struct EventBase {
        int _fd;
        EventType _type;

        EventBase() : _fd(-1), _type(EventType::UNINIT) {}
        EventBase(int fd, EventType type) : _fd(fd), _type(type) {
        }
        virtual ~EventBase() {}
    };
    struct PairEventData : public EventBase {
        PairEventData(int fd) : EventBase(fd, EventType::PAIR_IO), _eventBuffer(), _bytesInBuffer(0), bufferCap_(sizeof(_eventBuffer)) {}
        ~PairEventData() override = default;

        void resetBuffer() {
            memset(_eventBuffer, 0, bufferCap_);
        }

        char _eventBuffer[128];
        std::size_t _bytesInBuffer;
        std::size_t bufferCap_;
    };

    struct ConnData : public EventBase {
        HTTPRequest *req;
        HTTPResponse *res;
        AddrPair addr;

        ConnData() : EventBase(-1, EventType::CONN_IO), req(nullptr), res(nullptr), addr() {}
        ConnData(int fd) : EventBase(fd, EventType::CONN_IO), req(new HTTPRequest(BUFFER_SIZE)), res(new HTTPResponse(BUFFER_SIZE)), addr() {}
        ConnData(int fd, AddrPair &&addr_) : EventBase(fd, EventType::CONN_IO), req(new HTTPRequest(BUFFER_SIZE)), res(new HTTPResponse(BUFFER_SIZE)), addr(addr_) {}

        ~ConnData() {
            cleanupReq();
            cleanupRes();
        }

        void resetData() {
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
            res = new HTTPResponse(BUFFER_SIZE);
        }

        void initRequest() {
            cleanupReq();
            req = new HTTPRequest(BUFFER_SIZE);
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

        void add_or_modify_fd(int clientFd, int eventType, int opt, void *eventData) {
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
        Acceptor(SimpleServer *server, EpollHandle *epollHandle) : th_(nullptr), server_(server),
                                                                   sockEventData_(), pairEventData_(), socketFd_(-1), handle_(epollHandle) {}
        ~Acceptor();

        void start();
        bool stop();

    private:
        void initSocket();
        void eventLoop();

    private:
        std::unique_ptr<std::thread> th_;
        SimpleServer *server_;
        std::unique_ptr<EventBase> sockEventData_;
        std::unique_ptr<EventBase> pairEventData_;
        int socketFd_;
        EpollHandle *handle_;
        int pipes_[2];
    };

    class IOWorker {
    public:
        explicit IOWorker(SimpleServer *server) : handle_(), server_(server), th_(nullptr) {}
        ~IOWorker();
        void start();
        bool stop();
        void addConn(int fd, AddrPair &&addr);

    protected:
        void eventLoop();
        void handleRead(ConnData *eventDataPtr);
        void handleWrite(ConnData *eventDataPtr);
        void cleanupEvent(EventBase *event);

    private:
        static std::atomic_int id_;
        std::unique_ptr<EpollHandle> handle_;
        SimpleServer *server_;
        std::unique_ptr<std::thread> th_;
        int pipes_[2];
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
        struct Stat {
            std::atomic<size_t> successReq{0};
            std::atomic_int activeConn{0};
            std::atomic_int failedReq{0};
            std::atomic_int dropConn{0};
            std::atomic_int closedConn{0};
            std::atomic_int createdConn{0};

            inline void incActiveConn() {
                activeConn.fetch_add(1, std::memory_order_relaxed);
            }

            inline void decActiveConn() {
                activeConn.fetch_sub(1, std::memory_order_relaxed);
            }

            inline void incSuccessReq() {
                successReq.fetch_add(1, std::memory_order_relaxed);
            }

            inline void incFailedReq() {
                failedReq.fetch_add(1, std::memory_order_relaxed);
            }

            inline void incCreatedConn() {
                createdConn.fetch_add(1, std::memory_order_relaxed);
            }

            inline void incDropConn() {
                dropConn.fetch_add(1, std::memory_order_relaxed);
            }
        };
        struct StatVal {
            size_t successReq{0};
            int activeConn{0};
            int failedReq{0};
            int cacheConn{0};
            int dropConn{0};
            int closedConn{0};
            int createdConn{0};
        };
        // should write a override function that auto assignment Stat --> StatVal

    public:
        SimpleServer(std::string address, unsigned int port, std::size_t poolSize = 1);
        ~SimpleServer();

        void start();
        void stop();
        bool isRunning() const;
        void addHandlers(const HandlersMap &handlers);
        void addHandlers(URLFormat path, HTTPMethod method, HandlerFunction fn);
        StatVal getStat() const;

    private:
        // void workerFn(HandlerTask &&task);
        AddrPair addrParse(struct sockaddr *sa);
        std::pair<bool, HandlerFunction> getHandler(HTTPMethod method, const std::string &path);
        void addConnection(int i, int fd, AddrPair &&addr);
        ConnData *getOrCreateConn(int fd, AddrPair &&addr);
        bool pushCacheConn(ConnData *conn);

    private:
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
        EpollHandle accEpoll_;// for acceptor
        std::unique_ptr<Acceptor> acceptor_;
        std::vector<IOWorker *> ioWorkers_;
#ifdef CACHE_CONN
        std::stack<ConnData *> cacheConn_;
        mutable std::mutex cacheConnMtx_;// for caching connections
        std::condition_variable cv_;     // notify when cached connection available
#endif
        Stat stat_;
    };

}// namespace libs::http
