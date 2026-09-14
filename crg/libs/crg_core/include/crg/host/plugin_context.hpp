// Copyright (c) Ubisoft. All Rights Reserved.
// Licensed under the Apache 2.0 License. See LICENSE.md in the project root for license information.

// plugin_context.hpp — per-load context passed to plugin entry points

#pragma once
#include "crg/core/types.hpp"

namespace crg::host { struct HostDescriptor; }

namespace crg::host {

    extern "C" {

        // Value-typed so two PluginLoaders can coexist without trampling each other's state.
        struct CRG_Context {
            const ::crg::host::HostDescriptor*  m_HostDescriptor;
        };

    } // extern "C"

} // namespace crg::host
