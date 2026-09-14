// Copyright (c) Ubisoft. All Rights Reserved.
// Licensed under the Apache 2.0 License. See LICENSE.md in the project root for license information.

// domain_synchronizer.hpp — per-domain plugin load/unload hook template

#pragma once
#include "crg/core/config.hpp"
#include "crg/discovery/node_link.hpp"

#if CRG_PLUGINS_ENABLED

#include "crg/discovery/arena_populator.hpp"
#include "crg/discovery/plugin_chain_synchronizer.hpp"
#include "crg/host/host_descriptor.hpp"
#include "crg/routing/capability_router.hpp"

namespace crg::discovery {

    namespace internal {

        template<class TDomain>
        using PopulatorSyncAgent = PluginChainSynchronizer<TDomain, DomainArenaPopulator>;

        template<typename TDomain>
        struct SyncAgentHolder {
            static inline PopulatorSyncAgent<TDomain> Instance;
        };

        /**
         * `template struct UniversalAnchor<T>;` only emits static *symbols* —
         * it does not construct an instance, so the inherited NodeLink ctor
         * never runs and `internal::PluginLocalStorage<AnchorLink>::s_LocalHead`
         * stays null. This holder fixes that: instantiating it via
         * `template struct DomainAnchorRegistry<TDomain>;` emits each member
         * exactly once per binary (vague linkage on `static inline`), runs
         * their constructors, and populates the AnchorLink chain that the
         * plugin bootstrap walks to stitch `s_RemoteTarget` across binaries.
         */
        template<typename TDomain>
        struct DomainAnchorRegistry {
        private:
            using Populator = DomainArenaPopulator<TDomain>;
        public:
            static inline UniversalAnchor<Populator*>                    s_HeadAnchor{};
            static inline UniversalAnchor<std::vector<const Populator*>> s_CacheAnchor{};
        };
    }

} // namespace crg::discovery

#endif // CRG_PLUGINS_ENABLED
