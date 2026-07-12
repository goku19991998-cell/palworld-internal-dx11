#include "../pch.h"
#include "../include/Hooking.hpp"
namespace DX11_Base {
	Hooking::Hooking()
	{
		MH_Initialize();
#if DEBUG
		g_Console->printdbg("Hooking::Initialized\n", Console::Colors::pink);
#endif
		return;
	}

	Hooking::~Hooking()
	{
		MH_RemoveHook(MH_ALL_HOOKS);
	}

	void Hooking::Hook()
	{
		//������ע��HOOK
		g_GameVariables->Init();
		g_D3D11Window->Hook();
		Config.Init();
		InstallDamageMultiplierHook();
		MH_EnableHook(MH_ALL_HOOKS);
#if DEBUG
		g_Console->printdbg("Hooking::Hook Initialized\n", Console::Colors::pink);
#endif
		return;
	}

	// See Hooking.hpp for rationale. Patches:
	//   mov edx, [rdi+0x180]   ; original outgoing-damage load
	//   mov rcx, rbx           ; (left untouched, right after our 6-byte patch)
	// into a jmp to a trampoline that re-executes the load, conditionally multiplies
	// edx by Config.DamageUp when Config.IsAttackModiler is set, then jumps back.
	void InstallDamageMultiplierHook()
	{
		uintptr_t hookAddr = signature("8B 97 80 01 00 00 48 8B CB").GetPointer();
		if (!hookAddr)
			return;

		void* trampoline = allocate_near(hookAddr, 0x40);
		if (!trampoline)
			return;

		uint8_t* tramp = reinterpret_cast<uint8_t*>(trampoline);
		size_t offset = 0;

		// mov edx, [rdi+0x180]  (original instruction, re-executed)
		const uint8_t movEdx[] = { 0x8B, 0x97, 0x80, 0x01, 0x00, 0x00 };
		memcpy(tramp + offset, movEdx, sizeof(movEdx));
		offset += sizeof(movEdx);

		// push rax
		tramp[offset++] = 0x50;

		// mov rax, <&Config.IsAttackModiler>
		tramp[offset++] = 0x48;
		tramp[offset++] = 0xB8;
		uintptr_t pIsAttackModiler = reinterpret_cast<uintptr_t>(&Config.IsAttackModiler);
		memcpy(tramp + offset, &pIsAttackModiler, sizeof(pIsAttackModiler));
		offset += sizeof(pIsAttackModiler);

		// cmp byte ptr [rax], 0
		tramp[offset++] = 0x80;
		tramp[offset++] = 0x38;
		tramp[offset++] = 0x00;

		// je +13 (skip the multiply block below when the checkbox is off)
		tramp[offset++] = 0x74;
		tramp[offset++] = 0x0D;

		// mov rax, <&Config.DamageUp>
		tramp[offset++] = 0x48;
		tramp[offset++] = 0xB8;
		uintptr_t pDamageUp = reinterpret_cast<uintptr_t>(&Config.DamageUp);
		memcpy(tramp + offset, &pDamageUp, sizeof(pDamageUp));
		offset += sizeof(pDamageUp);

		// imul edx, [rax]
		tramp[offset++] = 0x0F;
		tramp[offset++] = 0xAF;
		tramp[offset++] = 0x10;

		// pop rax
		tramp[offset++] = 0x58;

		// jmp back to hookAddr + 6
		uintptr_t returnAddr = hookAddr + 6;
		uintptr_t jmpBackSrc = reinterpret_cast<uintptr_t>(tramp + offset) + 5;
		int32_t relJmpBack = static_cast<int32_t>(returnAddr - jmpBackSrc);
		tramp[offset++] = 0xE9;
		memcpy(tramp + offset, &relJmpBack, sizeof(relJmpBack));
		offset += sizeof(relJmpBack);

		DWORD oldProtect;
		VirtualProtect(reinterpret_cast<void*>(hookAddr), 6, PAGE_EXECUTE_READWRITE, &oldProtect);

		int32_t relJmpFwd = static_cast<int32_t>(reinterpret_cast<uintptr_t>(trampoline) - (hookAddr + 5));
		uint8_t patch[6];
		patch[0] = 0xE9;
		memcpy(patch + 1, &relJmpFwd, sizeof(relJmpFwd));
		patch[5] = 0x90; // nop, pads the 5-byte jmp out to the original 6-byte instruction length

		memcpy(reinterpret_cast<void*>(hookAddr), patch, sizeof(patch));

		DWORD tmp;
		VirtualProtect(reinterpret_cast<void*>(hookAddr), 6, oldProtect, &tmp);
		FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(hookAddr), 6);
		FlushInstructionCache(GetCurrentProcess(), trampoline, offset);
	}

	static uintptr_t s_mapObjectRespawnAddr = 0;
	static bool s_mapObjectRespawnResolved = false;
	static bool s_mapObjectRespawnApplied = false;

	void SetInstantMapObjectRespawn(bool bEnable)
	{
		if (!s_mapObjectRespawnResolved)
		{
			s_mapObjectRespawnResolved = true;
			s_mapObjectRespawnAddr = signature("48 39 ? ? ? 7C ? 48 8B ? 48 8B").GetPointer();
		}

		if (!s_mapObjectRespawnAddr || bEnable == s_mapObjectRespawnApplied)
			return;

		DWORD oldProtect;
		VirtualProtect(reinterpret_cast<void*>(s_mapObjectRespawnAddr), 2, PAGE_EXECUTE_READWRITE, &oldProtect);

		if (bEnable)
		{
			const uint8_t patch[2] = { 0xEB, 0x05 }; // jmp short +5, skips the cmp+jl entirely
			memcpy(reinterpret_cast<void*>(s_mapObjectRespawnAddr), patch, sizeof(patch));
		}
		else
		{
			const uint8_t original[2] = { 0x48, 0x39 }; // restores "cmp [rsp+..],rbx"
			memcpy(reinterpret_cast<void*>(s_mapObjectRespawnAddr), original, sizeof(original));
		}

		DWORD tmp;
		VirtualProtect(reinterpret_cast<void*>(s_mapObjectRespawnAddr), 2, oldProtect, &tmp);
		FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(s_mapObjectRespawnAddr), 2);

		s_mapObjectRespawnApplied = bEnable;
	}

	// Generic toggle-able byte patch: resolve an AOB signature once, then flip
	// between two fixed byte sequences at (signature + offset) on demand.
	struct BytePatch
	{
		const char* sig;
		uint32_t offset;
		std::vector<uint8_t> enableBytes;
		std::vector<uint8_t> disableBytes;
		uintptr_t addr = 0;
		bool resolved = false;
		bool applied = false;
	};

	static void ApplyBytePatch(BytePatch& patch, bool bEnable, const char* debugName)
	{
		if (!patch.resolved)
		{
			patch.resolved = true;
			uintptr_t base = signature(patch.sig).GetPointer();
			patch.addr = base ? base + patch.offset : 0;
		}

		if (!patch.addr || bEnable == patch.applied)
			return;

		const auto& bytes = bEnable ? patch.enableBytes : patch.disableBytes;

		DWORD oldProtect;
		VirtualProtect(reinterpret_cast<void*>(patch.addr), bytes.size(), PAGE_EXECUTE_READWRITE, &oldProtect);
		memcpy(reinterpret_cast<void*>(patch.addr), bytes.data(), bytes.size());
		DWORD tmp;
		VirtualProtect(reinterpret_cast<void*>(patch.addr), bytes.size(), oldProtect, &tmp);
		FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(patch.addr), bytes.size());

		patch.applied = bEnable;
	}

	// Function entry that gates crafting - found via a shared Cheat Engine table
	// (author "Byakuran") for an earlier Palworld version. Its AOB still resolves
	// unmodified against the current build, but doesn't actually work: the
	// workbench UI shows "0 required" as intended, but the "max craftable" count
	// next to it becomes garbage (near INT32_MAX - almost certainly an uninitialized
	// register/stack slot the early "ret" skips setting up), and the "not enough
	// material" gameplay check still fires when trying to craft. So this function
	// is apparently UI-display-only on the current build, not the real requirement
	// gate - the real check must live somewhere else, not yet found. Hard-disabled
	// (jump is never installed) so it can't corrupt the workbench display.
	static uintptr_t s_craftReqAddr = 0;
	static bool s_craftReqResolved = false;
	static bool s_craftReqApplied = false;
	static void* s_craftReqTrampoline = nullptr;

	void SetCanCraftAllItems(bool bEnable)
	{
		bEnable = false; // hard-disabled - see comment above, corrupts the workbench UI without actually unlocking crafting

		if (!s_craftReqResolved)
		{
			s_craftReqResolved = true;
			s_craftReqAddr = signature("48 89 5C 24 18 55 56 57 41 56 41 57 48 83 EC 40 0F 29 74 24 30 49 8B F9 49 8B D8 48 8B EA 48 8B F1 33 C0 41 89 41 08 41 39 41 0C").GetPointer();
			if (!s_craftReqAddr)
				g_Console->printdbg("[-] CanCraftAllItems: signature not found!\n", Console::Colors::red);
		}

		if (!s_craftReqAddr || bEnable == s_craftReqApplied)
			return;

		DWORD oldProtect;
		VirtualProtect(reinterpret_cast<void*>(s_craftReqAddr), 5, PAGE_EXECUTE_READWRITE, &oldProtect);

		if (bEnable)
		{
			if (!s_craftReqTrampoline)
				s_craftReqTrampoline = allocate_near(s_craftReqAddr, 0x20);

			if (s_craftReqTrampoline)
			{
				uint8_t* tramp = reinterpret_cast<uint8_t*>(s_craftReqTrampoline);
				size_t offset = 0;

				// mov dword ptr [r9+8], 0
				const uint8_t movZero[] = { 0x41, 0xC7, 0x41, 0x08, 0x00, 0x00, 0x00, 0x00 };
				memcpy(tramp + offset, movZero, sizeof(movZero));
				offset += sizeof(movZero);

				// ret - the real function's job was just to report "materials needed?",
				// so returning immediately (instead of jumping back) skips the whole
				// requirement check, same as the original Cheat Engine script.
				tramp[offset++] = 0xC3;

				int32_t relJmpFwd = static_cast<int32_t>(reinterpret_cast<uintptr_t>(s_craftReqTrampoline) - (s_craftReqAddr + 5));
				uint8_t patch[5];
				patch[0] = 0xE9;
				memcpy(patch + 1, &relJmpFwd, sizeof(relJmpFwd));

				memcpy(reinterpret_cast<void*>(s_craftReqAddr), patch, sizeof(patch));
				FlushInstructionCache(GetCurrentProcess(), s_craftReqTrampoline, offset);
			}
		}
		else
		{
			const uint8_t original[5] = { 0x48, 0x89, 0x5C, 0x24, 0x18 };
			memcpy(reinterpret_cast<void*>(s_craftReqAddr), original, sizeof(original));
		}

		DWORD tmp;
		VirtualProtect(reinterpret_cast<void*>(s_craftReqAddr), 5, oldProtect, &tmp);
		FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(s_craftReqAddr), 5);

		s_craftReqApplied = bEnable;

		// Debug_NotConsumeMaterialsInCraft (real Debug_ NetServer RPC) and
		// UPalCheatManager::NotConsumeMaterialsInCraft (real local Exec command) -
		// kept stacked alongside the AOB patch above in case either happens to help
		// on a server that does validate admin; harmless no-ops otherwise.
		static bool s_craftDebugApplied = false;
		if (bEnable != s_craftDebugApplied)
		{
			if (auto* pc = Config.GetPalPlayerController())
				pc->Debug_NotConsumeMaterialsInCraft();
			s_craftDebugApplied = bEnable;
		}

		static bool s_craftCheatMgrApplied = false;
		if (bEnable != s_craftCheatMgrApplied)
		{
			if (auto* pc = Config.GetPalPlayerController())
			{
				if (auto* cheatManager = (SDK::UPalCheatManager*)pc->CheatManager)
					cheatManager->NotConsumeMaterialsInCraft();
			}
			s_craftCheatMgrApplied = bEnable;
		}
	}

	// Scans the WHOLE module for the n-th (1-based) occurrence of an exact byte
	// pattern (no wildcards). Needed for itemsNeededForBuilding below: the struct-
	// copy function this patches is byte-IDENTICAL to another, unrelated struct-
	// copy elsewhere in the exe (same field layout, different types), so the
	// regular first-match-only signature() helper can't tell them apart - the
	// original Cheat Engine script also had to pick "targetIndex 2" for the same
	// reason.
	static uintptr_t FindNthPattern(const uint8_t* pattern, size_t patternLen, int n)
	{
		HMODULE mod = GetModuleHandle(nullptr);
		auto dos = reinterpret_cast<PIMAGE_DOS_HEADER>(mod);
		auto nt = reinterpret_cast<PIMAGE_NT_HEADERS>(reinterpret_cast<uint8_t*>(mod) + dos->e_lfanew);
		size_t imageSize = nt->OptionalHeader.SizeOfImage;
		const uint8_t* scan = reinterpret_cast<const uint8_t*>(mod);

		int count = 0;
		for (size_t i = 0; i + patternLen <= imageSize; ++i)
		{
			if (memcmp(scan + i, pattern, patternLen) == 0)
			{
				++count;
				if (count == n)
					return reinterpret_cast<uintptr_t>(scan + i);
			}
		}
		return 0;
	}

	// itemsNeededForBuilding: copies four "amount required" fields (each a
	// 3-byte "mov eax,[rdx+X]" / equivalent) out of a source struct while
	// evaluating whether a building can be placed. NOP-ing each load to
	// "xor eax,eax; nop" (31 C0 90) makes every required-item count read back
	// as 0, so the placement check always passes. Also from the shared Cheat
	// Engine table (Byakuran) - its AOB still matches this build exactly.
	//
	// Doesn't work: building still requires materials same as before. Same
	// story as CanCraftAllItems below - an exact byte-level AOB match doesn't
	// guarantee the function still plays the same logical role in this build.
	// Hard-disabled (never applied) so it can't silently do nothing while
	// claiming to be "on".
	static const uint8_t s_buildPattern[] = {
		0x8B, 0x42, 0x3C, 0x89, 0x41, 0x3C, 0x48, 0x8B, 0x42, 0x40, 0x48, 0x89, 0x41, 0x40,
		0x8B, 0x42, 0x48, 0x89, 0x41, 0x48, 0x48, 0x8B, 0x42, 0x4C, 0x48, 0x89, 0x41, 0x4C,
		0x8B, 0x42, 0x54, 0x89, 0x41, 0x54, 0x48, 0x8B, 0x42, 0x58, 0x48, 0x89, 0x41, 0x58,
		0x8B, 0x42, 0x60, 0x89, 0x41, 0x60, 0x48, 0x8B, 0x42, 0x64, 0x48, 0x89, 0x41, 0x64,
		0x48, 0x8B, 0x42, 0x6C, 0x48, 0x89, 0x41, 0x6C,
	};
	static const uint32_t s_buildPatchOffsets[4] = { 0x00, 0x0E, 0x1C, 0x2A };
	static const uint8_t s_buildPatchOriginal[4][3] = {
		{ 0x8B, 0x42, 0x3C }, { 0x8B, 0x42, 0x48 }, { 0x8B, 0x42, 0x54 }, { 0x8B, 0x42, 0x60 },
	};
	static const uint8_t s_buildPatchNop[3] = { 0x31, 0xC0, 0x90 };

	static uintptr_t s_buildReqAddr = 0;
	static bool s_buildReqResolved = false;
	static bool s_buildReqApplied = false;

	// Same story as CanCraftAllItems - APalPlayerController::Debug_NotConsumeMaterialsInBuild()
	// is a real Debug_ NetServer RPC, no parameters, toggled per-call.
	void SetCanBuildAllBuildings(bool bEnable)
	{
		if (!s_buildReqResolved)
		{
			s_buildReqResolved = true;
			s_buildReqAddr = FindNthPattern(s_buildPattern, sizeof(s_buildPattern), 2);
			if (!s_buildReqAddr)
				g_Console->printdbg("[-] CanBuildAllBuildings: signature not found!\n", Console::Colors::red);
		}

		bool bApplyAOB = false; // hard-disabled - see comment above, confirmed to not actually unlock building
		if (s_buildReqAddr && bApplyAOB != s_buildReqApplied)
		{
			for (int i = 0; i < 4; i++)
			{
				uintptr_t addr = s_buildReqAddr + s_buildPatchOffsets[i];
				const uint8_t* bytes = bApplyAOB ? s_buildPatchNop : s_buildPatchOriginal[i];

				DWORD oldProtect;
				VirtualProtect(reinterpret_cast<void*>(addr), 3, PAGE_EXECUTE_READWRITE, &oldProtect);
				memcpy(reinterpret_cast<void*>(addr), bytes, 3);
				DWORD tmp;
				VirtualProtect(reinterpret_cast<void*>(addr), 3, oldProtect, &tmp);
				FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(addr), 3);
			}
			s_buildReqApplied = bApplyAOB;
		}

		static bool s_buildDebugApplied = false;
		if (bEnable != s_buildDebugApplied)
		{
			if (auto* pc = Config.GetPalPlayerController())
				pc->Debug_NotConsumeMaterialsInBuild();
			s_buildDebugApplied = bEnable;
		}

		// UPalCheatManager::NotConsumeMaterialsInBuild() - same story as the Craft
		// version above (found via .idmap), a real local "Exec" console command.
		// Unlike Build/Craft's Debug_ RPCs, UPalCheatManager exposes a matching
		// getter (IsNotConsumeMaterialsInBuild) for this one specific toggle - so
		// this self-corrects every tick instead of firing once on the edge, in case
		// something else resets the underlying flag between our calls (no way to
		// tell without a getter, which is exactly why Craft below stays edge-
		// triggered instead).
		if (auto* pc = Config.GetPalPlayerController())
		{
			if (auto* cheatManager = (SDK::UPalCheatManager*)pc->CheatManager)
			{
				if (cheatManager->IsNotConsumeMaterialsInBuild() != bEnable)
					cheatManager->NotConsumeMaterialsInBuild();
			}
		}
	}

	// APalWeaponBase::IsFullMagazine - force it to always return true (al=1), so the
	// game never thinks it needs to consume ammo/spheres to "top up" the magazine.
	// Located via the surrounding, more distinctive instructions (the actual
	// "cmp [rax+7C],edx; setge al" pair is only 6 bytes and not unique enough alone).
	static BytePatch s_ammoPatch = {
		"48 8B 81 B0 04 00 00 48 85 C0 74 07 39 50 7C 0F 9D C0",
		12, // reach the "cmp [rax+7C],edx" (3) + "setge al" (3) pair, 6 bytes total
		{ 0xB0, 0x01, 0xC3, 0x90, 0x90, 0x90 },
		{ 0x39, 0x50, 0x7C, 0x0F, 0x9D, 0xC0 },
	};

	void SetUnlimitedAmmoNative(bool bEnable)
	{
		ApplyBytePatch(s_ammoPatch, bEnable, "UnlimitedAmmoNative");
	}

	// Dodge-roll weight-speed-penalty field gets forced to 0 every time it's about
	// to be written, via a trampoline (safer than the original CE version - uses a
	// GP-register store instead of repurposing xmm6, which might still be needed
	// by the surrounding code).
	static uintptr_t s_weightlessAddr = 0;
	static bool s_weightlessResolved = false;
	static bool s_weightlessApplied = false;

	void SetWeightlessDodge(bool bEnable)
	{
		if (auto* debug = SDK::UPalUtility::GetPalDebugSetting())
			debug->bIgnoreOverWeightMove = bEnable;

		if (!s_weightlessResolved)
		{
			s_weightlessResolved = true;
			s_weightlessAddr = signature("F3 0F 11 B3 60 01 00 00 72").GetPointer();
		}

		if (!s_weightlessAddr || bEnable == s_weightlessApplied)
			return;

		DWORD oldProtect;
		VirtualProtect(reinterpret_cast<void*>(s_weightlessAddr), 8, PAGE_EXECUTE_READWRITE, &oldProtect);

		if (bEnable)
		{
			void* trampoline = allocate_near(s_weightlessAddr, 0x30);
			if (!trampoline)
			{
				g_Console->printdbg("[-] WeightlessDodge: could not allocate trampoline!\n", Console::Colors::red);
				DWORD tmp2;
				VirtualProtect(reinterpret_cast<void*>(s_weightlessAddr), 8, oldProtect, &tmp2);
				return;
			}

			uint8_t* tramp = reinterpret_cast<uint8_t*>(trampoline);
			size_t offset = 0;

			// mov dword ptr [rbx+0x160], 0
			const uint8_t movZero[] = { 0xC7, 0x83, 0x60, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
			memcpy(tramp + offset, movZero, sizeof(movZero));
			offset += sizeof(movZero);

			uintptr_t returnAddr = s_weightlessAddr + 8;
			uintptr_t jmpBackSrc = reinterpret_cast<uintptr_t>(tramp + offset) + 5;
			int32_t relJmpBack = static_cast<int32_t>(returnAddr - jmpBackSrc);
			tramp[offset++] = 0xE9;
			memcpy(tramp + offset, &relJmpBack, sizeof(relJmpBack));
			offset += sizeof(relJmpBack);

			int32_t relJmpFwd = static_cast<int32_t>(reinterpret_cast<uintptr_t>(trampoline) - (s_weightlessAddr + 5));
			uint8_t patch[8];
			patch[0] = 0xE9;
			memcpy(patch + 1, &relJmpFwd, sizeof(relJmpFwd));
			patch[5] = patch[6] = patch[7] = 0x90;

			memcpy(reinterpret_cast<void*>(s_weightlessAddr), patch, sizeof(patch));
			FlushInstructionCache(GetCurrentProcess(), trampoline, offset);
		}
		else
		{
			const uint8_t original[8] = { 0xF3, 0x0F, 0x11, 0xB3, 0x60, 0x01, 0x00, 0x00 };
			memcpy(reinterpret_cast<void*>(s_weightlessAddr), original, sizeof(original));
		}

		DWORD tmp;
		VirtualProtect(reinterpret_cast<void*>(s_weightlessAddr), 8, oldProtect, &tmp);
		FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(s_weightlessAddr), 8);

		s_weightlessApplied = bEnable;
	}

	// InfPalballs / throwable items (Pal Spheres etc.): also from the shared Cheat
	// Engine table (Byakuran, "Inf Sphere") - its AOB still matches this build
	// exactly. Patches the comparison inside the throwable old-vs-new stack-count
	// check: "cmp [r8],eax" (41 39 00) becomes "mov [r8],eax" (41 89 00), turning
	// the compare into a store, so the code path that would otherwise detect "one
	// was consumed" and decrement the stack never sees a change.
	//
	// Doesn't work: spheres still get consumed on throw. Same pattern as
	// Craft/Build above - byte-identical AOB match doesn't guarantee the same
	// logical role in this build. Left dormant (ApplyBytePatch is never called
	// with bEnable=true for it - see SetNoConsumeSphereAndAmmo).
	static BytePatch s_noConsumeBallsPatch = {
		"41 39 00 75 15 F3 41 0F 10 82 58 01 00 00 0F 2E 43 F8",
		0,
		{ 0x41, 0x89, 0x00 },
		{ 0x41, 0x39, 0x00 },
	};

	// InfAmmo: flips "jg" (0x7F) to "je" (0x74) in a magazine/ammo-count comparison,
	// inverting which branch is taken.
	static BytePatch s_noConsumeAmmoPatch = {
		"7F ? 32 ? 48 83 ? ? C3 8D",
		0,
		{ 0x74, 0x07 },
		{ 0x7F, 0x07 },
	};

	// APalWeaponBase::RequestConsumeItem/RequestConsumeItem_ForThrowWeapon are the
	// real entry points the game calls to deduct ammo (regular weapons) and Pal
	// Spheres (throw weapons use the same base class) from the inventory. Instead
	// of patching compiled machine code (fragile - shifts with every recompile),
	// this hooks the UFunction's own native function pointer directly
	// (UFunction::ExecFunction, exposed in the SDK at CoreUObject_classes.hpp:509 -
	// "NOT AUTO-GENERATED PROPERTY" but a real, stable engine field, not a byte
	// offset guess) and replaces it with a no-op while enabled. This is much more
	// version-resilient than an AOB patch: it only depends on the function's name
	// still existing in the reflection system, not on exact compiled byte layout.
	static void __cdecl NoConsumeItemHook(void* Context, void* TheStack, void* Result) {}

	// APalWeaponBase::IsFullMagazine is a simple bool-returning query (found by
	// name in the .idmap exec-function dump: "execIsFullMagazine" - confirms the
	// exact UFunction name without needing a byte-pattern guess). Forcing it to
	// always report "already full" is much lower-risk than the RequestConsumeItem
	// no-op above: it doesn't skip any state-mutating logic, just answers a
	// question differently, so it shouldn't be able to soft-lock anything the way
	// skipping the consume call did.
	static void __cdecl AlwaysTrueBoolHook(void* Context, void* TheStack, void* Result)
	{
		if (Result)
			*reinterpret_cast<bool*>(Result) = true;
	}

	struct ExecFunctionHook
	{
		SDK::UClass* (*getOwnerClass)();
		const char* className;
		const char* funcName;
		SDK::UFunction* func = nullptr;
		SDK::UFunction::FNativeFuncPtr original = nullptr;
		bool resolved = false;
		bool applied = false;
	};
	static ExecFunctionHook s_noConsumeItemHook = { &SDK::APalWeaponBase::StaticClass, "PalWeaponBase", "RequestConsumeItem" };
	static ExecFunctionHook s_noConsumeThrowHook = { &SDK::APalWeaponBase::StaticClass, "PalWeaponBase", "RequestConsumeItem_ForThrowWeapon" };
	static ExecFunctionHook s_isFullMagazineHook = { &SDK::APalWeaponBase::StaticClass, "PalWeaponBase", "IsFullMagazine" };
	static ExecFunctionHook s_useBulletHook = { &SDK::UPalDynamicWeaponItemDataBase::StaticClass, "PalDynamicWeaponItemDataBase", "UseBullet" };

	static void ApplyExecFunctionHook(ExecFunctionHook& hook, bool bEnable, const char* debugName, SDK::UFunction::FNativeFuncPtr hookFn)
	{
		if (!hook.resolved)
		{
			hook.resolved = true;
			hook.func = hook.getOwnerClass()->GetFunction(hook.className, hook.funcName);
			if (!hook.func)
				g_Console->printdbg("[-] %s: UFunction not found!\n", Console::Colors::red, debugName);
			else
				hook.original = hook.func->ExecFunction;
		}

		if (!hook.func || bEnable == hook.applied)
			return;

		hook.func->ExecFunction = bEnable ? hookFn : hook.original;
		hook.applied = bEnable;
	}

	void SetNoConsumeSphereAndAmmo(bool bEnable)
	{
		ApplyBytePatch(s_noConsumeBallsPatch, false, "NoConsumeSphere"); // hard-disabled - doesn't work, see comment above
		ApplyBytePatch(s_noConsumeAmmoPatch, bEnable, "NoConsumeAmmo"); // dormant unless this build's sig happens to resolve

		// Freezes the game when throwing a Pal Sphere (the character gets stuck
		// mid-throw, evidently because RequestConsumeItem_ForThrowWeapon's native
		// body does something the throw action's input-lock state depends on
		// that a full no-op skips). Hard-disabled here regardless of what the
		// caller passes, on top of the GUI checkbox already being hidden, so
		// this can't be re-enabled by accident from either side. Re-enable only
		// after changing NoConsumeItemHook to call through to the original
		// function with the count parameter suppressed instead of skipping the
		// whole call.
		(void)bEnable;
		ApplyExecFunctionHook(s_noConsumeItemHook, false, "NoConsumeItem(ExecFunction)", &NoConsumeItemHook);
		ApplyExecFunctionHook(s_noConsumeThrowHook, false, "NoConsumeItemForThrow(ExecFunction)", &NoConsumeItemHook);

		// IsFullMagazine hook - see comment above AlwaysTrueBoolHook. Lower risk
		// than the RequestConsumeItem no-op, left enabled.
		ApplyExecFunctionHook(s_isFullMagazineHook, bEnable, "IsFullMagazine(ExecFunction)", &AlwaysTrueBoolHook);
	}

	// Risky: UPalDynamicWeaponItemDataBase::UseBullet() is a plain local
	// function (Final, Native, Public, BlueprintCallable - no Net flag, so not
	// admin-gated like the Debug_ RPCs) that appears to be the actual per-shot
	// ammo deduction call, next to SetBulletsNum/RemainingBullets (the real
	// backing field). Forcing it to always report "used a bullet successfully"
	// without running its real body is the same category of hook that froze
	// the game on Pal Sphere throw (RequestConsumeItem_ForThrowWeapon above) -
	// skipping a native function's body can skip more than the obvious state
	// change (muzzle flash/recoil/animation-notify bookkeeping tied to firing).
	// Kept as a separate, explicitly opt-in toggle instead of bundling it into
	// SetNoConsumeSphereAndAmmo so a freeze here doesn't come as a surprise via
	// an already-trusted checkbox. Save your game before testing this.
	void SetNoUseBulletHook(bool bEnable)
	{
		ApplyExecFunctionHook(s_useBulletHook, bEnable, "UseBullet(ExecFunction)", &AlwaysTrueBoolHook);
	}

	// Adds INT32_MAX to rdx right before it's stored as the new money value.
	static uintptr_t s_magicPurseAddr = 0;
	static bool s_magicPurseResolved = false;
	static bool s_magicPurseApplied = false;

	void SetMagicPurse(bool bEnable)
	{
		// Real Debug_AddMoney_ToServer NetServer RPC, still present on
		// APalPlayerController in the shipping game. Edge-triggered (only fires on
		// the off->on transition) so it doesn't spam a server RPC every tick -
		// re-toggle the checkbox to top the purse back up after spending it down.
		static bool s_magicPurseWasEnabled = false;
		if (bEnable && !s_magicPurseWasEnabled)
		{
			if (auto* pc = Config.GetPalPlayerController())
				pc->Debug_AddMoney_ToServer(100000000);
		}
		s_magicPurseWasEnabled = bEnable;

		if (!s_magicPurseResolved)
		{
			s_magicPurseResolved = true;
			s_magicPurseAddr = signature("48 89 57 50 48 89 54 24 30").GetPointer();
		}

		if (!s_magicPurseAddr || bEnable == s_magicPurseApplied)
			return;

		DWORD oldProtect;
		VirtualProtect(reinterpret_cast<void*>(s_magicPurseAddr), 9, PAGE_EXECUTE_READWRITE, &oldProtect);

		if (bEnable)
		{
			void* trampoline = allocate_near(s_magicPurseAddr, 0x30);
			if (!trampoline)
			{
				g_Console->printdbg("[-] MagicPurse: could not allocate trampoline!\n", Console::Colors::red);
				DWORD tmp2;
				VirtualProtect(reinterpret_cast<void*>(s_magicPurseAddr), 9, oldProtect, &tmp2);
				return;
			}

			uint8_t* tramp = reinterpret_cast<uint8_t*>(trampoline);
			size_t offset = 0;

			// add rdx, 0x7FFFFFFF
			const uint8_t addRdx[] = { 0x48, 0x81, 0xC2, 0xFF, 0xFF, 0xFF, 0x7F };
			memcpy(tramp + offset, addRdx, sizeof(addRdx));
			offset += sizeof(addRdx);

			// mov [rdi+50], rdx
			const uint8_t movRdi[] = { 0x48, 0x89, 0x57, 0x50 };
			memcpy(tramp + offset, movRdi, sizeof(movRdi));
			offset += sizeof(movRdi);

			// mov [rsp+30], rdx
			const uint8_t movRsp[] = { 0x48, 0x89, 0x54, 0x24, 0x30 };
			memcpy(tramp + offset, movRsp, sizeof(movRsp));
			offset += sizeof(movRsp);

			uintptr_t returnAddr = s_magicPurseAddr + 9;
			uintptr_t jmpBackSrc = reinterpret_cast<uintptr_t>(tramp + offset) + 5;
			int32_t relJmpBack = static_cast<int32_t>(returnAddr - jmpBackSrc);
			tramp[offset++] = 0xE9;
			memcpy(tramp + offset, &relJmpBack, sizeof(relJmpBack));
			offset += sizeof(relJmpBack);

			int32_t relJmpFwd = static_cast<int32_t>(reinterpret_cast<uintptr_t>(trampoline) - (s_magicPurseAddr + 5));
			uint8_t patch[9];
			patch[0] = 0xE9;
			memcpy(patch + 1, &relJmpFwd, sizeof(relJmpFwd));
			patch[5] = patch[6] = patch[7] = patch[8] = 0x90;

			memcpy(reinterpret_cast<void*>(s_magicPurseAddr), patch, sizeof(patch));
			FlushInstructionCache(GetCurrentProcess(), trampoline, offset);
		}
		else
		{
			const uint8_t original[9] = { 0x48, 0x89, 0x57, 0x50, 0x48, 0x89, 0x54, 0x24, 0x30 };
			memcpy(reinterpret_cast<void*>(s_magicPurseAddr), original, sizeof(original));
		}

		DWORD tmp;
		VirtualProtect(reinterpret_cast<void*>(s_magicPurseAddr), 9, oldProtect, &tmp);
		FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(s_magicPurseAddr), 9);

		s_magicPurseApplied = bEnable;
	}

	// "mov edi,0x65" (BF 65 00 00 00) -> "mov ebx,100" (BB 64 00 00 00). Ported
	// verbatim from a community CE script; exact effect on Pal capture/spawn stat
	// rolls is not independently verified.
	static BytePatch s_palPerfectRollPatch = {
		"?? 65 00 00 00 2B ?? 85",
		0,
		{ 0xBB, 0x64, 0x00, 0x00, 0x00 },
		{ 0xBF, 0x65, 0x00, 0x00, 0x00 },
	};

	void SetPalPerfectStatRoll(bool bEnable)
	{
		ApplyBytePatch(s_palPerfectRollPatch, bEnable, "PalPerfectStatRoll");

		if (auto* debug = SDK::UPalUtility::GetPalDebugSetting())
			debug->ForceFixTalent = bEnable ? 100 : 0;
	}

	// Original: movd xmm1,[rbx+0x4BC]  (32-bit load, 8 bytes incl. displacement)
	// Replacement (all 32-bit, matching the original operand width exactly):
	//   push rax
	//   mov eax,[rbx+0x4BC]
	//   imul eax,eax,0x10000
	//   movd xmm1,eax
	//   pop rax
	// A previous version of this patch used 64-bit rax/movq throughout ("ported as
	// close to the CE script as possible"). That was a real bug: [rbx+0x4BC] is a
	// 32-bit field, so `mov rax,[...]` over-read into the next field, and doing the
	// imul/movq at 64-bit width mathematically discards the top 16 bits of the
	// intended value once only the low 32-bit lane is consumed downstream by
	// cvtdq2ps/mulss - likely why the effect looked "cosmetic" in-game. Fixed to
	// operate on eax/32-bit throughout, matching the original instruction exactly.
	static uintptr_t s_workSpeedAddr = 0;
	static bool s_workSpeedResolved = false;
	static bool s_workSpeedApplied = false;

	void SetWorkSpeedAccelerator(bool bEnable)
	{
		if (auto* debug = SDK::UPalUtility::GetPalDebugSetting())
			debug->WorkExtraRate = bEnable ? Config.WorkSpeedExtraRate : 0.0f;

		if (!s_workSpeedResolved)
		{
			s_workSpeedResolved = true;
			s_workSpeedAddr = signature("66 0F 6E 8B BC 04 00 00").GetPointer();
			if (!s_workSpeedAddr)
				g_Console->printdbg("[-] WorkSpeedAccelerator: signature not found!\n", Console::Colors::red);
		}

		if (!s_workSpeedAddr || bEnable == s_workSpeedApplied)
			return;

		DWORD oldProtect;
		VirtualProtect(reinterpret_cast<void*>(s_workSpeedAddr), 8, PAGE_EXECUTE_READWRITE, &oldProtect);

		if (bEnable)
		{
			void* trampoline = allocate_near(s_workSpeedAddr, 0x40);
			if (!trampoline)
			{
				g_Console->printdbg("[-] WorkSpeedAccelerator: could not allocate trampoline!\n", Console::Colors::red);
				DWORD tmp2;
				VirtualProtect(reinterpret_cast<void*>(s_workSpeedAddr), 8, oldProtect, &tmp2);
				return;
			}

			uint8_t* tramp = reinterpret_cast<uint8_t*>(trampoline);
			size_t offset = 0;

			// push rax
			tramp[offset++] = 0x50;

			// mov eax,[rbx+0x4BC]  (32-bit load, matches the original field width)
			const uint8_t movEax[] = { 0x8B, 0x83, 0xBC, 0x04, 0x00, 0x00 };
			memcpy(tramp + offset, movEax, sizeof(movEax));
			offset += sizeof(movEax);

			// imul eax,eax,0x10000  (32-bit multiply, no upper-bit truncation bug)
			const uint8_t imulEax[] = { 0x69, 0xC0, 0x00, 0x00, 0x01, 0x00 };
			memcpy(tramp + offset, imulEax, sizeof(imulEax));
			offset += sizeof(imulEax);

			// movd xmm1,eax  (32-bit move, no REX.W)
			const uint8_t movdXmm1[] = { 0x66, 0x0F, 0x6E, 0xC8 };
			memcpy(tramp + offset, movdXmm1, sizeof(movdXmm1));
			offset += sizeof(movdXmm1);

			// pop rax
			tramp[offset++] = 0x58;

			uintptr_t returnAddr = s_workSpeedAddr + 8;
			uintptr_t jmpBackSrc = reinterpret_cast<uintptr_t>(tramp + offset) + 5;
			int32_t relJmpBack = static_cast<int32_t>(returnAddr - jmpBackSrc);
			tramp[offset++] = 0xE9;
			memcpy(tramp + offset, &relJmpBack, sizeof(relJmpBack));
			offset += sizeof(relJmpBack);

			int32_t relJmpFwd = static_cast<int32_t>(reinterpret_cast<uintptr_t>(trampoline) - (s_workSpeedAddr + 5));
			uint8_t patch[8];
			patch[0] = 0xE9;
			memcpy(patch + 1, &relJmpFwd, sizeof(relJmpFwd));
			patch[5] = patch[6] = patch[7] = 0x90;

			memcpy(reinterpret_cast<void*>(s_workSpeedAddr), patch, sizeof(patch));
			FlushInstructionCache(GetCurrentProcess(), trampoline, offset);
		}
		else
		{
			const uint8_t original[8] = { 0x66, 0x0F, 0x6E, 0x8B, 0xBC, 0x04, 0x00, 0x00 };
			memcpy(reinterpret_cast<void*>(s_workSpeedAddr), original, sizeof(original));
		}

		DWORD tmp;
		VirtualProtect(reinterpret_cast<void*>(s_workSpeedAddr), 8, oldProtect, &tmp);
		FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(s_workSpeedAddr), 8);

		s_workSpeedApplied = bEnable;
	}

	void Hooking::Unhook()
	{
		g_D3D11Window->Unhook();
		MH_DisableHook((Tick)(Config.ClientBase + Config.offset_Tick));
		MH_RemoveHook((Tick)(Config.ClientBase + Config.offset_Tick));
		g_Console->DestroyConsole();
		g_Running = FALSE;
		return;
	}
}
