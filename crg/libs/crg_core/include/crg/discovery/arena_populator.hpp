// Copyright (c) Ubisoft. All Rights Reserved.
// Licensed under the Apache 2.0 License. See LICENSE.md in the project root for license information.

// arena_populator.hpp — interface for injecting rules into TensorArena

#pragma once
#include "crg/discovery/node_link.hpp"
#include "crg/discovery/plugin_manifest.hpp"
#include <algorithm>
#include <vector>

namespace crg::discovery {

    struct IArenaPopulator {
        virtual ~IArenaPopulator() = default;
        virtual void Populate() = 0;
        virtual void Clear() {};
        virtual bool CanPopulate() const noexcept { return true; }
        virtual void Sort() {};
    };

    template<class TDomain>
    class DomainArenaPopulator : public NodeLink<DomainArenaPopulator<TDomain>, IArenaPopulator>
    {
        using Super = NodeLink<DomainArenaPopulator<TDomain>, IArenaPopulator>;

    public:
        struct Chain
        {
        private:
            // The populators of this module's chain, then those of the other loaded modules not already seen.
            template<class TFn>
            static void VisitAll(TFn&& fn) {
                std::vector<const DomainArenaPopulator*> seen;
                Super::Visit([&seen, &fn](DomainArenaPopulator& populator) {
                    seen.push_back(&populator);
                    fn(populator);
                });
                PluginRecordNode::VisitSegments<DomainArenaPopulator>([&seen, &fn](DomainArenaPopulator& populator) {
                    if (std::find(seen.begin(), seen.end(), &populator) != seen.end()) return;
                    seen.push_back(&populator);
                    fn(populator);
                });
            }

        public:
            static void Clear() {
                VisitAll([](auto& populator) {
                    populator.Clear();
                });
            }

            static void Populate() {
                VisitAll([](auto& populator) {
                    populator.Populate();
                });
            }

            static void Sort() {
                VisitAll([](auto& populator) {
                    populator.Sort();
                });
            }

            static void Build() {
                Clear();
                Super::RefreshCache();
                Populate();
                Sort();
            }
        };
    };

} // namespace crg::discovery
