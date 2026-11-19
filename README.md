# KenshiHybridQoL — Native C++ x64 Plugin for RE_Kenshi

**Languages:** C++20, Python (Capstone signature generator) · **Build System:** CMake (Visual Studio x64) · **Target:** Kenshi 1.0.65 (Steam x64, PE Timestamp `0x65D604D7`, `RE_Kenshi` plugin architecture)

`KenshiHybridQoL` is a native C++ x64 DLL plugin for *Kenshi* (1.0.65 Steam x64) built on top of the `RE_Kenshi` plugin ecosystem. It uses RVA hooks, binary pattern scanning, and Capstone-assisted signature generation to solve base logistics bottlenecks—most notably single-item material hauling—while maintaining strict engine safety boundaries.

---

## Feature Implementation & Verification Status

Every feature in this plugin is documented transparently according to its actual in-game verification status:

| Feature | Status | Technical Implementation & Notes |
| :--- | :--- | :--- |
| **Smart Bulk Hauling (Delivery)** | **Verified In-Game** | Hooks `addMaterials` + `removeItemAutoDestroy`; delivers multiple construction materials in a single tick instead of one item at a time. |
| **Smart Bulk Hauling (Storage Fetch)** | **Verified In-Game** | Hooks `takeOneItemOnly` (`RVA 0x33CF1C`); configurable via `EnableBulkFetch` and `FetchBatchLimit` (default: `10`). Fetches batches from storage barrels/containers. |
| **Base Knockout Persistence** | **Verified In-Game** | Hooks `canGetUpWakeUp` / `isItSafeToGetUp` to prevent knocked-out hostile raiders inside player bases from repeatedly standing back up. Does not force execution or furnace disposal. |
| **Turret Discipline** | **Experimental** | Monitors `TurretBuilding::operate`; `BlockMeleeGoalsForGunners` suppresses melee goal switching for assigned turret gunners (pending broader in-game edge-case observation). |
| **Tactical Escort Leash** | **Experimental / Unverified** | Tracks bodyguard jobs (`addJob` / `addOrder` where subject = VIP). If the bodyguard exceeds `MaxEscortLeashDistance` (`18m`), combat goals (`16, 4, 5, 9, 61, 15`) are cleared via `OrdersReceiver::removeGoal`. No VIP speed matching. |
| **Smart Loot & Smelt** | **Not Implemented (Disabled)** | Disabled by design (`EnableSmartLootAndSmelt = false`). `Item::quality` does not map safely to rarity tiers, and direct inventory removal carries item-loss risks without a verified engine entrypoint. |
| **Anti-Gate Clipping** | **Not Implemented in C++ (FCS Only)** | Disabled in C++ to avoid NavMesh corruption from forced coordinate displacement. Addressed purely through Forgotten Construction Set (FCS) values documented in `FCS_CHANGES.md`. |

---

## Architecture & Project Structure

```text
├── CMakeLists.txt                   # x64 DLL build configuration
├── config.ini                       # Runtime feature toggles, batch limits & diagnostic trace flags
├── FCS_CHANGES.md                   # Companion Forgotten Construction Set (FCS) configuration notes
├── docs/                            # Historical RVA & build verification notes (OFFSET_VERIFICATION.md, BUILD_VERIFICATION.md)
├── include/
│   ├── core/                        # Config.h, HookManager.h, Logger.h, PatternScanner.h
│   ├── features/                    # Features.h
│   └── sdk/                         # GameApi.h, KenshiSDK.h, SdkModel.h, Signatures.h, SignaturesPending.h
├── src/
│   ├── dllmain.cpp                  # DLL entrypoint & RE_Kenshi plugin initialization
│   ├── core/                        # Hook management, INI parser, structured logger & pattern scanner
│   ├── features/
│   │   ├── BulkHauling.cpp          # Batch material delivery & storage fetching hooks
│   │   ├── BaseKnockout.cpp         # Unconscious state wake-up control hooks
│   │   ├── TurretDiscipline.cpp     # Turret operator goal filtering
│   │   ├── TacticalEscort.cpp       # Bodyguard leash distance & combat goal pruning
│   │   ├── SmartLootSmelt.cpp       # Trace stub (disabled for inventory safety)
│   │   └── AntiGateClipping.cpp     # Stub (deferred to FCS_CHANGES.md)
│   └── sdk/
│       └── GameApi.cpp              # Engine RVA bindings & function pointers
└── tools/
    └── gen_signatures.py            # Python + Capstone utility to generate include/sdk/Signatures.h
```

---

## Build & Installation

### 1. Build with Visual Studio CMake
Use Visual Studio 2022's bundled CMake toolchain (configure for x64 first, then build `Release`; note that MinHook v1.3.3 is fetched automatically via CMake `FetchContent`):

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release
```

### 2. Output & Deployment
Copy the compiled `build/Release/KenshiHybridQoL.dll` and `KenshiHybridQoL.ini` into your Kenshi installation directory (`Kenshi\` and `Kenshi\RE_Kenshi\plugins\`).

> **Version Compatibility Note:** Tested specifically against PE timestamp `0x65D604D7` (Steam x64 Kenshi 1.0.65 with `RE_Kenshi`). Specific call-site RVAs for material delivery and fetching are version-dependent.

### 3. Signature Generation (`tools/gen_signatures.py`)
To regenerate binary signatures from `RE_Kenshi/RVAs/{GOG,Steam}_1.0.65.br` tables and `KenshiLib` headers (requires Python `capstone`):

```powershell
python tools/gen_signatures.py > include/sdk/Signatures.h
```

### 4. Diagnostic Mode
Setting `[BulkHauling] TraceHaulingCalls = true` in `KenshiHybridQoL.ini` writes detailed `[TaskTrace]`, `[BulkHauling/trace]`, and `[TacticalEscort/status]` entries to the log file for RVA debugging. Keep `false` during normal gameplay.

---

## Türkçe Özet (Turkish Summary)

- **Derleme:** Visual Studio dahili CMake aracı ile `cmake --build build --config Release`.
- **Doğrulanan Özellikler:**
  - *Smart Bulk Hauling (Teslimat & Depodan Alma):* `addMaterials`, `removeItemAutoDestroy` ve `takeOneItemOnly` (`RVA 0x33CF1C`) kancaları ile tek seferde çoklu malzeme taşıma ve depodan toplu (`FetchBatchLimit = 10`) malzeme çekme oyun içinde doğrulandı.
  - *Base Knockout:* `canGetUpWakeUp` / `isItSafeToGetUp` kancaları ile üs içinde bayılan düşmanların tekrar ayağa kalkması engellendi.
- **Deneysel / Devre Dışı Özellikler:** *Turret Discipline* ve *Tactical Escort* deneyseldir; *Smart Loot & Smelt* (envanter güvenliği nedeniyle) ve *Anti Gate Clipping* (NavMesh güvenliği nedeniyle, çözümü `FCS_CHANGES.md` içindedir) C++ tarafında devre dışıdır.

