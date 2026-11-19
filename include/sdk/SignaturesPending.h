#pragma once
// Signatures that are NOT resolved yet. Pattern = nullptr -> hook is skipped with a log line.
// Replace an entry by adding its function to TARGETS in tools/gen_signatures.py once its
// KenshiLib header RVA / real function has been identified (see docs/BUILD_VERIFICATION.md).
#include "sdk/Signatures.h"

namespace KenshiSDK::Signatures {

    // 2. Anti-Gate Clipping: no named separation/push function in KenshiLib (PhysicsActual/HavokCharacter).
    constexpr SignatureEntry SIG_APPLY_SEPARATION_FORCE   = { "Separation force (unmapped)", nullptr, 0, 0 };
    // 3. Tactical Escort: TaskType::BODYGUARD update lives in AI/Tasker; not named in KenshiLib.
    constexpr SignatureEntry SIG_UPDATE_BODYGUARD_TASK    = { "Bodyguard update (unmapped)", nullptr, 0, 0 };
    // 5. Smart Loot: FEED_CORPSE_INTO_MACHINE / FurnaceBuilding deposit path; not named in KenshiLib.
    constexpr SignatureEntry SIG_DEPOSIT_CORPSE_FURNACE   = { "Furnace deposit (unmapped)", nullptr, 0, 0 };

}

