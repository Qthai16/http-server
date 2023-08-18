#pragma once

#include <string>
#include <vector>

#include "LibDefines.h"
#include "LibTypes.h"

class LIB_DECLCLASS LibLoader {
public:
  LibLoader();
  virtual ~LibLoader();

  virtual bool load(const lib_string& in_lib_path);
  virtual void unload();
  virtual bool ready() const;
  void* proc(const lib_string& in_proc_name);

private:
  void* m_handle;
};
