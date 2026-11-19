#pragma once
#include "sdk/KenshiSDK.h"

namespace Features {

    // Resolved + hooked against the real game (stage 1):
    namespace BaseKnockout    { bool Init(); }  // MedicalSystem::canGetUpWakeUp, Character::isItSafeToGetUp
    namespace TurretDiscipline{ bool Init(); bool IsActiveGunner(const void* character); } // TurretBuilding::operate (+ optional addGoal)
    namespace BulkHauling     { bool Init(); }  // diagnostic tracer on ConstructionState::needsMat / getNeededMats

    // Not ported yet - signatures unresolved (see SignaturesPending.h); these still use the
    // legacy placeholder model and are skipped at runtime because their pattern is nullptr.
    namespace AntiGateClipping {
        bool Init();
        void FilterSeparationVector(KenshiSDK::Character*, KenshiSDK::Character*, KenshiSDK::Vector3*, float);
    }
    namespace TacticalEscort {
        bool Init();
        void SynchronizeEscort(KenshiSDK::AIState*, float);
        bool ShouldDropGoal(const void* guard, int taskType, const void* subject);   // used by Character::addGoal hook
    }
    namespace SmartLootSmelt {
        bool Init();
        void ProcessCorpseBeforeFurnace(KenshiSDK::Character*, KenshiSDK::Character*, KenshiSDK::Building*);
    }
}
