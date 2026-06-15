#include "SimpleServer.h"
#include "StrUtils.h"
#include "MemBuffer.h"

#include <sys/poll.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <atomic>

using namespace std::chrono_literals;

// todo: cleanup connection (close fd, delete event) when server stop
// todo: use readv and writev to read/write multiple app buffer

// more details stat, including:
// - COUNTER: total time processed, can be break down into:
//     - time in queue
//     - IO time (read/write from socket, buffer)
//     - handler time (time actually request is processed)
//     - can be splited to category name: std::vector<int>, with each name occupy a slot in vector
// - RATE: calculated based on total_req/processed_time
// - GAUGE: handler task queue depth

namespace libs::http {
    std::atomic_int IOWorker::id_{0};

    void Acceptor::initSocket() {
        auto addr = server_->_address.c_str();
        auto port = server_->_port;
        struct ::sockaddr_in serv_addr;
        memset(&serv_addr, 0, sizeof(serv_addr));
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
        if (::setsockopt(socketFd_, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable)) < 0)
            throw std::runtime_error("Failed to set SO_REUSEADDR");
        if (::setsockopt(socketFd_, SOL_SOCKET, SO_REUSEPORT, &enable, sizeof(enable)) < 0)
            throw std::runtime_error("Failed to set SO_REUSEPORT");

        if (::bind(socketFd_, (sockaddr *) &serv_addr, sizeof(serv_addr)))
            throw std::runtime_error("Failed to bind socket");

        if (::listen(socketFd_, QUEUEBACKLOG) < 0)
            throw std::runtime_error("Failed to listen socket");

        handle_.init();
    }

    void Acceptor::eventLoop() {
        std::unique_ptr<EventBase> sockEventData;
        std::unique_ptr<EventBase> pairEventData;
        sockEventData.reset(new EventBase(socketFd_, EventType::ACCEPTOR));
        pairEventData.reset(new PairEventData(pipes_[0]));
        handle_.add_or_modify_fd(socketFd_, EPOLLIN, EPOLL_CTL_ADD, sockEventData.get());
        handle_.add_or_modify_fd(pipes_[0], EPOLLIN, EPOLL_CTL_ADD, pairEventData.get());

        auto ind = 0;// round robin
        auto workerSize = server_->ioWorkers_.size();
        while (server_->isRunning()) {
            auto nfds = ::epoll_wait(handle_._epollFd, handle_.events, MAX_EPOLL_EVENTS, -1);// wait forever
            if (nfds <= 0)
                continue;
            for (auto i = 0; i < nfds; i++) {
                auto event = reinterpret_cast<EventBase *>(handle_.events[i].data.ptr);
                auto eventTypes = handle_.events[i].events;
                auto fd = event->_fd;
                switch (event->_type) {
                    case EventType::PAIR_IO: {
                        if (eventTypes & EPOLLIN) {
                            auto pairEvent = reinterpret_cast<PairEventData *>(handle_.events[i].data.ptr);
                            pairEvent->_bytesInBuffer = ::read(fd, pairEvent->_eventBuffer, 1);
                            assert(pairEvent->_bytesInBuffer == 1);
                            return;
                        }
                    } break;
                    case EventType::ACCEPTOR: {
                        assert(fd == socketFd_);
                        while (true) {
                            struct ::sockaddr_storage connAddr;
                            ::socklen_t connAddrSize = sizeof(connAddr);
                            auto clientfd = accept4(socketFd_, (struct sockaddr *) &connAddr, &connAddrSize, SOCK_NONBLOCK);
                            if (clientfd < 0) {
                                break;// EAGAIN/EWOULDBLOCK: backlog drained
                            }
                            auto addrPair = server_->addrParse((struct sockaddr *) &connAddr);
                            server_->ioWorkers_[ind]->stat_.incActiveConn();
                            server_->addConnection(ind++, clientfd, std::move(addrPair));
                            if (ind >= workerSize) ind = 0;
                        }
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
        static const char *buf = "a";
        assert(pipes_[1] > 0);
        auto rv = ::write(pipes_[1], buf, 1);
        return rv == 1;
    }

    Acceptor::~Acceptor() {
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
        // After the event loop exits connMap_ holds every still-open connection.
        // Drain it to close their fds; timeoutHeap_ shared_ptrs are released when the heap destructs.
        for (auto &[fd, conn] : connMap_) {
            handle_.delete_fd(fd);
            ::close(fd);
            conn->deadline_ = {};
            stat_.decActiveConn();
        }
        connMap_.clear();
        ::close(pipes_[0]);
        ::close(pipes_[1]);
        pipes_[0] = pipes_[1] = -1;
        if (completionPipes_[0] != -1) ::close(completionPipes_[0]);
        if (completionPipes_[1] != -1) ::close(completionPipes_[1]);
        completionPipes_[0] = completionPipes_[1] = -1;
    }

    void IOWorker::start() {
        auto id = id_.fetch_add(1, std::memory_order_acq_rel);
        if (::pipe(pipes_) != 0)
            throw std::runtime_error("pipe failed");
        if (::pipe(completionPipes_) != 0)
            throw std::runtime_error("completion pipe failed");
        ::fcntl(completionPipes_[0], F_SETFL, ::fcntl(completionPipes_[0], F_GETFL) | O_NONBLOCK);
        handle_.init();
        th_ = std::make_unique<std::thread>([this, id]() {
            std::unique_ptr<PairEventData> stopEvent;
            stopEvent.reset(new PairEventData(pipes_[0]));
            handle_.add_or_modify_fd(pipes_[0], EPOLLIN, EPOLL_CTL_ADD, stopEvent.get());
            std::unique_ptr<PairEventData> completionEvent;
            completionEvent.reset(new PairEventData(completionPipes_[0]));
            handle_.add_or_modify_fd(completionPipes_[0], EPOLLIN, EPOLL_CTL_ADD, completionEvent.get());
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
        auto [conn, created] = server_->getOrCreateConn(fd, std::move(addr));
        if (created) stat_.incCreatedConn();
        {
            std::lock_guard<std::mutex> lk(timeoutMtx_);
            connMap_[fd] = conn;
        }
        scheduleTimeout(conn.get(), server_->readTimeout_);
        handle_.add_or_modify_fd(fd, EPOLLIN, EPOLL_CTL_ADD, conn.get());
    }

    void IOWorker::eventLoop() {
        while (server_->isRunning()) {
            auto nfds = ::epoll_wait(handle_._epollFd, handle_.events, MAX_EPOLL_EVENTS, nextDeadlineMs());
            checkTimeouts();
            if (nfds <= 0) {
                continue;
            }
            for (auto i = 0; i < nfds; i++) {
                auto event = reinterpret_cast<EventBase *>(handle_.events[i].data.ptr);
                auto fd = event->_fd;
                auto eventTypes = handle_.events[i].events;
                switch (event->_type) {
                    case EventType::PAIR_IO: {
                        if (eventTypes & (EPOLLHUP | EPOLLERR)) {
                            cleanupEvent(event);
                        } else if (eventTypes & EPOLLIN) {
                            if (fd == pipes_[0]) {
                                auto pairEvent = reinterpret_cast<PairEventData *>(handle_.events[i].data.ptr);
                                pairEvent->_bytesInBuffer = ::read(fd, pairEvent->_eventBuffer, 1);
                                return;// stop signal
                            }
                            drainCompletions();// handler thread finished — reads 8-byte pointer chunks
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
                        if (eventTypes & (EPOLLHUP | EPOLLERR)) {
                            cleanupEvent(reqEventData);
                        } else if (eventTypes & EPOLLIN) {
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
        }
    }

    void IOWorker::handleRead(ConnData *connPtr) {
        try {
            assert(connPtr);
            auto fd = connPtr->_fd;
            auto httpReqPtr = connPtr->req;
            auto recvBuf = httpReqPtr->get_buf();
            if (expr_unlikely(recvBuf->wr_avail() == 0)) {
                // if buffer not inited, init it here
                if (!recvBuf->inited())
                    recvBuf->reserve(BUFFER_SIZE);
            }
            auto bytes = ::recv(fd, recvBuf->wr_pos(), recvBuf->wr_avail(), 0);
            if (bytes > 0) {
                recvBuf->incWrPos(bytes);
                httpReqPtr->parse_request();
                if (httpReqPtr->have_expect_continue()) {
                    // notify send expect continue
                    connPtr->req->reset_expect_continue(false);
                    server_->onExpectContinue(connPtr->req.get(), connPtr->res.get());
                    handle_.add_or_modify_fd(fd, EPOLLOUT, EPOLL_CTL_MOD, connPtr);
                    if (httpReqPtr->request_completed()) {
                        connPtr->deadline_ = {};// disarm: handler runs in pool, no fixed deadline
                        server_->taskPool_->push(SimpleServer::HandlerTask{connPtr, this, server_});
                    }
                    return;
                }
                if (httpReqPtr->request_completed()) {
                    connPtr->deadline_ = {};// disarm: handler runs in pool, no fixed deadline
                    // disarm while handler runs in the pool to prevent spurious EPOLLIN
                    handle_.add_or_modify_fd(fd, 0, EPOLL_CTL_MOD, connPtr);
                    server_->taskPool_->push(SimpleServer::HandlerTask{connPtr, this, server_});
                    return;
                }
                // reset read deadline on progress; connection is still actively sending data
                scheduleTimeout(connPtr, server_->readTimeout_);
                // continue read and parse req
                handle_.add_or_modify_fd(fd, EPOLLIN, EPOLL_CTL_MOD, connPtr);
            } else if ((bytes < 0) && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                printf("handleRead: retry\n");
                handle_.add_or_modify_fd(fd, EPOLLIN, EPOLL_CTL_MOD, connPtr);
            } else {
                // peer close connection or other errors
                cleanupEvent(connPtr);
            }
        } catch (const std::exception &e) {
            cleanupEvent(connPtr);
            stat_.incFailedReq();
            return;
        } catch (...) {
            fprintf(stderr, "IOWorker::handleRead unknown exception\n");
            cleanupEvent(connPtr);
            stat_.incFailedReq();
            return;
        }
    }

    void IOWorker::handleWrite(ConnData *connPtr) {
        assert(connPtr);
        auto fd = connPtr->_fd;
        connPtr->res->write_reponse();
        auto sendbuf = connPtr->res->get_buf();
        auto sendBytes = ::send(fd, sendbuf->rd_pos(), sendbuf->rd_avail(), MSG_NOSIGNAL);
        if (sendBytes >= 0) {
            sendbuf->incRdPos(sendBytes);
            if (sendbuf->empty()) {
                sendbuf->resetBuf();
            }
            if (!connPtr->res->write_done()) {// continue write
                handle_.add_or_modify_fd(fd, EPOLLOUT, EPOLL_CTL_MOD, connPtr);
                return;
            }
            // if (connPtr->req->have_expect_continue()) {// this res is expect continue
                // connPtr->req->reset_expect_continue(false);
                if (!connPtr->req->request_completed()) {
                    // 100 Continue was just sent; reset res so the handler gets a clean response object
                    connPtr->initResponse();
                    handle_.add_or_modify_fd(fd, EPOLLIN, EPOLL_CTL_MOD, connPtr);
                    return;
                }
            // }
            // this res is handler res
            // re-arm for next request (connecion keep-alive behavior)
            connPtr->resetData();
            scheduleTimeout(connPtr, server_->idleTimeout_);
            handle_.add_or_modify_fd(fd, EPOLLIN, EPOLL_CTL_MOD, connPtr);
            stat_.incSuccessReq();
        } else {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                printf("handleWrite: retry\n");
                handle_.add_or_modify_fd(fd, EPOLLOUT, EPOLL_CTL_MOD, connPtr);
            } else {
                cleanupEvent(connPtr);
            }
        }
    }

    void IOWorker::cleanupEvent(EventBase *event) {
        auto connData = dynamic_cast<ConnData *>(event);
        handle_.delete_fd(event->_fd);
        ::close(event->_fd);
        if (connData) {
            connData->deadline_ = {};
            stat_.decActiveConn();
            std::shared_ptr<ConnData> ptr;
            {
                std::lock_guard<std::mutex> lk(timeoutMtx_);
                auto it = connMap_.find(connData->_fd);
                // identity check guards against fd reuse between close and erase
                if (it != connMap_.end() && it->second.get() == connData) {
                    ptr = std::move(it->second);
                    connMap_.erase(it);
                }
            }
            if (ptr) server_->pushCacheConn(std::move(ptr));
            return;
        }
        delete event;
        event = nullptr;
    }

    void IOWorker::scheduleTimeout(ConnData *conn, std::chrono::seconds duration) {
        if (duration == std::chrono::seconds{0})
            return;
        conn->deadline_ = std::chrono::steady_clock::now() + duration;
        std::lock_guard<std::mutex> lk(timeoutMtx_);
        timeoutHeap_.push({conn->deadline_, conn->shared_from_this()});
    }

    void IOWorker::checkTimeouts() {
        auto now = std::chrono::steady_clock::now();
        std::vector<std::shared_ptr<ConnData>> expired;
        {
            std::lock_guard<std::mutex> lk(timeoutMtx_);
            while (!timeoutHeap_.empty() && timeoutHeap_.top().deadline <= now) {
                auto entry = timeoutHeap_.top();
                timeoutHeap_.pop();
                if (entry.conn->deadline_ == entry.deadline) // not stale; shared_ptr keeps alive
                    expired.push_back(entry.conn);
            }
        }
        for (auto &conn : expired)
            cleanupEvent(conn.get());
    }

    int IOWorker::nextDeadlineMs() {
        std::lock_guard<std::mutex> lk(timeoutMtx_);
        if (timeoutHeap_.empty())
            return 1000;
        auto now = std::chrono::steady_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(timeoutHeap_.top().deadline - now).count();
        if (ms <= 0) return 0;
        return static_cast<int>(std::min<long long>(ms, 1000));
    }

    void IOWorker::notifyHandlerDone(ConnData *conn) {
        // sizeof(ConnData*) == 8 bytes, well under PIPE_BUF — write is atomic
        ::write(completionPipes_[1], &conn, sizeof(conn));
    }

    void IOWorker::drainCompletions() {
        ConnData *conn;
        while (::read(completionPipes_[0], &conn, sizeof(conn)) == sizeof(conn)) {
            handle_.add_or_modify_fd(conn->_fd, EPOLLOUT, EPOLL_CTL_MOD, conn);
        }
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
    }

    void SimpleServer::start() {
        taskPool_ = std::make_unique<TaskPool>(_poolSize, [](HandlerTask &task) {
            auto *conn = task.data;
            auto *server = task.server;
            auto *req = conn->req.get();
            auto *res = conn->res.get();

            auto handler = server->getHandler(req->_method, req->_path);
            if (!handler) {
                std::shared_lock rlock(server->handlerMtx_);
                auto iter = server->defaultHandlers_.find(req->_method);
                if (iter != server->defaultHandlers_.end()) {
                    handler = iter->second;
                }
            }
            if (handler) {
                handler(req, res);
            } else {
                res->http_code(CODE_405);
                res->insert_header({"Content-Type", "application/json"});
                res->str_body(R"JSON({"errors": "method not allowed"})JSON");
            }
            task.ioWorker->notifyHandlerDone(conn);
        });

        ioWorkers_.resize(_poolSize, nullptr);
        for (auto i = 0; i < _poolSize; i++) {
            ioWorkers_[i] = new IOWorker(this);
        }
        // start acceptor
        acceptor_.reset(new Acceptor(this));
        acceptor_->start();
        // start io workers
        for (auto &w: ioWorkers_) {
            w->start();
        }
        // todo: condition variable to wait until all workers and acceptor are ready
        printf("Simple server start listening on %s:%d\n", _address.c_str(), _port);
    }

    void SimpleServer::stop() {
        if (!_stop.exchange(true)) {
            acceptor_->stop();
            acceptor_.reset();// wait acceptor join
            if (taskPool_) {
                taskPool_->stop(true);// drain in-flight handlers before closing sockets
                taskPool_.reset();
            }
            for (auto &io: ioWorkers_) {
                io->stop();
                delete io;
                io = nullptr;
            }
#ifdef CACHE_CONN
            printf("cached connections: %d\n", static_cast<int>(cacheConn_.size()));
            while (!cacheConn_.empty())
                cacheConn_.pop(); // shared_ptr destructs each ConnData naturally
#endif
        }
    }

    bool SimpleServer::isRunning() const {
        return !_stop.load(std::memory_order_acquire);
    }

    void SimpleServer::addHandlers(const std::vector<std::tuple<HTTPMethod, URLFormat, HandlerFunction>> &handlers) {
        std::lock_guard wlock(handlerMtx_);// lock write
        for (const auto &val: handlers) {
            auto [method, path, fn] = val;
            handlers_[path].pathRegex = std::regex(path);
            handlers_[path].funcs[static_cast<int>(method)] = fn;
        }
    }

    void SimpleServer::setDefaultHandler(HTTPMethod method, HandlerFunction fn) {
        std::lock_guard wlock(handlerMtx_);// lock write
        defaultHandlers_[method] = fn;
    }

    void SimpleServer::addHandler(URLFormat path, HTTPMethod method, HandlerFunction fn) {
        std::lock_guard wlock(handlerMtx_);// lock write
        handlers_[path].pathRegex = std::regex(path);
        handlers_[path].funcs[static_cast<int>(method)] = fn;
    }

    SimpleServer::StatVal SimpleServer::getStat() const {
        StatVal ret;
        for (const auto *w: ioWorkers_) {
            ret.activeConn += w->stat_.activeConn.load(std::memory_order_relaxed);
            ret.failedReq += w->stat_.failedReq.load(std::memory_order_relaxed);
            ret.successReq += w->stat_.successReq.load(std::memory_order_relaxed);
            ret.createdConn += w->stat_.createdConn.load(std::memory_order_relaxed);
            ret.dropConn += w->stat_.dropConn.load(std::memory_order_relaxed);
        }
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
        return {std::string(s), port};
    }

    SimpleServer::HandlerFunction SimpleServer::getHandler(HTTPMethod method, const std::string &path) {
        std::shared_lock rlock(handlerMtx_);// lock read
        // try exact match first
        auto iter = handlers_.find(path);
        if (iter != handlers_.end()) {
            return iter->second.funcs[static_cast<int>(method)];
        }
        // retry regex match
        for (const auto &[_, inf]: handlers_) {
            if (std::regex_match(path, inf.pathRegex)) {
                return inf.funcs[static_cast<int>(method)];
            }
        }
        return nullptr;
    }

    void SimpleServer::addConnection(int i, int fd, AddrPair &&addr) {
        if (i < 0 || i >= ioWorkers_.size()) return;
        ioWorkers_[i]->addConn(fd, std::move(addr));
    }

    std::pair<std::shared_ptr<ConnData>, bool> SimpleServer::getOrCreateConn(int fd, AddrPair &&addr) {
#ifdef CACHE_CONN
        std::unique_lock<std::mutex> lock(cacheConnMtx_);
        if (!cacheConn_.empty()) {
            auto conn = std::move(cacheConn_.top());
            cacheConn_.pop();
            lock.unlock();
            conn->_fd = fd;
            conn->addr = std::move(addr);
            conn->resetData();
            return {conn, false};
        }
        lock.unlock();
        return {std::make_shared<ConnData>(fd, std::move(addr)), true};
#else
        return {std::make_shared<ConnData>(fd, std::move(addr)), true};
#endif
    }

    bool SimpleServer::pushCacheConn(std::shared_ptr<ConnData> conn) {
        if (!conn) return false;
#ifdef CACHE_CONN
        std::unique_lock<std::mutex> lock(cacheConnMtx_);
        if (cacheConn_.size() >= MAX_CACHE_CONN) {
            lock.unlock();
            // conn goes out of scope and destructs naturally
            return false;
        }
        conn->resetData();
        cacheConn_.push(std::move(conn));
        lock.unlock();
        cv_.notify_one();
        return true;
#else
        // conn goes out of scope and destructs naturally
        return true;
#endif
    }

    void SimpleServer::onExpectContinue(HTTPRequest *, HTTPResponse *res) {
        // printf("expect continue\n");
        res->http_code(CODE_100);
    }
}// namespace libs::http
