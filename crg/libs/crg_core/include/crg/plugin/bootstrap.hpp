// Copyright (c) Ubisoft. All Rights Reserved.
// Licensed under the Apache 2.0 License. See LICENSE.md in the project root for license information.

#pragma once
#include "crg/plugin/platform.hpp"
#include "crg/host/plugin_context.hpp"

namespace crg::plugin {

    struct PluginSymbols {
        static constexpr const char* Bootstrap  = "CRG_Bootstrap";
        static constexpr const char* Shutdown   = "CRG_Shutdown";
        static constexpr const char* BridgeInit = "CRG_BridgeInit";
    };

    struct PluginLoader {
        using BootstrapFn  = void(*)(::crg::host::CRG_Context*);
        using ShutdownFn   = void(*)();
        using BridgeInitFn = void(*)(::crg::host::CRG_Context*);

        static BootstrapFn ResolveBootstrap(::crg::system::platform::ModuleHandle h) {
            return reinterpret_cast<BootstrapFn>(
                ::crg::system::platform::GetSymbolAddress(h, PluginSymbols::Bootstrap));
        }

        static ShutdownFn ResolveShutdown(::crg::system::platform::ModuleHandle h) {
            return reinterpret_cast<ShutdownFn>(
                ::crg::system::platform::GetSymbolAddress(h, PluginSymbols::Shutdown));
        }

        static BridgeInitFn ResolveBridgeInit(::crg::system::platform::ModuleHandle h) {
            return reinterpret_cast<BridgeInitFn>(
                ::crg::system::platform::GetSymbolAddress(h, PluginSymbols::BridgeInit));
        }
    };

}
