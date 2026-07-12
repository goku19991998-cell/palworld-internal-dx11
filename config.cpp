#include "pch.h"
#include "config.h"
#include <algorithm>
#include "include/Menu.hpp"

config Config;

Tick TickFunc;
Tick OldTickFunc;

void config::Update(const char* filterText)
{
    Config.db_filteredItems.clear();

    const auto& itemsToSearch = database::db_items;

    // Case-insensitive match - db_items entries are capitalized ("Accessory_...",
    // "PalSphere_...") so a plain strstr() against lowercase user input (e.g.
    // "sphere") never matched anything.
    std::string needle = filterText;
    std::transform(needle.begin(), needle.end(), needle.begin(), ::tolower);

    for (const auto& itemName : itemsToSearch) {
        std::string haystack = itemName;
        std::transform(haystack.begin(), haystack.end(), haystack.begin(), ::tolower);
        if (haystack.find(needle) != std::string::npos) {
            Config.db_filteredItems.push_back(itemName);
        }
    }
    std::sort(Config.db_filteredItems.begin(), Config.db_filteredItems.end());
}
const std::vector<std::string>& config::GetFilteredItems() { return Config.db_filteredItems; }

bool DetourTick(SDK::APalPlayerCharacter* m_this, float DeltaSecond)
{
    if (m_this->GetPalPlayerController() != NULL)
    {
        if (m_this->GetPalPlayerController()->IsLocalPlayerController())
        {
            Config.localPlayer = m_this;
            DX11_Base::g_Menu->Loops();
        }
    }
    return OldTickFunc(m_this, DeltaSecond);
}
SDK::UWorld* config::GetUWorld()
{
    static uint64_t gworld_ptr = 0;
    if (!gworld_ptr)
    {
        auto gworld = signature("48 8B 05 ? ? ? ? EB 05").instruction(3).add(7);
        gworld_ptr = gworld.GetPointer();
        if (gworld_ptr)
            Config.gWorld = *(SDK::UWorld**)gworld_ptr;
    }
    return (*(SDK::UWorld**)(gworld_ptr));
}


SDK::ULocalPlayer* config::GetLocalPlayer()
{
    SDK::UWorld* pWorld = Config.gWorld;
    if (!pWorld)
        return nullptr;

    SDK::UGameInstance* pGameInstance = pWorld->OwningGameInstance;
    if (!pGameInstance)
        return nullptr;
    
    return pGameInstance->LocalPlayers[0];
}

SDK::APalPlayerCharacter* config::GetPalPlayerCharacter()
{

    if (Config.localPlayer != NULL)
    {
        return Config.localPlayer;
    }
    return nullptr;
}

SDK::APalPlayerController* config::GetPalPlayerController()
{
    SDK::APalPlayerCharacter* pPlayer = GetPalPlayerCharacter();
    if (!pPlayer)
        return nullptr;

    return static_cast<SDK::APalPlayerController*>(pPlayer->GetPalPlayerController());
}

SDK::APalPlayerState* config::GetPalPlayerState()
{
    SDK::APalPlayerCharacter* pPlayer = GetPalPlayerCharacter();
    if (!pPlayer)
        return nullptr;

    return static_cast<SDK::APalPlayerState*>(pPlayer->PlayerState);
}

SDK::UPalPlayerInventoryData* config::GetInventoryComponent()
{
    SDK::APalPlayerState* pPlayerState = GetPalPlayerState();
    if (!pPlayerState)
        return nullptr;

    return pPlayerState->InventoryData;
}

SDK::APalWeaponBase* config::GetPlayerEquippedWeapon()
{
    SDK::APalPlayerCharacter* pPalCharacter = GetPalPlayerCharacter();
    if (!pPalCharacter)
        return nullptr;

    SDK::UPalShooterComponent* pWeaponInventory = pPalCharacter->ShooterComponent;
    if (!pWeaponInventory)
        return nullptr;

    return pWeaponInventory->HasWeapon;
}


bool config::GetTAllPals(SDK::TArray<class SDK::APalCharacter*>* outResult)
{
    SDK::UWorld* world = Config.GetUWorld();
    if (!world) return false;
    SDK::TArray<SDK::ULevel*> pLevelsArray = world->Levels;
    __int32 levelsCount = pLevelsArray.Num();
    for (int i = 0; i < levelsCount; i++)
    {
        if (!pLevelsArray.IsValidIndex(i)) continue;
        SDK::ULevel* pLevel = pLevelsArray[i];
        if (!pLevel) continue;
        SDK::TArray<SDK::AActor*> pActorsArray = pLevelsArray[i]->Actors;
        __int32 actorsCount = pActorsArray.Num();
        for (int j = 0; j < actorsCount; j++)
        {
            if (!pActorsArray.IsValidIndex(j)) continue;
            SDK::AActor* pActor = pActorsArray[j];
            if (!pActor || !pActor->IsA(SDK::APalCharacter::StaticClass())) continue;
            SDK::APalCharacter* pCharacter = static_cast<SDK::APalCharacter*>(pActor);
            if (!pCharacter) continue;
            outResult->Add(pCharacter);
        }
    }
    return true;
}

// There's no direct field anywhere in the SDK pointing at the local player's
// UPalPlayerPartyPalHolder instance, so it's located by scanning GObjects for the
// (singleton, in singleplayer) instance of that class, then asking it for its
// party member handles directly.
bool config::GetPartyPalHandles(std::vector<SDK::UPalIndividualCharacterHandle*>* outResult)
{
    for (int i = 0; i < SDK::UObject::GObjects->Num(); i++)
    {
        SDK::UObject* obj = SDK::UObject::GObjects->GetByIndex(i);
        if (!obj || !obj->IsA(SDK::UPalPlayerPartyPalHolder::StaticClass()))
            continue;

        SDK::UPalPlayerPartyPalHolder* holder = static_cast<SDK::UPalPlayerPartyPalHolder*>(obj);
        SDK::TArray<SDK::UPalIndividualCharacterHandle*> members = {};
        holder->GetPartyMember(&members);

        for (int j = 0; j < members.Num(); j++)
        {
            if (members[j])
                outResult->push_back(members[j]);
        }
        return outResult->size() > 0;
    }
    return false;
}

bool config::GetPartyPals(std::vector<SDK::AActor*>* outResult)
{
    std::vector<SDK::UPalIndividualCharacterHandle*> handles;
    if (!GetPartyPalHandles(&handles))
        return false;

    for (auto* handle : handles)
    {
        SDK::APalCharacter* actor = handle->TryGetIndividualActor();
        if (actor)
            outResult->push_back(actor);
    }
    return outResult->size() > 0;
}

//  @TODO:
bool config::GetPlayerDeathChests(std::vector<SDK::FVector>* outLocations)
{
    return false;
}

bool config::GetAllActorsofType(SDK::UClass* mType, std::vector<SDK::AActor*>* outArray, bool bLoopAllLevels, bool bSkipLocalPlayer)
{
    SDK::UWorld* pWorld = Config.GetUWorld();
    if (!pWorld) return false;

    SDK::AActor* pLocalPlayer = static_cast<SDK::AActor*>(GetPalPlayerCharacter());
    std::vector<SDK::AActor*> result;

    //	Get Levels
    SDK::TArray<SDK::ULevel*> pLevelsArray = pWorld->Levels;
    __int32 levelsCount = pLevelsArray.Num();

    //	Loop Levels Array
    for (int i = 0; i < levelsCount; i++)
    {
        if (!pLevelsArray.IsValidIndex(i))
            continue;

        SDK::ULevel* pLevel = pLevelsArray[i];
        if (!pLevel) {
            if (bLoopAllLevels) continue;
            else break;
        }
        SDK::TArray<SDK::AActor*> pActorsArray = pLevelsArray[i]->Actors;
        __int32 actorsCount = pActorsArray.Num();

        //	Loop Actor Array
        for (int j = 0; j < actorsCount; j++)
        {
            if (!pActorsArray.IsValidIndex(j))
                continue;

            SDK::AActor* pActor = pActorsArray[j];
            if (!pActor || !pActor->RootComponent || (pActor == pLocalPlayer && bSkipLocalPlayer))
                continue;

            if (!pActor->IsA(mType))
                continue;

            result.push_back(pActor);
        }

        if (bLoopAllLevels)
            continue;
        else
            break;
    }
    *outArray = result;
    return result.size() > 0;
}

void config::Init()
{
    //register hook
    Config.ClientBase = (DWORD64)GetModuleHandle(NULL);

    Config.gWorld = Config.GetUWorld();

    TickFunc = (Tick)(Config.ClientBase + Config.offset_Tick);

    MH_CreateHook(TickFunc, DetourTick, reinterpret_cast<void**>(&OldTickFunc));
}
