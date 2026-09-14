// Copyright (c) Ubisoft. All Rights Reserved.
// Licensed under the Apache 2.0 License. See LICENSE.md in the project root for license information.

#include "catch.hpp"
#include "crg/crg_discovery.hpp"
#include <algorithm>
#include <vector>

namespace crg::test::mono {
    using namespace crg::discovery;

    struct IMockGripper {
        virtual void Grip() const = 0;
        virtual ~IMockGripper() = default;
    };

    struct GripperPluginBase : public NodeLink<GripperPluginBase, IMockGripper> {};

    struct HeavyDutyGripper : public GripperPluginBase {
        void Grip() const override {}
    };
}

CRG_DECLARE_UNIVERSAL_NODE_ANCHOR(::crg::test::mono::GripperPluginBase)

namespace {
    static const crg::test::mono::HeavyDutyGripper s_HeavyPlugin;
}

TEST_CASE("Discovery: Local Domain Sync (Mono)", "[discovery][mono][robotics]") {
    using namespace crg::test::mono;

    int discoveredCount = 0;
    GripperPluginBase::Visit([&discoveredCount](const GripperPluginBase&) { discoveredCount++; });
    REQUIRE(discoveredCount == 1);
}

namespace crg::test::sort {
    using namespace crg::discovery;

    struct ISortable {
        int m_Priority{ 0 };
        virtual ~ISortable() = default;
    };

    struct SortableNode : public NodeLink<SortableNode, ISortable> {
        explicit SortableNode(int p) { m_Priority = p; }
    };
}

CRG_DECLARE_UNIVERSAL_NODE_ANCHOR(::crg::test::sort::SortableNode)

template<>
struct crg::discovery::NodeLinkTraits<crg::test::sort::SortableNode> {
    static void SortCache(std::vector<const crg::test::sort::SortableNode*>& cache) {
        std::sort(cache.begin(), cache.end(),
            [](const crg::test::sort::SortableNode* a, const crg::test::sort::SortableNode* b) {
                return a->m_Priority < b->m_Priority;
            });
    }
};

template class crg::discovery::NodeLink<crg::test::sort::SortableNode, crg::test::sort::ISortable>;

namespace {
    static const crg::test::sort::SortableNode s_NodeC{ 3 };
    static const crg::test::sort::SortableNode s_NodeA{ 1 };
    static const crg::test::sort::SortableNode s_NodeB{ 2 };
}

TEST_CASE("NodeLink: SortCache hook orders cache by priority", "[discovery][sort]") {
    using namespace crg::test::sort;

    SortableNode::RefreshCache();
    const auto& cache = SortableNode::GetCache();

    REQUIRE(cache.size() == 3);
    REQUIRE(cache[0]->m_Priority == 1);
    REQUIRE(cache[1]->m_Priority == 2);
    REQUIRE(cache[2]->m_Priority == 3);
}

TEST_CASE("NodeLink: SortCache applied on every RefreshCache call", "[discovery][sort]") {
    using namespace crg::test::sort;

    SortableNode::RefreshCache();
    SortableNode::RefreshCache();
    const auto& cache = SortableNode::GetCache();

    REQUIRE(cache.size() == 3);
    REQUIRE(cache[0]->m_Priority == 1);
    REQUIRE(cache[1]->m_Priority == 2);
    REQUIRE(cache[2]->m_Priority == 3);
}

TEST_CASE("NodeLink: without SortCache hook, no reordering occurs", "[discovery][sort]") {
    using namespace crg::test::mono;

    GripperPluginBase::RefreshCache();
    const auto& cache = GripperPluginBase::GetCache();
    REQUIRE(cache.size() == 1);
}
