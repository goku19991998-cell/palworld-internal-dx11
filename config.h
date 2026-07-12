#pragma once
#include <Windows.h>
#include "libs/utils/memory.h"
#include "database.h"
#include "ItemList.hpp"

typedef bool(*Tick)(SDK::APalPlayerCharacter* m_this, float DeltaSecond);

class config
{
public:
	//offsets
	DWORD64 ClientBase = 0;
	DWORD64 offset_Tick = 0;

	//check
	bool IsESP = false;
	bool IsForgeMode = false;
	bool IsTeleportAllToXhair = false;
	bool IsDeathAura = false;
	bool IsAimbot = false;
	bool IsSpeedHack = false;
	bool IsAttackModiler = false;
	bool IsDefuseModiler = false;
	bool IsInfStamina = false;
	bool IsSafe = true;
	bool IsInfinAmmo = false;
	bool IsGodMode = false;
	bool IsToggledFly = false;
	bool IsFlyEnabled = false; // "armed" - only when true does FlyToggleKey do anything
	bool IsMuteki = false;
	bool IsMonster = false;
	bool IsQuick = false;
	bool matchDbItems = true;
	bool isDebugESP = false;
	bool bisOpenManager = false;
	bool filterPlayer = false;
	bool bisRandomName = false;
	bool bisTeleporter = false;
	float SpeedModiflers = 1.0f;
	int SpeedHackToggleKey = 0; // 0 = unbound; a Win32 virtual-key code once bound
	int AttackHackToggleKey = 0;
	int FlyToggleKey = 0;
	bool IsSpeedHackClean = false; // WIP tab only - see SpeedHackClean() in feature.cpp
	float SpeedHackCleanMultiplier = 2.0f; // capped at 3.32x in UI, higher causes rubber-banding/desync
	bool IsPlayerESP = false;
	float mPlayerESPDistance = 1000.0f;
	bool IsPalESP = false;
	float mPalESPDistance = 1000.0f;
	//def and value
	float mDebugESPDistance = 5.0f;
	float mDebugEntCapDistance = 10.0f;
	float mDeathAuraDistance = 10.f;
	int mDeathAuraAmount = 100.f;
	int DamageUp = 1;
	int DefuseUp = 1;
	bool IsArmorDefenseHack = false;
	float ArmorDefenseMultiplier = 1.0f;
	int EXP = 0;
	int Item = 0;
	float Pos[3] = { 0,0,0 };
	char ItemName[255];
	char inputTextBuffer[255] = "";
	SDK::UWorld* gWorld = nullptr;
	SDK::APalPlayerCharacter* localPlayer = NULL;
	SDK::TArray<SDK::APalPlayerCharacter*> AllPlayers = {};
	SDK::UPalCharacterImportanceManager* UCIM = NULL;
	SDK::UObject* WorldContextObject = NULL;
	int AddItemSlot = 0;
	int AddItemCount = 2;
	int TechPointsToAdd = 1;
	int AncientTechPointsToAdd = 1;
	bool IsCraftSpeedHack = false;
	float CraftSpeedMultiplier = 1.0f;
	bool IsMaxWeightHack = false;
	float MaxWeightValue = 999999.0f;
	bool IsInstantMapObjectRespawn = false;
	bool IsFreezeSlot = false;
	int FreezeSlotIndex = 0;
	int FreezeSlotTarget = 99;
	bool IsFreezePalSphere = false;
	int PalSphereFreezeTarget = 99;
	bool IsCanCraftAllItems = false;
	bool IsCanBuildAllBuildings = false;
	bool IsWeightlessDodge = false;
	bool IsNoConsumeSphereAmmo = false;
	bool IsMagicPurse = false;
	bool IsPalPerfectStatRoll = false;
	bool IsWorkSpeedAccelerator = false;
	float WorkSpeedExtraRate = 5.0f;
	float RespawnForceRadius = 5000.0f;
	char ForceSaveName[64] = "NetCrackTest";
	int StatusPointLevel = 99;
	int ElixirTier = 2;
	bool IsNoUseBulletHook = false;

	// Pal Editor (Team)
	int SelectedPartyPalIndex = -1;
	int EditPalLevel = 1;
	int EditPalRank = 0;
	int EditPalRankHP = 0;
	int EditPalRankAttack = 0;
	int EditPalRankDefense = 0;
	int EditPalRankCraftSpeed = 0;
	int EditPalTalentHP = 0;
	int EditPalTalentMelee = 0;
	int EditPalTalentShot = 0;
	int EditPalTalentDefense = 0;
	char EditPalPassiveName[64] = "";

	enum QuickItemSet
	{
		basic_items_stackable,
		basic_items_single,
		pal_unlock_skills,
		spheres,
		tools

	};
	//Filtered Items
	std::vector<std::string> db_filteredItems;
	
	
	
	struct SWaypoint
	{
		std::string waypointName;
		SDK::FVector waypointLocation;
		SDK::FRotator waypointRotation;

		bool bIsShown = true;
		float* mColor[4];

		SWaypoint() {};
		SWaypoint(std::string wpName, SDK::FVector wpLocation, SDK::FRotator wpRotation) { waypointName = wpName; waypointLocation = wpLocation; waypointRotation = wpRotation; }
	};
	std::vector<SWaypoint> db_waypoints;
	std::vector<std::pair<std::string, SDK::UClass*>> db_filteredEnts;


	//static function
	static SDK::UWorld* GetUWorld();
	static SDK::ULocalPlayer* GetLocalPlayer();
	static SDK::APalPlayerCharacter* GetPalPlayerCharacter(); 
	static SDK::APalPlayerController* GetPalPlayerController();
	static SDK::APalPlayerState* GetPalPlayerState();
	static SDK::UPalPlayerInventoryData* GetInventoryComponent();
	static SDK::APalWeaponBase* GetPlayerEquippedWeapon();
	static bool	GetTAllPlayers(SDK::TArray<class SDK::APalCharacter*>* outResult);
	static bool	GetTAllImpNPC(SDK::TArray<class SDK::APalCharacter*>* outResult);
	static bool	GetTAllNPC(SDK::TArray<class SDK::APalCharacter*>* outResult);
	static bool	GetTAllPals(SDK::TArray<class SDK::APalCharacter*>* outResult);
	static bool GetPartyPals(std::vector<SDK::AActor*>* outResult);
	static bool GetPartyPalHandles(std::vector<SDK::UPalIndividualCharacterHandle*>* outResult);
	static bool GetPlayerDeathChests(std::vector<SDK::FVector>* outLocations);
	static bool GetAllActorsofType(SDK::UClass* mType, std::vector<SDK::AActor*>* outArray, bool bLoopAllLevels = false, bool bSkipLocalPlayer = false);
	static void Init();
	static void Update(const char* filterText);
	static const std::vector<std::string>& GetFilteredItems();
};
extern config Config;
