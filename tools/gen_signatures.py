#!/usr/bin/env python3
"""
gen_signatures.py - derive REAL AOB signatures for Kenshi 1.0.65 from the local game files.

Method (all inputs are local / open-source, nothing is guessed):
  1. KenshiLib headers carry "RVA = 0x.." comments. Those are the GOG 1.0.65 addresses
     (verified: 7534 of 7769 header RVAs appear in RE_Kenshi/RVAs/GOG_1.0.65.br).
  2. RE_Kenshi/RVAs/{GOG,Steam}_1.0.65.br are parallel int32 tables (same order).
     Steam RVA = STEAM[ GOG.index(header_rva) ].
  3. Steam RVAs are checked against RE_Kenshi/Kenshi_x64.exe (md5 df4a5a7e.., the
     build KenshiLib hashes as Steam 1.0.65): 7621/7622 land on function starts.
  4. A masked byte pattern (rip-relative disps / rel32 call+jmp targets wildcarded) is
     grown until it is unique in that exe AND matches the Steam RVA; it is then also
     tested against the top-level kenshi_x64.exe.
Usage: python tools/gen_signatures.py [KenshiDir] > include/sdk/Signatures.h
"""
import re, struct, sys, hashlib
from capstone import Cs, CS_ARCH_X86, CS_MODE_64, CS_OP_MEM, CS_OP_IMM
from capstone import x86_const as X

KDIR = sys.argv[1] if len(sys.argv) > 1 else r"C:\Program Files (x86)\Steam\steamapps\common\Kenshi"
PRISTINE = KDIR + r"\RE_Kenshi\Kenshi_x64.exe"
TOP = KDIR + r"\kenshi_x64.exe"

# (C++ id, display name, GOG header RVA from KenshiLib, source header)
TARGETS = [
    ("MEDICAL_CAN_GET_UP_WAKE_UP", "MedicalSystem::canGetUpWakeUp",       0x595CE0, "MedicalSystem.h"),
    ("CHARACTER_IS_IT_SAFE_TO_GET_UP", "Character::isItSafeToGetUp",      0x5C9CD0, "Character.h"),
    ("TURRET_OPERATE", "TurretBuilding::operate(Character*,float)",       0x4345A0, "Building/TurretBuilding.h"),
    ("CONSTRUCTION_NEEDS_MAT", "Building::ConstructionState::needsMat",   0x548B20, "Building/Building.h"),
    ("CONSTRUCTION_GET_NEEDED_MATS", "Building::ConstructionState::getNeededMats", 0x549CD0, "Building/Building.h"),
    ("CHARACTER_GET_CURRENT_TOWN", "Character::getCurrentTownLocation",   0x5C8230, "Character.h"),
    ("FACTION_REL_IS_ENEMY", "FactionRelations::isEnemy(Faction*)",       0x6B1C90, "FactionRelations.h"),
    ("FACTION_REL__IS_ENEMY", "FactionRelations::_isEnemy(Faction*)", 0x6B1B90, "FactionRelations.h"),
    ("FACTION_REL_GET_RELATION", "FactionRelations::getFactionRelation(Faction*)", 0x6B19C0, "FactionRelations.h"),
    ("CHARACTER_ADD_GOAL", "Character::addGoal(TaskType,RootObjectBase*)", 0x5C88A0, "Character.h"),
    ("CONSTRUCTION_ADD_MATERIALS", "Building::ConstructionState::addMaterials", 0x549C20, "Building/Building.h"),
    ("INVENTORY_REMOVE_AUTO_DESTROY", "Inventory::removeItemAutoDestroy", 0x744BD0, "Inventory.h"),
    ("INVENTORY_REMOVE_DONT_DESTROY", "Inventory::removeItemDontDestroy_returnsItem", 0x749800, "Inventory.h"),
    ("INVENTORY_TAKE_ONE_ONLY", "Inventory::takeOneItemOnly", 0x7499E0, "Inventory.h"),
    ("CHARACTER_ADD_ORDER", "Character::addOrder(Building*,TaskType,RootObject*,bool,bool,Vector3&)", 0x5D1950, "Character.h"),
    ("CHARACTER_UNIQUE_STATE_UPDATE", "Character::uniqueStateUpdate", 0x5CECF0, "Character.h"),
    ("CHARACTER_FOOD_UPDATE", "Character::foodUpdate", 0x5D2240, "Character.h"),
    ("FURNACE_OPERATE", "FurnaceBuilding::operate", 0x299AF0, "Building/FurnaceBuilding.h"),
    ("INVENTORY_ADD_ITEM", "Inventory::addItem", 0x744A70, "Inventory.h"),
    ("ORDERS_ADD_GOAL", "OrdersReceiver::addGoal(Tasker*)", 0x5071D0, "AI/AITaskSystem.h"),
    ("ORDERS_HAS_GOAL", "OrdersReceiver::hasGoal(TaskType)", 0x506CC0, "AI/AITaskSystem.h"),
    ("ORDERS_REMOVE_GOAL", "OrdersReceiver::removeGoal(TaskType)", 0x506C10, "AI/AITaskSystem.h"),
    ("CHARACTER_ADD_JOB", "Character::addJob",                            0x5C8620, "Character.h"),
]

def load(p): return open(p, "rb").read()
def sections(d):
    pe = struct.unpack_from("<I", d, 0x3c)[0]; n = struct.unpack_from("<H", d, pe+6)[0]
    opt = struct.unpack_from("<H", d, pe+20)[0]; s = pe+24+opt; out = []
    for i in range(n):
        o = s+40*i; vs, va, rs, rp = struct.unpack_from("<IIII", d, o+8); out.append((va, vs, rp, rs))
    return out
def tab(p):
    b = load(p); return struct.unpack("<%di" % (len(b)//4), b)

GOG = tab(KDIR + r"\RE_Kenshi\RVAs\GOG_1.0.65.br")
STEAM = tab(KDIR + r"\RE_Kenshi\RVAs\Steam_1.0.65.br")
pd, td = load(PRISTINE), load(TOP)
def text(d):
    va, vs, rp, rs = sections(d)[0]; return va, d[rp:rp+rs]
pva, ptext = text(pd); tva, ttext = text(td)
md = Cs(CS_ARCH_X86, CS_MODE_64); md.detail = True

def masked(code, rva, maxlen=96):
    """returns list of (byte or None) for decoded instructions"""
    out = []; 
    for i in md.disasm(code, rva):
        b = bytearray(i.bytes); mask = [False]*len(b)
        for op in i.operands:
            if op.type == CS_OP_MEM and op.mem.base == X.X86_REG_RIP:
                for k in range(i.disp_size): mask[i.disp_offset+k] = True
        if i.mnemonic in ("call", "jmp") or (i.mnemonic.startswith("j") and i.imm_size == 4):
            if i.imm_size == 4:
                for k in range(4): mask[i.imm_offset+k] = True
        for k, v in enumerate(b): out.append(None if mask[k] else v)
        if len(out) >= maxlen or i.mnemonic == "int3": break
    return out

def rx(pat): return re.compile(b"".join(b"." if v is None else re.escape(bytes([v])) for v in pat), re.S)
def count(t, pat, limit=2):
    n = 0; pos = 0; r = rx(pat); hits = []
    for m in r.finditer(t):
        hits.append(m.start()); n += 1
        if n >= limit: break
    return hits

results = []
for cid, name, gog_rva, hdr in TARGETS:
    if gog_rva not in GOG: results.append((cid, name, None, "GOG RVA not in table")); continue
    idx = GOG.index(gog_rva); srva = STEAM[idx]
    code = ptext[srva-pva: srva-pva+400]
    full = masked(code, srva, 160)
    chosen = None
    for L in range(14, len(full)+1):
        pat = full[:L]
        if pat[-1] is None: continue
        h = count(ptext, pat)
        if len(h) == 1 and h[0]+pva == srva:
            chosen = pat; break
    if not chosen: results.append((cid, name, srva, "no unique AOB (function too small/similar)")); continue
    ht = count(ttext, chosen)
    results.append((cid, name, srva, chosen, gog_rva, len(ht), (ht[0]+tva) if len(ht) == 1 else None, hdr))

def fmt(p): return " ".join("?" if v is None else "%02X" % v for v in p)
print("#pragma once")
print("// AUTO-GENERATED by tools/gen_signatures.py - DO NOT EDIT BY HAND.")
print("// Build: Steam 1.0.65, RE_Kenshi/Kenshi_x64.exe md5 df4a5a7ef8a29deb24b70e7b7f4a222a (KenshiLib Steam 1.0.65 hash)")
print("// Each pattern is unique in that exe and resolves to the listed RVA. 'newer_exe_rva' = RVA found in")
print("// top-level kenshi_x64.exe (md5 %s) if the same pattern is unique there too, else 0." % hashlib.md5(td).hexdigest())
print("// RVAs are informational (for the verification report); the plugin only uses the AOB patterns.")
print("#include <cstdint>\nnamespace KenshiSDK::Signatures {")
print("struct SignatureEntry { const char* name; const char* pattern; uint32_t steamRva; uint32_t newerExeRva; };")
for r in results:
    if len(r) == 4:
        print(f"// UNRESOLVED {r[0]} ({r[1]}): {r[3]}")
        print(f"constexpr SignatureEntry SIG_{r[0]} = {{ \"{r[1]}\", nullptr, 0, 0 }};")
    else:
        cid, name, srva, pat, g, nt, trva, hdr = r
        print(f"// {name}  [{hdr}; GOG hdr 0x{g:X} -> Steam 0x{srva:X}; matches in newer exe: {nt}]")
        print(f"constexpr SignatureEntry SIG_{cid} = {{ \"{name}\", \"{fmt(pat)}\", 0x{srva:X}, 0x{(trva or 0):X} }};")
print("}")

