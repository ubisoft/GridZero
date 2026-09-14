// Copyright (c) Ubisoft. All Rights Reserved.
// Licensed under the Apache 2.0 License. See LICENSE.md in the project root for license information.

// node_link.hpp — intrusive linked-list auto-registration for typed nodes

#pragma once
#include <vector>
#include <type_traits>
#include "crg/core/config.hpp"

#if CRG_PLUGINS_ENABLED
    namespace crg::discovery::internal {
        template <typename TNode>
        struct PluginLocalStorage {
            static constexpr bool HasLocalHead = false;
        };
    }

    #define CRG_MARK_INTERFACE_BOOTSTRAP(NodeType) \
        namespace crg::discovery::internal { \
            template <> \
            struct PluginLocalStorage<NodeType> { \
                static constexpr bool HasLocalHead = true; \
                inline static NodeType* s_LocalHead{nullptr}; \
            }; \
        }
#endif

namespace crg::discovery {

    template<typename T> struct UniversalAnchor;
    struct AnchorLink;
    struct ISyncNode;

    template<class TNode>
    struct NodeLinkTraits {
        static void SortCache(std::vector<const TNode*>&) {}
    };

    template<class TNode>
    using NodeLinkAnchor = UniversalAnchor<TNode*>;

    template<class TNode>
    using CacheAnchor = UniversalAnchor<std::vector<const TNode*>>;

    template<class TNode, class TContract>
    class NodeLink : public TContract {
    public:
        TNode* m_Next{nullptr};

// Some host environments redefine `static_cast` as a macro that performs a
// dynamic_cast-based RTTI check. During base-class construction the
// most-derived vtable is not yet installed, so such a check would spuriously
// fail on this well-defined downcast. Suspend the macro locally; push_macro
// is a no-op when the macro is not defined.
#pragma push_macro("static_cast")
#undef static_cast
        NodeLink() {
            static_assert(std::is_base_of_v<NodeLink, TNode>,
                "CRTP: TNode must inherit from NodeLink<TNode, TContract>");
            TNode* derivedThis = static_cast<TNode*>(this);

#if CRG_PLUGINS_ENABLED
            if constexpr (internal::PluginLocalStorage<TNode>::HasLocalHead) {
                this->m_Next = internal::PluginLocalStorage<TNode>::s_LocalHead;
                internal::PluginLocalStorage<TNode>::s_LocalHead = derivedThis;
            } else
#endif
            {
                this->m_Next = NodeLinkAnchor<TNode>::Get();
                NodeLinkAnchor<TNode>::Get() = derivedThis;
            }
        }
#pragma pop_macro("static_cast")

    private:
        struct Linearizer {
            Linearizer() {
                RefreshCache();
            }

            void RefreshCache() {
                auto& cache = CacheAnchor<TNode>::Get();
                cache.clear();

                const TNode* current = NodeLinkAnchor<TNode>::Get();
                while (current) {
                    cache.push_back(current);
                    current = current->m_Next;
                }

                NodeLinkTraits<TNode>::SortCache(cache);
            }
        };

        static Linearizer& GetLinearizer() {
            static Linearizer s_Linearizer;
            return s_Linearizer;
        }

    public:
        static TNode& GetAnchor() noexcept {
            return *NodeLinkAnchor<TNode>::Get();
        }

        static bool HasAnchor() noexcept {
            return NodeLinkAnchor<TNode>::Get() != nullptr;
        }

        static void RefreshCache() {
            GetLinearizer().RefreshCache();
        }

        static const std::vector<const TNode*>& GetCache() {
            [[maybe_unused]] Linearizer& triggerLinearizer = GetLinearizer();
            return CacheAnchor<TNode>::Get();
        }

        template<typename CallableT>
        static void Visit(CallableT&& func) {
            using ReturnT = std::invoke_result_t<CallableT, TNode&>;
            constexpr bool is_cancellable = std::is_same_v<ReturnT, bool>;

            for (const TNode* nodePtr : GetCache()) {
                if constexpr (is_cancellable) {
                    if (!func(*const_cast<TNode*>(nodePtr))) break;
                } else {
                    func(*const_cast<TNode*>(nodePtr));
                }
            }
        }
    };

}

// Pulled in at the END so that NodeLink and CRG_MARK_INTERFACE_BOOTSTRAP are
// already defined when universal_anchor_plugins.inl re-enters via #include cycle.
// In monolithic mode, this simply completes UniversalAnchor for all subsequent
// instantiations of NodeLink::RefreshCache / GetCache (GCC requires complete
// type at the point a static member function is named).
#include "crg/discovery/universal_anchor.hpp"
