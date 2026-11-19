#pragma once
// ============================================================================
// KenshiSDK.h - offsets for Kenshi 1.0.65 x64.
// Source: KenshiLib headers ("// 0x.. Member"). Status per entry:
//   [DISASM]  confirmed by disassembling the Steam 1.0.65 exe (see docs/BUILD_VERIFICATION.md)
//   [KLIB]    taken from KenshiLib only, not independently confirmed yet
// Only primitive types / raw pointers are used: no game STL (std::string, vector,
// lektor, ogre containers) is ever touched, so there is no CRT/ABI coupling.
// ============================================================================
#include <cstdint>
#include "sdk/SdkModel.h"   // legacy placeholder model, used only by not-yet-ported features

namespace KenshiSDK::Verified {

    template <typename T>
    inline T ReadAt(const void* obj, uintptr_t off) {
        return *reinterpret_cast<const T*>(reinterpret_cast<const uint8_t*>(obj) + off);
    }

    // class RootObjectBase (base of Character, Building, TownBase ...)
    namespace RootObjectBase {
        constexpr uintptr_t Owner_FactionPtr      = 0x10;   // [KLIB] Faction*
        constexpr uintptr_t VT_GetFaction         = 0x58;   // [DISASM] vtable byte offset of getFaction()
    }

    // class Faction
    namespace Faction {
        constexpr uintptr_t Relations_Ptr         = 0x78;   // [KLIB] FactionRelations*
        constexpr uintptr_t IsPlayer_Ptr          = 0x250;  // [DISASM] != nullptr  <=> Faction::isThePlayer()
    }

    // class FactionRelations: isEnemy(a,b) == (a!=b && getFactionRelation(a,b) <= -30.0f)  [DISASM]
    namespace FactionRelations {
        constexpr float EnemyThreshold            = -30.0f; // [DISASM] constant at 0x16CBCFC
    }

    // class Character
    namespace Character {
        constexpr uintptr_t IsChained             = 0x320;  // [KLIB] bool
        constexpr uintptr_t Inventory_Ptr         = 0x2E8;  // [DISASM] Inventory*
        constexpr uintptr_t IsCarryingSomething   = 0x348;  // [DISASM] bool
        constexpr uintptr_t IsBeingCarried        = 0x3D4;  // [KLIB] bool
        constexpr uintptr_t Medical_Embedded      = 0x458;  // [DISASM] MedicalSystem (embedded, getMedical()==this+0x458)
        constexpr uintptr_t PlayerWantsMeToGetUp  = 0x0EC;  // [KLIB] bool
        constexpr uintptr_t IsUsingTurret_Hand    = 0x258;  // [KLIB] hand
    }

    // class MedicalSystem
    namespace MedicalSystem {
        constexpr uintptr_t OwnerFromThis         = Character::Medical_Embedded; // Character* = (uint8_t*)this - 0x458
        constexpr uintptr_t FlagA                 = 0x161;  // [DISASM] bool read by canGetUpWakeUp
        constexpr uintptr_t FlagB                 = 0x164;  // [DISASM] bool read by canGetUpWakeUp
    }

    // Building::ConstructionState (needsMat returns (int)(buildMatsTotal - amountOfMaterials))  [DISASM]
    namespace ConstructionState {
        constexpr uintptr_t Mats_Count            = 0x18;   // lektor<BuildMaterial*>.count
        constexpr uintptr_t Mats_Data             = 0x20;   // lektor<BuildMaterial*>.data
        constexpr uintptr_t Mat_BuildMatsTotal    = 0x08;   // BuildMaterial::buildMatsTotal (float)
        constexpr uintptr_t Mat_Amount            = 0x0C;   // BuildMaterial::amountOfMaterials (float)
    }

    // class Item / InventoryItemBase  [KLIB]
    namespace Item {
        constexpr uintptr_t Quality               = 0x11C;  // float
        constexpr uintptr_t Weight                = 0x120;  // float
        constexpr uintptr_t Quantity              = 0x12C;  // int
        constexpr uintptr_t ObjectType            = 0x13C;  // itemType
    }

    // class TownBase / Town  [KLIB]
    namespace Town {
        constexpr uintptr_t PlayerTownLevel       = 0x388;  // int (Town only)
        constexpr uintptr_t PlayerHasBuildings    = 0x3D0;  // bool (Town only)
    }

    // enum TaskType (KenshiLib Enums.h, no explicit values -> ordinal order, 291 entries)
    namespace TaskType {
        constexpr int MELEE_ATTACK                 = 4;
        constexpr int FOCUSED_MELEE_ATTACK         = 5;
        constexpr int CHOOSE_ENEMY_AND_ATTACK      = 9;
        constexpr int ATTACK_ENEMIES               = 16;
        constexpr int ATTACK_TOWN                  = 23;
        constexpr int BODYGUARD                    = 45;
        constexpr int CHASE                        = 46;
        constexpr int GET_UP_STAY_DOWN_THOUGH      = 52;
        constexpr int UNPROVOKED_FOCUSED_MELEE     = 61;
        constexpr int GET_UP_STAND_UP              = 65;
        constexpr int FORCE_GET_UP                 = 66;
        constexpr int ADD_MATERIALS_TO_BUILDING    = 71;
        constexpr int DELIVER_RESOURCES            = 88;
    }
}
