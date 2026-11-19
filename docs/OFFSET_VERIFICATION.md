> **Historical Intermediate Report Note (2026-11-12 15:08):** This document captures the earliest exploratory check explaining why raw KenshiLib header RVA comments (which are GOG 1.0.65 addresses) did not directly match the Steam 1.0.65 executable. This finding led to the creation of 	ools/gen_signatures.py, which pairs RE_Kenshi/RVAs/GOG_1.0.65.br and Steam_1.0.65.br tables to generate verified AOB patterns in include/sdk/Signatures.h. See [BUILD_VERIFICATION.md](./BUILD_VERIFICATION.md) and the root [README.md](../README.md) for final results.
# Offset / RVA Verification Report

Source of truth used: KenshiLib (GPLv3) `Include/kenshi/*.h`, cloned to `external/KenshiLib` (commit 1ce4a16). Local binary checked: `C:\Program Files (x86)\Steam\steamapps\common\Kenshi\kenshi_x64.exe` (36,718,592 bytes, MD5 `8a03c256f0da1555d9cceb939b41530a`). Scripts: `scratch/verify.py`, `scratch/v2.py`.

## Findings

1. **Old `KenshiSDK.h` layouts were invented.** Real `Character`, `Item`, `Inventory`, `TownBase`, `Building` are deep inheritance chains (`RootObject` → ...) with very different offsets (e.g. `Character::inventory` = 0x2E8, `Item::quantity` = 0x12C, not 0x40/0x14). Invented fields were removed (kept only in `SdkModel.h` as a stripped, non-offset placeholder).
2. **Old AOB patterns matched 0 locations** in the local exe (all 7 checked). Old fallback RVAs (0x0054A820 etc.) were not from any source. All removed.
3. **Several assumed functions don't exist by that name** in KenshiLib (`CalculateHaulTransferAmount`, `ApplyCharacterSeparationForce`, `UpdateBodyguardTask`, `OnCharacterWakeUpTransition`, `CanDisposeAsCorpseOrExecute`, `OnDepositBodyIntoFurnace`, `ShouldAbandonTurretForMelee`). Real nearby functions are listed in `Signatures.h` hints.
4. **KenshiLib RVAs do not match the local exe.** Of 804 sampled RVAs, only 201 (~25%) sit right after padding/ret (expected for real function starts); known examples land mid-instruction (e.g. `canGetUpWakeUp` 0x595CE0 → `CC CC...`). The installed exe is therefore a different build than KenshiLib's table (or KenshiLib resolves RVAs per-version at runtime). **Cannot confirm 1.0.65 from this machine.**
5. KenshiLib is **GPLv3**, built with **VS2010 x64 toolset + Boost 1.60 + multihook MinHook**, and asks plugins to use its own hook system (not a separate MinHook) so multiple plugins can hook one function. This project's CMake/C++20/MinHook setup is incompatible with that.

## Offsets now kept (from KenshiLib, unconfirmed on local exe)
See `include/sdk/KenshiSDK.h` (`Verified::*`).

## Recommended next step
Port to a KenshiLib plugin (VS2010 toolset, `kenshi/*.h`, KenshiLib hooks) and hook real functions: `MedicalSystem::knockout/canGetUpWakeUp`, `TurretBuilding::operate`, `Building::needsMat`. Remaining hooks need reversing.


