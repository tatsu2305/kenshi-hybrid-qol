#pragma once
// Thin wrappers around game functions resolved by AOB (never by guessed RVA).
#include <cstdint>

namespace GameApi {

    // Resolves all helpers; returns false if any required one is missing.
    bool Init();

    // Character::getCurrentTownLocation() -> TownBase* (may be null)
    void* GetCurrentTown(void* character);
    // RootObjectBase::getFaction() through the object's vtable (+0x58) -> Faction*
    void* GetFaction(void* rootObject);
    // Faction::isThePlayer()  (== *(void**)(f+0x250) != nullptr)
    bool  IsPlayerFaction(const void* faction);
    // FactionRelations::getFactionRelation(Faction*) -> float. relations = *(void**)(faction+0x78)
    float GetRelation(void* factionRelations, void* otherFaction);
    // a != b && relation(a->b) <= -30.0f   (reimplementation of FactionRelations::isEnemy)
    bool  IsEnemyFaction(void* factionA, void* factionB);

}

