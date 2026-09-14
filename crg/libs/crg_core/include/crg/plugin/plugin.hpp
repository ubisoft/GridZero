// Copyright (c) Ubisoft. All Rights Reserved.
// Licensed under the Apache 2.0 License. See LICENSE.md in the project root for license information.

// plugin.hpp — macros for defining and bootstrapping a CRG plugin

#pragma once
#include "crg/core/config.hpp"
#include "crg/host/host_descriptor.hpp"
#include "crg/plugin/bootstrap.hpp"
#include "crg/discovery/sync_node.hpp"

#ifndef CRG_EXPORT
    #error "CRG Build Error: CRG_EXPORT not defined. Inject via build system."
#endif

#define CRG_PROTECT_TEMPLATE(...) __VA_ARGS__

#define CRG_DEFINE_PLUGIN(PluginName)                                                                                                \
    extern "C" CRG_EXPORT void CRG_Bootstrap(::crg::host::CRG_Context* ctx) {                                                       \
        if (!ctx || !ctx->m_HostDescriptor) return;                                                                                  \
        ::crg::discovery::ISyncNode::VisitLocal([ctx](::crg::discovery::ISyncNode& a) { a.OnPluginPreLoad(*ctx); });                 \
        ::crg::discovery::AnchorLink* anchor = ::crg::discovery::AnchorLink::GetLocalHead();                                         \
        while (anchor) {                                                                                                             \
            void* hostAddr = ctx->m_HostDescriptor->FindAnchorAddressByHash(anchor->GetTypeHash());                                  \
            if (hostAddr) { anchor->SetRemoteAddress(hostAddr); }                                                                    \
            anchor = anchor->m_Next;                                                                                                 \
        }                                                                                                                            \
        ::crg::discovery::ISyncNode::VisitLocal([ctx](::crg::discovery::ISyncNode& a) { a.OnPluginLoad(*ctx); });                    \
    }                                                                                                                                \
    extern "C" CRG_EXPORT void CRG_Shutdown() {                                                                                     \
        ::crg::discovery::ISyncNode::VisitLocal([](::crg::discovery::ISyncNode& a) { a.OnPluginUnload(); });                         \
    }                                                                                                                                \
    extern "C" CRG_EXPORT void CRG_BridgeInit(::crg::host::CRG_Context* ctx) { CRG_Bootstrap(ctx); }

#define CRG_IMPLEMENT_PARTITION(TDomain)     CRG_DEFINE_UNIVERSAL_ANCHOR(CRG_PROTECT_TEMPLATE(::crg::core::internal::DenseIndexState<TDomain>))
