#include "SimpleServer.h"
#include "libs/StrUtils.h"
#include "libs/MemBuffer.h"

#include <sys/poll.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <atomic>

using namespace std::chrono_literals;

// todo: cleanup connection (close fd, delete event) when server stop
// todo: use readv and writev to read/write multiple app buffer
// todo: socket with timeout, push to min heap and cleanup if expired
// todo: seperated handler thread for calling request handler

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

        auto rv = ::pipe(pipes_);
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
        pairEventData_.reset(new PairEventData(pipes_[0]));
        handle_->add_or_modify_fd(socketFd_, EPOLLIN, EPOLL_CTL_ADD, sockEventData_.get());
        handle_->add_or_modify_fd(pipes_[0], EPOLLIN, EPOLL_CTL_ADD, pairEventData_.get());
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
                auto fd = event->_fd;
                switch (event->_type) {
                    case EventType::PAIR_IO: {
                        if (eventTypes & EPOLLIN) {
                            auto pairEvent = reinterpret_cast<PairEventData *>(handle_->events[i].data.ptr);
                            pairEvent->_bytesInBuffer = ::read(fd, pairEvent->_eventBuffer, 1);
                            assert(pairEvent->_bytesInBuffer == 1);
                            return;
                        }
                    } break;
                    case EventType::ACCEPTOR: {
                        assert(fd == socketFd_);
                        struct ::sockaddr_storage connAddr;
                        ::socklen_t connAddrSize = sizeof(connAddr);
                        auto clientfd = accept4(socketFd_, (struct sockaddr *) &connAddr, &connAddrSize, SOCK_NONBLOCK);
                        if (clientfd < 0) {
                            int errno_copy = errno;
                            if (errno_copy == EAGAIN || errno_copy == EWOULDBLOCK) {
                                goto stop_accept;
                                // break;// no more connections
                            } else {
                                // printf("Acceptor::start() accept() errno: %s\n", strerror(errno_copy));
                                // perror("accept failed");
                                // server_->stat_.incDropConn();
                                goto stop_accept;
                                // break;
                            }
                        }

                        auto addrPair = server_->addrParse((struct sockaddr *) &connAddr);
                        // printf("New connection: %s:%d\n", addrPair.first.c_str(), addrPair.second);
                        server_->stat_.incActiveConn();
                        server_->addConnection(ind++, clientfd, std::move(addrPair));
                        if (ind >= workerSize) ind = 0;
                    } break;
                    default:
                        printf("unknown event: %d\n", static_cast<int>(event->_type));
                        assert(0);
                }
            }
        stop_accept:
            assert(true);
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
        static const char *buf = "a";
        assert(pipes_[1] > 0);
        auto rv = ::write(pipes_[1], buf, 1);
        return rv == 1;
    }

    Acceptor::~Acceptor() {
        // stop();
        // todo: stat for drop connection due to too many open files
        // todo: wrong active conns metric due to conn dropped
        if (th_) {
            th_->join();
            th_.reset();
        }
        if (socketFd_ > 0) ::close(socketFd_);
        ::close(pipes_[0]);
        ::close(pipes_[1]);
        pipes_[0] = pipes_[1] = -1;
    }

    IOWorker::~IOWorker() {
        if (th_.get()) {
            th_->join();
            th_.reset();
        }
        ::close(pipes_[0]);
        ::close(pipes_[1]);
        pipes_[0] = pipes_[1] = -1;
    }

    void IOWorker::start() {
        auto id = id_.fetch_add(1, std::memory_order_acq_rel);
        auto rv = ::pipe(pipes_);
        if (rv != 0)
            throw std::runtime_error("pipe failed");
        handle_.reset(new EpollHandle());
        handle_->init();
        auto stopEvent = new PairEventData(pipes_[0]);
        handle_->add_or_modify_fd(pipes_[0], EPOLLIN, EPOLL_CTL_ADD, stopEvent);
        th_ = std::make_unique<std::thread>([this, id]() {
            printf("IOWorker[%d] started\n", id);
            eventLoop();
            printf("IOWorker[%d] stopped\n", id);
        });
    }

    bool IOWorker::stop() {
        static const char *buf = "b";
        assert(pipes_[1] > 0);
        auto rv = ::write(pipes_[1], buf, 1);
        return rv == 1;
    }

    void IOWorker::addConn(int fd, AddrPair &&addr) {
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
                            auto pairEvent = reinterpret_cast<PairEventData *>(handle_->events[i].data.ptr);
                            pairEvent->_bytesInBuffer = ::read(fd, pairEvent->_eventBuffer, 1);
                            // assert(event->_bytesInBuffer == 1);
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

    void IOWorker::handleRead(ConnData *connPtr) {
        try {
            assert(connPtr);
            auto fd = connPtr->_fd;
            auto httpReqPtr = connPtr->req;
            auto recvBuf = httpReqPtr->get_buf();
            if (recvBuf->wr_avail() == 0) {
                // receive buffer full without completing headers — oversized request
                cleanupEvent(connPtr);
                server_->stat_.incFailedReq();
                return;
            }
            auto bytes = ::recv(fd, recvBuf->wr_pos(), recvBuf->wr_avail(), 0);
            if (bytes > 0) {
                recvBuf->incWrPos(bytes);
                httpReqPtr->parse_request();
                if (httpReqPtr->have_expect_continue()) {
                    printf("expect continue\n");
                    httpReqPtr->_expectContinue = false;
                    auto tmpEvent = std::make_unique<ConnData>(fd);
                    server_->onExpectContinue(tmpEvent->req, tmpEvent->res);
                    tmpEvent->res->write_reponse();
                    // tmpEvent->_bytesInBuffer = tmpEvent->res->serialize_reponse(tmpEvent->_eventBuffer, tmpEvent->bufferCap_);
                    // send here in order to not mess up with handleWrite state
                    ::send(fd, tmpEvent->res->get_buf()->rd_pos(), tmpEvent->res->get_buf()->size(), MSG_NOSIGNAL);
                }
                if (!httpReqPtr->request_completed()) {// still have bytes to read
                    handle_->add_or_modify_fd(fd, EPOLLIN, EPOLL_CTL_MOD, connPtr);
                } else {// read done
                    auto httpResPtr = connPtr->res;
                    // connPtr->resetBuffer();
                    auto pair = server_->getHandler(httpReqPtr->_method, httpReqPtr->_path);
                    if (pair.first) {
                        pair.second(httpReqPtr, httpResPtr);// call registered handler
                        handle_->add_or_modify_fd(fd, EPOLLOUT, EPOLL_CTL_MOD, connPtr);
                    } else {
                        if (server_->defaultHandlers_.count(httpReqPtr->_method)) {// not registered path
                            server_->defaultHandlers_.at(httpReqPtr->_method)(httpReqPtr, httpResPtr);
                            handle_->add_or_modify_fd(fd, EPOLLOUT, EPOLL_CTL_MOD, connPtr);
                        } else {
                            // method not supported or default handler for method not found
                            // todo: handle this case
                        }
                    }
                }
            } else if ((bytes < 0) && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                // no data available, try again
                printf("handleRead: retry\n");
                connPtr->resetData();
                handle_->add_or_modify_fd(fd, EPOLLIN, EPOLL_CTL_MOD, connPtr);
            } else {
                // bytes = 0 (connection close by client) and other errors
                cleanupEvent(connPtr);
            }
        } catch (const std::exception &e) {
            // fprintf(stderr, "IOWorker::handleRead exception: %s\n", e.what());
            cleanupEvent(connPtr);
            server_->stat_.incFailedReq();
            return;
        } catch (...) {
            fprintf(stderr, "IOWorker::handleRead unknown exception\n");
            cleanupEvent(connPtr);
            server_->stat_.incFailedReq();
            return;
        }
    }

    void IOWorker::handleWrite(ConnData *connPtr) {
        assert(connPtr);
        auto fd = connPtr->_fd;
        connPtr->res->write_reponse();
        auto sendbuf = connPtr->res->get_buf();
        auto sendBytes = ::send(fd, sendbuf->rd_pos(), sendbuf->size(), MSG_NOSIGNAL);
        if (sendBytes >= 0) {
            sendbuf->incRdPos(sendBytes);
            if (sendbuf->empty()) {// all data is sent, reset buf for next write_response
                sendbuf->resetBuf();
            }
            if (!connPtr->res->write_done()) {// still have bytes to send
                handle_->add_or_modify_fd(fd, EPOLLOUT, EPOLL_CTL_MOD, connPtr);
            } else {
                // cleanupEvent(connPtr);
                connPtr->resetData();
                handle_->add_or_modify_fd(fd, EPOLLIN, EPOLL_CTL_MOD, connPtr);
                server_->stat_.incSuccessReq();
            }
        } else {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {// retry
                // todo: add to metrics (retry)
                printf("handleWrite: retry\n");
                handle_->add_or_modify_fd(fd, EPOLLOUT, EPOLL_CTL_ADD, connPtr);
            } else {
                cleanupEvent(connPtr);
            }
        }
    }

    void IOWorker::cleanupEvent(EventBase *event) {
        auto connData = dynamic_cast<ConnData *>(event);
        handle_->delete_fd(event->_fd);
        ::close(event->_fd);
        if (connData) {
            // printf("cleanup conn: %s:%d\n", connData->addr.first.c_str(), connData->addr.second);
            server_->stat_.decActiveConn();
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
#ifdef CACHE_CONN
            printf("cached connections: %d\n", static_cast<int>(cacheConn_.size()));
            while (!cacheConn_.empty()) {
                auto conn = cacheConn_.top();
                cacheConn_.pop();
                delete conn;
            }
#endif
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

    SimpleServer::StatVal SimpleServer::getStat() const {
        StatVal ret;
        ret.activeConn = stat_.activeConn.load(std::memory_order_relaxed);
        ret.failedReq = stat_.failedReq.load(std::memory_order_relaxed);
        ret.successReq = stat_.successReq.load(std::memory_order_relaxed);
        ret.createdConn = stat_.createdConn.load(std::memory_order_relaxed);
        ret.dropConn = stat_.dropConn.load(std::memory_order_relaxed);
#ifdef CACHE_CONN
        {
            std::lock_guard<std::mutex> lock(cacheConnMtx_);
            ret.cacheConn = cacheConn_.size();
        }
#else
        ret.cacheConn = 0;
#endif
        return ret;
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
#ifdef CACHE_CONN
        std::unique_lock<std::mutex> lock(cacheConnMtx_);
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
        stat_.incCreatedConn();
        return new ConnData(fd, std::move(addr));
#else
        stat_.incCreatedConn();
        return new ConnData(fd, std::move(addr));
#endif
    }

    bool SimpleServer::pushCacheConn(ConnData *conn) {
        if (!conn) return false;
#ifdef CACHE_CONN
        std::unique_lock<std::mutex> lock(cacheConnMtx_);
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
#else
        delete conn;
        conn = nullptr;
        return true;
#endif
    }

    void SimpleServer::onExpectContinue(HTTPRequest *, HTTPResponse *res) {
        assert(res);
        res->http_code(CODE_100);
    }

    void SimpleServer::onDefaultGet(HTTPRequest *req, HTTPResponse *res) {
        res->http_code(CODE_404);
        res->insert_header({"Content-Type", "application/json"});
        auto sendData = R"JSON({"errors": "resource not found"})JSON";
        res->str_body(sendData);
    }
};// namespace simple_http
