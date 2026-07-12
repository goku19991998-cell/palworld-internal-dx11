#pragma once
#include "include/Menu.hpp"
#include "config.h"
#include <Windows.h>

void ESP();

void ESP_DEBUG(float mDist, ImVec4 color = { 1.0f, 1.0f, 1.0f, 01.0f }, SDK::UClass* mEntType = SDK::AActor::StaticClass());

void DrawUActorComponent(SDK::TArray<SDK::UActorComponent*> Comps, ImColor color);

void UnlockAllEffigies();

// Clears the fog-of-war render target directly - local rendering only, no
// server call. One-shot, not a toggle, call on button press.
void RevealWholeMap();

// Grants mCount more of slot mIndex via RequestMove_ToServer, moving the slot
// onto itself with a negative Num - Num=-N grants +N for free.
SDK::EPalItemOperationResult IncrementInventoryItemCountByIndex(__int32 mCount, __int32 mIndex = 0);

// Tops slot mIndex back up to mTargetCount whenever it drops (thrown Pal
// Sphere, eaten food, ...). Call every tick; no-ops once already at target.
void FreezeSlotItemCount(__int32 mIndex, __int32 mTargetCount);

// Tops up every PalSphere-tier item (PalSphere, _Mega, _Giga, ...) to
// mTargetCount. Call every tick.
void FreezeAllPalSpheres(__int32 mTargetCount);

// Returns "[index] StaticId x StackCount" for every slot in the main inventory container.
std::vector<std::string> GetInventorySlotOverview();

// Forces a world save via UPalCheatManager::DebugSaveWorldData under the given slot name.
void ForceSaveWorld(const char* saveName);

// Doubles slot mIndex's stack via the negative-move dupe (IncrementInventoryItemCountByIndex).
void DuplicateItemInSlot(__int32 mIndex);

// Runs DuplicateItemInSlot over every non-empty slot in the main inventory container.
void DuplicateAllSlots();

// Calls the game's own Dev_ForceRespawnNear*Spawners_ToServer RPCs to respawn
// map objects/resources within mRadiusCM. One-shot, not per-tick.
void ForceRespawnNearby(float mRadiusCM);

// --- Pal Editor (Team) ---
// Returns "[index] Name Lv.X HP:cur/max" for every pal in the local player's party.
std::vector<std::string> GetPartyPalOverview();

// Loads party pal mIndex into Config.EditPal* for the UI. False if index out of range.
bool LoadSelectedPartyPalStats(__int32 mIndex);

// Writes Config.EditPal* back onto party pal mIndex.
void ApplySelectedPartyPalStats(__int32 mIndex);

// Heals party pal mIndex to max HP.
void FullHealPartyPal(__int32 mIndex);

// Returns the internal FName of every passive skill on pal mIndex, one per line.
std::vector<std::string> GetSelectedPartyPalPassives(__int32 mIndex);

// Adds/removes a passive skill by its internal FName (e.g. "Serious") - no
// in-game lookup, source IDs from wikis/dumps.
void AddPassiveToPartyPal(__int32 mIndex, const char* passiveName);
void RemovePassiveFromPartyPal(__int32 mIndex, const char* passiveName);

void AddItemToInventoryByName(SDK::UPalPlayerInventoryData* data, char* itemName, int count);

// Maps EPalItemOperationResult to a readable string for console diagnostics.
const char* ItemOpResultToString(SDK::EPalItemOperationResult r);

// Grants an item via the real RequestAddItem_ToServer RPC (Transmitter->Player)
// instead of the ghost-item AddItem_ServerInternal path used by
// AddItemToInventoryByName above.
SDK::EPalItemOperationResult GiveRealItemByName(const char* itemName, __int32 mCount);

void SpawnMultiple_ItemsToInventory(config::QuickItemSet Set);

void AnyWhereTP(SDK::FVector& vector, bool IsSafe);

// Scans the live world for fast-travel point actors, returns FastTravelPointID
// (+" (locked)" if not yet unlocked) and location.
std::vector<std::pair<std::string, SDK::FVector>> GetFastTravelPoints();

// Scans dungeon entrances with a currently active instance
// (StageModel/InstanceModel != null) - most dungeons are instanced and come
// and go. Label has the real dungeon name/level and minutes left.
std::vector<std::pair<std::string, SDK::FVector>> GetActiveDungeonEntrances();

void ExploitFly(bool IsFly);

// Adds vertical fly movement (Space up, Left Ctrl down) - StartFlyToServer
// only handles horizontal. Call every tick while Config.IsToggledFly is on.
void UpdateFlyVerticalMovement();

void SpeedHack(float mSpeed);

// Alternative to SpeedHack via MaxWalkSpeed. Separate function, doesn't touch SpeedHack().
void SpeedHackClean(bool bEnable, float mMultiplier);

// Scales Walk/Swim/Fly/Custom movement speed together.
void SpeedHackAllInOne(bool bEnable, float mMultiplier);

// Dumps MovementMode/CustomMovementMode + all four Max*Speed fields + current speed to console.
void DumpMovementSpeedState();

void SetDemiGodMode(bool bIsSet);

void RespawnLocalPlayer(bool bIsSafe);

void SetPlayerHealth(float newHealthPercentage);

void ReviveLocalPlayer();

void ResetStamina();

void GiveExperiencePoints(__int32 mXP);

void SetPlayerAttackParam(__int32 mNewAtk);

void SetPlayerDefenseParam(__int32 mNewDef);

// Scales the equipped armor's own DefenseValue instead - DefenseUp above is
// Transient like AttackUp and doesn't stick. Armor sits in the
// PlayerEquipArmor container like any item; UPalItemSlot::TryGetStaticItemData()
// resolves to the UPalStaticArmorItemData holding the real Defense stat. See
// feature.cpp for capture/restore.
void SetArmorDefenseBoost(float mMultiplier, bool bRestoreDefault = false);

// Diagnostic: confirmed GetDefense() (the aggregate stat) correctly reflects
// SetArmorDefenseBoost's write live, no re-equip needed. Prints GetDefense(),
// DefenseUp, and per equipped armor slot the item's StaticId and the
// DefenseValue read back from UPalStaticArmorItemData.
void DumpArmorDefenseState();

void SetInfiniteAmmo(bool bInfAmmo);

// Prints the equipped weapon's ammo fields (IsRequiredBullet,
// IsInfinityMagazine, bullet counts, IsFullMagazine) to console.
void DumpWeaponAmmoState();

void SetCraftingSpeed(float mNewSpeed, bool bRestoreDefault = false);

void SetMaxInventoryWeight(float mNewMaxWeight, bool bRestoreDefault = false);

void AddTechPoints(__int32 mPoints);

void AddAncientTechPoints(__int32 mPoints);

void RemoveTechPoints(__int32 mPoints);

void RemoveAncientTechPoint(__int32 mPoints);

// Permanent player stat increases. mCategory (shared by all of these below):
// 0=MaxHP, 1=MaxSP(stamina), 2=MaxInventoryWeight, 3=Power(attack), 4=WorkSpeed, 5=CaptureLevel.

// Debug_SetStatusPoint_ToServer, absolute level - real RPC, doesn't work in-game. Kept for reference.
void SetPlayerStatusPointLevel_DebugRPC(__int32 mCategory, __int32 mLevel);

// AddPlayerStatusPoint_ToServer, additive - the real RPC a level-up spend goes through.
void AddPlayerStatusPointReal(__int32 mCategory, __int32 mPoints);

// AddPlayerStatusPointReal only spends unused points already earned by
// leveling, usually 0. Grants unused points via SaveParameter first, then
// spends them through the real RPC.
void GrantAndSpendStatusPoint(__int32 mCategory, __int32 mPoints);

// Spawns a Technical Manual (TechnologyBook_G1/G2/G3, mGrade 1-3) and consumes
// it via UPalTechnologyData::RequestAddTechnologyPointByItem - the real
// consume path (RequestUseItemToCharacter_ToServer doesn't consume it, the
// book just keeps accumulating).
void GiveAndUseTechBook(__int32 mGrade);

// Same for Ancient Tech Points: spawns PalCrystal_Ex mCount times, consumes
// each via RequestAddBossTechnologyPointByItem.
void GiveAndUseAncientCivilizationParts(__int32 mCount);

// Flat permanent stat increases via "Elixir" consumables (from the game's
// .pak data, not the SDK). mCategory: 0=HP, 1=Stamina, 2=Weight, 3=Attack,
// 4=WorkSpeed. mTier: 1 or 2.
void GiveAndUseElixir(__int32 mCategory, __int32 mTier);

// Uses the item in slot mIndex mTimes in a row (one RPC per use, fired once on
// button press, not an auto-clicker). Works on any real item since it doesn't
// touch the ghost-item path. Caps at the slot's StackCount.
void UseInventorySlotItemMultiple(__int32 mIndex, __int32 mTimes);

float GetDistanceToActor(SDK::AActor* pLocal, SDK::AActor* pTarget);

void ForgeActor(SDK::AActor* pTarget, float mDistance, float mHeight = 0.0f, float mAngle = 0.0f);

void SendDamageToActor(SDK::APalCharacter* pTarget, __int32 damage, bool bSpoofAttacker = false);

void DeathAura(__int32 dmgAmount, float mDistance, bool bIntensityEffect = false, bool bVisualEffect = false, SDK::EPalVisualEffectID visID = SDK::EPalVisualEffectID::None);

void TeleportAllPalsToCrosshair(float mDistance);

void AddWaypointLocation(std::string wpName);

void RenderWaypointsToScreen();

SDK::FGuid genGuid();

//void ForceJoinGuild( SDK::APalPlayerCharacter* targetPlayer );
