// Copyright (c) Ubisoft. All Rights Reserved.
// Licensed under the Apache 2.0 License. See LICENSE.md in the project root for license information.

// config.hpp — Build-injected compile-time configuration macros

#pragma once
#include <cstddef>

#ifndef CRG_CACHE_LINE_SIZE
    #define CRG_CACHE_LINE_SIZE 64
#endif

#ifndef CRG_BUFFER_ALIGNMENT
    #define CRG_BUFFER_ALIGNMENT alignof(std::max_align_t)
#endif

#ifndef CRG_MODEL_SHELL_SIZE
    #define CRG_MODEL_SHELL_SIZE CRG_CACHE_LINE_SIZE
#endif

#ifndef CRG_SHARED_LIB_PREFIX
    #error "CRG Build Error: CRG_SHARED_LIB_PREFIX not defined. Inject via build system."
#endif
#ifndef CRG_SHARED_LIB_EXTENSION
    #error "CRG Build Error: CRG_SHARED_LIB_EXTENSION not defined. Inject via build system."
#endif

#define CRG_SHARED_LIB(name) CRG_SHARED_LIB_PREFIX name CRG_SHARED_LIB_EXTENSION

#ifndef CRG_PLUGINS_ENABLED
    #define CRG_PLUGINS_ENABLED 0
#endif

#if CRG_PLUGINS_ENABLED
    #define CRG_PLUGINS_ENABLED_ONLY(...) __VA_ARGS__
    #define CRG_DLL_DISABLED_ONLY(...)
#else
    #define CRG_PLUGINS_ENABLED_ONLY(...)
    #define CRG_DLL_DISABLED_ONLY(...) __VA_ARGS__
#endif

#if !CRG_PLUGINS_ENABLED
    #ifndef CRG_MISSING_TYPEHASH_AS_ERROR
        #define CRG_MISSING_TYPEHASH_AS_ERROR 0
    #endif
#endif


#ifndef CRG_ASSERT_ENABLED
    #define CRG_ASSERT_ENABLED 1
#endif

#if CRG_ASSERT_ENABLED
    #include <cassert>
    #define CRG_ASSERT(cond, msg) assert((cond) && (msg))
    #define CRG_ASSERT_ENABLED_ONLY(...) __VA_ARGS__
#else
    #define CRG_ASSERT(cond, msg) ((void)0)
    #define CRG_ASSERT_ENABLED_ONLY(...)
#endif

// CRG_VERIFY is an assert that survives NDEBUG. It is on by default in every build, release included;
// an application that does not want it defines CRG_VERIFY_ENABLED=0.
// Unlike CRG_ASSERT it is not removed by NDEBUG, so a failed check always reports and stops.
#ifndef CRG_VERIFY_ENABLED
    #define CRG_VERIFY_ENABLED 1
#endif

#if CRG_VERIFY_ENABLED
    #include <cstdio>
    #include <cstdlib>
    namespace crg::core {
        [[noreturn]] inline void VerifyFailed(const char* expr, const char* msg, const char* file, int line) {
            std::fprintf(stderr, "CRG_VERIFY failed: %s (%s) at %s:%d\n", msg, expr, file, line);
            std::abort();
        }
    }
    #define CRG_VERIFY(cond, msg) \
        ((cond) ? static_cast<void>(0) : ::crg::core::VerifyFailed(#cond, msg, __FILE__, __LINE__))
    #define CRG_VERIFY_ENABLED_ONLY(...) __VA_ARGS__
#else
    #define CRG_VERIFY(cond, msg) ((void)0)
    #define CRG_VERIFY_ENABLED_ONLY(...)
#endif

// CRG_WARNING is a soft CRG_VERIFY: it reports to stderr and continues. It exists in debug builds only
// (never under NDEBUG) unless the application defines CRG_WARNING_ENABLED=1.
#ifndef CRG_WARNING_ENABLED
    #ifdef NDEBUG
        #define CRG_WARNING_ENABLED 0
    #else
        #define CRG_WARNING_ENABLED 1
    #endif
#endif

#if CRG_WARNING_ENABLED
    #include <cstdio>
    #define CRG_WARNING(cond, msg) \
        ((cond) ? static_cast<void>(0) : static_cast<void>(std::fprintf(stderr, "CRG_WARNING: %s (%s) at %s:%d\n", msg, #cond, __FILE__, __LINE__)))
    #define CRG_WARNING_ENABLED_ONLY(...) __VA_ARGS__
#else
    #define CRG_WARNING(cond, msg) ((void)0)
    #define CRG_WARNING_ENABLED_ONLY(...)
#endif

#ifndef CRG_HASH_NAME_ENABLED
    #define CRG_HASH_NAME_ENABLED 0
#endif

#if CRG_HASH_NAME_ENABLED
    #define CRG_HASH_NAME_ENABLED_ONLY(...) __VA_ARGS__
#else
    #define CRG_HASH_NAME_ENABLED_ONLY(...)
#endif
