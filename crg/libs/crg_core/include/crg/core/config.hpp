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
    #define CRG_ASSERT_ENABLED_ONLY(...) ((void)0)
#endif

#ifndef CRG_HASH_NAME_ENABLED
    #define CRG_HASH_NAME_ENABLED 0
#endif

#if CRG_HASH_NAME_ENABLED
    #define CRG_HASH_NAME_ENABLED_ONLY(...) __VA_ARGS__
#else
    #define CRG_HASH_NAME_ENABLED_ONLY(...)
#endif
