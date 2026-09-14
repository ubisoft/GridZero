// Copyright (c) Ubisoft. All Rights Reserved.
// Licensed under the Apache 2.0 License. See LICENSE.md in the project root for license information.

// model_shell.hpp — type-erased fixed-size buffer wrapper for domain Models

#pragma once
#include "crg/core/config.hpp"
#include "crg/core/types.hpp"
#include "crg/type_erasure/model_shell_traits.hpp"
#include "crg/models/model_token.hpp"
#include "crg/routing/capability_router.hpp"
#include <type_traits>
#include <utility>
#include <new>

namespace crg::shell {

    template<typename TDomain>
    class alignas(::crg::shell::ModelShellTraits<TDomain>::MaxAlignment) ModelShell {
    private:
        template<class T>
        using DomainTraits    = ::crg::models::DomainTraits<T>;
        using ShellTraits     = ::crg::shell::ModelShellTraits<TDomain>;

        using Router     = ::crg::routing::CapabilityRouter<TDomain>;
        using ModelToken = ::crg::models::ModelToken<TDomain>;

        struct Concept {
            virtual ~Concept() = default;
            virtual u64 GetTypeKey() const = 0;
            virtual ModelToken GetToken() const = 0;
            virtual void Clone(void* buffer) const = 0;
            virtual void Relocate(void* buffer) noexcept = 0;
        };
        using ConceptPtr = Concept*;

        // Specialize ModelShellTraits<TDomain> to widen the fixed-size buffer capacity for a
        // domain. There is no heap fallback: a payload that exceeds StorageSize fails to
        // compile (see the static_asserts below), it never falls back to a heap allocation.
        static constexpr std::size_t FixedOverhead = sizeof(ConceptPtr);
        static constexpr std::size_t MaxSize        = ShellTraits::MaxSize;
        static_assert(MaxSize > FixedOverhead,
            "ModelShellTraits<TDomain>::MaxSize is too small to hold ModelShell's fixed "
            "overhead (Concept*) and still leave room for a model payload");
        // Guarantees zero padding between m_Buffer and m_Concept: sizeof(ConceptPtr) is
        // always a multiple of alignof(ConceptPtr) (a universal guarantee for any complete
        // type), so if MaxSize is too, StorageSize = MaxSize - sizeof(ConceptPtr) is a
        // multiple of alignof(ConceptPtr) as well, meaning m_Buffer's own end offset already
        // satisfies m_Concept's alignment with no compiler-inserted gap.
        static_assert(MaxSize % alignof(ConceptPtr) == 0,
            "ModelShellTraits<TDomain>::MaxSize must be a multiple of alignof(Concept*), "
            "otherwise hidden inter-member padding would push sizeof(ModelShell) past MaxSize");
        static constexpr std::size_t StorageSize   = MaxSize - FixedOverhead;
        static constexpr std::size_t MaxAlignment   = ShellTraits::MaxAlignment;

        // False-sharing guard: alignment must cover a full cache line. Alignment values
        // are powers of two, so this also forces sizeof(ModelShell) to be a multiple of
        // CRG_CACHE_LINE_SIZE — no domain specialization can pack two shells into one line.
        static_assert(MaxAlignment >= CRG_CACHE_LINE_SIZE,
            "ModelShellTraits<TDomain>::MaxAlignment must be >= CRG_CACHE_LINE_SIZE, "
            "otherwise a std::vector<ModelShell> loses false-sharing immunity");
        // MaxAlignment is a per-domain-customizable trait, not pinned to 64: the only
        // constraint above is a floor (>= CRG_CACHE_LINE_SIZE), so a domain is free to
        // specialize it to any value satisfying that floor. This checks the property
        // m_Concept's placement actually depends on directly, instead of leaving it as an
        // unstated consequence of CRG_CACHE_LINE_SIZE (currently 64) happening to exceed
        // alignof(ConceptPtr) (8 today).
        static_assert(MaxAlignment >= alignof(ConceptPtr),
            "ModelShellTraits<TDomain>::MaxAlignment must be >= alignof(Concept*)");

        template<typename TModel>
        struct ModelWrapper final : Concept {

            static_assert(sizeof(TModel) <= StorageSize,        "Model size exceeds ModelShell fixed-size buffer capacity");
            static_assert(alignof(TModel) <= MaxAlignment, "Model alignment exceeds ModelShell domain limit");

            TModel m_Instance;

            ModelWrapper(TModel&& model) : m_Instance(std::move(model)) {}
            ModelWrapper(const TModel& model) : m_Instance(model) {}

            u64 GetTypeKey() const override {
                return DomainTraits<TDomain>::template TypeKey<TModel>;
            }

            // FromType<TModel>() depends only on the type TModel, never on instance data —
            // every ModelWrapper<TModel> anywhere in the program has the same token. A
            // function-local static memoizes DenseIndex::Find's runtime lookup once per
            // TModel for the process lifetime, shared across every instance, instead of
            // recomputing it per call or duplicating it as per-instance storage.
            ModelToken GetToken() const override {
                static const ModelToken token = [] {
                    const ModelToken t = ModelToken::template FromType<TModel>();
                    CRG_ASSERT(t.IsValid(), "ModelShell: ModelToken::FromType<TModel> resolved to "
                        "an invalid token - is TModel registered via CRG_DECLARE_DOMAIN_MODELS "
                        "for this domain?");
                    return t;
                }();
                return token;
            }

            void Clone(void* buffer) const override {
                new (buffer) ModelWrapper<TModel>(m_Instance);
            }

            void Relocate(void* buffer) noexcept override {
                new (buffer) ModelWrapper<TModel>(std::move(*this));
            }
        };

    public:
        ModelShell() = default;

        template<typename TModel, typename = std::enable_if_t<!std::is_same_v<std::decay_t<TModel>, ModelShell>>>
        ModelShell(TModel&& model) {
            Set(std::forward<TModel>(model));
        }

        ~ModelShell() {
            if (m_Concept) m_Concept->~Concept();
        }

        ModelShell(const ModelShell& other) {
            if (other.m_Concept) {
                other.m_Concept->Clone(m_Buffer);
                m_Concept = reinterpret_cast<ConceptPtr>(m_Buffer);
            }
        }

        ModelShell(ModelShell&& other) noexcept {
            if (other.m_Concept) {
                other.m_Concept->Relocate(m_Buffer);
                m_Concept = reinterpret_cast<ConceptPtr>(m_Buffer);
                other.m_Concept = nullptr;
            }
        }

        ModelShell& operator=(const ModelShell& other) {
            if (this != &other) {
                this->~ModelShell();
                if (other.m_Concept) {
                    other.m_Concept->Clone(m_Buffer);
                    m_Concept = reinterpret_cast<ConceptPtr>(m_Buffer);
                } else {
                    m_Concept = nullptr;
                }
            }
            return *this;
        }

        ModelShell& operator=(ModelShell&& other) noexcept {
            if (this != &other) {
                this->~ModelShell();
                if (other.m_Concept) {
                    other.m_Concept->Relocate(m_Buffer);
                    m_Concept = reinterpret_cast<ConceptPtr>(m_Buffer);
                    other.m_Concept = nullptr;
                } else {
                    m_Concept = nullptr;
                }
            }
            return *this;
        }

        u64 GetTypeKey() const {
            return m_Concept ? m_Concept->GetTypeKey() : 0;
        }

        template<typename TModel>
        void Set(TModel&& value) {
            using Impl = ModelWrapper<std::decay_t<TModel>>;
            static_assert(sizeof(Impl) <= StorageSize, "Payload exceeds ModelShell fixed-size buffer capacity");

            if (m_Concept) m_Concept->~Concept();
            m_Concept = new (m_Buffer) Impl(std::forward<TModel>(value));
        }

        template<typename TModel>
        const TModel& Cast() const {
            CRG_ASSERT(m_Concept, "ModelShell: Attempting to access an empty shell");
            CRG_ASSERT(GetTypeKey() == (DomainTraits<TDomain>::template TypeKey<TModel>), "ModelShell: Type mismatch during Cast()");
            return static_cast<const ModelWrapper<TModel>&>(*m_Concept).m_Instance;
        }

        template<typename TModel>
        const TModel* TryCast() const {
            if (!m_Concept || GetTypeKey() != (DomainTraits<TDomain>::template TypeKey<TModel>)) {
                return nullptr;
            }
            return &(static_cast<const ModelWrapper<TModel>&>(*m_Concept).m_Instance);
        }

        template<typename TModel, typename TFunc, typename = std::enable_if_t<::crg::shell::ModelShellMutabilityTraits<TDomain>::IsMutable>>
        void Mutate(TFunc&& func) {
            func(const_cast<TModel&>(this->template Cast<TModel>()));
        }

        template<typename TModel, typename TFunc, typename = std::enable_if_t<::crg::shell::ModelShellMutabilityTraits<TDomain>::IsMutable>>
        void TryMutate(TFunc&& func) {
            if (TModel* p = const_cast<TModel*>(this->template TryCast<TModel>())) {
                func(*p);
            }
        }

        template<auto FuncPtr, class... TArgs>
        auto Invoke(TArgs&&... args) const {
            using MTraits = ModelShellMethodTraits<TDomain, decltype(FuncPtr)>;
            using I = typename MTraits::Interface;

            CRG_ASSERT(m_Concept, "Invoke called on empty shell");

            const ModelToken token = m_Concept->GetToken();
            const auto capHandle = Router::template Find<I>(token, args...);
            CRG_ASSERT(capHandle, "Required interface not routed for this model");

            return ((*capHandle).*FuncPtr)(*this, std::forward<TArgs>(args)...);
        }

        template<auto FuncPtr, class... TArgs>
        auto TryInvoke(TArgs&&... args) const -> TryInvokeResult_t<typename ModelShellMethodTraits<TDomain, decltype(FuncPtr)>::ReturnType> {
            using MTraits = ModelShellMethodTraits<TDomain, decltype(FuncPtr)>;
            using I = typename MTraits::Interface;
            using R = typename MTraits::ReturnType;

            if (!m_Concept) {
                if constexpr (std::is_void_v<R>) return; else return {};
            }

            const ModelToken token = m_Concept->GetToken();
            const auto capHandle = Router::template Find<I>(token, args...);

            if constexpr (std::is_void_v<R>) {
                if (capHandle) {
                    ((*capHandle).*FuncPtr)(*this, std::forward<TArgs>(args)...);
                }
            } else {
                if (capHandle) {
                    return ((*capHandle).*FuncPtr)(*this, std::forward<TArgs>(args)...);
                }
                return {};
            }
        }

    private:
        alignas(MaxAlignment) std::byte m_Buffer[StorageSize];
        ConceptPtr m_Concept{ nullptr };
    };

} // namespace crg::shell
