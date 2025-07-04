/*
 * File:   OSUtils.h
 * Author: thaipq
 *
 * Created on Fri Jun 13 2025 10:11:48 AM
 */

#ifndef LIBS_OSUTILS_H
#define LIBS_OSUTILS_H

#include <string>
#include <stdexcept>

#include <stdio.h>
#include <cstdarg>
#include <stdio.h>
#include <unistd.h>
#include <cstring>
#include <signal.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>

namespace libs {

    static void beDaemon(const char *logfile = nullptr) {
        pid_t pid;
        if ((pid = ::fork()) < 0)
            throw std::runtime_error("cannot fork daemon process");
        else if (pid != 0)// parent process
            ::exit(0);

        ::setsid(); // create new session and detach from terminal
        ::umask(0); // no limit on file permissions
        FILE *fin = ::freopen("/dev/null", "r", stdin);
        if (!fin)
            throw std::runtime_error("Cannot attach stdin to /dev/null");
        if (logfile && ::strcmp(logfile, "/dev/null") != 0) {
            auto fd = ::open(logfile, O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC, 0644);
            if (fd < 0) {
                std::string errmsg = std::string("Failed to open log: ") + std::string{logfile};
                throw std::runtime_error(errmsg.c_str());
            }
            ::dup2(fd, STDOUT_FILENO);
            ::dup2(fd, STDERR_FILENO);
            ::close(fd);
        } else {
            FILE *fout = ::freopen("/dev/null", "w", stdout);
            if (!fout) {
                throw std::runtime_error("Cannot attach stdout");
            }
            FILE *ferr = ::freopen("/dev/null", "w", stderr);
            if (!ferr) {
                throw std::runtime_error("Cannot attach stderr");
            }
        }
    }

    static void waitForTerminationRequest() {
        sigset_t sset;
        ::sigemptyset(&sset);
        ::sigaddset(&sset, SIGINT);
        ::sigaddset(&sset, SIGQUIT);
        ::sigaddset(&sset, SIGTERM);
        ::sigprocmask(SIG_UNBLOCK, &sset, NULL);
        int sig;
        ::sigwait(&sset, &sig);
    }

    // std::atomic_int gSignalVal{-1};
    // extern "C" void signalHandler(int signum) {
    //     gSignalVal.store(signum);
    // }
    // void useSignal() {
    //     signal(SIGTERM, signalHandler);
    //     signal(SIGINT, signalHandler);
    //     signal(SIGQUIT, signalHandler);
    // }
}// namespace libs


#endif// LIBS_OSUTILS_H
