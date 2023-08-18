#pragma once

#ifdef _WIN32
  #ifdef LIB_EXPORT    
    #define LIB_DECLSPEC __declspec(dllexport)
  #else
    #define LIB_DECLSPEC __declspec(dllimport)
  #endif

  /**
   *  @brief Defines the export/import macro for class declarations
   */
  #define LIB_DECLCLASS LIB_DECLSPEC

#elif defined(__linux__)
  /**
    *  @brief Defines export function names of the API
    */
  #define LIB_DECLSPEC __attribute__ ((visibility ("default")))

  /**
   *  @brief Defines the export/import macro for class declarations
   */
  #define LIB_DECLCLASS LIB_DECLSPEC
#endif