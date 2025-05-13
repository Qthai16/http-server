#include <string>
#include <iostream>
#include <thread>
#include <mutex>
#include <atomic>
#include <vector>
#include <chrono>
#include <memory>

#include <cstring>
#include <cassert>
#include <sys/socket.h>
#include <sys/poll.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <unistd.h>

#include "libs/Defines.h"

// #define expr_likely(x)      __builtin_expect(!!(x), 1)
// #define expr_unlikely(x)    __builtin_expect(!!(x), 0)

class IOThread {
public:
    explicit IOThread() : worker_(nullptr), name_(), pair_(), recvTimeout_(0), requestStop_(false) { init(); };
    explicit IOThread(const std::string &name) : worker_(nullptr), name_(name), pair_(), recvTimeout_(0), requestStop_(false) { init(); }
    ~IOThread() {
        // send interrupt signal to socket pair
        requestStop_ = true;
        if (worker_) {
            worker_->join();
            delete worker_;
            worker_ = nullptr;
        }
        ::close(pair_[0]);
        ::close(pair_[1]);
        pair_[0] = pair_[1] = -1;
    }

public:
    struct PairEvent {
        enum Type {
            UNINIT = 0,
            GET_NAME,
            SET_NAME,
            SET_RECV_TIMEOUT,
        };
        Type type;
        void *data = nullptr;

        PairEvent() = default;
    };
    struct EventSetRecvTimeout {
        int timeout_;
    };
    struct EventSetName {
        std::string name_;
    };

    void interrupt() {
        static char buf[1] = {'\0'};
        if (expr_unlikely(pair_[1] == -1)) return;
        auto rv = write(pair_[1], buf, 1);
        assert(rv == 1);
    }

    void pairNotify(const PairEvent &e) {
        if (expr_unlikely(e.type == PairEvent::UNINIT)) return;
        if (expr_unlikely(pair_[0] == -1)) return;
        auto rv = write(pair_[0], &e, sizeof(e));
        assert(rv == sizeof(e));
    }

private:
    void eventLoop() {
        struct pollfd fds[2];
        // std::memset(fds, 0, sizeof(fds));
        fds[0].fd = pair_[0];
        fds[0].events = POLLIN;
        fds[0].revents = 0;
        fds[1].fd = pair_[1];
        fds[1].events = POLLIN;
        fds[1].revents = 0;

        int cursize = 2;
        int rv;
        do {
            rv = poll(fds, cursize, (recvTimeout_ != 0) ? recvTimeout_ : -1);
            if (expr_unlikely(rv == -1)) {
                printf("[%s] poll failed: %s\n", name_.c_str(), strerror(errno));
                continue;
            }
            if (expr_unlikely(fds[0].revents & POLLIN)) {// interrupted
                printf("[%s] interrupted\n", name_.c_str());
                fds[0].events = POLLIN;
                fds[0].revents = 0;
                break;
            }
            if (fds[1].revents & POLLIN) {// notify
                if (expr_unlikely(rv % sizeof(PairEvent) != 0)) {
                    printf("[%s] receive event with invalid size\n", name_.c_str());
                    fds[1].events = POLLIN;
                    fds[1].revents = 0;
                    continue;
                }
            }

            // printf("[%s] heartbeat\n", name_.c_str());
            // std::this_thread::sleep_for(std::chrono::seconds(1));
        } while (!requestStop_);
        printf("[%s] stopped\n", name_.c_str());
    }

    void init() {
        auto id = id_.fetch_add(1, std::memory_order_acq_rel);
        if (name_.empty())
            name_ = std::string{"iothread_"} + std::to_string(id);
        int rv = socketpair(AF_UNIX, SOCK_STREAM, 0, pair_);
        if (rv != 0)
            throw std::runtime_error("socketpair FAIL");
        worker_ = new std::thread([this]() {
            this->eventLoop();
        });
    }

private:
    std::thread *worker_;
    std::string name_;
    int pair_[2];
    int recvTimeout_;
    bool requestStop_;
    static std::atomic_int id_;
};


std::atomic_int IOThread::id_;
// int main(int argc, const char* argv[]) {
//     std::cout << "socket pair test" << std::endl;
//     std::vector<std::unique_ptr<IOThread>> iothreads(4);
//     printf("-------------- starting --------------\n");
//     for (auto& t : iothreads) {
//         t.reset(new IOThread);
//     }
//     std::this_thread::sleep_for(std::chrono::seconds(3));
//     printf("-------------- stopping --------------\n");
//     for (auto& t : iothreads) {
//         t->interrupt();
//     }
//     iothreads.clear();
//     return 0;
// }


// #include <yaml-cpp/yaml.h>
#include "libs/StrUtils.h"
#include "libs/RandomUtils.h"
#include "libs/SSLUtils.h"

#include <string_view>
#include <limits>
#include <functional>

#include <stdint.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

using namespace std::string_view_literals;

#include "libs/FileUtils.h"

#define OPEN_PERM 00644

class Defer final {
public:
    explicit Defer(std::function<void()> &&fn) : fn_(std::move(fn)) {}
    ~Defer() { fn_(); }
    Defer(const Defer &) = delete;
    Defer &operator=(const Defer &) = delete;

private:
    std::function<void()> fn_;
};

std::string key_from_str(const EVP_CIPHER *cipher, std::string_view str) {
    // key len can be 128, 192, or 256 bits (16, 24 or 32 bytes)
    std::string key(EVP_CIPHER_key_length(cipher), '\0');
    auto hashStr = libs::hash_utils::sha256(str.data(), str.size());
    for (auto i = 0; i < key.size(); i++) {
        key[i] = hashStr[i];
    }
    return std::move(key);
}

std::string rand_iv(const EVP_CIPHER *cipher) {
    // iv must be 128 bits (16 bytes)
    std::string iv(EVP_CIPHER_iv_length(cipher), '\0');
    for (auto i = 0; i < iv.size() / sizeof(int); i++) {
        auto num = libs::random_num(0, std::numeric_limits<int>::max());
        // printf("gen num: %x\n", num);
        for (auto j = 0; j < sizeof(int) / sizeof(char); j++) {
            iv[j + i * sizeof(int)] = 0xff & (num >> (32 - 8 * (j + 1)));
        }
    }
    return std::move(iv);
}

void testFileReadWrite(const char *in, const char *out) {
    using namespace libs;
    assert(in && out);
    int32_t oflags = O_RDWR | O_CREAT;
    auto fd = ::open(in, O_RDONLY, OPEN_PERM);
    auto fd2 = ::open(out, O_RDWR | O_CREAT | O_TRUNC, OPEN_PERM);
    if (fd < 0 || fd2 < 0) {
        printf("open failed (%s)", strerror(errno));
        return;
    }

    auto inCloser = [fd]() { ::close(fd); };
    auto outCloser = [fd2]() { ::close(fd2); };
    std::string readBuf;
    struct ::stat st;
    if (::fstat(fd, &st) != 0) {
        printf("failed to get stat: %s\n", in);
        return;
    }
    if (st.st_size > 0) readBuf.resize(st.st_size);
    printf("read data from %s\n", in);
    libs::read(fd, 0, readBuf.data(), st.st_size);
    const auto cipher = EVP_aes_256_cbc();
    auto key = key_from_str(cipher, "simple_key");
    auto iv = rand_iv(cipher);
    printf("key: %s\n", libs::toHexStr(key.data(), key.size()).c_str());
    printf("iv: %s\n", libs::toHexStr(iv.data(), iv.size()).c_str());

    printf("encrypt data using: %s\n", EVP_CIPHER_name(cipher));
    auto encCipher = ssl::encrypt(cipher, readBuf.data(), key, iv);
    std::string encValue;
    if (encCipher.has_value()) {
        encValue = encCipher.value();
        auto writeData = base64::encode(encValue);
        printf("write base64 cipher text to %s\n", out);
        libs::write(fd2, 0, writeData.data(), writeData.size());
    } else {
        printf("encrypt failed\n");
        return;
    }
}

#include "libs/WorkerPool.h"
#include "libs/RandomUtils.h"
#include <cstring>

using SimpleWorkerPool = libs::NotifyQueueWorker<int>;
int nworkers_ = 0;

void parseArgs(int argc, const char *argv[]) {
    if (argc <= 1) return;
    for (int i = 1; i < argc; ++i) {
        if (!std::strcmp(argv[i], "--nworker")) {
            ++i;
            if (i < argc) {
                nworkers_ = std::stoi(argv[i]);
            } else
                printf("missing argument value: %s\n", argv[i]);
            // } else if (!std::strcmp(argv[i], "--abc")) {
            //     abc_ = true;
        } else {
            printf("unknown argument: %s\n", argv[i]);
        }
    }
}

int main(int argc, const char *argv[]) {
    parseArgs(argc, argv);
    std::atomic<int64_t> totalCnt{0};
    auto intHandler = [&totalCnt](SimpleWorkerPool::job_type &value) {
        totalCnt.fetch_add(value, std::memory_order_relaxed);
    };
    // auto nworkers = std::thread::hardware_concurrency();
    if (nworkers_ <= 0) nworkers_ = std::thread::hardware_concurrency();
    bool sendDone = false;
    std::atomic_int modifyTickCnt{0};
    printf("start worker pool with %d threads\n", nworkers_);
    auto simpleWorkers = std::make_shared<SimpleWorkerPool>(nworkers_, std::move(intHandler));
    std::thread waiter([&]() {
        size_t jobCnt = 0;
        do {
            jobCnt = simpleWorkers->totalJobs();
            auto randInt = libs::random_num(0, 1000);
            if (randInt >= 800) {
                auto val = std::max(1, randInt % 4);
                printf("set worker %d\n", val);
                simpleWorkers->setWorkerSize(val);
                printf("set worker %d\n", val);
            } else if ((randInt <= 200)) {
                simpleWorkers->modifyJobQueue([&modifyTickCnt](SimpleWorkerPool::container_type &cont) {
                    // printf("job queue size: %ld\n", cont.size());
                    modifyTickCnt.fetch_add(cont.size(), std::memory_order_relaxed);
                });
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        } while (!sendDone || jobCnt > 0);
        printf("waiter total tick: %d\n", modifyTickCnt.load());
        std::cout << "job done" << std::endl;
    });
    for (auto i = 1; i <= 500000; i++) {
        simpleWorkers->push(i);
    }
    sendDone = true;
    std::cout << "send done" << std::endl;
    waiter.join();
    printf("total result: %ld\n", totalCnt.load());
    return 0;

    testFileReadWrite("input.txt", "cipher.out");
    return 0;
    using namespace libs;
    std::cout << "ssl utils test" << std::endl;
    std::string plain("this is a text message");
    auto plainB64 = base64::encode(plain.data(), plain.size());
    printf("plain: %s, base64: %s\n", plain.c_str(), plainB64.c_str());

    auto val1 = "this is test data";
    auto hash1 = hash_utils::sha256(val1, strlen(val1));
    std::cout << libs::toHexStr((const char *) hash1.data(), hash1.size()) << std::endl;

    // auto hash2 = hash_utils::md5(val1, strlen(val1));
    // std::cout << libs::toHexStr((const char*) hash2.data(), hash2.size()) << std::endl;

    // auto hash3 = hash_utils::sha1(val1, strlen(val1));
    // std::cout << libs::toHexStr((const char*) hash3.data(), hash3.size()) << std::endl;

    // auto val = hash_utils::sha256_file("/home/thaipq/proj/commondb/custom/cmake-build-debug/server");
    // std::cout << libs::toHexStr((const char*) val.data(), val.size()) << std::endl;

    std::string rawdata("this is a secret data");
    const auto cipher = EVP_aes_256_cbc();
    // std::string key(EVP_CIPHER_key_length(cipher), '\0');
    // std::string iv(EVP_CIPHER_iv_length(cipher), '\0');

    auto key = key_from_str(cipher, "my_sensitive_encryption_key");
    auto iv = rand_iv(cipher);
    // auto hashKey = hash_utils::sha256(encKey.data(), encKey.size());
    // for (auto i = 0; i < key.size(); i++) {
    //     key[i] = hashKey[i];
    // }
    // for (auto i = 0; i < iv.size() / sizeof(int); i++) {
    //     auto num = libs::random_num(0, std::numeric_limits<int>::max());
    //     // printf("gen num: %x\n", num);
    //     for (auto j = 0; j < sizeof(int) / sizeof(char); j++) {
    //         iv[j + i * sizeof(int)] = 0xff & (num >> (32 - 8 * (j + 1)));
    //     }
    // }
    printf("key: %s\n", libs::toHexStr(key.data(), key.size()).c_str());
    printf("iv: %s\n", libs::toHexStr(iv.data(), iv.size()).c_str());
    printf("data: '%s'\n", rawdata.c_str());
    auto encCipher = ssl::encrypt(cipher, rawdata, key, iv);
    std::string encValue;
    if (encCipher.has_value()) {
        encValue = encCipher.value();
        printf("aes-256-cbc encrypt (base64): '%s'\n", base64::encode(encValue).c_str());
    } else {
        printf("encrypt aes-256-cbc failed\n");
        return -1;
    }
    auto decData = ssl::decrypt(cipher, encValue, key, iv);
    if (decData.has_value()) {
        printf("aes-256-cbc decrypt: '%s'\n", decData.value().c_str());
    } else {
        printf("decrypt aes-256-cbc failed\n");
        return -1;
    }
    return 0;
}
