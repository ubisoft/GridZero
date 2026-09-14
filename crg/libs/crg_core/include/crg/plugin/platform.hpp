// Copyright (c) Ubisoft. All Rights Reserved.
// Licensed under the Apache 2.0 License. See LICENSE.md in the project root for license information.

// platform.hpp — OS-agnostic dynamic library loading interface

#pragma once

namespace crg::system::platform {

    using ModuleHandle = void*;

    ModuleHandle LoadDynamicLibrary(const char* filePath);

    ModuleHandle LoadBridgeLibrary(const char* filePath);

    void* GetSymbolAddress(ModuleHandle handle, const char* symbolName);

    void UnloadDynamicLibrary(ModuleHandle handle);

}
