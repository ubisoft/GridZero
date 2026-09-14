// Copyright (c) Ubisoft. All Rights Reserved.
// Licensed under the Apache 2.0 License. See LICENSE.md in the project root for license information.

// model_key.hpp — macro suite to register models, domains, and format tags

#pragma once
#include "crg/core/hash.hpp"
#include "crg/core/macro_utils.hpp"
#include "crg/models/domain_key.hpp"
#include "crg/models/model_domain.hpp"
#include "crg/models/model_node.hpp"

namespace crg::models {

    template<class TModel>
    struct ModelKey;

}

// Value hashes the model's full qualified #ModelType (namespaces included, up
// to a leading "::" — see RemoveLeadingGlobalNamespace in core/hash.hpp), so
// two distinct types never collide on a shared leaf name (studio::Scout vs
// mod::Scout hash differently). Name carries the same full text for display
// (CRG_HASH_NAME_ENABLED_ONLY builds only).
#define CRG_DECLARE_MODEL(ModelType)                                                 \
    namespace crg::hash {                                                            \
        template<>                                                                   \
        struct TypeHashBase<ModelType> {                                             \
            static constexpr ::crg::u64 Value =                                     \
                ::crg::hash::internal::HashString(                                   \
                    ::crg::hash::internal::RemoveLeadingGlobalNamespace(#ModelType)); \
            CRG_HASH_NAME_ENABLED_ONLY(                                              \
                static constexpr std::string_view Name = #ModelType;)                \
        };                                                                           \
    }                                                                                \
    namespace crg::models {                                                          \
        template<>                                                                   \
        struct ModelKey<ModelType> : public ::crg::hash::TypeHash<ModelType> {};    \
    }

#define CRG_DDM_NODE_CAT2(a, b) a##b
#define CRG_DDM_NODE_CAT(a, b)  CRG_DDM_NODE_CAT2(a, b)
#define CRG_DDM_NODE_NAME       CRG_DDM_NODE_CAT(s_CrgModelNode_, __COUNTER__)

#if CRG_PLUGINS_ENABLED
#define CRG_DDM_NODE_STORAGE inline
#else
#define CRG_DDM_NODE_STORAGE static
#endif

#define CRG_DDM_BIND(D, ModelType)                                               \
    CRG_DECLARE_MODEL(ModelType)                                                 \
    namespace crg::models {                                                      \
        template<> struct ModelDomain<ModelType> { using Type = D; };            \
        namespace internal {                                                     \
            CRG_DDM_NODE_STORAGE const ModelNode<                                \
                D, ::crg::hash::TypeHash<ModelType>::Value>                      \
                CRG_DDM_NODE_NAME{                                               \
                    CRG_HASH_NAME_ENABLED_ONLY(                                  \
                        &::crg::hash::TypeHashBase<ModelType>::Name)             \
                };                                                               \
        }                                                                        \
    }

#define CRG_DDM_1(D, x)         CRG_DDM_BIND(D, x)
#define CRG_DDM_2(D, x, ...)    CRG_DDM_BIND(D, x) CRG_EXPAND(CRG_DDM_1(D, __VA_ARGS__))
#define CRG_DDM_3(D, x, ...)    CRG_DDM_BIND(D, x) CRG_EXPAND(CRG_DDM_2(D, __VA_ARGS__))
#define CRG_DDM_4(D, x, ...)    CRG_DDM_BIND(D, x) CRG_EXPAND(CRG_DDM_3(D, __VA_ARGS__))
#define CRG_DDM_5(D, x, ...)    CRG_DDM_BIND(D, x) CRG_EXPAND(CRG_DDM_4(D, __VA_ARGS__))
#define CRG_DDM_6(D, x, ...)    CRG_DDM_BIND(D, x) CRG_EXPAND(CRG_DDM_5(D, __VA_ARGS__))
#define CRG_DDM_7(D, x, ...)    CRG_DDM_BIND(D, x) CRG_EXPAND(CRG_DDM_6(D, __VA_ARGS__))
#define CRG_DDM_8(D, x, ...)    CRG_DDM_BIND(D, x) CRG_EXPAND(CRG_DDM_7(D, __VA_ARGS__))
#define CRG_DDM_9(D, x, ...)    CRG_DDM_BIND(D, x) CRG_EXPAND(CRG_DDM_8(D, __VA_ARGS__))
#define CRG_DDM_10(D, x, ...)   CRG_DDM_BIND(D, x) CRG_EXPAND(CRG_DDM_9(D, __VA_ARGS__))
#define CRG_DDM_11(D, x, ...)   CRG_DDM_BIND(D, x) CRG_EXPAND(CRG_DDM_10(D, __VA_ARGS__))
#define CRG_DDM_12(D, x, ...)   CRG_DDM_BIND(D, x) CRG_EXPAND(CRG_DDM_11(D, __VA_ARGS__))
#define CRG_DDM_13(D, x, ...)   CRG_DDM_BIND(D, x) CRG_EXPAND(CRG_DDM_12(D, __VA_ARGS__))
#define CRG_DDM_14(D, x, ...)   CRG_DDM_BIND(D, x) CRG_EXPAND(CRG_DDM_13(D, __VA_ARGS__))
#define CRG_DDM_15(D, x, ...)   CRG_DDM_BIND(D, x) CRG_EXPAND(CRG_DDM_14(D, __VA_ARGS__))
#define CRG_DDM_16(D, x, ...)   CRG_DDM_BIND(D, x) CRG_EXPAND(CRG_DDM_15(D, __VA_ARGS__))

#define CRG_DDM_GET_NTH(_1,_2,_3,_4,_5,_6,_7,_8,_9,_10,_11,_12,_13,_14,_15,_16,N,...) N

#define CRG_DECLARE_DOMAIN_MODELS(TDomain, ...)                                  \
    static_assert(sizeof(::crg::models::DomainKey<TDomain>) > 0,                 \
        "\n[CRG ERROR] CRG_DECLARE_DOMAIN_MODELS(" #TDomain ", ...): "           \
        "domain not declared. Call CRG_DECLARE_CAPABILITY_DOMAIN(" #TDomain ")"  \
        " or CRG_DECLARE_PIPELINE_DOMAIN(" #TDomain ") first.\n");               \
    CRG_EXPAND(CRG_DDM_GET_NTH(__VA_ARGS__,                                      \
        CRG_DDM_16, CRG_DDM_15, CRG_DDM_14, CRG_DDM_13,                          \
        CRG_DDM_12, CRG_DDM_11, CRG_DDM_10, CRG_DDM_9,                           \
        CRG_DDM_8,  CRG_DDM_7,  CRG_DDM_6,  CRG_DDM_5,                           \
        CRG_DDM_4,  CRG_DDM_3,  CRG_DDM_2,  CRG_DDM_1)(TDomain, __VA_ARGS__))

#define CRG_DECLARE_FORMAT(FmtName, ExtStr)                                          \
    namespace crg::formats { struct FmtName {}; }                                   \
    namespace crg::hash {                                                            \
        template<>                                                                   \
        struct TypeHash<crg::formats::FmtName> {                                     \
            static constexpr ::crg::u64 Value =                                      \
                ::crg::hash::internal::HashStringLower(ExtStr);                      \
            CRG_HASH_NAME_ENABLED_ONLY(                                              \
                static constexpr std::string_view Name = #FmtName;)                  \
        };                                                                           \
    }                                                                                \
    namespace crg::models {                                                          \
        template<>                                                                   \
        struct ModelKey<crg::formats::FmtName>                                       \
            : public ::crg::hash::TypeHash<crg::formats::FmtName> {};               \
    }
