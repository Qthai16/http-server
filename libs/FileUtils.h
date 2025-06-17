/*
 * File:   FileUtils.h
 * Author: thaipq
 *
 * Created on Fri May 09 2025 10:36:26 AM
 */

#ifndef LIBS_FILEUTILS_H
#define LIBS_FILEUTILS_H

#include <unistd.h>
#include <stdint.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <functional>

namespace libs {

    class Defer final {
    public:
        explicit Defer(std::function<void()> &&fn) : fn_(std::move(fn)) {}
        ~Defer() { fn_(); }
        Defer(const Defer &) = delete;
        Defer &operator=(const Defer &) = delete;
        Defer(Defer&&) = delete;
        Defer& operator=(Defer&&) = delete;

    private:
        std::function<void()> fn_;
    };

    inline ssize_t write(int fd, int64_t off, const void *buf, size_t size) {
        ssize_t ret = 0;
        while (true) {
            ssize_t wb = ::pwrite(fd, buf, size, off);
            if (wb >= (ssize_t) size) {
                ret += wb;
                break;
            } else if (wb > 0) {
                buf = (char *) buf + wb;
                size -= wb;
                off += wb;
                ret += wb;
            } else if (wb == -1) {
                if (errno != EINTR)
                    return 0;
            } else if (size > 0) {
                return 0;
            }
        }
        return ret;
    }

    inline ssize_t read(int32_t fd, int64_t off, void *buf, size_t size) {
        ssize_t ret = 0;
        while (true) {
            ssize_t rb = ::pread(fd, buf, size, off);
            if (rb >= (ssize_t) size) {
                ret += rb;
                break;
            } else if (rb > 0) {
                buf = (char *) buf + rb;
                size -= rb;
                off += rb;
                ret += rb;
            } else if (rb == -1) {
                if (errno != EINTR)
                    return ret;
            } else {
                return ret;
            }
        }
        return ret;
    }

    inline int makeDir(const char *dir, int mode = S_IRWXU) {
        char tmp[4096];
        char *p = NULL;
        size_t len;

        snprintf(tmp, sizeof(tmp), "%s", dir);
        len = strlen(tmp);
        if (tmp[len - 1] == '/')
            tmp[len - 1] = 0;
        for (p = tmp + 1; *p; p++)
            if (*p == '/') {
                *p = 0;
                mkdir(tmp, mode);
                *p = '/';
            }
        return mkdir(tmp, mode);
    }

    inline bool isDir(const char *path) {
        struct ::stat st;
        return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
    }

    inline bool isFile(const char *path) {
        struct ::stat st;
        return stat(path, &st) == 0 && S_ISREG(st.st_mode);
    }

};// namespace libs

#endif// LIBS_FILEUTILS_H