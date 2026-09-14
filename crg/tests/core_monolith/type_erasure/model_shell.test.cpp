// Copyright (c) Ubisoft. All Rights Reserved.
// Licensed under the Apache 2.0 License. See LICENSE.md in the project root for license information.

// =============================================================================
// CRG UNIT TEST: MODEL SHELL AND INVOCATION
// =============================================================================

#include "catch.hpp"
#include "crg/crg.hpp"

using namespace crg;
using namespace crg::models;
using namespace crg::routing;
using namespace crg::capabilities;

// =============================================================================
// 1. MOCKS & FIXTURES
// =============================================================================

namespace crg::shell::test {
    struct SimulationPartition {};
    
    // Alias ModelShell parameterized by its domain
    using SimulationModelShell = ModelShell<SimulationPartition>;

    struct UnitModel { 
        int m_Health = 100; 
        int m_Armor = 50; 
    };

    struct GhostModel { 
        // Ghost model to test TryInvoke failures
    };

    // Contract Interface (Methods take the shell as the first argument)
    struct IHealthController {
        virtual void ApplyDamage(const SimulationModelShell& shell, int& damageApplied) const = 0;
        virtual int GetPriority(const SimulationModelShell& shell) const = 0;
        virtual ~IHealthController() = default;
    };
}

// =============================================================================
// 2. STRICT ENFORCEMENT REGISTRATION
// =============================================================================

CRG_DECLARE_DOMAIN(crg::shell::test::SimulationPartition)
// IHealthController also gets a hash so DomainTraits::TypeKey<IHealthController> resolves.
CRG_DECLARE_DOMAIN_MODELS(crg::shell::test::SimulationPartition,
    crg::shell::test::UnitModel,
    crg::shell::test::GhostModel,
    crg::shell::test::IHealthController)

namespace crg::shell {
    template<> struct ModelShellMutabilityTraits<crg::shell::test::SimulationPartition> {
        static constexpr bool IsMutable = true;
    };
}

// =============================================================================
// 3. CAPABILITY PLUGINS & BINDINGS
// =============================================================================

namespace crg::shell::test {

    /**
     * @brief Templated plugin acting on the fixed-size buffer copy.
     * Inherits cleanly from Capability, which now properly exposes the interface.
     */
    template <typename TModel>
    struct ArmorPiercingLogic : public Capability<IHealthController> {
        
        void ApplyDamage(const SimulationModelShell& shell, int& damageApplied) const override {
            // Retrieve data using the restored Cast<T>() method
            const auto& modelData = shell.Cast<TModel>();
            
            if (modelData.m_Armor > 0) {
                damageApplied = 10; // Mitigated
            } else {
                damageApplied = 50; // Full damage
            }
        }

        int GetPriority(const SimulationModelShell& shell) const override {
            return 42;
        }
    };

    // Binding the templated plugin to the specific model
    CapabilityBinding<SimulationPartition, UnitModel, ArmorPiercingLogic> s_UnitLogicBinding;
}

// =============================================================================
// 4. TEST SUITES
// =============================================================================


TEST_CASE("ModelShell: Fixed-Size Buffer Wrapper and Execution via Router", "[models][shell]") {
    using namespace crg::shell::test;

    SECTION("Invoke() casts and binds the member function call to the plugin") {
        UnitModel myUnit{100, 50};
        SimulationModelShell shell(myUnit);

        int damage = 0;
        
        // Resolves the interface and calls ApplyDamage
        shell.Invoke<&IHealthController::ApplyDamage>(damage);

        REQUIRE(damage == 10);
    }

    SECTION("TryInvoke() returns void and does nothing if R=void and not found") {
        GhostModel ghost;
        SimulationModelShell shell(ghost);

        int damage = 0;
        shell.TryInvoke<&IHealthController::ApplyDamage>(damage);

        REQUIRE(damage == 0); // Reference was not touched
    }

    SECTION("TryInvoke() returns an empty optional-like wrapper if R!=void and not found") {
        GhostModel ghost;
        SimulationModelShell shell(ghost);

        auto result = shell.TryInvoke<&IHealthController::GetPriority>();

        // The wrapper must evaluate to false because the interface was not found
        REQUIRE(static_cast<bool>(result) == false);
    }

    SECTION("Mutate() mutates the stored model in place, no copy round-trip") {
        UnitModel myUnit{100, 50};
        SimulationModelShell shell(myUnit);

        shell.Mutate<UnitModel>([](UnitModel& unit) { unit.m_Armor = 0; });

        REQUIRE(shell.Cast<UnitModel>().m_Armor == 0);
        REQUIRE(shell.Cast<UnitModel>().m_Health == 100);
    }

    SECTION("TryMutate() calls func when the type matches") {
        UnitModel myUnit{100, 50};
        SimulationModelShell shell(myUnit);

        bool called = false;
        shell.TryMutate<UnitModel>([&called](UnitModel& unit) { called = true; unit.m_Health = 1; });

        REQUIRE(called);
        REQUIRE(shell.Cast<UnitModel>().m_Health == 1);
    }

    SECTION("TryMutate() does nothing on a type mismatch") {
        GhostModel ghost;
        SimulationModelShell shell(ghost);

        bool called = false;
        shell.TryMutate<UnitModel>([&called](UnitModel&) { called = true; });

        REQUIRE_FALSE(called);
    }
}
