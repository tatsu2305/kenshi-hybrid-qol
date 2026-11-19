// ============================================================================
// WARNING: PLACEHOLDER MODEL - NOT A REAL MEMORY LAYOUT.
// Field ORDER/OFFSETS here are invented and do NOT match Kenshi. Offsets were
// removed on purpose. Never dereference these types on live game memory until a
// field is replaced by a verified accessor from KenshiSDK.h (Verified::*) or by
// KenshiLib headers (kenshi/Character.h etc.). Features using it must stay off.
// ============================================================================
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace KenshiSDK {

    // ============================================================================
    // Core Math & Ogre Types
    // ============================================================================
    struct Vector3 {
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;

        float distanceSquared(const Vector3& other) const {
            float dx = x - other.x;
            float dy = y - other.y;
            float dz = z - other.z;
            return dx * dx + dy * dy + dz * dz;
        }
    };

    // ============================================================================
    // Kenshi Enumerations (v1.0.65 x64)
    // ============================================================================
    enum class TaskType : int32_t {
        NULL_TASK = 0,
        MOVE_ON_FREE_WILL = 1,
        MELEE_ATTACK = 2,
        RANGED_ATTACK = 3,
        CHASE = 4,
        FLEE = 5,
        BODYGUARD = 12,
        FOLLOW_PLAYER_ORDER = 14,
        REPAIR_BUILDING = 23,
        OPERATE_MACHINERY = 26,
        OPERATE_TURRET = 27,
        HAUL_TO_STORAGE = 31,
        FILL_MACHINE = 32,
        DELIVER_CONSTRUCTION_RESOURCES = 33,
        CORPSE_DISPOSAL = 42,
        LOOT_CONTAINER = 45,
        OPERATE_ITEM_FURNACE = 48,
        ATTACK_TOWN = 55,
        PLAY_DEAD = 61,
        EXECUTE_TARGET = 68
    };

    enum class CharacterState : int32_t {
        NORMAL = 0,
        UNCONSCIOUS = 1,
        PLAYING_DEAD = 2,
        DEAD = 3,
        BEING_CARRIED = 4,
        CRIPPLED = 5
    };

    enum class ItemType : int32_t {
        ITEM_GENERIC = 0,
        BUILDING_MATERIAL = 1,
        WEAPON = 2,
        ARMOR = 3,
        FOOD = 4,
        MEDKIT = 5,
        AMMO = 6,
        CURRENCY = 7,
        LIMB_REPLACEMENT = 8,
        BLUEPRINT = 9
    };

    // Unified Rarity / Quality Tier (Normalized across Armor & Weapon manufacturers)
    // Armor grades in Kenshi:
    //   0 = Prototype (White/Grey)
    //   1 = Shoddy (White/Grey)
    //   2 = Standard (Green)
    //   3 = High (Green)
    //   4 = Specialist (Blue)
    //   5 = Masterwork (Yellow)
    // Weapon grades mapped onto the same color tier:
    //   0 = Rusted Junk (Grey)
    //   1 = Rusting Blade / Ancient (Grey)
    //   2 = Refitted Blade / Catun No.1-2 (Green)
    //   3 = Catun No.3 (Green)
    //   4 = Skeleton Smiths Mk I - Mk III (Blue)
    //   5 = Edge Type 1 - 3 (Yellow)
    //   6 = Meitou / Cross (Orange/Gold)
    enum class RarityTier : int32_t {
        PROTOTYPE = 0,  // Grey
        SHODDY    = 1,  // White
        STANDARD  = 2,  // Green (Low)
        HIGH      = 3,  // Green (High)
        BLUE      = 4,  // Specialist (Armor) / Skeleton Mk I-III (Weapon)
        YELLOW    = 5,  // Masterwork (Armor) / Edge Type 1-3 (Weapon)
        MEITOU    = 6   // Meitou (Weapon) / Unique
    };

    enum class BuildingFunction : int32_t {
        BF_NONE = 0,
        BF_GATE = 1,
        BF_WALL = 2,
        BF_STORAGE_RESOURCE = 3,
        BF_STORAGE_WEAPON = 4,
        BF_STORAGE_ARMOR = 5,
        BF_STORAGE_GENERAL = 6,
        BF_CORPSE_FURNACE = 7,
        BF_ITEM_FURNACE = 8,
        BF_TURRET = 9,
        BF_CRAFTING_STATION = 10
    };

    // ============================================================================
    // Forward Declarations
    // ============================================================================
    struct GameData;
    struct Item;
    struct InventoryLayout;
    struct Inventory;
    struct Faction;
    struct TownBase;
    struct Building;
    struct AIState;
    struct Character;

    // ============================================================================
    // Struct Layouts (Kenshi 1.0.65 x64 Compatible Accessors)
    // ============================================================================
    struct GameData {
        void* vtable;
        char pad_08[0x18];
        const char* stringID;
        const char* name;
        int32_t dataType;
    };

    struct Item {
        void* vtable;
        GameData* gameData;
        ItemType itemType;
        int32_t stackCount;
        int32_t maxStack;
        float itemWeight;
        int32_t gridWidth;
        int32_t gridHeight;
        int32_t qualityLevel;
        int32_t colorTier;
        bool isStolen;
        char pad_31[7];

        RarityTier getRarityTier() const {
            if (colorTier >= 0 && colorTier <= 6) {
                return static_cast<RarityTier>(colorTier);
            }
            // Fallback conversion from raw 0..100 Kenshi quality scale:
            // Armor/Weapon color thresholds in Kenshi:
            // < 20: Prototype/Rusted (0)
            // 20..39: Shoddy/Ancient (1)
            // 40..59: Standard/Catun (2)
            // 60..79: High/Catun 3 (3)
            // 80..94: Specialist/Mk I-III [BLUE] (4)
            // 95..99: Masterwork/Edge [YELLOW] (5)
            // >= 100: Meitou (6)
            if (qualityLevel >= 100) return RarityTier::MEITOU;
            if (qualityLevel >= 95)  return RarityTier::YELLOW;
            if (qualityLevel >= 80)  return RarityTier::BLUE;
            if (qualityLevel >= 60)  return RarityTier::HIGH;
            if (qualityLevel >= 40)  return RarityTier::STANDARD;
            if (qualityLevel >= 20)  return RarityTier::SHODDY;
            return RarityTier::PROTOTYPE;
        }

        bool isEquipment() const {
            return itemType == ItemType::WEAPON || itemType == ItemType::ARMOR;
        }

        bool isConsumableOrResource() const {
            return itemType == ItemType::FOOD ||
                   itemType == ItemType::MEDKIT ||
                   itemType == ItemType::CURRENCY ||
                   itemType == ItemType::AMMO ||
                   itemType == ItemType::BUILDING_MATERIAL ||
                   itemType == ItemType::BLUEPRINT;
        }
    };

    struct Inventory {
        void* vtable;
        Character* ownerCharacter;
        Building* ownerBuilding;
        int32_t totalFreeSlots;
        int32_t maxCapacityCount;
        float currentTotalWeight;
        float maxCarryWeight;
        Item** itemsArray;
        int32_t itemCount;
        int32_t itemArrayCapacity;

        int32_t getItemCountByData(const GameData* resData) const {
            if (!itemsArray || itemCount <= 0 || !resData) return 0;
            int32_t total = 0;
            for (int32_t i = 0; i < itemCount; ++i) {
                if (itemsArray[i] && itemsArray[i]->gameData == resData) {
                    total += (itemsArray[i]->stackCount > 0) ? itemsArray[i]->stackCount : 1;
                }
            }
            return total;
        }
    };

    struct Faction {
        void* vtable;
        GameData* factionData;
        bool isPlayerFaction;
        char pad_11[3];
        int32_t factionID;
    };

    struct TownBase {
        void* vtable;
        const char* townName;
        Faction* ownerFaction;
        Vector3 centerPos;
        float townRadius;
        Building** buildings;
        int32_t buildingCount;
        bool isPlayerOutpost;
        char pad_35[3];

        bool isOwnedByPlayer() const {
            if (isPlayerOutpost) return true;
            return ownerFaction && ownerFaction->isPlayerFaction;
        }
    };

    struct Building {
        void* vtable;
        GameData* buildingData;
        Vector3 position;
        float interactionRadius;
        TownBase* parentTown;
        Inventory* storageInventory;
        BuildingFunction functionType;
        bool isConstructed;
        bool isGateClosed;
        char pad_36[2];
        float currentHealth;
        float maxHealth;
        GameData* requiredMaterialData;
        int32_t totalRequiredMaterials;
        int32_t currentDeliveredMaterials;
        Character* currentOperator;

        int32_t getRemainingMaterialNeed(const GameData* matData) const {
            if (requiredMaterialData && matData && requiredMaterialData != matData) {
                return 0;
            }
            int32_t need = totalRequiredMaterials - currentDeliveredMaterials;
            return (need > 0) ? need : 0;
        }
    };

    struct AIState {
        void* vtable;
        Character* owner;
        TaskType currentTask;
        TaskType queuedTask;
        Character* targetCharacter;
        Character* protectTargetVIP;
        Building* targetBuilding;
        Building* activeTurret;
        bool isPassiveOrHold;
        bool isInCombat;
        bool hasCorpseDisposalJob;
        char pad_3B[5];
    };

    struct Character {
        void* vtable;
        GameData* characterData;
        const char* displayName;
        Vector3 position;
        float moveSpeed;
        float maxRunSpeed;
        CharacterState state;
        Faction* faction;
        TownBase* currentTown;
        Inventory* inventory;
        Inventory* backpack;
        AIState* ai;
        Character* carriedCharacter;
        float bloodLevel;
        float koTimer;

        bool isPlayerCharacter() const {
            return faction && faction->isPlayerFaction;
        }

        bool isHostileToPlayer() const {
            // Hostile non-player character inside or attacking player town
            if (!faction || faction->isPlayerFaction) return false;
            if (ai && ai->currentTask == TaskType::ATTACK_TOWN) return true;
            if (ai && ai->targetCharacter && ai->targetCharacter->isPlayerCharacter()) return true;
            return true;
        }
    };

} // namespace KenshiSDK


