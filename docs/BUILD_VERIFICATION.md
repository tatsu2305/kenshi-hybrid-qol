> **Historical Intermediate Report Note (2026-11-12 15:22):** This document records the initial static signature and RVA verification pass performed *before* compiling with the Visual Studio bundled CMake toolchain and testing in-game. Later the same day (17:46), the plugin was compiled and verified in-game for **Smart Bulk Hauling (Delivery & Storage Fetch at RVA 0x33CF1C)** and **Base Knockout Persistence**. See the root [README.md](../README.md) for the final feature verification status.
# Build Verification Report (Kenshi 1.0.65 x64)

Reproduce: `python tools/gen_signatures.py > include/sdk/Signatures.h` (needs `pip install capstone`).

## 1. Which build is this?

| File | MD5 | PE timestamp | Identification |
|---|---|---|---|
| `Kenshi\RE_Kenshi\Kenshi_x64.exe` (36,713,984 B) | `df4a5a7ef8a29deb24b70e7b7f4a222a` | 0x65D604D7 (2024-02-21) | **Steam 1.0.65** - exact hash in KenshiLib `HashToVersionMap` (Kenshi.cpp:61) and in the installed `KenshiLib.dll` |
| `Kenshi\kenshi_x64.exe` (36,718,592 B) | `8a03c256f0da1555d9cceb939b41530a` | 0x6602D59D (2024-03-26) | Steam, **newer build than KenshiLib's table**; hash not in any table. `currentVersion.txt`: "Kenshi 1.0.65 - x64 (Newland)", Steam buildid 13871665 |
| GOG | `213349bf...` | - | not installed here (GOG tables exist in `RE_Kenshi/RVAs/GOG_1.0.65.br`) |

Platform = **Steam**, version 1.0.65 (`kenshi_info.log`: "Version: 1.0.65 - x64 (Newland) (Steam)"; `RE_Kenshi_log.txt`: "platform: Steam / Version supported").
Caveat: which of the two exes is the live process can't be seen while the game is closed. RE_Kenshi logs "supported", which only fits the `df4a5a7e` hash, so most likely the RE_Kenshi copy is run. The plugin uses AOB scans and all 9 resolved patterns also match the newer exe, so both work.

## 2. Why earlier RVAs didn't match
* The `RVA = 0x..` comments in KenshiLib headers are **GOG 1.0.65** addresses (7534/7769 appear in `GOG_1.0.65.br`; only 772 in the Steam table).
* RE_Kenshi resolves Steam addresses from `RE_Kenshi/RVAs/Steam_1.0.65.br` (7630 int32, parallel to the GOG file). Mapping used: `Steam = STEAM[GOG.index(header_rva)]`.
* Check: 7621/7622 Steam RVAs sit on function starts in `RE_Kenshi\Kenshi_x64.exe` (GOG table: 2047/7622). The index-by-`symbols.asm` route was **not** used: the installed `.br` (KenshiLib 0.5.0) has a different function order than the repo (0.5.1).
* Consistency of the GOG→Steam pairing: neighbouring functions share the same delta (e.g. TurretBuilding::update/operate both +0x3E0).

## 3. Resolved functions (Steam RVA, all patterns unique in both exes)

| Function | GOG hdr | Steam RVA | Newer exe RVA |
|---|---|---|---|
| MedicalSystem::canGetUpWakeUp | 0x595CE0 | 0x5959D0 | 0x596460 |
| Character::isItSafeToGetUp | 0x5C9CD0 | 0x5C99C0 | 0x5CA450 |
| TurretBuilding::operate | 0x4345A0 | 0x434980 | 0x434D20 |
| ConstructionState::needsMat | 0x548B20 | 0x548810 | 0x5492A0 |
| ConstructionState::getNeededMats | 0x549CD0 | 0x5499C0 | 0x54A450 |
| Character::getCurrentTownLocation | 0x5C8230 | 0x5C7F20 | 0x5C89B0 |
| FactionRelations::getFactionRelation | 0x6B19C0 | 0x6B20B0 | 0x6B2400 |
| Character::addGoal | 0x5C88A0 | 0x5C8590 | 0x5C9020 |
| Character::addJob | 0x5C8620 | 0x5C8310 | 0x5C8DA0 |
| FactionRelations::isEnemy / _isEnemy | - | 0x6B2380 / 0x6B2280 | - (identical code => no unique AOB; reimplemented) |

## 4. Disassembly-confirmed facts (Steam exe)
* `Character::getTotalCarryWeight`: `[rcx+0x2E8]` = inventory, `[rbx+0x348]` = isCarryingSomething -> matches KenshiLib offsets, confirming the table pairing.
* `Character::getMedical` = `lea rax,[rcx+0x458]` -> MedicalSystem is embedded at 0x458.
* `Faction::isThePlayer` = `[rcx+0x250] != 0`; `Character::isPlayerCharacter` = `getFaction()` (vtable +0x58) then the same test.
* `FactionRelations::isEnemy(a)` = `a && a != this->[+8] && rel <= -30.0f` (constant at 0x16CBCFC).
* `ConstructionState::needsMat(GameData*)` returns `(int)(total - delivered)` of the matching BuildMaterial (+8 / +0xC).
* `MedicalSystem::canGetUpWakeUp` reads bool flags at +0x161/+0x164, then a helper, returns 1 if none set.

## 5. What the three first hooks really do
1. **Wake-up (Feature 4)** - implemented. Hostile (relation <= -30 to the town owner), non-player, not chained/carried, inside a player-owned town -> `canGetUpWakeUp` / `isItSafeToGetUp` return false. `HoldDownMaxSeconds` bounds it (0 = indefinite). Not yet done: forced execution / furnace of unconscious enemies.
2. **Turret (Feature 6)** - `TurretBuilding::operate` only *observes* gunners. It cannot prevent leaving the turret; the abandon decision is in unmapped AI code. Experimental `addGoal` melee-goal blocking is provided but off by default.
3. **Bulk hauling (Feature 1)** - `needsMat`/`getNeededMats` report the need but don't choose the carried amount, so they cannot be changed to haul more. Implemented as a call-site tracer (`TraceHaulingCalls`). Real fix needs the code behind `DELIVER_RESOURCES` (88) / `ADD_MATERIALS_TO_BUILDING` (71).

## 6. Not tested
No MSVC/CMake on this machine: nothing has been compiled or run in-game. Signatures were verified statically against both exes only.


