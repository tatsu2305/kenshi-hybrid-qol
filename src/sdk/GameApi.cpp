#include "sdk/GameApi.h"
#include "sdk/KenshiSDK.h"
#include "sdk/Signatures.h"
#include "core/PatternScanner.h"
#include "core/Logger.h"

namespace GameApi {

    using GetTownFn     = void*(__fastcall*)(void* character);
    using GetRelationFn = float(__fastcall*)(void* relations, void* otherFaction);
    using GetFactionFn  = void*(__fastcall*)(void* obj);

    static GetTownFn     s_getTown     = nullptr;
    static GetRelationFn s_getRelation = nullptr;

    bool Init() {
        using namespace KenshiSDK::Signatures;
        s_getTown     = reinterpret_cast<GetTownFn>(
            Core::PatternScanner::ResolveSignature(SIG_CHARACTER_GET_CURRENT_TOWN, false));
        s_getRelation = reinterpret_cast<GetRelationFn>(
            Core::PatternScanner::ResolveSignature(SIG_FACTION_REL_GET_RELATION, false));
        const bool ok = s_getTown && s_getRelation;
        if (!ok) Core::Logger::Error("[GameApi] required helper functions not resolved.");
        return ok;
    }

    void* GetCurrentTown(void* character) {
        return (s_getTown && character) ? s_getTown(character) : nullptr;
    }

    void* GetFaction(void* obj) {
        if (!obj) return nullptr;
        void** vtbl = *reinterpret_cast<void***>(obj);
        if (!vtbl) return nullptr;
        auto fn = reinterpret_cast<GetFactionFn>(vtbl[KenshiSDK::Verified::RootObjectBase::VT_GetFaction / sizeof(void*)]);
        return fn ? fn(obj) : nullptr;
    }

    bool IsPlayerFaction(const void* faction) {
        return faction && KenshiSDK::Verified::ReadAt<void*>(faction, KenshiSDK::Verified::Faction::IsPlayer_Ptr) != nullptr;
    }

    float GetRelation(void* relations, void* other) {
        return (s_getRelation && relations) ? s_getRelation(relations, other) : 0.0f;
    }

    bool IsEnemyFaction(void* a, void* b) {
        if (!a || !b || a == b) return false;
        void* rel = KenshiSDK::Verified::ReadAt<void*>(a, KenshiSDK::Verified::Faction::Relations_Ptr);
        if (!rel) return false;
        return GetRelation(rel, b) <= KenshiSDK::Verified::FactionRelations::EnemyThreshold;
    }

}

