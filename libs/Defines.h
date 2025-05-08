/*
 * File:   Defines.h
 * Author: thaipq
 *
 * Created on Wed May 07 2025 11:26:48 AM
 */

#ifndef LIBS_DEFINES_H
#define LIBS_DEFINES_H

#define expr_likely(x)   __builtin_expect(!!(x), 1)
#define expr_unlikely(x) __builtin_expect(!!(x), 0)

#endif// LIBS_DEFINES_H