// Copyright (c) Ubisoft. All Rights Reserved.
// Licensed under the Apache 2.0 License. See LICENSE.md in the project root for license information.

// Plugin DLL compiled twice (crg_join_r0, crg_join_r1, JOIN_ROLE 0/1): each copy adds one model and one arena populator
// to JoinDomain, which the HOST defines, so this module's chains join the host's. The exported bootstrap splices the
// plugin record chain either before or after every other synchronizer; the probes report what the module observes.

#include "crg/crg_plugin.hpp"
#include "crg/crg.hpp"
#include "order_common.hpp"

CRG_DEFINE_PLUGIN(JoinPlugin)
CRG_DEFINE_DOMAIN(JoinDomain)
CRG_DEFINE_SHARED_STATE(OrderLog)

#define JOIN_CAT2(a, b) a##b
#define JOIN_CAT(a, b)  JOIN_CAT2(a, b)
struct JOIN_CAT(JoinRole, JOIN_ROLE) {};
CRG_DECLARE_DOMAIN_MODELS(JoinDomain, JOIN_CAT(JoinRole, JOIN_ROLE))

namespace {
    int s_Populates = 0;
    struct JoinPopulator final : ::crg::discovery::DomainArenaPopulator<JoinDomain> {
        void Populate() override {
            ++s_Populates;
            auto& log = ::crg::discovery::GetSharedState<OrderLog>();
            if (log.m_SeqCount < 16) log.m_Seq[log.m_SeqCount++] = JOIN_ROLE;
        }
    };
    const JoinPopulator s_Populator;

    using RecordSync = ::crg::discovery::UniversalAnchorSynchronizer<::crg::discovery::PluginRecordNode*>;
    int s_RecordSyncs = 0;
}

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

// which: 0 = host model, 1 = role 0 model, 2 = role 1 model. Slot in THIS module's view of the dense index.
extern "C" CRG_EXPORT unsigned CRG_JoinSlot(int which) {
    const char* names[3] = { "JoinHost", "JoinRole0", "JoinRole1" };
    return ::crg::core::DenseIndexStore<JoinDomain>::Find(::crg::hash::internal::HashString(names[which]));
}
extern "C" CRG_EXPORT unsigned CRG_JoinSize() { return ::crg::core::DenseIndexStore<JoinDomain>::Size(); }
extern "C" CRG_EXPORT int CRG_JoinPopulates() { return s_Populates; }
extern "C" CRG_EXPORT int CRG_JoinRecordSyncs() { return s_RecordSyncs; }
