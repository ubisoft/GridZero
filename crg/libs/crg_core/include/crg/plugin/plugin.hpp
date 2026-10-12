// Copyright (c) Ubisoft. All Rights Reserved.
// Licensed under the Apache 2.0 License. See LICENSE.md in the project root for license information.

// plugin.hpp — macros for defining and bootstrapping a CRG plugin

#pragma once
#include "crg/core/config.hpp"
#include "crg/plugin/bootstrap.hpp"
#include "crg/discovery/sync_node.hpp"
#include "crg/discovery/plugin_manifest.hpp"
#include "crg/discovery/universal_anchor.hpp"
#include "crg/host/host_descriptor.hpp"

#ifndef CRG_EXPORT
    #error "CRG Build Error: CRG_EXPORT not defined. Inject via build system."
#endif

#define CRG_PROTECT_TEMPLATE(...) __VA_ARGS__

#define CRG_DEFINE_PLUGIN_RECORD(PluginName)                                                                                         \
    namespace {                                                                                                                      \
        struct PluginTag_##PluginName {                                                                                              \
            static constexpr std::string_view Name = #PluginName;                                                                    \
        };                                                                                                                           \
        static const ::crg::discovery::PluginManifest<PluginTag_##PluginName>                                                        \
            s_PluginManifest{};                                                                                                      \
    }  

#define CRG_DEFINE_PLUGIN(PluginName)                                                                                                \
    CRG_DEFINE_PLUGIN_RECORD(PluginName)                                                                                             \
    extern "C" CRG_EXPORT void CRG_Bootstrap(::crg::host::CRG_Context* ctx) {                                                        \
        if (!ctx || !ctx->m_HostDescriptor) return;                                                                                  \
        ::crg::discovery::ISyncNode::VisitLocal([ctx](::crg::discovery::ISyncNode& a) { a.BindStorage(*ctx); });                     \
        ::crg::discovery::ISyncNode::VisitLocal([ctx](::crg::discovery::ISyncNode& a) { a.SpliceTopology(*ctx); });                  \
        ::crg::discovery::ISyncNode::VisitLocal([ctx](::crg::discovery::ISyncNode& a) { a.LinkStorage(*ctx); });                  \
    }                                                                                                                                \
    extern "C" CRG_EXPORT void CRG_Shutdown() {                                                                                      \
        ::crg::discovery::ISyncNode::VisitLocal([](::crg::discovery::ISyncNode& a) { a.UnlinkStorage(); });                          \
        ::crg::discovery::ISyncNode::VisitLocal([](::crg::discovery::ISyncNode& a) { a.UnspliceTopology(); });                       \
        ::crg::discovery::ISyncNode::VisitLocal([](::crg::discovery::ISyncNode& a) { a.UnbindStorage(); });                          \
    }                                                                                                                                \
    extern "C" CRG_EXPORT void CRG_BridgeInit(::crg::host::CRG_Context* ctx) { CRG_Bootstrap(ctx); }
