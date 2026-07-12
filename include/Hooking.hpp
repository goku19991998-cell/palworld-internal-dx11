#pragma once
#include "helper.h"
#include "Console.hpp"
#include "Game.hpp"
#include "D3D11Window.hpp"

namespace DX11_Base {
	class Hooking
	{
	public:

		explicit Hooking();
		~Hooking() noexcept;
		Hooking(Hooking const&) = delete;
		Hooking(Hooking&&) = delete;
		Hooking& operator=(Hooking const&) = delete;
		Hooking& operator=(Hooking&&) = delete;

		void Hook();
		void Unhook();
	};
	inline std::unique_ptr<Hooking> g_Hooking;

	// Manual inline code-cave hook that multiplies outgoing damage by Config.DamageUp,
	// only when Config.IsAttackModiler is enabled. Patches the native damage-calc
	// instruction directly (AttackUp on the character component is Transient and gets
	// recalculated every tick, so writing to it has no lasting effect).
	void InstallDamageMultiplierHook();

	// Locates the 2-byte "cmp" that gates map-object (chest/resource) respawn timing
	// inside APalMapObjectSpawnerBase::TryRespawnFor. SetInstantMapObjectRespawn(true)
	// overwrites those 2 bytes with an unconditional short jump that skips the elapsed-
	// time check entirely (so objects are always eligible to respawn); false restores
	// the original bytes. Safe to call every tick - it's a no-op once already applied.
	void SetInstantMapObjectRespawn(bool bEnable);

	// Simple direct byte patches (find via AOB, toggle between two fixed byte
	// sequences at a fixed offset from the match). Dormant no-ops on the current
	// game version (signature doesn't resolve) - kept in case it matches a future
	// build. Each of these now ALSO writes the equivalent field on the game's own
	// UPalDebugSetting object (Pal.PalUtility.GetPalDebugSetting - a real developer
	// cheat-flags singleton, reflection-callable, left in the shipping game) as the
	// primary, much more reliable fix. Both paths are safe to leave stacked: the
	// dormant ASM path only ever activates if its signature happens to resolve.
	void SetCanCraftAllItems(bool bEnable);
	void SetCanBuildAllBuildings(bool bEnable);
	void SetUnlimitedAmmoNative(bool bEnable);

	// Dormant ASM trampoline (see above) + now also sets
	// UPalDebugSetting::bIgnoreOverWeightMove, a purpose-built dev flag for exactly
	// this ("ignore over-weight move penalty") - should no longer break rolling the
	// way a raw field-zeroing patch might.
	void SetWeightlessDodge(bool bEnable);

	// Dormant ASM patches (see above) + now also sets
	// UPalDebugSetting::bNotRequiredBulletWhenReload for the ammo half. No
	// equivalent dev flag found for sphere/ball consumption specifically - Freeze
	// Slot / Freeze Pal Sphere (feature.cpp) already cover that case by topping the
	// stack back up via a real server call.
	void SetNoConsumeSphereAndAmmo(bool bEnable);

	// Risky: hooks UPalDynamicWeaponItemDataBase::UseBullet() (the likely real
	// per-shot ammo deduction call) to always report success without running
	// its real body. Same hook category that froze the game on Pal Sphere
	// throw via RequestConsumeItem - could do the same here. Save before
	// testing. See Hooking.cpp for details.
	void SetNoUseBulletHook(bool bEnable);

	// Dormant ASM trampoline (see above) + now also calls the real
	// APalPlayerController::Debug_AddMoney_ToServer(int64) RPC (a genuine "Debug_"
	// server command left in the shipping game) once per off->on transition, adding
	// a large lump sum. Edge-triggered rather than per-tick to avoid spamming a
	// NetServer RPC 60x/second.
	void SetMagicPurse(bool bEnable);

	// Dormant ASM patch (see above) + now also sets UPalDebugSetting::ForceFixTalent
	// to 100 (0 = disabled, matches the field's own ZeroConstructor default). Only
	// affects pals rolled/captured AFTER this is enabled - won't retroactively fix
	// pals already in your party (use the Pal Editor's Talent fields for that).
	void SetPalPerfectStatRoll(bool bEnable);

	// Dormant ASM trampoline (see above) + now also sets
	// UPalDebugSetting::WorkExtraRate to Config.WorkSpeedExtraRate (0 = disabled).
	void SetWorkSpeedAccelerator(bool bEnable);
}