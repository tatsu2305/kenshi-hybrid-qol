# Project Status — KenshiHybridQoL

## Overall Classification: `Partial` (Core Features `Completed` & Verified In-Game; Secondary Features `Experimental` or Disabled by Design)

| Component / Feature | Status | Details & Verification |
| :--- | :--- | :--- |
| **x64 C++20 CMake DLL Build (`CMakeLists.txt`)** | **Completed** | Verified local compilation via Visual Studio 2022 MSVC `19.44.35230.0` (`cmake --build --config Release`), producing `KenshiHybridQoL.dll` and `minhook.lib`. |
| **Smart Bulk Hauling (Construction Delivery)** | **Completed** | Verified in-game on Kenshi 1.0.65 (Steam x64, PE Timestamp `0x65D604D7`). Hooks `addMaterials` + `removeItemAutoDestroy` to deliver multiple construction materials in a single tick. |
| **Smart Bulk Hauling (Storage Batch Fetch)** | **Completed** | Verified in-game. Hooks `takeOneItemOnly` (`RVA 0x33CF1C`) to pull configurable batches (`FetchBatchLimit = 10`) from storage containers. |
| **Base Knockout Persistence** | **Completed** | Verified in-game. Hooks `canGetUpWakeUp` and `isItSafeToGetUp` to prevent knocked-out hostile raiders inside player-owned bases from repeatedly standing back up. |
| **Capstone AOB Signature Generator (`tools/gen_signatures.py`)** | **Completed** | Python syntax verified; pairs `RE_Kenshi/RVAs/{GOG,Steam}_1.0.65.br` tables and `KenshiLib` headers to generate unique masked AOB signatures in `include/sdk/Signatures.h`. |
| **Turret Discipline (`TurretDiscipline.cpp`)** | **Experimental** | Monitors `TurretBuilding::operate` and filters melee goal switches via `BlockMeleeGoalsForGunners`. Pending broader in-game edge-case observation. |
| **Tactical Escort Leash (`TacticalEscort.cpp`)** | **Experimental / Not Verified** | Tracks bodyguard jobs (`addJob` / `addOrder`) and clears combat goals via `OrdersReceiver::removeGoal` beyond `18m`. In-game goal-cancellation edge cases are not yet finalized. |
| **Smart Loot & Auto-Smelt (`SmartLootSmelt.cpp`)** | **Not Verified (Disabled)** | Disabled by design (`EnableSmartLootAndSmelt = false`) to prevent inventory item-loss risks because `Item::quality` does not map directly to rarity tiers. |
| **Anti-Gate Clipping (`AntiGateClipping.cpp`)** | **Not Verified (FCS Only)** | Disabled in C++ (`EnableAntiGateClipping = false`) to avoid NavMesh corruption; addressed via Forgotten Construction Set configuration documented in `FCS_CHANGES.md`. |
