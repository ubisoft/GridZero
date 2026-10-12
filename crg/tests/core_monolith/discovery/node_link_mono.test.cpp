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

CRG_DECLARE_NODE_LINK(::crg::test::mono::GripperPluginBase)

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

CRG_DECLARE_NODE_LINK(::crg::test::sort::SortableNode)

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

    const auto& cache = SortableNode::GetCache();

    REQUIRE(cache.size() == 3);
    REQUIRE(cache[0]->m_Priority == 1);
    REQUIRE(cache[1]->m_Priority == 2);
    REQUIRE(cache[2]->m_Priority == 3);
}

TEST_CASE("NodeLink: SortCache applied on every RefreshCache call", "[discovery][sort]") {
    using namespace crg::test::sort;

    const auto& cache = SortableNode::GetCache();

    REQUIRE(cache.size() == 3);
    REQUIRE(cache[0]->m_Priority == 1);
    REQUIRE(cache[1]->m_Priority == 2);
    REQUIRE(cache[2]->m_Priority == 3);
}

TEST_CASE("NodeLink: without SortCache hook, no reordering occurs", "[discovery][sort]") {
    using namespace crg::test::mono;

    const auto& cache = GripperPluginBase::GetCache();
    REQUIRE(cache.size() == 1);
}

namespace crg::test::reverse_unsorted {
    using namespace crg::discovery;

    struct IRevMock { virtual ~IRevMock() = default; };
    struct RevNode : public NodeLink<RevNode, IRevMock> {
        int m_Id{ 0 };
        explicit RevNode(int id) : m_Id(id) {}
    };
}

CRG_DECLARE_NODE_LINK(::crg::test::reverse_unsorted::RevNode)

namespace {
    // Instantiated in order: Node1, then Node2, then Node3.
    // Due to push_front, link order is Node3 -> Node2 -> Node1.
    // Forward cache order: [3, 2, 1].
    static const crg::test::reverse_unsorted::RevNode s_RevNode1{ 1 };
    static const crg::test::reverse_unsorted::RevNode s_RevNode2{ 2 };
    static const crg::test::reverse_unsorted::RevNode s_RevNode3{ 3 };
}

TEST_CASE("NodeLink: Reverse visit without SortCache (Without Find)", "[discovery][reverse][unsorted]") {
    using namespace crg::test::reverse_unsorted;

    const auto& cache = RevNode::GetCache();
    REQUIRE(cache.size() == 3);

    std::vector<int> visitedIds;
    RevNode::Visit<VisitOrder::Backward>([&visitedIds](RevNode& node) {
        visitedIds.push_back(node.m_Id);
    });

    // Backward traversal of the forward cache [3, 2, 1] visits 1, then 2, then 3.
    REQUIRE(visitedIds.size() == 3);
    REQUIRE(visitedIds[0] == 1);
    REQUIRE(visitedIds[1] == 2);
    REQUIRE(visitedIds[2] == 3);
}

TEST_CASE("NodeLink: Reverse visit without SortCache (With Find / Cancellable predicate)", "[discovery][reverse][unsorted][find]") {
    using namespace crg::test::reverse_unsorted;

    int foundId = -1;
    // Search backwards to find the first node matching a condition
    RevNode::Visit<VisitOrder::Backward>([&foundId](RevNode& node) {
        if (node.m_Id == 2) {
            foundId = node.m_Id;
            return false; // Stop traversal
        }
        return true; // Continue traversal
    });

    REQUIRE(foundId == 2);
}

TEST_CASE("NodeLink: Reverse visit with SortCache (Without Find)", "[discovery][reverse][sorted]") {
    using namespace crg::test::sort;

    const auto& cache = SortableNode::GetCache();
    REQUIRE(cache.size() == 3);
    // Forward cache is sorted by priority: [1, 2, 3]

    std::vector<int> visitedPriorities;
    SortableNode::Visit<VisitOrder::Backward>([&visitedPriorities](SortableNode& node) {
        visitedPriorities.push_back(node.m_Priority);
    });

    // Backward traversal of sorted cache [1, 2, 3] visits 3, then 2, then 1.
    REQUIRE(visitedPriorities.size() == 3);
    REQUIRE(visitedPriorities[0] == 3);
    REQUIRE(visitedPriorities[1] == 2);
    REQUIRE(visitedPriorities[2] == 1);
}

TEST_CASE("NodeLink: Reverse visit with SortCache (With Find / Cancellable predicate)", "[discovery][reverse][sorted][find]") {
    using namespace crg::test::sort;

    const SortableNode* foundNode = nullptr;
    // Search backwards in the sorted cache (which visits priorities 3, 2, 1)
    // Looking for the first priority <= 2 encountered from the back -> should find priority 2.
    SortableNode::Visit<VisitOrder::Backward>([&foundNode](SortableNode& node) {
        if (node.m_Priority <= 2) {
            foundNode = &node;
            return false; // Stop traversal (found)
        }
        return true; // Continue traversal
    });

    REQUIRE(foundNode != nullptr);
    REQUIRE(foundNode->m_Priority == 2);
}