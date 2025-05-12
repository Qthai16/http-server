/*
 * File:   Defines.h
 * Author: thaipq
 *
 * Created on Wed May 07 2025 11:26:48 AM
 */

#ifndef LIBS_DEFINES_H
#define LIBS_DEFINES_H

#ifndef expr_likely
#define expr_likely(x)   __builtin_expect(!!(x), 1)
#endif

#ifndef expr_unlikely
#define expr_unlikely(x) __builtin_expect(!!(x), 0)
#endif

#endif// LIBS_DEFINES_H