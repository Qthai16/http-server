/*
 * File:   Defines.h
 * Author: thaipq
 *
 * Created on Wed May 07 2025 11:26:48 AM
 */

#ifndef LIBS_DEFINES_H
#define LIBS_DEFINES_H

#ifndef expr_likely
#define expr_likely(x) __builtin_expect(!!(x), 1)
#endif

#ifndef expr_unlikely
#define expr_unlikely(x) __builtin_expect(!!(x), 0)
#endif

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
#define LIB_DECLSPEC  __attribute__((visibility("default")))

/**
   *  @brief Defines the export/import macro for class declarations
   */
#define LIB_DECLCLASS LIB_DECLSPEC
#endif

#endif// LIBS_DEFINES_H