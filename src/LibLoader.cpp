#include "LibLoader.h"

#ifdef _WIN32
#include <Windows.h>
#elif defined(__linux__)
#include <dlfcn.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace Internal {
  bool direntExists(const lib_string& path, bool is_dir) {
    struct stat st;
    if(stat(path.c_str(), &st) < 0) {
      // log errno
      return false;
    }

    if(S_ISDIR(st.st_mode))
      return is_dir;
    if(S_ISREG(st.st_mode))
      return !is_dir;
    return false;
  }

  bool fileExists(const lib_string& path) {
    return direntExists(path, false);
  }

  bool dirExists(const lib_string& path) {
    return direntExists(path, true);
  }
} // namespace Internal

LibLoader::LibLoader() :
    m_handle(nullptr) {
}

LibLoader::~LibLoader() {
  unload();
}

bool LibLoader::load(const lib_string& in_lib_path) {
  auto rc = false;
  if(!Internal::fileExists(in_lib_path)) {
    // log lib not exist
    return false;
  }

#ifdef _WIN32
  m_handle = LoadLibrary(in_lib_path.c_str());
#else
  m_handle = dlopen(in_lib_path.c_str(), RTLD_NOW);
#endif
  if(!m_handle) {
    // log lib load error: dlerror()
    return false;
  }
  return true;
}

void LibLoader::unload() {
  if(!m_handle)
    return;
#ifdef _WIN32
  FreeLibrary((HMODULE)m_handle);
#elif defined(__linux__)
  dlclose(m_handle);
#endif
  m_handle = nullptr;
}

bool LibLoader::ready() const {
  return (m_handle != nullptr);
}

void* LibLoader::proc(const lib_string& in_proc_name) {
  if(!m_handle)
    return nullptr;

#ifdef _WIN32
  return GetProcAddress((HMODULE)m_handle, in_proc_name.c_str());
#else
  return dlsym(m_handle, in_proc_name.c_str());
#endif
}
