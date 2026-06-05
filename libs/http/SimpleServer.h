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
#include <mutex>
#include <queue>
#include <shared_mutex>
#include <vector>

#include "HttpMessage.h"

#include "WorkerPool.h"

#define BUFFER_SIZE      4096
#define QUEUEBACKLOG     1024
#define MAX_EPOLL_EVENTS 512
// #define MAX_CACHE_CONN   128
// #define CACHE_CONN

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

    struct ConnData : public EventBase, public std::enable_shared_from_this<ConnData> {
        std::shared_ptr<HTTPRequest>  req;
        std::shared_ptr<HTTPResponse> res;
        AddrPair addr;
        std::chrono::steady_clock::time_point deadline_{}; // zero = not scheduled; set by IOWorker::scheduleTimeout

        ConnData() : EventBase(-1, EventType::CONN_IO) {}
        ConnData(int fd)
            : EventBase(fd, EventType::CONN_IO),
              req(std::make_shared<HTTPRequest>(BUFFER_SIZE)),
              res(std::make_shared<HTTPResponse>(BUFFER_SIZE)) {}
        ConnData(int fd, AddrPair &&addr_)
            : EventBase(fd, EventType::CONN_IO),
              req(std::make_shared<HTTPRequest>(BUFFER_SIZE)),
              res(std::make_shared<HTTPResponse>(BUFFER_SIZE)),
              addr(std::move(addr_)) {}

        ~ConnData() = default;

        // Copy: shares req/res ownership with the source (both point to the same objects).
        // enable_shared_from_this weak_ptr is NOT propagated — the copy must be managed by
        // its own shared_ptr before shared_from_this() is callable on it.
        ConnData(const ConnData &o)
            : EventBase(o._fd, o._type), req(o.req), res(o.res),
              addr(o.addr), deadline_(o.deadline_) {}

        ConnData(ConnData &&o) noexcept
            : EventBase(o._fd, o._type), req(std::move(o.req)), res(std::move(o.res)),
              addr(std::move(o.addr)), deadline_(o.deadline_) {
            o._fd = -1; o._type = EventType::UNINIT; o.deadline_ = {};
        }

        ConnData &operator=(const ConnData &o) {
            if (this != &o) {
                _fd = o._fd; _type = o._type;
                req = o.req; res = o.res; addr = o.addr; deadline_ = o.deadline_;
            }
            return *this;
        }

        ConnData &operator=(ConnData &&o) noexcept {
            if (this != &o) {
                _fd = o._fd; _type = o._type;
                req = std::move(o.req); res = std::move(o.res);
                addr = std::move(o.addr); deadline_ = o.deadline_;
                o._fd = -1; o._type = EventType::UNINIT; o.deadline_ = {};
            }
            return *this;
        }

        void resetData()    { req->resetData(); res->resetData(); }
        void cleanup()      { req.reset(); res.reset(); }
        void cleanupReq()   { req.reset(); }
        void cleanupRes()   { res.reset(); }
        void initResponse() { res = std::make_shared<HTTPResponse>(BUFFER_SIZE); }
        void initRequest()  { req = std::make_shared<HTTPRequest>(BUFFER_SIZE); }
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
            if (epoll_ctl(_epollFd, EPOLL_CTL_DEL, clientFd, nullptr) == -1)
                fprintf(stderr, "epoll_ctl DEL fd %d: %s\n", clientFd, strerror(errno));
            // always return — caller owns close() and object lifetime
        }

        int _epollFd;
        epoll_event events[MAX_EPOLL_EVENTS];
    };
    class SimpleServer;
    class Acceptor {
    public:
        Acceptor(SimpleServer *server) : th_(nullptr), server_(server), handle_(), socketFd_(-1), pipes_{-1, -1} {}
        ~Acceptor();

        void start();
        bool stop();

    private:
        void initSocket();
        void eventLoop();

    private:
        std::unique_ptr<std::thread> th_;
        SimpleServer *server_;
        EpollHandle handle_;
        int socketFd_;
        int pipes_[2];
    };

    class IOWorker {
    public:
        explicit IOWorker(SimpleServer *server) : handle_(), server_(server), th_(nullptr), pipes_{-1, -1}, completionPipes_{-1, -1} {}
        ~IOWorker();
        void start();
        bool stop();
        void addConn(int fd, AddrPair &&addr);
        void notifyHandlerDone(ConnData *conn);

        // Per-worker stats to avoid false sharing: a single shared Stat on SimpleServer would
        // pack all atomics into one cache line, causing every write from any IO worker or handler
        // thread to invalidate the line on all other cores. Owning stat here means each worker
        // writes only to its own cache line; getStat() aggregates on the cold read path.
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
        Stat stat_;

    protected:
        void eventLoop();
        void handleRead(ConnData *eventDataPtr);
        void handleWrite(ConnData *eventDataPtr);
        void cleanupEvent(EventBase *event);
        void drainCompletions();
        void scheduleTimeout(ConnData *conn, std::chrono::seconds duration);
        void checkTimeouts();
        int nextDeadlineMs();

    private:
        struct TimeoutEntry {
            std::chrono::steady_clock::time_point deadline;
            ConnData *conn;
            bool operator>(const TimeoutEntry &o) const { return deadline > o.deadline; }
        };

        static std::atomic_int id_;
        EpollHandle handle_;
        SimpleServer *server_;
        std::unique_ptr<std::thread> th_;
        int pipes_[2];
        int completionPipes_[2];
        std::priority_queue<TimeoutEntry, std::vector<TimeoutEntry>, std::greater<TimeoutEntry>> timeoutHeap_;
        std::mutex timeoutMtx_;
    };

    class SimpleServer {
    public:
        using URLFormat = std::string;
        using HandlerFunction = std::function<void(HTTPRequest *, HTTPResponse *)>;
        struct HandlerInfo {
            std::regex pathRegex;
            std::vector<HandlerFunction> funcs;

            HandlerInfo() : pathRegex(), funcs(static_cast<int>(HTTPMethod::_SIZE_)) {}
        };

        using HandlersMap = std::unordered_map<URLFormat, HandlerInfo>;
        using DefaultHandlersMap = std::map<HTTPMethod, HandlerFunction>;

        struct HandlerTask {
            ConnData *data;
            IOWorker *ioWorker;
            SimpleServer *server;
        };
        using TaskPool = libs::NotifyQueueWorker<HandlerTask>;
        struct StatVal {
            size_t successReq{0};
            int activeConn{0};
            int failedReq{0};
            int cacheConn{0};
            int dropConn{0};
            int closedConn{0};
            int createdConn{0};
        };
        // todo: should write a override function that auto assignment Stat --> StatVal

    public:
        SimpleServer(std::string address, unsigned int port, std::size_t poolSize = 1);
        ~SimpleServer();

        void start();
        void stop();
        bool isRunning() const;
        void addHandler(URLFormat path, HTTPMethod method, HandlerFunction fn);
        void addHandlers(const std::vector<std::tuple<HTTPMethod, URLFormat, HandlerFunction>>& handlers);
        void setDefaultHandler(HTTPMethod method, HandlerFunction fn);
        StatVal getStat() const;
        void setReadTimeout(std::chrono::seconds d) { readTimeout_ = d; }
        void setIdleTimeout(std::chrono::seconds d) { idleTimeout_ = d; }
        // bool changePort(int newPort);
        // void changePoolSize(int newSize);

    private:
        // void workerFn(HandlerTask &&task);
        AddrPair addrParse(struct sockaddr *sa);
        HandlerFunction getHandler(HTTPMethod method, const std::string &path);
        void addConnection(int i, int fd, AddrPair &&addr);
        ConnData *getOrCreateConn(int fd, AddrPair &&addr);
        bool pushCacheConn(ConnData *conn);

    private:
        static void onExpectContinue(HTTPRequest *req, HTTPResponse *res);

    private:
        friend class Acceptor;
        friend class IOWorker;

    private:
        std::string _address;
        unsigned int _port;
        std::size_t _poolSize;
        std::chrono::seconds readTimeout_{30};
        std::chrono::seconds idleTimeout_{60};
        HandlersMap handlers_;
        DefaultHandlersMap defaultHandlers_;
        std::shared_mutex handlerMtx_;
        std::atomic_bool _stop;
        std::unique_ptr<Acceptor> acceptor_;
        std::vector<IOWorker *> ioWorkers_;
        std::unique_ptr<TaskPool> taskPool_;
#ifdef CACHE_CONN
        std::stack<ConnData *> cacheConn_;
        mutable std::mutex cacheConnMtx_;// for caching connections
        std::condition_variable cv_;     // notify when cached connection available
#endif
    };

}// namespace libs::http
