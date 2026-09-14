// Copyright (c) Ubisoft. All Rights Reserved.
// Licensed under the Apache 2.0 License. See LICENSE.md in the project root for license information.

// Copyright (c) 2026 Cyril Tissier. All rights reserved.
// Licensed under the Apache License, Version 2.0.
// =============================================================================
// CRG UNIT TESTS (CATCH2) - ROUTER ROBUSTNESS
// =============================================================================

#include "catch.hpp"
#include "crg/crg.hpp"
#include "crg/models/domain_key.hpp"
#include "crg/models/model_key.hpp"
#include <cstdint>

using namespace crg;
using namespace crg::models;
using namespace crg::routing;
using namespace crg::capabilities;

// 1. Defined in a named namespace to guarantee stable FNV-1a TypeHashes
namespace crg::test::robustness {
    struct RoboticsPartition {};
    struct DroneModel { std::uint8_t m_Data[16]; };

    struct ILocomotion { struct Params { float m_Speed{ 0.0f }; }; };
    struct ISensor     { struct Params { std::uint32_t m_SignalStrength{ 0 }; }; };
    enum class State { Active, Standby };
}

// 2. Global registration for traits visibility
CRG_DECLARE_DOMAIN(crg::test::robustness::RoboticsPartition)
CRG_DECLARE_DOMAIN_MODELS(crg::test::robustness::RoboticsPartition,
    crg::test::robustness::DroneModel)

namespace crg {
    template<> struct EnumTraits<crg::test::robustness::State> { static constexpr std::size_t Count = 2; };
}

namespace crg::routing {
    template<> struct CapabilityRoutingTraits<crg::test::robustness::RoboticsPartition, crg::test::robustness::ILocomotion> { using SpaceType = CapabilitySpace<crg::test::robustness::State>; };
    template<> struct CapabilityRoutingTraits<crg::test::robustness::RoboticsPartition, crg::test::robustness::ISensor>      { using SpaceType = CapabilitySpace<crg::test::robustness::State>; };
}

// 3. Bindings
namespace crg::test::robustness {
    template<typename TModel, typename TAt>
    struct DroneMove : public Capability<ILocomotion> {
        static void Execute(ILocomotion::Params& p) { p.m_Speed = 50.0f; }
    };

    template<typename TModel, typename TAt>
    struct DroneSense : public Capability<ISensor> {
        static void Execute(ISensor::Params& p) { p.m_SignalStrength = 100; }
    };

    CapabilityBinding<RoboticsPartition, DroneModel, DroneMove>  s_MoveBinding;
    CapabilityBinding<RoboticsPartition, DroneModel, DroneSense> s_SenseBinding;
}

// 4. Test Suite
// =============================================================================
// DEBUG SUITE: TRACKING THE ROUTER NULLPTR
// =============================================================================

TEST_CASE("Debug: Robustness Nullptr Tracker", "[debug]") {
    using namespace crg::test::robustness;
    using namespace crg::routing;
    using namespace crg::capabilities;

    SECTION("PROBE 1: Offset Math & Dimension Validation") {
        using Space = CapabilityRoutingTraits<RoboticsPartition, ILocomotion>::SpaceType;
        
        // Verify the dimensions are correctly extracted
        STATIC_REQUIRE(Space::Dimensions == 1);
        STATIC_REQUIRE(Space::Volume == 2);

        // Verify Horner() does give 0 for Active (modelIndex=0 isolates pure geometry)
        auto offset = Space::ComputeOffset(0, State::Active);
        REQUIRE(offset == 0);
    }

    SECTION("PROBE 2: Strong Type Generation (The Tuple Ghost)") {
        using Space = CapabilityRoutingTraits<RoboticsPartition, ILocomotion>::SpaceType;
        
        // What the framework generates at index 0
        using GeneratedType = typename Space::template AtType<0>;

        // What the router expects to find to match the signature
        using ExpectedType = ::crg::At<State::Active>;

        // IF THIS FAILS: capability_space.hpp is still generating a std::tuple
        // or a corrupted type instead of the expected crg::At.
        STATIC_REQUIRE(std::is_same_v<GeneratedType, ExpectedType>);
    }

    SECTION("PROBE 3: SFINAE Contract Matching") {
        // Simulate the framework instantiating the implementation
        using ExpectedType = ::crg::At<State::Active>;
        using Impl = DroneMove<DroneModel, ExpectedType>;

        // IF THIS FAILS: capability_binding.hpp refuses to write because it doesn't
        // recognize DroneMove as inheriting from Capability<ILocomotion>.
        STATIC_REQUIRE(std::is_base_of_v<Capability<ILocomotion>, Impl>);
        STATIC_REQUIRE(crg::capabilities::IsStaticContract<ILocomotion>);
        STATIC_REQUIRE(crg::capabilities::HasStaticExecute<Impl, ILocomotion::Params>);
    }

    SECTION("PROBE 4: Tensor Arena Memory State") {
        auto token = crg::models::ModelToken<RoboticsPartition>::FromType<DroneModel>();

        // Triggers the arena's lazy build
        CapabilityRouter<RoboticsPartition>::Find<ILocomotion>(token, State::Active);

        // IF THIS FAILS: the arena wasn't even allocated. The Populator didn't run.
        REQUIRE(TensorArena<RoboticsPartition, ILocomotion>::GetSize() > 0);

        const auto& cell = TensorArena<RoboticsPartition, ILocomotion>::GetData()[0]; // DenseID(0) * Vol(2) + Offset(0) = 0

        // IF THIS FAILS: the arena exists, but the cell stayed empty. SFINAE or the build loop skipped it.
        REQUIRE(cell.m_Fallback.m_Target != nullptr);
    }
}

// =============================================================================
// DEBUG SUITE 2: ROUTER MEMORY STATE (FIXED)
// =============================================================================

TEST_CASE("Debug: Router Execution & Memory Alignment", "[debug]") {
    using namespace crg::test::robustness;
    using namespace crg::routing;
    using namespace crg::capabilities;
    using namespace crg::models;

    SECTION("PROBE 5: Routing Data State Validation") {
        auto token = ModelToken<RoboticsPartition>::FromType<DroneModel>();
        REQUIRE(token.IsValid() == true);

        // Manually triggers the build for DroneModel
        CapabilityRouter<RoboticsPartition>::Find<ILocomotion>(token, State::Active);

        // IF THIS FAILS: the Router didn't call the ArenaBuilder!
        REQUIRE(TensorArena<RoboticsPartition, ILocomotion>::GetSize() > 0);

        using Space = CapabilityRoutingTraits<RoboticsPartition, ILocomotion>::SpaceType;
        std::size_t index = Space::ComputeOffset(token.GetDenseIndex(), State::Active);

        const auto& cell = TensorArena<RoboticsPartition, ILocomotion>::GetData()[index];

        // IF THIS FAILS: the Token's ID doesn't match the row written by the Populator
        REQUIRE(cell.m_Fallback.m_Target != nullptr);
    }

    SECTION("PROBE 6: Router Dispatch Check") {
        auto token = ModelToken<RoboticsPartition>::FromType<DroneModel>();

        // Memory is confirmed by Probe 5, let's look at what the router returns
        auto gate = CapabilityRouter<RoboticsPartition>::Find<ILocomotion>(token, State::Active);

        // IF THIS FAILS WHILE PROBE 5 PASSES:
        // the problem is ONLY in CapabilityRouter::Find's return.
        // Find returns a CapabilityHandle by value; explicit operator bool()
        // is true as long as m_Target isn't nullptr.
        REQUIRE(gate);
    }
}

TEST_CASE("Router: Multi-Capability Stacking", "[routing][stacking]") {
    using namespace crg::test::robustness;

    auto token = ModelToken<RoboticsPartition>::FromType<DroneModel>();

    SECTION("Independent Tensor Resolution") {
        auto moveCap  = CapabilityRouter<RoboticsPartition>::Find<ILocomotion>(token, State::Active);
        auto senseCap = CapabilityRouter<RoboticsPartition>::Find<ISensor>(token, State::Active);

        REQUIRE(moveCap);
        REQUIRE(senseCap);

        ILocomotion::Params moveParams;
        moveCap(moveParams);
        REQUIRE(moveParams.m_Speed == 50.0f);

        ISensor::Params senseParams;
        senseCap(senseParams);
        REQUIRE(senseParams.m_SignalStrength == 100);
    }
}
