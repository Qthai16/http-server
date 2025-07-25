#include "SimpleServer.h"
#include "libs/StrUtils.h"

#include <sys/poll.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <atomic>

using namespace std::chrono_literals;

// todo: cleanup connection (close fd, delete event) when server stop

#include <chrono>
class SimpleTimer {
public:
    SimpleTimer(const char *name) : begin_(std::chrono::steady_clock::now()), name_(name) {}
    ~SimpleTimer() {
        if (!stop_)
            stop();
    }
    void stop() {
        auto end = std::chrono::steady_clock::now();
        fprintf(stderr, "  [%s]: %ldus\n", name_.c_str(), std::chrono::duration_cast<std::chrono::microseconds>(end - begin_).count());
        stop_ = true;
    }

private:
    std::chrono::steady_clock::time_point begin_;
    std::string name_;
    bool stop_;
};

namespace simple_http {
    std::map<std::string, std::string> _mimeTypes = {
            {".js", "text/javascript"},
            {".txt", "text/plain"},
            {".html", "text/html"},
            {".htm", "text/html"},
            {".svg", "image/svg+xml"},
            {".png", "image/png"},
            {".jpg", "image/jpeg"},
            {".jpeg", "image/jpeg"},
            {".css", "text/css"}};
    std::atomic_int IOWorker::id_{0};

    void Acceptor::initSocket() {
        auto addr = server_->_address.c_str();
        auto port = server_->_port;
        struct ::sockaddr_in serv_addr;
        memset(&serv_addr, '0', sizeof(serv_addr));
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_addr.s_addr = INADDR_ANY;
        ::inet_pton(AF_INET, addr, &(serv_addr.sin_addr.s_addr));
        serv_addr.sin_port = ::htons(port);

        int rv = ::socketpair(AF_UNIX, SOCK_STREAM, 0, pair_);
        if (rv != 0)
            throw std::runtime_error("socket pair failed");

        socketFd_ = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
        if (socketFd_ < 0)
            throw std::runtime_error("Failed to open socket");

        int enable = 1;
        if (::setsockopt(socketFd_, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &enable, sizeof(enable)) < 0)
            throw std::runtime_error("Failed to set socket options");

        if (::bind(socketFd_, (sockaddr *) &serv_addr, sizeof(serv_addr)))
            throw std::runtime_error("Failed to bind socket");

        if (::listen(socketFd_, QUEUEBACKLOG) < 0)
            throw std::runtime_error("Failed to listen socket");
        sockEventData_.reset(new EventBase(socketFd_, EventType::ACCEPTOR));
        pairEventData_.reset(new PairEventData(pair_[0]));
        handle_->add_or_modify_fd(socketFd_, EPOLLIN, EPOLL_CTL_ADD, sockEventData_.get());
        handle_->add_or_modify_fd(pair_[0], EPOLLIN, EPOLL_CTL_ADD, pairEventData_.get());
    }

    void Acceptor::eventLoop() {
        auto ind = 0;// round robin
        auto workerSize = server_->ioWorkers_.size();
        while (server_->isRunning()) {
            auto nfds = ::epoll_wait(handle_->_epollFd, handle_->events, MAX_EPOLL_EVENTS, -1);// wait forever
            if (nfds <= 0)
                continue;
            for (auto i = 0; i < nfds; i++) {
                auto event = reinterpret_cast<EventBase *>(handle_->events[i].data.ptr);
                auto eventTypes = handle_->events[i].events;
                switch (event->_type) {
                    case EventType::PAIR_IO: {
                        if (eventTypes & EPOLLIN) {
                            event->_bytesInBuffer = ::recv(event->_fd, event->_eventBuffer, event->bufferCap_, 0);
                            assert(event->_bytesInBuffer == 1);
                            printf("acceptor interrupted\n");
                            return;
                        }
                    } break;
                    case EventType::ACCEPTOR: {
                        assert(event->_fd == socketFd_);
                        struct ::sockaddr_storage connAddr;
                        ::socklen_t connAddrSize = sizeof(connAddr);
                        auto clientfd = accept4(socketFd_, (struct sockaddr *) &connAddr, &connAddrSize, SOCK_NONBLOCK);
                        if (clientfd < 0) {
                            int errno_copy = errno;
                            if (errno_copy == EAGAIN || errno_copy == EWOULDBLOCK) {
                                break;// no more connections
                            } else {
                                printf("Acceptor::start() accept() errno: %s\n", strerror(errno_copy));
                                perror("accept failed");
                                break;
                            }
                        }

                        auto addrPair = server_->addrParse((struct sockaddr *) &connAddr);
                        // printf("New connection: %s:%d\n", addrPair.first.c_str(), addrPair.second);
                        // todo: change this to addNewConnCallback, should not directly modify server epoll handle
                        server_->incActiveConn();
                        server_->addConnection(ind++, clientfd, std::move(addrPair));
                        if (ind >= workerSize) ind = 0;
                    } break;
                    default:
                        printf("unknown event: %d\n", static_cast<int>(event->_type));
                        assert(0);
                }
            }
        }
    }

    void Acceptor::start() {
        initSocket();
        th_ = std::make_unique<std::thread>([this]() {
            printf("Acceptor started\n");
            eventLoop();
            printf("Acceptor stopped\n");
        });
    }

    bool Acceptor::stop() {
        static char buf[1] = {'\0'};
        assert(pair_[1] > 0);
        auto rv = write(pair_[1], buf, 1);
        return rv == 1;
    }

    Acceptor::~Acceptor() {
        // stop();
        if (th_) {
            th_->join();
            th_.reset();
        }
        if (socketFd_ > 0) ::close(socketFd_);
        ::close(pair_[0]);
        ::close(pair_[1]);
        pair_[0] = pair_[1] = -1;
    }

    IOWorker::~IOWorker() {
        if (th_.get()) {
            th_->join();
            th_.reset();
        }
        ::close(pair_[0]);
        ::close(pair_[1]);
        pair_[0] = pair_[1] = -1;
    }

    void IOWorker::start() {
        auto id = id_.fetch_add(1, std::memory_order_acq_rel);
        int rv = ::socketpair(AF_UNIX, SOCK_STREAM, 0, pair_);
        if (rv != 0)
            throw std::runtime_error("socket pair failed");
        handle_.reset(new EpollHandle());
        handle_->init();
        auto stopEvent = new PairEventData(pair_[0]);
        handle_->add_or_modify_fd(pair_[0], EPOLLIN, EPOLL_CTL_ADD, stopEvent);
        th_ = std::make_unique<std::thread>([this, id]() {
            printf("IOWorker[%d] started\n", id);
            eventLoop();
            printf("IOWorker[%d] stopped\n", id);
        });
    }

    bool IOWorker::stop() {
        static char buf[1] = {'\0'};
        assert(pair_[1] > 0);
        auto rv = write(pair_[1], buf, 1);
        return rv == 1;
    }

    void IOWorker::addConn(int fd, std::pair<std::string, int> &&addr) {
        auto eventData = server_->getOrCreateConn(fd, std::move(addr));
        handle_->add_or_modify_fd(fd, EPOLLIN, EPOLL_CTL_ADD, eventData);// leveled triggered
    }

    void IOWorker::eventLoop() {
        while (server_->isRunning()) {
            auto nfds = ::epoll_wait(handle_->_epollFd, handle_->events, MAX_EPOLL_EVENTS, -1);// wait forever
            if (nfds <= 0) {
                continue;
            }
            for (auto i = 0; i < nfds; i++) {
                auto event = reinterpret_cast<EventBase *>(handle_->events[i].data.ptr);
                auto fd = event->_fd;
                auto eventTypes = handle_->events[i].events;
                switch (event->_type) {
                    case EventType::PAIR_IO: {
                        if (eventTypes & (EPOLLHUP | EPOLLERR)) {
                            cleanupEvent(event);
                        } else if (eventTypes & EPOLLIN) {
                            event->_bytesInBuffer = ::recv(fd, event->_eventBuffer, event->bufferCap_, 0);
                            assert(event->_bytesInBuffer == 1);
                            delete event;
                            event = nullptr;
                            // printf("iothread interrupted\n");
                            return;
                        } else if (eventTypes & EPOLLOUT) {
                            printf("unhandled pair epoll out\n");
                            assert(0);
                        } else {
                            printf("unknown event\n");
                            assert(0);
                        }
                    } break;
                    case EventType::CONN_IO: {
                        auto reqEventData = (ConnData *) event;
                        if (eventTypes & (EPOLLHUP | EPOLLERR)) {// errors or event is not handled
                            // printf("connection close: %d\n", eventTypes);
                            cleanupEvent(reqEventData);
                        } else if (eventTypes & EPOLLIN) {
                            // todo: add total request metrics
                            handleRead(reqEventData);
                        } else if (eventTypes & EPOLLOUT) {
                            handleWrite(reqEventData);
                        } else {
                            printf("unknown event\n");
                            assert(0);
                        }
                    } break;
                    default:
                        printf("invalid type\n");
                        assert(0);
                }
            }
            // todo: process the remain requests
        }
    }

    void IOWorker::handleRead(ConnData *eventDataPtr) {
        try {
            assert(eventDataPtr);
            auto fd = eventDataPtr->_fd;
            auto httpReqPtr = eventDataPtr->req;
            auto byte_count = ::recv(fd, eventDataPtr->_eventBuffer, eventDataPtr->bufferCap_, 0);
            if (byte_count > 0) {
                eventDataPtr->_bytesInBuffer = byte_count;
                httpReqPtr->parse_request(eventDataPtr->_eventBuffer, eventDataPtr->_bytesInBuffer);
                if (httpReqPtr->have_expect_continue()) {
                    printf("expect continue\n");
                    httpReqPtr->_expectContinue = false;
                    auto tmpEvent = std::make_unique<ConnData>(fd);
                    server_->onExpectContinue(tmpEvent->req, tmpEvent->res);
                    tmpEvent->_bytesInBuffer = tmpEvent->res->serialize_reponse(tmpEvent->_eventBuffer, tmpEvent->bufferCap_);
                    // send here in order to not mess up with handleWrite state
                    ::send(fd, tmpEvent->_eventBuffer, tmpEvent->_bytesInBuffer, MSG_NOSIGNAL);
                }
                if (!httpReqPtr->request_completed()) {// still have bytes to read
                    handle_->add_or_modify_fd(fd, EPOLLIN, EPOLL_CTL_MOD, eventDataPtr);
                } else {// read done
                    auto httpResPtr = eventDataPtr->res;
                    // eventDataPtr->resetBuffer();
                    auto pair = server_->getHandler(httpReqPtr->_method, httpReqPtr->_path);
                    if (pair.first) {
                        pair.second(httpReqPtr, httpResPtr);// call registered handler
                        // serialize first part of response, the rest of body will be handle in HandleWriteEvent
                        // eventDataPtr->_bytesInBuffer = httpResPtr->serialize_reponse(eventDataPtr->_eventBuffer, BUFFER_SIZE);
                        handle_->add_or_modify_fd(fd, EPOLLOUT, EPOLL_CTL_MOD, eventDataPtr);
                    } else {
                        if (server_->defaultHandlers_.count(httpReqPtr->_method)) {// not registered path
                            server_->defaultHandlers_.at(httpReqPtr->_method)(httpReqPtr, httpResPtr);
                            // eventDataPtr->_bytesInBuffer = httpResPtr->serialize_reponse(eventDataPtr->_eventBuffer, BUFFER_SIZE);
                            handle_->add_or_modify_fd(fd, EPOLLOUT, EPOLL_CTL_MOD, eventDataPtr);
                        } else {
                            // method not supported or default handler for method not found
                        }
                    }
                }
            } else if ((byte_count < 0) && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                // no data available, try again
                printf("handleRead: retry\n");
                eventDataPtr->resetData();
                handle_->add_or_modify_fd(fd, EPOLLIN, EPOLL_CTL_MOD, eventDataPtr);
            } else {
                // byte_count = 0 (connection close by client) and other errors
                cleanupEvent(eventDataPtr);
            }
        } catch (const std::exception &e) {
            fprintf(stderr, "IOWorker::handleRead exception: %s\n", e.what());
            cleanupEvent(eventDataPtr);
            return;
        } catch (...) {
            fprintf(stderr, "IOWorker::handleRead unknown exception\n");
            cleanupEvent(eventDataPtr);
            return;
        }
    }

    void IOWorker::handleWrite(ConnData *eventDataPtr) {
        assert(eventDataPtr);
        auto fd = eventDataPtr->_fd;
        if (eventDataPtr->res->writeDone()) {
            eventDataPtr->resetData();
            handle_->add_or_modify_fd(fd, EPOLLIN, EPOLL_CTL_MOD, eventDataPtr);
            // todo: add success request metrics
            return;
        }
        eventDataPtr->_bytesInBuffer = eventDataPtr->res->serialize_reponse(eventDataPtr->_eventBuffer, eventDataPtr->bufferCap_);
        auto sendBytes = ::send(fd, eventDataPtr->_eventBuffer, eventDataPtr->_bytesInBuffer, MSG_NOSIGNAL);
        if (sendBytes >= 0) {
            handle_->add_or_modify_fd(fd, EPOLLOUT, EPOLL_CTL_MOD, eventDataPtr);
            if (!eventDataPtr->res->writeDone()) {// still have bytes to send
                // eventDataPtr->_bytesInBuffer = eventDataPtr->res->serialize_reponse(eventDataPtr->_eventBuffer, BUFFER_SIZE);
                handle_->add_or_modify_fd(fd, EPOLLOUT, EPOLL_CTL_MOD, eventDataPtr);
            } else {
                eventDataPtr->resetData();
                handle_->add_or_modify_fd(fd, EPOLLIN, EPOLL_CTL_MOD, eventDataPtr);
            }
        } else {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {// retry
                // todo: how to test this condition??
                printf("handleWrite: retry\n");
                handle_->add_or_modify_fd(fd, EPOLLOUT, EPOLL_CTL_ADD, eventDataPtr);
            } else {
                cleanupEvent(eventDataPtr);
            }
        }
    }

    void IOWorker::cleanupEvent(EventBase *event) {
        auto connData = dynamic_cast<ConnData *>(event);
        handle_->delete_fd(event->_fd);
        ::close(event->_fd);
        if (connData) {
            // printf("cleanup conn: %s:%d\n", connData->addr.first.c_str(), connData->addr.second);
            server_->decActiveConn();
            server_->pushCacheConn(connData);
            return;
        }
        delete event;
        event = nullptr;
    }

    SimpleServer::~SimpleServer() {
        stop();
    }

    SimpleServer::SimpleServer(std::string address, unsigned int port, std::size_t poolSize) : _address(address),
                                                                                               _port(port),
                                                                                               _poolSize(poolSize),
                                                                                               handlers_(),
                                                                                               defaultHandlers_(),
                                                                                               _stop(false),
                                                                                               acceptor_(nullptr),
                                                                                               ioWorkers_() {
        defaultHandlers_[HTTPMethod::GET] = &SimpleServer::onDefaultGet;
    }

    void SimpleServer::start() {
        ioWorkers_.resize(_poolSize, nullptr);
        for (auto i = 0; i < _poolSize; i++) {
            ioWorkers_[i] = new IOWorker(this);
        }
        accEpoll_.init();
        acceptor_.reset(new Acceptor(this, &accEpoll_));
        acceptor_->start();

        for (auto &w: ioWorkers_) {
            w->start();
        }
        printf("Simple server start listening on %s:%d\n", _address.c_str(), _port);
    }

    void SimpleServer::stop() {
        if (!_stop.exchange(true)) {
            acceptor_->stop();
            acceptor_.reset();// wait acceptor join
            for (auto &io: ioWorkers_) {
                io->stop();
                delete io;
                io = nullptr;
            }
            printf("cached connections: %d\n", static_cast<int>(cacheConn_.size()));
            while (!cacheConn_.empty()) {
                auto conn = cacheConn_.top();
                cacheConn_.pop();
                delete conn;
            }
        }
    }

    bool SimpleServer::isRunning() const {
        return !_stop.load(std::memory_order_acquire);
    }

    void SimpleServer::addHandlers(const HandlersMap &handlers) {
        for (const auto &ele: handlers) {
            handlers_[ele.first] = ele.second;
        }
    }
    void SimpleServer::addHandlers(URLFormat path, HTTPMethod method, HandlerFunction fn) {
        handlers_[path] = {method, fn};
    }

    int SimpleServer::getActiveConnStat() const {
        return activeConnCnt.load(std::memory_order_relaxed);
    }

    std::pair<std::string, int> SimpleServer::addrParse(struct sockaddr *sa) {
        if (!sa || sa->sa_family != AF_INET && sa->sa_family != AF_INET6) {
            return {};
        }
        char s[INET6_ADDRSTRLEN];
        void *host;
        int port;
        if (sa->sa_family == AF_INET) {
            host = &(((struct sockaddr_in *) sa)->sin_addr);
            port = ((struct sockaddr_in *) sa)->sin_port;
        } else {
            host = &(((struct sockaddr_in6 *) sa)->sin6_addr);
            port = ((struct sockaddr_in6 *) sa)->sin6_port;
        }
        ::inet_ntop(sa->sa_family, host, s, sizeof(s));
        return {std::string(s, sizeof(s)), port};
    }

    std::pair<bool, SimpleServer::HandlerFunction> SimpleServer::getHandler(HTTPMethod method, const std::string &path) {
        // exact match first
        // todo: getHandler are called by multiple io thread without mutex
        std::pair<bool, SimpleServer::HandlerFunction> ret;
        ret.first = false;
        auto iter = handlers_.find(path);
        if (iter != handlers_.end()) {
            ret.first = (method == iter->second.first);
            ret.second = iter->second.second;
            return ret;
        }
        // todo: move to regex handler
        // try regex match
        for (const auto &ele: handlers_) {
            auto pathRegex = std::regex(ele.first);// todo: this regex should be pre-built
            if (std::regex_match(path, pathRegex)) {
                ret.first = (method == ele.second.first);
                ret.second = ele.second.second;
                return ret;
            }
        }
        return ret;
    }

    void SimpleServer::addConnection(int i, int fd, AddrPair &&addr) {
        if (i < 0 || i >= ioWorkers_.size()) return;
        ioWorkers_[i]->addConn(fd, std::move(addr));
    }

    ConnData *SimpleServer::getOrCreateConn(int fd, AddrPair &&addr) {
        std::unique_lock<std::mutex> lock(mtx_);
        if (!cacheConn_.empty()) {
            auto conn = cacheConn_.top();
            assert(conn);
            cacheConn_.pop();
            lock.unlock();
            conn->_fd = fd;
            conn->addr = std::move(addr);
            return conn;
        }
        auto status = cv_.wait_for(lock, 1ms, [this] {
            return !cacheConn_.empty();
        });
        if (status) {
            auto conn = cacheConn_.top();
            assert(conn);
            cacheConn_.pop();
            lock.unlock();
            conn->_fd = fd;
            conn->addr = std::move(addr);
            return conn;
        }
        lock.unlock();
        return new ConnData(fd, std::move(addr));
    }

    bool SimpleServer::pushCacheConn(ConnData *conn) {
        if (!conn) return false;
        std::unique_lock<std::mutex> lock(mtx_);
        if (cacheConn_.size() >= MAX_CACHE_CONN) {
            lock.unlock();
            delete conn;
            return false;
        }
        conn->resetData();
        cacheConn_.push(conn);
        lock.unlock();
        cv_.notify_one();
        return true;
    }

    void SimpleServer::incActiveConn() {
        activeConnCnt.fetch_add(1, std::memory_order_relaxed);
    }

    void SimpleServer::decActiveConn() {
        activeConnCnt.fetch_sub(1, std::memory_order_relaxed);
    }

    // todo: dung socket pair de notify write response done --> trigger memcpy tu HTTPResponse vao buffer
    // event write buffer: memcpy tu HTTPResponse vao buffer
    // event write socket: write tu buffer vao socket

    // event read buffer: parse_request tu buffer vao HTTPRequest
    // event read socket: read tu socket vao buffer

    void SimpleServer::onExpectContinue(HTTPRequest *, HTTPResponse *res) {
        assert(res);
        res->status_code(HTTPStatusCode::Continue);
    }

    void SimpleServer::onDefaultGet(HTTPRequest *req, HTTPResponse *res) {
        res->status_code(HTTPStatusCode::NotFound);
        res->insert_header({"Content-Type", "application/json"});
        auto sendData = R"JSON({"errors": "resource not found"})JSON";
        res->set_str_body(sendData);
    }
};// namespace simple_http
