// Copyright (c) Ubisoft. All Rights Reserved.
// Licensed under the Apache 2.0 License. See LICENSE.md in the project root for license information.

// Plugin DLL compiled twice (crg_order_a, crg_order_b, ORDER_VARIANT 0/1): each copy owns one model and one arena
// populator of OrderDomain. Besides the normal bootstrap it exports a bootstrap that splices the plugin record chain
// either before or after every other synchronizer, to witness whether the result depends on that order.

#include "crg/crg_plugin.hpp"
#include "crg/crg.hpp"
#include "order_common.hpp"

#include <cstring>

CRG_DEFINE_PLUGIN(OrderPlugin)
CRG_DEFINE_DOMAIN(OrderDomain)
CRG_DEFINE_SHARED_STATE(OrderLog)

struct OrderModelA {};
struct OrderModelB {};

#if ORDER_VARIANT == 0
CRG_DECLARE_DOMAIN_MODELS(OrderDomain, OrderModelA)
#else
CRG_DECLARE_DOMAIN_MODELS(OrderDomain, OrderModelB)
#endif

namespace {
    struct OrderPopulator final : ::crg::discovery::DomainArenaPopulator<OrderDomain> {
        void Populate() override { ++::crg::discovery::GetSharedState<OrderLog>().m_Populates[ORDER_VARIANT]; }
    };
    const OrderPopulator s_Populator;

    using RecordSync = ::crg::discovery::UniversalAnchorSynchronizer<::crg::discovery::PluginRecordNode*>;
}

namespace { int s_RecordSyncs = 0; }

extern "C" CRG_EXPORT void CRG_BootstrapOrdered(::crg::host::CRG_Context* ctx, int recordFirst) {
    using ::crg::discovery::ISyncNode;
    ISyncNode::VisitLocal([ctx](ISyncNode& a) { a.BindStorage(*ctx); });
    for (int pass = 0; pass < 2; ++pass) {
        const bool wantRecord = (pass == 0) == (recordFirst != 0);
        ISyncNode::VisitLocal([ctx, wantRecord, pass](ISyncNode& a) {
            const bool isRecord = dynamic_cast<RecordSync*>(&a) != nullptr;
            if (isRecord && pass == 0) ++s_RecordSyncs;
            if (isRecord == wantRecord) a.SpliceTopology(*ctx);
        });
    }
    ISyncNode::VisitLocal([ctx](ISyncNode& a) { a.LinkStorage(*ctx); });
}

// Slot of each model in THIS module's dense index (InvalidDenseSlot when the module does not know it).
extern "C" CRG_EXPORT unsigned CRG_OrderSlot(int which) {
    const ::crg::u64 hash = ::crg::hash::internal::HashString(which == 0 ? "OrderModelA" : "OrderModelB");
    return ::crg::core::DenseIndexStore<OrderDomain>::Find(hash);
}
extern "C" CRG_EXPORT unsigned CRG_OrderSize() { return ::crg::core::DenseIndexStore<OrderDomain>::Size(); }
// How many synchronizers of this module the bootstrap recognised as the plugin record chain (the witness needs exactly one).
extern "C" CRG_EXPORT int CRG_OrderRecordSyncs() { return s_RecordSyncs; }
