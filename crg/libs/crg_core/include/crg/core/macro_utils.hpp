// Copyright (c) Ubisoft. All Rights Reserved.
// Licensed under the Apache 2.0 License. See LICENSE.md in the project root for license information.

// macro_utils.hpp — MSVC-safe variadic FOR_EACH macro expansion

#pragma once

#define CRG_EXPAND(x) x
#define CRG_CONCAT_(a, b) a##b
#define CRG_CONCAT(a, b)  CRG_CONCAT_(a, b)

#define CRG_FE_1(M, x)         M(x)
#define CRG_FE_2(M, x, ...)    M(x) CRG_EXPAND(CRG_FE_1(M, __VA_ARGS__))
#define CRG_FE_3(M, x, ...)    M(x) CRG_EXPAND(CRG_FE_2(M, __VA_ARGS__))
#define CRG_FE_4(M, x, ...)    M(x) CRG_EXPAND(CRG_FE_3(M, __VA_ARGS__))
#define CRG_FE_5(M, x, ...)    M(x) CRG_EXPAND(CRG_FE_4(M, __VA_ARGS__))
#define CRG_FE_6(M, x, ...)    M(x) CRG_EXPAND(CRG_FE_5(M, __VA_ARGS__))
#define CRG_FE_7(M, x, ...)    M(x) CRG_EXPAND(CRG_FE_6(M, __VA_ARGS__))
#define CRG_FE_8(M, x, ...)    M(x) CRG_EXPAND(CRG_FE_7(M, __VA_ARGS__))
#define CRG_FE_9(M, x, ...)    M(x) CRG_EXPAND(CRG_FE_8(M, __VA_ARGS__))
#define CRG_FE_10(M, x, ...)   M(x) CRG_EXPAND(CRG_FE_9(M, __VA_ARGS__))
#define CRG_FE_11(M, x, ...)   M(x) CRG_EXPAND(CRG_FE_10(M, __VA_ARGS__))
#define CRG_FE_12(M, x, ...)   M(x) CRG_EXPAND(CRG_FE_11(M, __VA_ARGS__))
#define CRG_FE_13(M, x, ...)   M(x) CRG_EXPAND(CRG_FE_12(M, __VA_ARGS__))
#define CRG_FE_14(M, x, ...)   M(x) CRG_EXPAND(CRG_FE_13(M, __VA_ARGS__))
#define CRG_FE_15(M, x, ...)   M(x) CRG_EXPAND(CRG_FE_14(M, __VA_ARGS__))
#define CRG_FE_16(M, x, ...)   M(x) CRG_EXPAND(CRG_FE_15(M, __VA_ARGS__))

// CRG_EXPAND wrapper is required: MSVC legacy preprocessor expands __VA_ARGS__
// as a single token without it, causing CRG_GET_NTH_ARG to receive one argument
// instead of N, selecting the wrong CRG_FE_N variant.
#define CRG_GET_NTH_ARG(_1,_2,_3,_4,_5,_6,_7,_8,_9,_10,_11,_12,_13,_14,_15,_16,N,...) N

#define CRG_FOR_EACH(M, ...)                                                  \
    CRG_EXPAND(CRG_GET_NTH_ARG(__VA_ARGS__,                                   \
        CRG_FE_16, CRG_FE_15, CRG_FE_14, CRG_FE_13,                           \
        CRG_FE_12, CRG_FE_11, CRG_FE_10, CRG_FE_9,                            \
        CRG_FE_8,  CRG_FE_7,  CRG_FE_6,  CRG_FE_5,                            \
        CRG_FE_4,  CRG_FE_3,  CRG_FE_2,  CRG_FE_1)(M, __VA_ARGS__))
