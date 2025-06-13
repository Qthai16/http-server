/*
 * File:   LibLoader.h
 * Author: thaipq
 *
 * Created on Wed Jun 11 2025 2:29:50 PM
 */

#ifndef LIBS_LIBLOADER_H
#define LIBS_LIBLOADER_H

#include <string>
#include <vector>

#include "Defines.h"

class LIB_DECLCLASS LibLoader {
public:
    LibLoader();
    virtual ~LibLoader();
    virtual bool load(const std::string &in_lib_path);
    virtual void unload();
    virtual bool ready() const;
    void *proc(const std::string &in_proc_name);

private:
    void *m_handle;
};

#endif// LIBS_LIBLOADER_H
