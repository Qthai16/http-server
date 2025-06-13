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

namespace libs {
    static void beDaemon() {
        pid_t pid;
        if ((pid = ::fork()) < 0)
            throw std::runtime_error("cannot fork daemon process");
        else if (pid != 0)
            ::exit(0);

        ::setsid();
        ::umask(0);

        // attach stdin, stdout, stderr to /dev/null
        // instead of just closing them. This avoids
        // issues with third party/legacy code writing
        // stuff to stdout/stderr.
        FILE *fin = ::freopen("/dev/null", "r+", stdin);
        if (!fin)
            throw std::runtime_error("Cannot attach stdin to /dev/null");
        FILE *fout = ::freopen("/dev/null", "r+", stdout);
        if (!fout)
            throw std::runtime_error("Cannot attach stdout to /dev/null");
        FILE *ferr = ::freopen("/dev/null", "r+", stderr);
        if (!ferr)
            throw std::runtime_error("Cannot attach stderr to /dev/null");
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
