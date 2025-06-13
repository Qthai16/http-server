#include "LibLoader.h"

#ifdef _WIN32
#include <Windows.h>
#elif defined(__linux__)
#include <dlfcn.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#include "FileUtils.h"

LibLoader::LibLoader() : m_handle(nullptr) {
}

LibLoader::~LibLoader() {
    unload();
}

bool LibLoader::load(const std::string &in_lib_path) {
    auto rc = false;
    if (!libs::isFile(in_lib_path.c_str()))
        return false;

#ifdef _WIN32
    m_handle = LoadLibrary(in_lib_path.c_str());
#else
    m_handle = dlopen(in_lib_path.c_str(), RTLD_NOW); // switch to lazy load
#endif
    if (!m_handle) {
        // log lib load error: dlerror()
        return false;
    }
    return true;
}

void LibLoader::unload() {
    if (!m_handle)
        return;
#ifdef _WIN32
    FreeLibrary((HMODULE) m_handle);
#elif defined(__linux__)
    dlclose(m_handle);
#endif
    m_handle = nullptr;
}

bool LibLoader::ready() const {
    return (m_handle != nullptr);
}

void *LibLoader::proc(const std::string &in_proc_name) {
    if (!m_handle)
        return nullptr;

#ifdef _WIN32
    return GetProcAddress((HMODULE) m_handle, in_proc_name.c_str());
#else
    return dlsym(m_handle, in_proc_name.c_str());
#endif
}
