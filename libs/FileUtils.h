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
#include <errno.h>

namespace libs {

    inline bool write(int fd, int64_t off, const void *buf, size_t size) {
        while (true) {
            ssize_t wb = ::pwrite(fd, buf, size, off);
            if (wb >= (ssize_t) size) {
                return true;
            } else if (wb > 0) {
                buf = (char *) buf + wb;
                size -= wb;
                off += wb;
            } else if (wb == -1) {
                if (errno != EINTR)
                    return false;
            } else if (size > 0) {
                return false;
            }
        }
        return true;
    }

    inline size_t read(int32_t fd, int64_t off, void *buf, size_t size) {
        while (true) {
            ssize_t rb = ::pread(fd, buf, size, off);
            if (rb >= (ssize_t) size) {
                break;
            } else if (rb > 0) {
                buf = (char *) buf + rb;
                size -= rb;
                off += rb;
            } else if (rb == -1) {
                if (errno != EINTR)
                    return false;
            } else {
                return false;
            }
        }
        return true;
    }

    // size_t file_size(const char* filename) {

    // }

};// namespace libs

#endif// LIBS_FILEUTILS_H