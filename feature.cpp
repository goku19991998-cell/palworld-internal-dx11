#include "pch.h"
#include "feature.h"
using namespace SDK;

//	should only be called from a GUI thread with ImGui context
void ESP()
{
	APalPlayerCharacter* pPalCharacter = Config.GetPalPlayerCharacter();
	if (!pPalCharacter)
		return;

	UPalShooterComponent* pShootComponent = pPalCharacter->ShooterComponent;
	if (!pShootComponent)
		return;

	APalWeaponBase* pWeapon = pShootComponent->HasWeapon;
	if (pWeapon)
		DrawUActorComponent(pWeapon->InstanceComponents, ImColor(128, 0, 0));

	if (!Config.UCIM)
		return;

	TArray<SDK::APalCharacter*> T = {};
	Config.UCIM->GetAllPalCharacter(&T);
	if (!T.IsValid())
		return;

	for (int i = 0; i < T.Num(); i++)
		ImGui::GetBackgroundDrawList()->AddText(nullptr, 16, ImVec2(10, 10 + (i * 30)), ImColor(128,0,0), T[i]->GetFullName().c_str());
}

FGuid genGuid() {
	SDK::UKismetGuidLibrary* lib = SDK::UKismetGuidLibrary::GetDefaultObj();
	if (!lib) { return FGuid({ 0,0,0,0 }); }
	else { return lib->NewGuid(); }
}
// credit: xCENTx
//	draws debug information for the input actor array
//	should only be called from a GUI thread with ImGui context
void ESP_DEBUG(float mDist, ImVec4 color, UClass* mEntType)
{
	APalPlayerCharacter* pLocalPlayer = Config.GetPalPlayerCharacter();
	if (!pLocalPlayer)
		return;

	APalPlayerController* pPlayerController = static_cast<APalPlayerController*>(pLocalPlayer->Controller);
	if (!pPlayerController)
		return;

	std::vector<AActor*> actors;
	if (!config::GetAllActorsofType(mEntType, &actors, true,true))
		return;
	
	auto draw = ImGui::GetBackgroundDrawList();

	__int32 actorsCount = actors.size();
	for (AActor* actor : actors)
	{
		FVector actorLocation = actor->K2_GetActorLocation();
		FVector localPlayerLocation = pLocalPlayer->K2_GetActorLocation();
		float distanceTo = GetDistanceToActor(pLocalPlayer, actor);
		if (distanceTo > mDist)
			continue;

		FVector2D outScreen;
		if (!pPlayerController->ProjectWorldLocationToScreen(actorLocation, &outScreen, true))
			continue;

		char data[0x256];
		const char* StringData = "OBJECT: [%s]\nCLASS: [%s]\nPOSITION: { %0.0f, %0.0f, %0.0f }\nDISTANCE: [%.0fm]";
		if (distanceTo >= 1000.f)
		{
			distanceTo /= 1000.f;
			StringData = "OBJECT: [%s]\nCLASS: [%s]\nPOSITION: { %0.0f, %0.0f, %0.0f }\nDISTANCE: [%.0fkm]";
		}
		sprintf_s(data, StringData, actor->GetName().c_str(), actor->Class->GetFullName().c_str(), actorLocation.X, actorLocation.Y, actorLocation.Z, distanceTo);

		ImVec2 screen = ImVec2(static_cast<float>(outScreen.X), static_cast<float>(outScreen.Y));
		draw->AddText(screen, ImColor(color), data);
	}
}

//	should only be called from a GUI thread with ImGui context
void DrawUActorComponent(TArray<UActorComponent*> Comps,ImColor color)
{
	ImGui::GetBackgroundDrawList()->AddText(nullptr, 16, ImVec2(ImGui::GetIO().DisplaySize.x / 2, ImGui::GetIO().DisplaySize.y / 2), color, "Drawing...");
	if (!Comps.IsValid())
		return; 
	for (int i = 0; i < Comps.Num(); i++)
	{
		
		if (!Comps[i])
			continue;

		ImGui::GetBackgroundDrawList()->AddText(nullptr, 16, ImVec2(10, 10 + (i * 30)), color, Comps[i]->GetFullName().c_str());
	}
}

//	credit: 
void UnlockAllEffigies()
{
	APalPlayerController* pPalController = Config.GetPalPlayerController();
	if (!pPalController)
		return;
	UWorld* world = Config.GetUWorld();
	if (!world) return;
	SDK::TArray<SDK::ULevel*> pLevelsArray = world->Levels;
	__int32 levelsCount = pLevelsArray.Num();
	for (int i = 0; i < levelsCount; i++)
	{
		if (!pLevelsArray.IsValidIndex(i))
			continue;
		SDK::ULevel* pLevel = pLevelsArray[i];
		if (!pLevel)
			continue;
		SDK::TArray<SDK::AActor*> pActorsArray = pLevelsArray[i]->Actors;
		__int32 actorsCount = pActorsArray.Num();
		for (int j = 0; j < actorsCount; j++)
		{
			if (!pActorsArray.IsValidIndex(j))
				continue;
			SDK::AActor* pActor = pActorsArray[j];
			if (!pActor || !pActor->IsA(APalLevelObjectRelic::StaticClass()))
				continue;
			APalLevelObjectObtainable* relic = (APalLevelObjectObtainable*)pActor;
			pPalController->Transmitter->Player->RequestObtainLevelObject_ToServer(relic);
		}
	}
}

// UPalWorldMapUIData tracks each map's fog-of-war "mask" as a render target texture;
// RemoveMaskByLocation/RemoveMaskByTexture (what the game itself calls as you walk
// around) subtract from that mask to reveal terrain. Clearing the whole render
// target does the same for the entire map at once. Purely local rendering, no
// server call - one-shot, call on button press, not every tick, since each
// call is a real GPU clear.
// Not sure which clear color the shader treats as "revealed" - transparent
// black (0,0,0,0) is the best guess (mask value = fog amount, zeroing it
// should mean no fog). If the map looks more hidden instead, try
// FLinearColor{1,1,1,1}.
void RevealWholeMap()
{
	APalPlayerCharacter* appc = Config.GetPalPlayerCharacter();
	if (appc == nullptr)
		return;

	UPalWorldMapUIData* mapData = UPalUtility::GetLocalWorldMapData(appc);
	if (mapData == nullptr)
		return;

	TArray<FName> mapNames;
	mapData->GetAllWorldMapNames(&mapNames);

	for (int i = 0; i < mapNames.Num(); i++)
	{
		UTextureRenderTarget2D* rt = mapData->GetRenderTargetByMapName(mapNames[i]);
		if (rt != nullptr)
			UKismetRenderingLibrary::ClearRenderTarget2D(appc, rt, FLinearColor{ 0.0f, 0.0f, 0.0f, 0.0f });
	}
}

// AddItem_ServerInternal returns EPalItemOperationResult - actually check and log
// it instead of firing blind, so a silent failure (full inventory, stack cap, item
// not spawnable this way, etc.) shows up in the console instead of just "nothing
// happened".
const char* ItemOpResultToString(SDK::EPalItemOperationResult r)
{
	switch (r)
	{
	case SDK::EPalItemOperationResult::Success: return "Success";
	case SDK::EPalItemOperationResult::SuccessNoOperation: return "SuccessNoOperation (already at target?)";
	case SDK::EPalItemOperationResult::FailedTerminatedManager: return "FailedTerminatedManager";
	case SDK::EPalItemOperationResult::FailedNotExistsInventoryData: return "FailedNotExistsInventoryData";
	case SDK::EPalItemOperationResult::FailedContainerOverflowSlotNum: return "FailedContainerOverflowSlotNum (inventory full)";
	case SDK::EPalItemOperationResult::FailedContainerItemInfoOverSlotNum: return "FailedContainerItemInfoOverSlotNum";
	case SDK::EPalItemOperationResult::FailedContainerOverflowItemsInSlot: return "FailedContainerOverflowItemsInSlot (stack cap hit)";
	case SDK::EPalItemOperationResult::FailedContainerNotFoundContainer: return "FailedContainerNotFoundContainer";
	case SDK::EPalItemOperationResult::FailedContainerNotFoundSlot: return "FailedContainerNotFoundSlot";
	case SDK::EPalItemOperationResult::FailedContainerIsLocalOnly: return "FailedContainerIsLocalOnly";
	case SDK::EPalItemOperationResult::FailedContainerNotEqualsId: return "FailedContainerNotEqualsId";
	case SDK::EPalItemOperationResult::FailedCreateDynamicItemData: return "FailedCreateDynamicItemData";
	case SDK::EPalItemOperationResult::FailedNoDynamicItemIds: return "FailedNoDynamicItemIds";
	case SDK::EPalItemOperationResult::FailedNotFoundContainer: return "FailedNotFoundContainer";
	case SDK::EPalItemOperationResult::FailedNotFoundSlot: return "FailedNotFoundSlot";
	case SDK::EPalItemOperationResult::FailedNotFoundStaticItemData: return "FailedNotFoundStaticItemData (item can't be spawned this way)";
	case SDK::EPalItemOperationResult::FailedNotEnoughSlotSpace: return "FailedNotEnoughSlotSpace";
	case SDK::EPalItemOperationResult::FailedSameSlotUseProduceAndConsume: return "FailedSameSlotUseProduceAndConsume";
	case SDK::EPalItemOperationResult::FailedNotEnoughConsumes: return "FailedNotEnoughConsumes";
	default: return "Unknown";
	}
}

// The slot-index functions below used to grab InventoryMultiHelper->Containers[0]
// and assume that's the main bag - not guaranteed, registration order isn't fixed
// (slot 0 turned out to be the equipped item, not the bag).
// TryGetContainerFromInventoryType asks for Common by type instead of guessing an index.
static SDK::UPalPlayerInventoryData* GetLocalInventoryDataAndCommonSlots(TArray<SDK::UPalItemSlot*>* outSlots)
{
	APalPlayerCharacter* appc = Config.GetPalPlayerCharacter();
	if (appc == nullptr) return nullptr;
	APalPlayerController* appco = appc->GetPalPlayerController();
	if (appco == nullptr) return nullptr;
	APalPlayerState* apps = appco->GetPalPlayerState();
	if (apps == nullptr) return nullptr;
	SDK::UPalPlayerInventoryData* InventoryData = apps->GetInventoryData();
	if (InventoryData == nullptr) return nullptr;

	SDK::UPalItemContainer* container = nullptr;
	if (!InventoryData->TryGetContainerFromInventoryType(SDK::EPalPlayerInventoryType::Common, &container) || !container)
		return nullptr;

	if (outSlots)
		*outSlots = container->ItemSlotArray;
	return InventoryData;
}

// AddItem_ServerInternal clamps both directions and doesn't survive a
// reload/sort - wrong path. RequestMove_ToServer with a negative Num, moving a
// slot onto itself, grants that amount for free instead (-1 -> +1, -99 -> +99,
// tested on Stone and PalSphere).
//
// UPalNetworkItemComponent::RequestMove_ToServer takes a request GUID plus a
// From list and a To slot; here both are the same slot, only Num differs.
// TArray::Add() no-ops on a fresh TArray (no backing buffer - Palworld's TArray
// only grows into pre-allocated slack), so the single-element array is built
// directly via the (Data, Num, Max) constructor over a stack local instead.
SDK::EPalItemOperationResult IncrementInventoryItemCountByIndex(__int32 mCount, __int32 mIndex)
{
	TArray<SDK::UPalItemSlot*> upisa;
	if (GetLocalInventoryDataAndCommonSlots(&upisa) == nullptr) return SDK::EPalItemOperationResult::FailedNotExistsInventoryData;
	if (!upisa.IsValidIndex(mIndex) || !upisa[mIndex]) return SDK::EPalItemOperationResult::FailedNotFoundSlot;

	SDK::FName staticId = upisa[mIndex]->ItemId.StaticId;
	if (staticId.IsNone()) return SDK::EPalItemOperationResult::FailedNotFoundStaticItemData;

	APalPlayerController* appco = Config.GetPalPlayerController();
	if (appco == nullptr || appco->Transmitter == nullptr || appco->Transmitter->Item == nullptr)
	{
		DX11_Base::g_Console->printdbg("[DupeItem] slot %d (%s): no Transmitter->Item component available\n", DX11_Base::Console::Colors::red, mIndex, staticId.ToString().c_str());
		return SDK::EPalItemOperationResult::FailedTerminatedManager;
	}

	SDK::FPalItemSlotId slotId;
	slotId.ContainerId = upisa[mIndex]->ContainerId;
	slotId.SlotIndex = mIndex;

	SDK::FPalItemSlotIdAndNum fromEntry;
	fromEntry.SlotId = slotId;
	fromEntry.Num = -mCount; // negative move-onto-self grants +mCount for free

	TArray<SDK::FPalItemSlotIdAndNum> froms(&fromEntry, 1, 1);

	SDK::FGuid requestId = genGuid();
	appco->Transmitter->Item->RequestMove_ToServer(requestId, slotId, froms);

	// RequestMove_ToServer is fire-and-forget (void) - no result code to report,
	// unlike AddItem_ServerInternal. Success here just means "the request was sent".
	return SDK::EPalItemOperationResult::Success;
}

//
void FreezeSlotItemCount(__int32 mIndex, __int32 mTargetCount)
{
	TArray<SDK::UPalItemSlot*> upisa;
	if (GetLocalInventoryDataAndCommonSlots(&upisa) == nullptr) return;
	if (!upisa.IsValidIndex(mIndex) || !upisa[mIndex]) return;

	int currentCount = upisa[mIndex]->StackCount;
	if (currentCount >= mTargetCount || currentCount <= 0)
		return; // don't top up genuinely empty slots - nothing there to "freeze"

	IncrementInventoryItemCountByIndex(mTargetCount - currentCount, mIndex);
}

//
void FreezeAllPalSpheres(__int32 mTargetCount)
{
	TArray<SDK::UPalItemSlot*> upisa;
	if (GetLocalInventoryDataAndCommonSlots(&upisa) == nullptr) return;

	for (int i = 0; i < upisa.Num(); i++)
	{
		if (!upisa[i] || upisa[i]->StackCount <= 0)
			continue;

		std::string staticId = upisa[i]->ItemId.StaticId.ToString();
		if (staticId.rfind("PalSphere", 0) != 0)
			continue; // not a sphere-type item, skip

		if (upisa[i]->StackCount < mTargetCount)
			IncrementInventoryItemCountByIndex(mTargetCount - upisa[i]->StackCount, i);
	}
}

// Same slot-lookup as IncrementInventoryItemCountByIndex, then re-adds the exact
// current stack size - a clean "duplicate" via the proven AddItem_ServerInternal
// path instead of the old raw ASM hook (which register-logging showed was hitting
// an unrelated function - rcx/rdx/r8/r9 stayed constant across 394 hits, meaning
// it wasn't being called with per-item data at all).
void DuplicateItemInSlot(__int32 mIndex)
{
	TArray<SDK::UPalItemSlot*> upisa;
	if (GetLocalInventoryDataAndCommonSlots(&upisa) == nullptr) return;
	if (!upisa.IsValidIndex(mIndex) || !upisa[mIndex] || upisa[mIndex]->StackCount <= 0) return;

	IncrementInventoryItemCountByIndex(upisa[mIndex]->StackCount, mIndex);
}

// Runs DuplicateItemInSlot over every non-empty slot in one pass. Snapshots the
// slot list first since AddItem_ServerInternal can shuffle slot contents around
// (merges into existing stacks elsewhere) - iterating the live array while
// mutating it could skip or double-hit slots.
void DuplicateAllSlots()
{
	TArray<SDK::UPalItemSlot*> upisa;
	if (GetLocalInventoryDataAndCommonSlots(&upisa) == nullptr) return;

	std::vector<std::pair<int, int>> snapshot; // (slotIndex, stackCountAtSnapshotTime)
	for (int i = 0; i < upisa.Num(); i++)
		if (upisa[i] && upisa[i]->StackCount > 0)
			snapshot.emplace_back(i, upisa[i]->StackCount);

	for (const auto& [index, count] : snapshot)
		IncrementInventoryItemCountByIndex(count, index);
}

// UPalNetworkPlayerComponent::Dev_ForceRespawnNear*Spawners_ToServer are real
// "Dev_..._ToServer" RPCs left in the shipping game (found by grepping the SDK for
// "Respawn" - genuine developer cheat commands, not a guess). Fetched via the
// standard AActor::GetComponentByClass reflection call.
void ForceRespawnNearby(float mRadiusCM)
{
	APalPlayerCharacter* appc = Config.GetPalPlayerCharacter();
	if (appc == nullptr) return;

	auto* comp = (SDK::UPalNetworkPlayerComponent*)appc->GetComponentByClass(SDK::UPalNetworkPlayerComponent::StaticClass());
	if (comp == nullptr) return;

	SDK::FVector playerLocation = appc->K2_GetActorLocation();
	comp->Dev_ForceRespawnNearItemSpawners_ToServer(playerLocation, mRadiusCM);
	comp->Dev_ForceRespawnNearSpawners_ToServer(playerLocation, mRadiusCM);
}

//
std::vector<std::string> GetInventorySlotOverview()
{
	std::vector<std::string> result;

	TArray<SDK::UPalItemSlot*> upisa;
	if (GetLocalInventoryDataAndCommonSlots(&upisa) == nullptr) return result;

	for (int i = 0; i < upisa.Num(); i++)
	{
		if (!upisa[i] || upisa[i]->StackCount <= 0)
			continue;

		char line[128];
		sprintf_s(line, "[%d] %s x%d", i, upisa[i]->ItemId.StaticId.ToString().c_str(), upisa[i]->StackCount);
		result.push_back(line);
	}

	return result;
}

// AddItem_ServerInternal isn't a Net/NetServer RPC (Final, Native, Public,
// BlueprintCallable - no networking), so a client/server desync isn't why
// duped items vanish on reload. More likely the autosave interval just
// hasn't run yet, so nothing from the current playthrough has hit disk.
// UPalCheatManager::DebugSaveWorldData is a real "Debug_"-style save trigger -
// forces an immediate save under saveName so duped items actually reach disk.
void ForceSaveWorld(const char* saveName)
{
	APalPlayerController* appco = Config.GetPalPlayerController();
	if (appco == nullptr) return;

	auto* cheatManager = (SDK::UPalCheatManager*)appco->CheatManager;
	if (cheatManager == nullptr)
	{
		DX11_Base::g_Console->printdbg("[ForceSave] no CheatManager on the local player controller\n", DX11_Base::Console::Colors::red);
		return;
	}

	wchar_t ws[255];
	swprintf(ws, 255, L"%hs", saveName);
	cheatManager->DebugSaveWorldData(SDK::FString(ws));
	DX11_Base::g_Console->printdbg("[ForceSave] DebugSaveWorldData(\"%s\") called - check if items survive a reload from that slot\n", DX11_Base::Console::Colors::yellow, saveName);
}

//	--- Pal Editor (Team) ---
std::vector<std::string> GetPartyPalOverview()
{
	std::vector<std::string> result;

	std::vector<SDK::UPalIndividualCharacterHandle*> handles;
	if (!Config.GetPartyPalHandles(&handles))
		return result;

	for (size_t i = 0; i < handles.size(); i++)
	{
		SDK::UPalIndividualCharacterParameter* param = handles[i]->TryGetIndividualParameter();
		if (!param)
			continue;

		auto& save = param->SaveParameter;
		std::string name = save.NickName.ToString();
		if (name.empty())
			name = save.CharacterID.ToString();

		char line[160];
		sprintf_s(line, "[%zu] %s Lv.%d HP:%lld/%lld", i, name.c_str(), save.Level, save.Hp.Value, save.MaxHP.Value);
		result.push_back(line);
	}

	return result;
}

bool LoadSelectedPartyPalStats(__int32 mIndex)
{
	std::vector<SDK::UPalIndividualCharacterHandle*> handles;
	if (!Config.GetPartyPalHandles(&handles))
		return false;
	if (mIndex < 0 || (size_t)mIndex >= handles.size())
		return false;

	SDK::UPalIndividualCharacterParameter* param = handles[mIndex]->TryGetIndividualParameter();
	if (!param)
		return false;

	auto& save = param->SaveParameter;
	Config.EditPalLevel = save.Level;
	Config.EditPalRank = save.Rank;
	Config.EditPalRankHP = save.Rank_HP;
	Config.EditPalRankAttack = save.Rank_Attack;
	Config.EditPalRankDefense = save.Rank_Defence;
	Config.EditPalRankCraftSpeed = save.Rank_CraftSpeed;
	Config.EditPalTalentHP = save.Talent_HP;
	Config.EditPalTalentMelee = save.Talent_Melee;
	Config.EditPalTalentShot = save.Talent_Shot;
	Config.EditPalTalentDefense = save.Talent_Defense;
	return true;
}

void ApplySelectedPartyPalStats(__int32 mIndex)
{
	std::vector<SDK::UPalIndividualCharacterHandle*> handles;
	if (!Config.GetPartyPalHandles(&handles))
		return;
	if (mIndex < 0 || (size_t)mIndex >= handles.size())
		return;

	SDK::UPalIndividualCharacterParameter* param = handles[mIndex]->TryGetIndividualParameter();
	if (!param)
		return;

	auto& save = param->SaveParameter;
	save.Level = (uint8)Config.EditPalLevel;
	save.Rank = (uint8)Config.EditPalRank;
	save.Rank_HP = (uint8)Config.EditPalRankHP;
	save.Rank_Attack = (uint8)Config.EditPalRankAttack;
	save.Rank_Defence = (uint8)Config.EditPalRankDefense;
	save.Rank_CraftSpeed = (uint8)Config.EditPalRankCraftSpeed;
	save.Talent_HP = (uint8)Config.EditPalTalentHP;
	save.Talent_Melee = (uint8)Config.EditPalTalentMelee;
	save.Talent_Shot = (uint8)Config.EditPalTalentShot;
	save.Talent_Defense = (uint8)Config.EditPalTalentDefense;
}

void FullHealPartyPal(__int32 mIndex)
{
	std::vector<SDK::UPalIndividualCharacterHandle*> handles;
	if (!Config.GetPartyPalHandles(&handles))
		return;
	if (mIndex < 0 || (size_t)mIndex >= handles.size())
		return;

	SDK::UPalIndividualCharacterParameter* param = handles[mIndex]->TryGetIndividualParameter();
	if (!param)
		return;

	param->SaveParameter.Hp.Value = param->SaveParameter.MaxHP.Value;
}

std::vector<std::string> GetSelectedPartyPalPassives(__int32 mIndex)
{
	std::vector<std::string> result;

	std::vector<SDK::UPalIndividualCharacterHandle*> handles;
	if (!Config.GetPartyPalHandles(&handles)) return result;
	if (mIndex < 0 || (size_t)mIndex >= handles.size()) return result;

	SDK::UPalIndividualCharacterParameter* param = handles[mIndex]->TryGetIndividualParameter();
	if (!param) return result;

	auto& list = param->SaveParameter.PassiveSkillList;
	for (int i = 0; i < list.Num(); i++)
		result.push_back(list[i].ToString());

	return result;
}

// Note: TArray here is a thin wrapper around the game's own allocated buffer, not
// a dynamically-growable container - Add() silently fails once the underlying
// array has no free slack. Won't always work depending on how many passives the
// pal already has room for.
void AddPassiveToPartyPal(__int32 mIndex, const char* passiveName)
{
	std::vector<SDK::UPalIndividualCharacterHandle*> handles;
	if (!Config.GetPartyPalHandles(&handles)) return;
	if (mIndex < 0 || (size_t)mIndex >= handles.size()) return;

	SDK::UPalIndividualCharacterParameter* param = handles[mIndex]->TryGetIndividualParameter();
	if (!param) return;

	static SDK::UKismetStringLibrary* lib = SDK::UKismetStringLibrary::GetDefaultObj();
	wchar_t ws[255];
	swprintf(ws, 255, L"%hs", passiveName);
	SDK::FName newName = lib->Conv_StringToName(SDK::FString(ws));

	param->SaveParameter.PassiveSkillList.Add(newName);
}

void RemovePassiveFromPartyPal(__int32 mIndex, const char* passiveName)
{
	std::vector<SDK::UPalIndividualCharacterHandle*> handles;
	if (!Config.GetPartyPalHandles(&handles)) return;
	if (mIndex < 0 || (size_t)mIndex >= handles.size()) return;

	SDK::UPalIndividualCharacterParameter* param = handles[mIndex]->TryGetIndividualParameter();
	if (!param) return;

	static SDK::UKismetStringLibrary* lib = SDK::UKismetStringLibrary::GetDefaultObj();
	wchar_t ws[255];
	swprintf(ws, 255, L"%hs", passiveName);
	SDK::FName targetName = lib->Conv_StringToName(SDK::FString(ws));

	auto& list = param->SaveParameter.PassiveSkillList;
	for (int i = 0; i < list.Num(); i++)
	{
		if (list[i] == targetName)
		{
			list.Remove(i);
			break;
		}
	}
}

//
void AddItemToInventoryByName(UPalPlayerInventoryData* data, char* itemName, int count)
{
	// obtain lib instance
	static UKismetStringLibrary* lib = UKismetStringLibrary::GetDefaultObj();

	// Convert FNAME
	wchar_t  ws[255];
	swprintf(ws, 255, L"%hs", itemName);
	FName Name = lib->Conv_StringToName(FString(ws));

	// Call
	data->AddItem_ServerInternal(Name, count, true, 0.0f, true);
}

// Broken: attempts to grant a real item via
// UPalNetworkPlayerComponent::RequestAddItem_ToServer, a genuine _ToServer RPC
// unlike AddItem_ServerInternal above, reached via Transmitter->Player. The
// call goes through fine (fire-and-forget, no error to check) but the item
// never shows up as real/usable. So "_ToServer = real, _ServerInternal =
// ghost" is too simple a rule: RequestMove_ToServer and
// RequestUseItemToCharacter_ToServer operate on a slot that already has real
// server-tracked data, but creating a brand new item from nothing apparently
// needs an actual drop/loot/craft/purchase event, not just any RPC with the
// right name. Kept for reference, not called from the UI.
SDK::EPalItemOperationResult GiveRealItemByName(const char* itemName, __int32 mCount)
{
	APalPlayerController* appco = Config.GetPalPlayerController();
	if (appco == nullptr || appco->Transmitter == nullptr || appco->Transmitter->Player == nullptr)
		return SDK::EPalItemOperationResult::FailedTerminatedManager;

	static UKismetStringLibrary* lib = UKismetStringLibrary::GetDefaultObj();
	if (lib == nullptr)
		return SDK::EPalItemOperationResult::FailedTerminatedManager;

	wchar_t ws[255];
	swprintf(ws, 255, L"%hs", itemName);
	FName staticId = lib->Conv_StringToName(FString(ws));

	appco->Transmitter->Player->RequestAddItem_ToServer(staticId, mCount, false);
	return SDK::EPalItemOperationResult::Success;
}

// Credit: asashi
void SpawnMultiple_ItemsToInventory(config::QuickItemSet Set)
{
	SDK::UPalPlayerInventoryData* InventoryData = Config.GetPalPlayerCharacter()->GetPalPlayerController()->GetPalPlayerState()->GetInventoryData();
	switch (Set)
	{
	case 0:
		for (int i = 0; i < IM_ARRAYSIZE(database::basic_items_stackable); i++)
			AddItemToInventoryByName(InventoryData, _strdup(database::basic_items_stackable[i].c_str()), 100);
		break;
	case 1:
		for (int i = 0; i < IM_ARRAYSIZE(database::basic_items_single); i++)
			AddItemToInventoryByName(InventoryData, _strdup(database::basic_items_single[i].c_str()), 1);
		break;
	case 2:
		for (int i = 0; i < IM_ARRAYSIZE(database::pal_unlock_skills); i++)
			AddItemToInventoryByName(InventoryData, _strdup(database::pal_unlock_skills[i].c_str()), 1);
		break;
	case 3:
		for (int i = 0; i < IM_ARRAYSIZE(database::spheres); i++)
			AddItemToInventoryByName(InventoryData, _strdup(database::spheres[i].c_str()), 100);
		break;
	case 4:
		for (int i = 0; i < IM_ARRAYSIZE(database::tools); i++)
			AddItemToInventoryByName(InventoryData, _strdup(database::tools[i].c_str()), 1);
		break;
	default:
		break;
	}
}

//	
void AnyWhereTP(FVector& vector, bool IsSafe)
{
	APalPlayerState* pPalPlayerState = Config.GetPalPlayerState();
	APalPlayerController* pPalPlayerController = Config.GetPalPlayerController();
	if (!pPalPlayerController || !pPalPlayerState)
		return;

	vector = { vector.X,vector.Y + 100,vector.Z };
	FQuat pRotation = FQuat({0,0,0,1});
	FGuid guid = pPalPlayerController->GetPlayerUId();
	pPalPlayerController->Transmitter->Player->RegisterRespawnPoint_ToServer(guid, vector, pRotation);
	pPalPlayerState->RequestRespawn();
}

// database::locationMap (the hardcoded boss-location list in the Teleporter tab)
// isn't sourced from the game at all - it's a fixed set of coordinates from an
// external source (a community wiki/guide), so it can go stale or miss anything
// added in later updates. This instead scans the LIVE world for real fast-travel
// point actors (APalLevelObjectUnlockableFastTravelPoint) - the same actor class
// the game's own fast-travel menu reads from - and returns their real
// FastTravelPointID + current location + unlock state directly, so the list is
// always accurate for whatever's actually in the loaded world. FastTravelPointID
// is a raw internal FName, not a display string, so it's run through
// database::fastTravelPointNames for a friendly label - falls back to the raw
// ID for anything not yet in that map (see database.h, currently empty).
std::vector<std::pair<std::string, FVector>> GetFastTravelPoints()
{
	std::vector<std::pair<std::string, FVector>> result;

	std::vector<AActor*> actors;
	if (!config::GetAllActorsofType(APalLevelObjectUnlockableFastTravelPoint::StaticClass(), &actors, true, false))
		return result;

	for (AActor* actor : actors)
	{
		auto* point = static_cast<APalLevelObjectUnlockableFastTravelPoint*>(actor);
		std::string rawId = point->FastTravelPointID.ToString();
		auto it = database::fastTravelPointNames.find(rawId);
		std::string label = (it != database::fastTravelPointNames.end()) ? it->second : rawId;
		if (!point->IsUnlocked())
			label += " (locked)";
		result.emplace_back(label, point->K2_GetActorLocation());
	}
	return result;
}

// Every APalDungeonEntrance found is now listed unconditionally - the earlier
// "only if TryGetDungeonInstanceModel() succeeds" filter came back still
// empty in-game even at a real entrance, which points at most Palworld
// dungeons never having an instance model at all (that machinery looks to be
// for timed/raid-style dungeons specifically; regular field dungeons are just
// always-there, no active/inactive state). Requiring one made the list
// useless for the common case. Uses GetWarpPoint() (the entrance's own
// designated entry point) rather than the entrance actor's raw location,
// since that's the exact point the game's own "enter dungeon" flow would
// place you at. When an instance model IS present (raid-style), the label is
// enriched with the real dungeon name/level and remaining minutes before it
// disappears; otherwise it just says "Dungeon Entrance".
std::vector<std::pair<std::string, FVector>> GetActiveDungeonEntrances()
{
	std::vector<std::pair<std::string, FVector>> result;

	std::vector<AActor*> actors;
	if (!config::GetAllActorsofType(APalDungeonEntrance::StaticClass(), &actors, true, false))
		return result;

	APalPlayerCharacter* appc = Config.GetPalPlayerCharacter();

	for (AActor* actor : actors)
	{
		auto* entrance = static_cast<APalDungeonEntrance*>(actor);

		std::string label = "Dungeon Entrance";

		UPalDungeonInstanceModel* instanceModel = nullptr;
		if (entrance->TryGetDungeonInstanceModel(&instanceModel) && instanceModel && appc)
		{
			std::string name = UKismetTextLibrary::Conv_TextToString(instanceModel->GetDungeonNameText()).ToString();
			float remainSeconds = UPalDungeonInstanceModel::CalcDisappearRemainSeconds(appc, instanceModel->GetDisappearTimeAt());
			int remainMinutes = remainSeconds > 0.0f ? static_cast<int>(remainSeconds / 60.0f) : 0;

			char buf[128];
			sprintf_s(buf, "%s Lv.%d (%dmin left)", name.c_str(), instanceModel->GetLevel(), remainMinutes);
			label = buf;
		}

		result.emplace_back(label, entrance->GetWarpPoint().Translation);
	}
	return result;
}

//	
void ExploitFly(bool IsFly)
{
	SDK::APalPlayerCharacter* pPalPlayerCharacter = Config.GetPalPlayerCharacter();
	if (!pPalPlayerCharacter)
		return;

	APalPlayerController* pPalPlayerController = pPalPlayerCharacter->GetPalPlayerController();
	if (!pPalPlayerController)
		return;

	IsFly ? pPalPlayerController->StartFlyToServer() : pPalPlayerController->EndFlyToServer();
}

// StartFlyToServer only sets the movement mode to Flying and handles
// horizontal input (normal WASD), but has no vertical input bound
// (no jump-to-ascend/crouch-to-descend) - without this the player just hovers
// at whatever height fly was toggled at. Call every tick while fly is toggled
// on; nudges the actor's Z location on Space (up) and C (down) - not Left
// Ctrl, that's already roll/dodge. Fixed speed, no need to make it adjustable.
static constexpr float kFlyVerticalSpeed = 20.0f;
void UpdateFlyVerticalMovement()
{
	bool up = (GetAsyncKeyState(VK_SPACE) & 0x8000) != 0;
	bool down = (GetAsyncKeyState('C') & 0x8000) != 0;
	if (!up && !down)
		return;

	SDK::APalPlayerCharacter* pPalPlayerCharacter = Config.GetPalPlayerCharacter();
	if (!pPalPlayerCharacter)
		return;

	FVector loc = pPalPlayerCharacter->K2_GetActorLocation();
	loc.Z += up ? kFlyVerticalSpeed : -kFlyVerticalSpeed;
	pPalPlayerCharacter->K2_SetActorLocation(loc, false, nullptr, true);
}

//	
void SpeedHack(float mSpeed)
{
	auto player_controller = Config.GetPalPlayerController();
	if (!player_controller)
		return;

	auto acknowledged_pawn = player_controller->AcknowledgedPawn;
	if (!acknowledged_pawn)
		return;

	acknowledged_pawn->CustomTimeDilation = mSpeed;
}

// Alternative to SpeedHack() above. CustomTimeDilation scales the whole
// actor's time (animations, cooldowns, everything), which is why SpeedHack
// looks like a sped-up cartoon. This raises
// UCharacterMovementComponent::MaxWalkSpeed directly instead, so only ground
// movement changes. Separate from SpeedHack(), doesn't touch
// CustomTimeDilation. Captures the real MaxWalkSpeed once as the 1.0x
// baseline, then scales from there; restores it when disabled.
void SpeedHackClean(bool bEnable, float mMultiplier)
{
	APalPlayerCharacter* pPalPlayerCharacter = Config.GetPalPlayerCharacter();
	if (!pPalPlayerCharacter)
		return;

	UCharacterMovementComponent* movement = pPalPlayerCharacter->CharacterMovement;
	if (!movement)
		return;

	static float s_baseMaxWalkSpeed = 0.0f;
	static bool s_hasCapturedBase = false;
	if (!s_hasCapturedBase)
	{
		s_baseMaxWalkSpeed = movement->MaxWalkSpeed;
		s_hasCapturedBase = true;
	}

	movement->MaxWalkSpeed = bEnable ? (s_baseMaxWalkSpeed * mMultiplier) : s_baseMaxWalkSpeed;
}

// "All in one" version of SpeedHackClean - MaxWalkSpeed alone doesn't affect
// swimming or climbing, and not flying either (reads MaxFlySpeed instead),
// since each movement mode in UE's CharacterMovementComponent has its own
// max-speed field. Scales all four (Walk/Swim/Fly/Custom - Custom is
// presumably what climbing uses) together so speed stays consistent across
// every mode. Same capture-once-then-restore pattern as SpeedHackClean.
//
// Scaling the player's own CharacterMovement does nothing while riding a
// flying Pal - MovementMode reads back as 5 (Flying) with MaxFlySpeed scaled
// correctly, but CurrentSpeed stays low, because while mounted the player pawn
// is just visually attached; the ridden Pal's own CharacterMovementComponent
// drives velocity. UPalUtility::GetRidePal() returns that mount (nullptr if
// not riding) - it's an APalCharacter like the player so it has the same
// CharacterMovement field. The mount changes across mount/dismount, so its
// base speeds get re-captured whenever the tracked ride actor pointer changes,
// instead of the player's "capture once ever" static.
void SpeedHackAllInOne(bool bEnable, float mMultiplier)
{
	APalPlayerCharacter* pPalPlayerCharacter = Config.GetPalPlayerCharacter();
	if (!pPalPlayerCharacter)
		return;

	UCharacterMovementComponent* movement = pPalPlayerCharacter->CharacterMovement;
	if (!movement)
		return;

	static float s_baseWalk = 0.0f, s_baseSwim = 0.0f, s_baseFly = 0.0f, s_baseCustom = 0.0f;
	static bool s_hasCapturedBase = false;
	if (!s_hasCapturedBase)
	{
		s_baseWalk = movement->MaxWalkSpeed;
		s_baseSwim = movement->MaxSwimSpeed;
		s_baseFly = movement->MaxFlySpeed;
		s_baseCustom = movement->MaxCustomMovementSpeed;
		s_hasCapturedBase = true;
	}

	movement->MaxWalkSpeed = bEnable ? (s_baseWalk * mMultiplier) : s_baseWalk;
	movement->MaxSwimSpeed = bEnable ? (s_baseSwim * mMultiplier) : s_baseSwim;
	movement->MaxFlySpeed = bEnable ? (s_baseFly * mMultiplier) : s_baseFly;
	movement->MaxCustomMovementSpeed = bEnable ? (s_baseCustom * mMultiplier) : s_baseCustom;

	APalCharacter* ridePal = UPalUtility::GetRidePal(pPalPlayerCharacter);
	static APalCharacter* s_lastRidePal = nullptr;
	static float s_rideBaseWalk = 0.0f, s_rideBaseSwim = 0.0f, s_rideBaseFly = 0.0f, s_rideBaseCustom = 0.0f;

	if (ridePal && ridePal->CharacterMovement)
	{
		UCharacterMovementComponent* rideMovement = ridePal->CharacterMovement;
		if (ridePal != s_lastRidePal)
		{
			s_rideBaseWalk = rideMovement->MaxWalkSpeed;
			s_rideBaseSwim = rideMovement->MaxSwimSpeed;
			s_rideBaseFly = rideMovement->MaxFlySpeed;
			s_rideBaseCustom = rideMovement->MaxCustomMovementSpeed;
			s_lastRidePal = ridePal;
		}

		rideMovement->MaxWalkSpeed = bEnable ? (s_rideBaseWalk * mMultiplier) : s_rideBaseWalk;
		rideMovement->MaxSwimSpeed = bEnable ? (s_rideBaseSwim * mMultiplier) : s_rideBaseSwim;
		rideMovement->MaxFlySpeed = bEnable ? (s_rideBaseFly * mMultiplier) : s_rideBaseFly;
		rideMovement->MaxCustomMovementSpeed = bEnable ? (s_rideBaseCustom * mMultiplier) : s_rideBaseCustom;
	}
	else
	{
		s_lastRidePal = nullptr;
	}
}

// Diagnostic: dumps the live MovementMode/CustomMovementMode, all four
// Max*Speed fields, and current velocity magnitude to console.
void DumpMovementSpeedState()
{
	DX11_Base::g_Console->Show();

	APalPlayerCharacter* pPalPlayerCharacter = Config.GetPalPlayerCharacter();
	if (!pPalPlayerCharacter)
	{
		DX11_Base::g_Console->printdbg("[MoveDump] no local player\n", DX11_Base::Console::Colors::red);
		return;
	}

	UCharacterMovementComponent* movement = pPalPlayerCharacter->CharacterMovement;
	if (!movement)
	{
		DX11_Base::g_Console->printdbg("[MoveDump] no CharacterMovement component\n", DX11_Base::Console::Colors::red);
		return;
	}

	FVector vel = movement->Velocity;
	float speed = sqrtf(vel.X * vel.X + vel.Y * vel.Y + vel.Z * vel.Z);

	DX11_Base::g_Console->printdbg(
		"[MoveDump] MovementMode=%d CustomMovementMode=%d | MaxWalkSpeed=%.1f MaxSwimSpeed=%.1f MaxFlySpeed=%.1f MaxCustomMovementSpeed=%.1f | CurrentSpeed=%.1f\n",
		DX11_Base::Console::Colors::green,
		(int)movement->MovementMode, (int)movement->CustomMovementMode,
		movement->MaxWalkSpeed, movement->MaxSwimSpeed, movement->MaxFlySpeed, movement->MaxCustomMovementSpeed,
		speed);

	// Riding a Pal drives velocity through the mount's own CharacterMovementComponent,
	// not the player's (see SpeedHackAllInOne) - dump that too when GetRidePal() finds one.
	APalCharacter* ridePal = UPalUtility::GetRidePal(pPalPlayerCharacter);
	if (ridePal && ridePal->CharacterMovement)
	{
		UCharacterMovementComponent* rideMovement = ridePal->CharacterMovement;
		FVector rideVel = rideMovement->Velocity;
		float rideSpeed = sqrtf(rideVel.X * rideVel.X + rideVel.Y * rideVel.Y + rideVel.Z * rideVel.Z);

		DX11_Base::g_Console->printdbg(
			"[MoveDump][RidePal] MovementMode=%d CustomMovementMode=%d | MaxWalkSpeed=%.1f MaxSwimSpeed=%.1f MaxFlySpeed=%.1f MaxCustomMovementSpeed=%.1f | CurrentSpeed=%.1f\n",
			DX11_Base::Console::Colors::green,
			(int)rideMovement->MovementMode, (int)rideMovement->CustomMovementMode,
			rideMovement->MaxWalkSpeed, rideMovement->MaxSwimSpeed, rideMovement->MaxFlySpeed, rideMovement->MaxCustomMovementSpeed,
			rideSpeed);
	}
}


//
void SetDemiGodMode(bool bIsSet)
{
	auto pCharacter = Config.GetPalPlayerCharacter();
	if (!pCharacter)
		return;

	auto pParams = pCharacter->CharacterParameterComponent;
	if (!pParams)
		return;

	auto mIVs = pParams->IndividualParameter;
	if (!mIVs)
		return;

	auto sParams = mIVs->SaveParameter;

	pParams->bIsEnableMuteki = bIsSet;	//	Credit: Mokobake
	if (!bIsSet)
		return;

	//	attempt additional parameters
	//sParams.HP.Value = sParams.MaxHP.Value;
	sParams.MP.Value = sParams.MaxMP.Value;
	sParams.FullStomach = sParams.MaxFullStomach;
	sParams.PhysicalHealth = EPalStatusPhysicalHealthType::Healthful;
	sParams.SanityValue = 100.f;
	sParams.HungerType = EPalStatusHungerType::Default;
}

//	
// When bIsSafe was true (the "Safe Teleport" checkbox's default state), this
// called TeleportToSafePoint_ToServer - an anti-softlock "rescue me, I fell
// out of the world" RPC (bWasOutOfWorld param), not a "go to my respawn
// point" call. Under normal conditions the server just ignores it, so "Home"
// silently did nothing with Safe Teleport checked. Now always uses
// APalPlayerState::RequestRespawn() - the same respawn-at-registered-point
// call AnyWhereTP relies on.
void RespawnLocalPlayer(bool bIsSafe)
{
	APalPlayerState* pPalPlayerState = Config.GetPalPlayerState();
	if (!pPalPlayerState)
		return;

	pPalPlayerState->RequestRespawn();
}

void SetPlayerHealth(float newHealthPercentage)
{
	APalPlayerCharacter* pPalPlayerCharacter = Config.GetPalPlayerCharacter();
	if (!pPalPlayerCharacter)
		return;

	APalPlayerController* pPalPlayerController = pPalPlayerCharacter->GetPalPlayerController();
	if (!pPalPlayerController)
		return;
	UPalCharacterParameterComponent* pParams = pPalPlayerCharacter->CharacterParameterComponent;
	if (!pParams)
		return;

	if (pParams->IsDying())
	{
		pPalPlayerController->Transmitter->CharacterStatusOperation->RequestReviveCharacterFromDying_ToServer(pPalPlayerCharacter);
	}

	if ((pParams->GetHP().Value/ pParams->GetMaxHP().Value) < newHealthPercentage)
	{
		pPalPlayerController->Transmitter->CharacterStatusOperation->RequestReviveCharacterFromDying_ToServer(pPalPlayerCharacter);
	}
}

//	
void ReviveLocalPlayer()
{
	APalPlayerCharacter* pPalPlayerCharacter = Config.GetPalPlayerCharacter();
	if (!pPalPlayerCharacter)
		return;

	APalPlayerController* pPalPlayerController = pPalPlayerCharacter->GetPalPlayerController();
	if (!pPalPlayerController)
		return;

	UPalCharacterParameterComponent* pParams = pPalPlayerCharacter->CharacterParameterComponent;
	if (!pParams)
		return;

	if (pParams->IsDying())
	{
		pParams->ReviveFromDying();
		pPalPlayerController->Transmitter->CharacterStatusOperation->RequestReviveCharacterFromDying_ToServer(pPalPlayerCharacter);
	}

	pPalPlayerController->Transmitter->CharacterStatusOperation->RequestReviveCharacterFromDying_ToServer(pPalPlayerCharacter);
}

//	
void ResetStamina()
{
	APalPlayerCharacter* pPalCharacter = Config.GetPalPlayerCharacter();
	if (!pPalCharacter)
		return;

	UPalCharacterParameterComponent* pParams = pPalCharacter->CharacterParameterComponent;
	if (!pParams)
		return;

	pParams->ResetSP();
}

//	
void GiveExperiencePoints(__int32 mXP)
{
	auto pPalPlayerState = Config.GetPalPlayerState();
	if (!pPalPlayerState)
		return;

	//pPalPlayerState->GrantExpForParty(mXP);
}

//	
void SetPlayerAttackParam(__int32 mNewAtk)
{
	APalPlayerCharacter* pPalPlayerCharacter = Config.GetPalPlayerCharacter();
	if (!pPalPlayerCharacter)
		return;

	UPalCharacterParameterComponent* pParams = pPalPlayerCharacter->CharacterParameterComponent;
	if (!pParams)
		return;

	if (pParams->AttackUp != mNewAtk)
		pParams->AttackUp = mNewAtk;
}

//	
void SetPlayerDefenseParam(__int32 mNewDef)
{
	APalPlayerCharacter* pPalPlayerCharacter = Config.GetPalPlayerCharacter();
	if (!pPalPlayerCharacter)
		return;

	UPalCharacterParameterComponent* pParams = pPalPlayerCharacter->CharacterParameterComponent;
	if (!pParams)
		return;
	
	if (pParams->DefenseUp != mNewDef)
		pParams->DefenseUp = mNewDef;
}

// UPalStaticArmorItemData::DefenseValue is the item TYPE's shared/cached base
// stat - one instance per armor item type (e.g. all "Leather Armor" pieces
// point at the same object), not one per equipped instance. Base value is
// captured once per distinct armor type the first time it's seen (keyed by
// pointer). Restoring only the currently-equipped pieces would leave anything
// swapped out stuck at the boosted value, so disabling restores every armor
// type touched so far, equipped or not.
static std::map<SDK::UPalStaticArmorItemData*, int32> s_baseArmorDefense;
void SetArmorDefenseBoost(float mMultiplier, bool bRestoreDefault)
{
	if (bRestoreDefault)
	{
		for (auto& pair : s_baseArmorDefense)
			pair.first->DefenseValue = pair.second;
		return;
	}

	SDK::UPalPlayerInventoryData* InventoryData = Config.GetInventoryComponent();
	if (!InventoryData)
		return;

	UPalItemContainer* container = nullptr;
	if (!InventoryData->TryGetContainerFromInventoryType(EPalPlayerInventoryType::PlayerEquipArmor, &container) || !container)
		return;

	for (int32 i = 0; i < container->Num(); i++)
	{
		UPalItemSlot* slot = container->Get(i);
		if (!slot || slot->IsEmpty())
			continue;

		UPalStaticItemDataBase* staticData = nullptr;
		if (!slot->TryGetStaticItemData(&staticData) || !staticData)
			continue;

		auto* armorData = static_cast<UPalStaticArmorItemData*>(staticData);

		auto it = s_baseArmorDefense.find(armorData);
		if (it == s_baseArmorDefense.end())
			it = s_baseArmorDefense.emplace(armorData, armorData->DefenseValue).first;

		armorData->DefenseValue = static_cast<int32>(it->second * mMultiplier);
	}
}

// Diagnostic - see feature.h for what this checks.
void DumpArmorDefenseState()
{
	DX11_Base::g_Console->Show();

	APalPlayerCharacter* pPalCharacter = Config.GetPalPlayerCharacter();
	if (!pPalCharacter)
	{
		DX11_Base::g_Console->printdbg("[DefenseDump] no local player\n", DX11_Base::Console::Colors::red);
		return;
	}

	UPalCharacterParameterComponent* pParams = pPalCharacter->CharacterParameterComponent;
	if (!pParams)
	{
		DX11_Base::g_Console->printdbg("[DefenseDump] no CharacterParameterComponent\n", DX11_Base::Console::Colors::red);
		return;
	}

	DX11_Base::g_Console->printdbg("[DefenseDump] GetDefense()=%d DefenseUp=%d\n", DX11_Base::Console::Colors::green, pParams->GetDefense(), pParams->DefenseUp);

	SDK::UPalPlayerInventoryData* InventoryData = Config.GetInventoryComponent();
	if (!InventoryData)
	{
		DX11_Base::g_Console->printdbg("[DefenseDump] no inventory data\n", DX11_Base::Console::Colors::red);
		return;
	}

	UPalItemContainer* container = nullptr;
	if (!InventoryData->TryGetContainerFromInventoryType(EPalPlayerInventoryType::PlayerEquipArmor, &container) || !container)
	{
		DX11_Base::g_Console->printdbg("[DefenseDump] no PlayerEquipArmor container\n", DX11_Base::Console::Colors::red);
		return;
	}

	for (int32 i = 0; i < container->Num(); i++)
	{
		UPalItemSlot* slot = container->Get(i);
		if (!slot || slot->IsEmpty())
			continue;

		UPalStaticItemDataBase* staticData = nullptr;
		if (!slot->TryGetStaticItemData(&staticData) || !staticData)
		{
			DX11_Base::g_Console->printdbg("[DefenseDump] slot %d (%s): no static item data\n", DX11_Base::Console::Colors::yellow, i, slot->ItemId.StaticId.ToString().c_str());
			continue;
		}

		auto* armorData = static_cast<UPalStaticArmorItemData*>(staticData);
		DX11_Base::g_Console->printdbg("[DefenseDump] slot %d (%s): DefenseValue=%d\n", DX11_Base::Console::Colors::green, i, slot->ItemId.StaticId.ToString().c_str(), armorData->DefenseValue);
	}
}

//
void SetInfiniteAmmo(bool bInfAmmo)
{
	if (!bInfAmmo) return;
	SDK::APalPlayerCharacter* localPlayer = Config.localPlayer;
	if (localPlayer == nullptr) return;
	SDK::UPalShooterComponent* upsc = localPlayer->ShooterComponent;
	if (upsc == nullptr) return;
	SDK::APalWeaponBase* apwb = upsc->GetHasWeapon();
	if (apwb == nullptr) return;
	// Note: removed the CanShoot() gate here - it returned false for throw-type
	// weapons (Pal Spheres), which skipped setting the flag below and meant
	// infinite ammo never applied to spheres, only regular firearms.
	apwb->IsRequiredBullet = false;
	apwb->IsRequiredBulletForAltFire = false;

	// IsInfinityMagazine is a real developer flag directly on the weapon actor
	// (found in the SDK, offset 0x04F8) - much more likely to be the actual thing
	// the ammo-consume logic checks than trying to force IsFullMagazine (a
	// computed getter that only gates reloading, not per-shot consumption, which
	// is why the ExecFunction hook on IsFullMagazine alone didn't stop ammo from
	// depleting when firing).
	apwb->IsInfinityMagazine = true;

	// SetBulletsNum(999) never sticks on the authoritative side per
	// DumpWeaponAmmoState - dynamicData->RemainingBullets reads back as 1 no
	// matter how often it's called. Only GetRemainBulletCount() (a separate,
	// locally-cached getter) reflects the write. A Net-replicated field just
	// doesn't take a plain reflection write. Kept anyway since it's harmless
	// and might help the displayed value, but it's not the actual fix.
	SDK::UPalDynamicWeaponItemDataBase* dynamicData = apwb->TryGetDynamicWeaponData();
	if (dynamicData != nullptr)
		dynamicData->SetBulletsNum(99);

	// Real fix: ammo is just another inventory item (GetCurrentBulletItemId()
	// gives its StaticId), so keep it topped up via the same
	// RequestMove_ToServer dupe mechanism the Duping tab uses. Requires owning
	// at least 1 round of real ammo to begin with - a ghost round from
	// AddItem_ServerInternal wouldn't register either - but once you have one,
	// this keeps the stack topped up automatically.
	SDK::FName bulletId = apwb->GetCurrentBulletItemId();
	if (!bulletId.IsNone())
	{
		TArray<SDK::UPalItemSlot*> upisa;
		if (GetLocalInventoryDataAndCommonSlots(&upisa) != nullptr)
		{
			for (int i = 0; i < upisa.Num(); i++)
			{
				if (!upisa[i] || upisa[i]->ItemId.StaticId != bulletId)
					continue;
				if (upisa[i]->StackCount > 0 && upisa[i]->StackCount < 99)
					IncrementInventoryItemCountByIndex(99 - upisa[i]->StackCount, i);
				break;
			}
		}
	}
}

// Diagnostic: prints the equipped weapon's actual ammo-related field/getter
// values to console, so we can see directly whether IsRequiredBullet/
// IsInfinityMagazine are really being set (and staying set) and whether the
// game's own bullet counters move when firing despite them being set.
void DumpWeaponAmmoState()
{
	DX11_Base::g_Console->Show();

	SDK::APalPlayerCharacter* localPlayer = Config.localPlayer;
	if (localPlayer == nullptr)
	{
		DX11_Base::g_Console->printdbg("[AmmoDump] no local player\n", DX11_Base::Console::Colors::red);
		return;
	}
	SDK::UPalShooterComponent* upsc = localPlayer->ShooterComponent;
	if (upsc == nullptr)
	{
		DX11_Base::g_Console->printdbg("[AmmoDump] no ShooterComponent\n", DX11_Base::Console::Colors::red);
		return;
	}
	SDK::APalWeaponBase* apwb = upsc->GetHasWeapon();
	if (apwb == nullptr)
	{
		DX11_Base::g_Console->printdbg("[AmmoDump] no equipped weapon (GetHasWeapon() == null)\n", DX11_Base::Console::Colors::red);
		return;
	}

	DX11_Base::g_Console->printdbg(
		"[AmmoDump] weapon=%llx IsRequiredBullet=%d IsRequiredBulletForAltFire=%d IsInfinityMagazine=%d RemainBullet=%d MagazineSize=%d InventoryBullet=%d IsFullMagazine=%d\n",
		DX11_Base::Console::Colors::yellow,
		(unsigned long long)apwb,
		apwb->IsRequiredBullet,
		apwb->IsRequiredBulletForAltFire,
		apwb->IsInfinityMagazine,
		apwb->GetRemainBulletCount(),
		apwb->GetMagazineSize(),
		apwb->GetInventoryBulletCount(),
		apwb->IsFullMagazine());

	SDK::UPalDynamicWeaponItemDataBase* dynamicData = apwb->TryGetDynamicWeaponData();
	if (dynamicData != nullptr)
	{
		DX11_Base::g_Console->printdbg(
			"[AmmoDump] dynamicData=%llx RemainingBullets=%d MaxMagazineSize=%d bIsEmptyBulletInventory=%d\n",
			DX11_Base::Console::Colors::yellow,
			(unsigned long long)dynamicData,
			dynamicData->RemainingBullets,
			dynamicData->MaxMagazineSize,
			dynamicData->bIsEmptyBulletInventory);
	}
	else
	{
		DX11_Base::g_Console->printdbg("[AmmoDump] TryGetDynamicWeaponData() == null\n", DX11_Base::Console::Colors::red);
	}

	// For live-debugger use: find the exact inventory slot backing the ammo
	// count and print the raw memory address of its StackCount field, so a
	// hardware write-breakpoint can be set on it directly (ba w4 <addr>) to
	// catch whichever native function actually decrements it on fire.
	SDK::FName bulletId = apwb->GetCurrentBulletItemId();
	if (!bulletId.IsNone())
	{
		TArray<SDK::UPalItemSlot*> upisa;
		if (GetLocalInventoryDataAndCommonSlots(&upisa) != nullptr)
		{
			for (int i = 0; i < upisa.Num(); i++)
			{
				if (!upisa[i] || upisa[i]->ItemId.StaticId != bulletId)
					continue;
				DX11_Base::g_Console->printdbg(
					"[AmmoDump] ammo slot %d: UPalItemSlot=%llx &StackCount=%llx (current value=%d)\n",
					DX11_Base::Console::Colors::green,
					i,
					(unsigned long long)upisa[i],
					(unsigned long long)&upisa[i]->StackCount,
					upisa[i]->StackCount);
				break;
			}
		}
	}
}

// Edits both CraftSpeed and CraftSpeedRates.Values[0] directly through the
// pointer - copying SaveParameter into a local var and editing that would
// touch CraftSpeedRates fine (TArray is a thin pointer wrapper) but not
// CraftSpeed, a plain int32, which looks like the actual cached value
// crafting reads.
void SetCraftingSpeed(float mNewSpeed, bool bRestoreDefault)
{
	APalPlayerCharacter* pPalCharacter = Config.GetPalPlayerCharacter();
	if (!pPalCharacter)
		return;

	UPalCharacterParameterComponent* pParams = pPalCharacter->CharacterParameterComponent;
	if (!pParams)
		return;

	UPalIndividualCharacterParameter* ivParams = pParams->IndividualParameter;
	if (!ivParams)
		return;

	auto& save = ivParams->SaveParameter;

	int32 newSpeed = bRestoreDefault ? 100 : (int32)(mNewSpeed * 100.0f);
	save.CraftSpeed = newSpeed;

	TArray<FFloatContainer_FloatPair>& mCraftSpeedArray = save.CraftSpeedRates.Values;
	if (mCraftSpeedArray.Num() > 0)
		mCraftSpeedArray[0].Value = bRestoreDefault ? 1.0f : mNewSpeed;
}

// bRestoreDefault added - previously had no restore path at all, so unchecking
// "Max Weight Hack" left MaxInventoryWeight stuck at the inflated value forever.
// Captures the real base weight once (on first call - Loops() now calls this
// unconditionally every tick, so that happens on frame 1 before the hack can
// ever be enabled), same capture-once pattern as SpeedHackAllInOne/SpeedHackClean.
void SetMaxInventoryWeight(float mNewMaxWeight, bool bRestoreDefault)
{
	APalPlayerCharacter* pPalCharacter = Config.GetPalPlayerCharacter();
	if (!pPalCharacter)
		return;

	SDK::UPalPlayerInventoryData* InventoryData = Config.GetInventoryComponent();
	if (!InventoryData)
		return;

	static float s_baseMaxWeight = 0.0f;
	static bool s_hasCapturedBase = false;
	if (!s_hasCapturedBase)
	{
		s_baseMaxWeight = InventoryData->MaxInventoryWeight;
		s_hasCapturedBase = true;
	}

	float newWeight = bRestoreDefault ? s_baseMaxWeight : mNewMaxWeight;
	InventoryData->MaxInventoryWeight = newWeight;
	InventoryData->MaxInventoryWeight_Cached = newWeight;
}

//	
void AddTechPoints(__int32 mPoints)
{
	APalPlayerState* mPlayerState = Config.GetPalPlayerState();
	if (!mPlayerState)
		return;

	UPalTechnologyData* pTechData = mPlayerState->TechnologyData;
	if (!pTechData)
		return;

	pTechData->TechnologyPoint += mPoints;
}

//	
void AddAncientTechPoints(__int32 mPoints)
{
	APalPlayerState* mPlayerState = Config.GetPalPlayerState();
	if (!mPlayerState)
		return;

	UPalTechnologyData* pTechData = mPlayerState->TechnologyData;
	if (!pTechData)
		return;

	pTechData->bossTechnologyPoint += mPoints;
}

//	
void RemoveTechPoints(__int32 mPoints)
{
	APalPlayerState* mPlayerState = Config.GetPalPlayerState();
	if (!mPlayerState)
		return;

	UPalTechnologyData* pTechData = mPlayerState->TechnologyData;
	if (!pTechData)
		return;

	pTechData->TechnologyPoint -= mPoints;
}

//	
void RemoveAncientTechPoint(__int32 mPoints)
{
	APalPlayerState* mPlayerState = Config.GetPalPlayerState();
	if (!mPlayerState)
		return;

	UPalTechnologyData* pTechData = mPlayerState->TechnologyData;
	if (!pTechData)
		return;

	pTechData->bossTechnologyPoint -= mPoints;
}

// Debug_SetStatusPoint_ToServer - a real "Debug_" NetServer RPC that sets an
// absolute status point rank directly. Doesn't work in-game despite being a
// genuine server call, probably disabled on the shipping build. Kept for
// reference; AddPlayerStatusPointReal below is the one actually wired into the UI.
void SetPlayerStatusPointLevel_DebugRPC(__int32 mCategory, __int32 mLevel)
{
	APalPlayerController* appco = Config.GetPalPlayerController();
	if (appco == nullptr) return;

	SDK::FName statusName;
	switch (mCategory)
	{
	case 0: statusName = SDK::UPalDefine::StatusPointName_AddMaxHP(); break;
	case 1: statusName = SDK::UPalDefine::StatusPointName_AddMaxSP(); break;
	case 2: statusName = SDK::UPalDefine::StatusPointName_AddMaxInventoryWeight(); break;
	case 3: statusName = SDK::UPalDefine::StatusPointName_AddPower(); break;
	case 4: statusName = SDK::UPalDefine::StatusPointName_AddWorkSpeed(); break;
	case 5: statusName = SDK::UPalDefine::StatusPointName_AddCaptureLevel(); break;
	default: return;
	}

	appco->Debug_SetStatusPoint_ToServer(statusName, mLevel);
}

// AddPlayerStatusPoint_ToServer - the real NetServer RPC a level-up status
// point spend goes through, separate from the Debug_ command above. Additive
// amount, not absolute, via a single-element FPalGotStatusPoint array (same
// stack-local trick as the item-move array, since TArray::Add() no-ops on a
// fresh TArray).
void AddPlayerStatusPointReal(__int32 mCategory, __int32 mPoints)
{
	APalPlayerController* appco = Config.GetPalPlayerController();
	if (appco == nullptr) return;

	SDK::FName statusName;
	switch (mCategory)
	{
	case 0: statusName = SDK::UPalDefine::StatusPointName_AddMaxHP(); break;
	case 1: statusName = SDK::UPalDefine::StatusPointName_AddMaxSP(); break;
	case 2: statusName = SDK::UPalDefine::StatusPointName_AddMaxInventoryWeight(); break;
	case 3: statusName = SDK::UPalDefine::StatusPointName_AddPower(); break;
	case 4: statusName = SDK::UPalDefine::StatusPointName_AddWorkSpeed(); break;
	case 5: statusName = SDK::UPalDefine::StatusPointName_AddCaptureLevel(); break;
	default: return;
	}

	SDK::FPalGotStatusPoint entry;
	entry.StatusName = statusName;
	entry.StatusPoint = mPoints;

	TArray<SDK::FPalGotStatusPoint> points(&entry, 1, 1);
	appco->AddPlayerStatusPoint_ToServer(points);
}

// UPalIndividualCharacterParameter::GetUnusedStatusPoint/DecrementUnusedStatusPoint
// is a real, limited pool of points earned via leveling but not spent yet,
// backed by SaveParameter.UnusedStatusPoint. AddPlayerStatusPointReal above
// only spends points you already have - with 0 unused points normally, it has
// nothing to spend and silently no-ops. This grants mPoints of unused points
// directly (same SaveParameter-pointer-write as SetCraftingSpeed), then calls
// the real spend RPC on top, so the rank-up still goes through validated
// server logic and only the prerequisite resource is bypassed.
void GrantAndSpendStatusPoint(__int32 mCategory, __int32 mPoints)
{
	APalPlayerCharacter* pPalCharacter = Config.GetPalPlayerCharacter();
	if (pPalCharacter == nullptr) return;
	UPalCharacterParameterComponent* pParams = pPalCharacter->CharacterParameterComponent;
	if (pParams == nullptr) return;
	UPalIndividualCharacterParameter* ivParams = pParams->IndividualParameter;
	if (ivParams == nullptr) return;

	ivParams->SaveParameter.UnusedStatusPoint = (uint16)mPoints;

	AddPlayerStatusPointReal(mCategory, mPoints);
}

// RequestUseItemToCharacter_ToServer (generic "use item on a character", meant
// for food/medicine) silently does nothing here - book quantity just keeps
// growing instead of being consumed. Tech Points don't go through that path:
// UPalTechnologyData has its own dedicated consume functions instead:
//   RequestAddTechnologyPointByItem(FPalItemSlotId)      - regular Tech Points
//   RequestAddBossTechnologyPointByItem(FPalItemSlotId)  - Ancient ("Boss") Tech Points
// Both take a slot ID and consume whatever item is in that slot directly -
// the actual mechanism a real book-read goes through.
static bool FindFirstSlotWithItem(const SDK::FName& staticId, TArray<SDK::UPalItemSlot*>& upisa, int32* outIndex)
{
	for (int i = 0; i < upisa.Num(); i++)
	{
		if (upisa[i] && upisa[i]->ItemId.StaticId == staticId && upisa[i]->StackCount > 0)
		{
			*outIndex = i;
			return true;
		}
	}
	return false;
}

static bool GiveItemAndFindSlot(const char* itemName, SDK::FPalItemSlotId* outSlotId)
{
	APalPlayerCharacter* appc = Config.GetPalPlayerCharacter();
	if (appc == nullptr) return false;
	APalPlayerController* appco = appc->GetPalPlayerController();
	if (appco == nullptr) return false;
	APalPlayerState* apps = appco->GetPalPlayerState();
	if (apps == nullptr) return false;
	SDK::UPalPlayerInventoryData* InventoryData = apps->GetInventoryData();
	if (InventoryData == nullptr) return false;

	AddItemToInventoryByName(InventoryData, _strdup(itemName), 1);

	TArray<SDK::UPalItemSlot*> upisa;
	if (GetLocalInventoryDataAndCommonSlots(&upisa) == nullptr) return false;

	static UKismetStringLibrary* lib = UKismetStringLibrary::GetDefaultObj();
	wchar_t ws[255];
	swprintf(ws, 255, L"%hs", itemName);
	SDK::FName staticId = lib->Conv_StringToName(SDK::FString(ws));

	int32 slotIndex = -1;
	if (!FindFirstSlotWithItem(staticId, upisa, &slotIndex))
	{
		DX11_Base::g_Console->printdbg("[TechBook] gave %s but couldn't find it in inventory afterward\n", DX11_Base::Console::Colors::red, itemName);
		return false;
	}

	outSlotId->ContainerId = upisa[slotIndex]->ContainerId;
	outSlotId->SlotIndex = slotIndex;
	return true;
}

void GiveAndUseTechBook(__int32 mGrade)
{
	const char* itemName = nullptr;
	switch (mGrade)
	{
	case 1: itemName = "TechnologyBook_G1"; break;
	case 2: itemName = "TechnologyBook_G2"; break;
	case 3: itemName = "TechnologyBook_G3"; break;
	default: return;
	}

	APalPlayerState* apps = Config.GetPalPlayerState();
	if (apps == nullptr) return;
	UPalTechnologyData* pTechData = apps->TechnologyData;
	if (pTechData == nullptr) return;

	SDK::FPalItemSlotId slotId;
	if (!GiveItemAndFindSlot(itemName, &slotId)) return;

	pTechData->RequestAddTechnologyPointByItem(slotId);
}

// PalCrystal_Ex ("Ancient Civilization Parts") is what a real Ancient Tech point
// gain consumes - via the same dedicated RequestAddBossTechnologyPointByItem
// function above, not by just adding the raw item to the inventory (that alone
// doesn't register as points, same lesson as the regular tech book).
void GiveAndUseAncientCivilizationParts(__int32 mCount)
{
	APalPlayerState* apps = Config.GetPalPlayerState();
	if (apps == nullptr) return;
	UPalTechnologyData* pTechData = apps->TechnologyData;
	if (pTechData == nullptr) return;

	for (int i = 0; i < mCount; i++)
	{
		SDK::FPalItemSlotId slotId;
		if (!GiveItemAndFindSlot("PalCrystal_Ex", &slotId)) return;
		pTechData->RequestAddBossTechnologyPointByItem(slotId);
	}
}

// "Elixir" consumables grant a flat, permanent stat increase on use - a third
// system, separate from StatusPoint allocation and Tech Points. Item names
// (not in the SDK/exe, only the cooked .pak data): Elixir_hp, Elixir_stamina_01/02,
// Elixir_weight, Elixir_attack_01/02, Elixir_workspeed_01/02. Naming is
// inconsistent (some categories have a bare tier-1 name, others need _01), so
// this tries the exact name first and falls back to the bare variant. These
// are plain Food-type consumables, so unlike Tech Books the generic
// RequestUseItemToCharacter_ToServer is the right path here.
void GiveAndUseElixir(__int32 mCategory, __int32 mTier)
{
	const char* baseName = nullptr;
	switch (mCategory)
	{
	case 0: baseName = "Elixir_hp"; break;
	case 1: baseName = "Elixir_stamina"; break;
	case 2: baseName = "Elixir_weight"; break;
	case 3: baseName = "Elixir_attack"; break;
	case 4: baseName = "Elixir_workspeed"; break;
	default: return;
	}

	char itemName[64];
	sprintf_s(itemName, "%s_%02d", baseName, mTier);

	APalPlayerCharacter* appc = Config.GetPalPlayerCharacter();
	if (appc == nullptr) return;
	APalPlayerController* appco = appc->GetPalPlayerController();
	if (appco == nullptr) return;

	SDK::FPalItemSlotId slotId;
	if (!GiveItemAndFindSlot(itemName, &slotId))
	{
		// Fallback: some categories (hp, weight) use a bare name with no _01/_02
		// suffix for their base tier instead.
		DX11_Base::g_Console->printdbg("[Elixir] %s not found, trying bare name %s\n", DX11_Base::Console::Colors::yellow, itemName, baseName);
		if (!GiveItemAndFindSlot(baseName, &slotId))
			return;
	}

	SDK::FPalItemSlotIdAndNum itemData;
	itemData.SlotId = slotId;
	itemData.Num = 1;

	SDK::FPalInstanceID selfId = appc->CharacterParameterComponent->IndividualHandle->ID;
	appco->RequestUseItemToCharacter_ToServer(itemData, selfId);
}

// Uses an already-owned item in slot mIndex, mTimes in a row, via the same
// RequestUseItemToCharacter_ToServer RPC as GiveAndUseElixir above. Works on
// whatever real item is already in the slot (e.g. a Technical Manual, or
// something duped via the Duping tab), so it isn't blocked by the ghost-item
// limitation. Not an auto-clicker - fires exactly mTimes requests once on
// button press. Caps mTimes at the slot's current StackCount.
void UseInventorySlotItemMultiple(__int32 mIndex, __int32 mTimes)
{
	if (mTimes <= 0)
		return;

	TArray<SDK::UPalItemSlot*> upisa;
	if (GetLocalInventoryDataAndCommonSlots(&upisa) == nullptr)
		return;
	if (!upisa.IsValidIndex(mIndex) || !upisa[mIndex] || upisa[mIndex]->StackCount <= 0)
		return;

	APalPlayerCharacter* appc = Config.GetPalPlayerCharacter();
	if (appc == nullptr || appc->CharacterParameterComponent == nullptr || appc->CharacterParameterComponent->IndividualHandle == nullptr)
		return;
	APalPlayerController* appco = appc->GetPalPlayerController();
	if (appco == nullptr)
		return;

	SDK::FPalItemSlotId slotId;
	slotId.ContainerId = upisa[mIndex]->ContainerId;
	slotId.SlotIndex = mIndex;

	SDK::FPalInstanceID selfId = appc->CharacterParameterComponent->IndividualHandle->ID;
	__int32 usesToSend = min(mTimes, upisa[mIndex]->StackCount);

	for (__int32 i = 0; i < usesToSend; i++)
	{
		SDK::FPalItemSlotIdAndNum itemData;
		itemData.SlotId = slotId;
		itemData.Num = 1;
		appco->RequestUseItemToCharacter_ToServer(itemData, selfId);
	}
}

// credit: xCENTx
float GetDistanceToActor(AActor* pLocal, AActor* pTarget)
{
	if (!pLocal || !pTarget)
		return -1.f;
	
	FVector pLocation = pLocal->K2_GetActorLocation();
	FVector pTargetLocation = pTarget->K2_GetActorLocation();
	double distance = sqrt(pow(pTargetLocation.X - pLocation.X, 2.0) + pow(pTargetLocation.Y - pLocation.Y, 2.0) + pow(pTargetLocation.Z - pLocation.Z, 2.0));

	return distance / 100.0f;
}

// credit xCENTx
void ForgeActor(SDK::AActor* pTarget, float mDistance, float mHeight, float mAngle)
{
	APalPlayerCharacter* pPalPlayerCharacter = Config.GetPalPlayerCharacter();
	APlayerController* pPlayerController = Config.GetPalPlayerController();
	if (!pTarget || !pPalPlayerCharacter || !pPlayerController)
		return;

	APlayerCameraManager* pCamera = pPlayerController->PlayerCameraManager;
	if (!pCamera)
		return;

	FVector playerLocation = pPalPlayerCharacter->K2_GetActorLocation();
	FVector camFwdDir = pCamera->GetActorForwardVector() * ( mDistance * 100.f ); 
	FVector targetLocation = playerLocation + camFwdDir;

	if (mHeight != 0.0f)
		targetLocation.Y += mHeight;
	
	FRotator targetRotation = pTarget->K2_GetActorRotation();
	if (mAngle != 0.0f)
		targetRotation.Roll += mAngle;

	pTarget->K2_SetActorLocation(targetLocation, false, nullptr, true);
	pTarget->K2_SetActorRotation(targetRotation, true);
}

//	credit: 
void SendDamageToActor(APalCharacter* pTarget, int32 damage, bool bSpoofAttacker)
{
	APalPlayerCharacter* pPalPlayerCharacter = Config.GetPalPlayerCharacter();
	if (!pPalPlayerCharacter) return;
	APalPlayerController* pPalPlayerController = Config.GetPalPlayerController();
	if (!pPalPlayerController) return;
	FPalDamageInfo  info = FPalDamageInfo();
	info.AttackElementType = EPalElementType::Normal;
	info.Attacker = pPalPlayerCharacter;		//	@TODO: spoof attacker
	info.AttackerGroupID = Config.GetPalPlayerState()->IndividualHandleId.PlayerUId;
	info.AttackerLevel = 50;	
	info.AttackType = EPalAttackType::Weapon;
	info.bApplyNativeDamageValue = true;
	info.bAttackableToFriend = true;
	info.IgnoreShield = true;
	info.NativeDamageValue = damage;
	pPalPlayerController->DamageReactionComponent_ProcessDamage_ToServer_ToNPC(info, pTarget);
}

//	 NOTE: only targets pals
void DeathAura(__int32 dmgAmount, float mDistance, bool bIntensityEffect, bool bVisualAffect, EPalVisualEffectID visID)
{
	APalCharacter* pPalCharacter = Config.GetPalPlayerCharacter();
	if (!pPalCharacter)
		return;

	UPalCharacterParameterComponent* pParams = pPalCharacter->CharacterParameterComponent;
	if (!pParams)
		return;

	APalCharacter* pPlayerPal = pParams->OtomoPal;

	TArray<APalCharacter*> outPals;
	if (!Config.GetTAllPals(&outPals))
		return;
		
	DWORD palsCount = outPals.Num();
	for (auto i = 0; i < palsCount; i++)
	{
		APalCharacter* cEnt = outPals[i];
		
		if (!cEnt || !cEnt->IsA(APalMonsterCharacter::StaticClass()) || cEnt == pPlayerPal)
			continue;

		float distanceTo = GetDistanceToActor(pPalCharacter, cEnt);
		if (distanceTo > mDistance)
			continue;

		float dmgScalar = dmgAmount * (1.0f - distanceTo / mDistance);
		if (bIntensityEffect)
			dmgAmount = dmgScalar;

		UPalVisualEffectComponent* pVisComp = cEnt->VisualEffectComponent;
		if (bVisualAffect && pVisComp)
		{
			FPalVisualEffectDynamicParameter fvedp;
			if (!pVisComp->ExecutionVisualEffects.Num())
				pVisComp->AddVisualEffect_ToServer(visID, fvedp, 1);	//	uc: killer1478
		}
		SendDamageToActor(cEnt, dmgAmount);
	}
}

// credit: xCENTx
void TeleportAllPalsToCrosshair(float mDistance)
{
	TArray<APalCharacter*> outPals;
	Config.GetTAllPals(&outPals);
	DWORD palsCount = outPals.Num();
	for (int i = 0; i < palsCount; i++)
	{
		APalCharacter* cPal = outPals[i];

		if (!cPal || !cPal->IsA(APalMonsterCharacter::StaticClass()))
			continue;
		
		//	@TODO: displace with entity width for true distance, right now it is distance from origin
		//	FVector palOrigin;
		//	FVector palBounds;
		//	cPal->GetActorBounds(true, &palOrigin, &palBounds, false);
		//	float adj = palBounds.X * .5 + mDistance;

		ForgeActor(cPal, mDistance);
	}
}

// credit: xCENTx
void AddWaypointLocation(std::string wpName)
{
	APalCharacter* pPalCharacater = Config.GetPalPlayerCharacter();
	if (!pPalCharacater)
		return;

	FVector wpLocation = pPalCharacater->K2_GetActorLocation();
	FRotator wpRotation = pPalCharacater->K2_GetActorRotation();
	config::SWaypoint newWaypoint = config::SWaypoint("[WAYPOINT]" + wpName, wpLocation, wpRotation);
	Config.db_waypoints.push_back(newWaypoint);
}

// credit: xCENTx
//	must be called from a rendering thread with imgui context
void RenderWaypointsToScreen()
{
	APalCharacter* pPalCharacater = Config.GetPalPlayerCharacter();
	APalPlayerController* pPalController = Config.GetPalPlayerController();
	if (!pPalCharacater || !pPalController)
		return;

	ImDrawList* draw = ImGui::GetWindowDrawList();

	for (auto waypoint : Config.db_waypoints)
	{
		FVector2D vScreen;
		if (!pPalController->ProjectWorldLocationToScreen(waypoint.waypointLocation, &vScreen, false))
			continue;

		auto color = ImColor(1.0f, 1.0f, 1.0f, 1.0f);

		draw->AddText(ImVec2( vScreen.X, vScreen.Y ), color, waypoint.waypointName.c_str());
	}
}

//void ForceJoinGuild( SDK::APalPlayerCharacter* targetPlayer )
//{
//	if ( !targetPlayer->CharacterParameterComponent->IndividualHandle )
//		return;
//	if ( !Config.GetPalPlayerController() )
//		return;
//
//	UPalNetworkGroupComponent* group = Config.GetPalPlayerController()->Transmitter->Group;
//	if ( !group )
//		return;
//
//	SDK::FGuid myPlayerId = Config.GetPalPlayerController()->GetPlayerUId();
//	SDK::FGuid playerId = targetPlayer->CharacterParameterComponent->IndividualHandle->ID.PlayerUId;
//
//	group->RequestJoinGuildForPlayer_ToServer( myPlayerId, playerId );
//}

///	OLDER METHODS
//SDK::FPalDebugOtomoPalInfo palinfo = SDK::FPalDebugOtomoPalInfo();
//SDK::TArray<SDK::EPalWazaID> EA = { 0U };
//void SpawnPal(char* PalName, bool IsMonster, int rank=1, int lvl = 1, int count=1)
//{
//    SDK::UKismetStringLibrary* lib = SDK::UKismetStringLibrary::GetDefaultObj();
//
//    //Convert FNAME
//    wchar_t  ws[255];
//    swprintf(ws, 255, L"%hs", PalName);
//    SDK::FName Name = lib->Conv_StringToName(SDK::FString(ws));
//    //Call
//    if (Config.GetPalPlayerCharacter() != NULL)
//    {
//        if (Config.GetPalPlayerCharacter()->GetPalPlayerController() != NULL)
//        {
//            if (Config.GetPalPlayerCharacter()->GetPalPlayerController())
//            {
//                if (Config.GetPalPlayerCharacter()->GetPalPlayerController()->GetPalPlayerState())
//                {
//                    if (IsMonster)
//                    {
//                        Config.GetPalPlayerCharacter()->GetPalPlayerController()->GetPalPlayerState()->RequestSpawnMonsterForPlayer(Name, count, lvl);
//                        return;
//                    }
//                    EA[0] = SDK::EPalWazaID::AirCanon;
//                    palinfo.Level = lvl;
//                    palinfo.Rank = rank;
//                    palinfo.PalName.Key = Name;
//                    palinfo.WazaList = EA;
//                    palinfo.PassiveSkill = NULL;
//                    Config.GetPalPlayerCharacter()->GetPalPlayerController()->GetPalPlayerState()->Debug_CaptureNewMonsterByDebugOtomoInfo_ToServer(palinfo);
//                }
//            }
//        }
//    }
//}