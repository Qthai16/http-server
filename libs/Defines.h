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

#define LIB_CONCAT(A, B)                 LIB_CONCAT_(A, B)
#define LIB_CONCAT_(A, B)                A##B

// refer from PhotonLibOS
#define __INLINE__ __attribute__((always_inline))
#define __FORCE_INLINE__ __INLINE__ inline

template<typename T>
class Defer final {
public:
    explicit Defer(T fn) : fn_(std::move(fn)) {}
    ~Defer() { fn_(); }
    Defer(const Defer &) = delete;
    Defer &operator=(const Defer &) = delete;
    Defer(Defer &&) = delete;
    Defer &operator=(Defer &&) = delete;
private:
    T fn_;
};

template<typename T> __FORCE_INLINE__
Defer<T> make_defer(T func) { return Defer<T>(func); }

#define DEFER(func)   auto LIB_CONCAT(defer, __LINE__) = make_defer([&]() __INLINE__ { func; })



// #include <functional>
// class Defer final {
// public:
//     explicit Defer(std::function<void()> &&fn) : fn_(std::move(fn)) {}
//     ~Defer() { fn_(); }
//     Defer(const Defer &) = delete;
//     Defer &operator=(const Defer &) = delete;
//     Defer(Defer &&) = delete;
//     Defer &operator=(Defer &&) = delete;

// private:
//     std::function<void()> fn_;
// };

// defer using template instead of std::function

#endif// LIBS_DEFINES_H