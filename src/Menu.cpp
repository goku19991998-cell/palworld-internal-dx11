#include "../pch.h"
#include "../include/Menu.hpp"
#include "SDK.hpp"
#include "config.h"
#include <algorithm>

int InputTextCallback(ImGuiInputTextCallbackData* data) {
    char inputChar = data->EventChar;

    Config.Update(Config.inputTextBuffer);

    return 0;
}

// Human-readable label for a Win32 virtual-key code, for the keybind UI. Mouse
// buttons don't have scancode-based names (GetKeyNameTextA is keyboard-only), so
// they're special-cased; everything else goes through the normal Win32 API.
static std::string VKToDisplayName(int vk)
{
    switch (vk)
    {
    case 0: return "(unbound)";
    case VK_LBUTTON: return "MB1";
    case VK_RBUTTON: return "MB2";
    case VK_MBUTTON: return "MB3";
    case VK_XBUTTON1: return "MB4";
    case VK_XBUTTON2: return "MB5";
    // Shortened - these are otherwise the longest names GetKeyNameTextA returns
    // and don't fit well on a compact button.
    case VK_LCONTROL: return "LCtrl";
    case VK_RCONTROL: return "RCtrl";
    case VK_LSHIFT: return "LShift";
    case VK_RSHIFT: return "RShift";
    case VK_LMENU: return "LAlt";
    case VK_RMENU: return "RAlt";
    case VK_PRIOR: return "PgUp";
    case VK_NEXT: return "PgDn";
    }

    UINT scanCode = MapVirtualKeyA(vk, MAPVK_VK_TO_VSC);
    if (scanCode == 0)
    {
        char buf[16];
        sprintf_s(buf, "VK 0x%02X", vk);
        return buf;
    }

    // Extended keys (arrows, ins/del/home/end/pgup/pgdn, numpad enter/divide, etc.)
    // need bit 24 set in the "lParam" GetKeyNameTextA expects, or they resolve to
    // the wrong (non-extended) key's name.
    switch (vk)
    {
    case VK_LEFT: case VK_UP: case VK_RIGHT: case VK_DOWN:
    case VK_PRIOR: case VK_NEXT: case VK_END: case VK_HOME:
    case VK_INSERT: case VK_DELETE: case VK_DIVIDE: case VK_NUMLOCK:
        scanCode |= 0x100;
        break;
    }

    char name[64] = { 0 };
    LONG lParam = static_cast<LONG>(scanCode << 16);
    if (GetKeyNameTextA(lParam, name, sizeof(name)) == 0)
    {
        char buf[16];
        sprintf_s(buf, "VK 0x%02X", vk);
        return buf;
    }
    return name;
}

// Generic click-to-bind UI: a button that shows the current binding (or "Bind
// Key" while unbound) - clicking it (re)starts capture mode, where it prompts
// for the next key/mouse press (Esc cancels) and stores the result in *outKey.
// Each call site needs its own persistent "is capturing" bool (a local static),
// and uniqueId must be unique per call site so the ImGui button IDs don't
// collide when multiple binds are shown on the same screen. Pass label = nullptr
// to skip the separate "label: current binding" text (the button already shows
// the binding) - useful when packing this onto an already-labeled row.
static void KeybindUI(const char* label, const char* uniqueId, int* outKey, bool* isCapturing)
{
    if (label)
    {
        ImGui::Text("%s: %s", label, VKToDisplayName(*outKey).c_str());
        ImGui::SameLine();
    }

    // std::string (not const char*) - VKToDisplayName returns by value, so a raw
    // pointer captured from a temporary .c_str() here would dangle by the time
    // sprintf_s reads it.
    std::string btnText = *isCapturing ? "Press a key/mouse button (Esc to cancel)..."
        : (*outKey == 0 ? "Bind Key" : VKToDisplayName(*outKey));
    char btnLabel[128];
    sprintf_s(btnLabel, "%s##%s", btnText.c_str(), uniqueId);
    if (ImGui::Button(btnLabel))
        *isCapturing = !*isCapturing;

    if (!*isCapturing)
        return;

    if (GetAsyncKeyState(VK_ESCAPE) & 0x8000)
    {
        *isCapturing = false;
        *outKey = 0;
        return;
    }

    // ImGui::Button only returns true on release, so the mouse button that opened
    // capture mode is already back up by the time we get here - checking
    // "currently held" (0x8000) instead of "was pressed since last poll" (bit 0)
    // means that click can't self-trigger a bind.
    for (int vk = 1; vk < 255; vk++)
    {
        if (vk == VK_ESCAPE)
            continue;
        if (GetAsyncKeyState(vk) & 0x8000)
        {
            *outKey = vk;
            *isCapturing = false;
            break;
        }
    }
}

// database::db_items (the Database tab's searchable list) only has raw internal
// item IDs - unlike itemlist::* (the Item Spawner tab's lists), which pairs each
// ID with a friendly display name ("ID|Display Name"). Builds a lookup from the
// same itemlist:: data so the Database tab can show friendly names too, falling
// back to the raw ID for anything itemlist:: doesn't cover. Built once, lazily.
static const std::map<std::string, std::string>& GetItemDisplayNameMap()
{
    static std::map<std::string, std::string> map;
    if (map.empty())
    {
        auto addList = [](const auto& list)
        {
            for (const auto& entry : list)
            {
                std::string s = entry;
                auto pipePos = s.find('|');
                if (pipePos != std::string::npos)
                    map[s.substr(0, pipePos)] = s.substr(pipePos + 1);
            }
        };
        addList(itemlist::accessories);
        addList(itemlist::ammo);
        addList(itemlist::armor);
        addList(itemlist::craftingmaterials);
        addList(itemlist::eggs);
        addList(itemlist::food);
        addList(itemlist::hats);
        addList(itemlist::medicine);
        addList(itemlist::money);
        addList(itemlist::other);
        addList(itemlist::palspheres);
        addList(itemlist::seeds);
        addList(itemlist::tools);
        addList(itemlist::weapons);
    }
    return map;
}

namespace DX11_Base
{
    // helper variables
    char inputBuffer_getFnAddr[100];
    char inputBuffer_getClass[100];
    char inputBuffer_setWaypoint[32];

    namespace Styles 
    {
        void InitStyle()
        {
            ImGuiStyle& style = ImGui::GetStyle();
            ImVec4* colors = ImGui::GetStyle().Colors;

            //	STYLE PROPERTIES
            style.WindowTitleAlign = ImVec2(0.5f, 0.5f);
            ImGui::StyleColorsClassic();

            if (g_Menu->dbg_RAINBOW_THEME) 
            {
                //  RGB MODE STLYE PROPERTIES
                colors[ImGuiCol_Separator] = ImVec4(g_Menu->dbg_RAINBOW);
                colors[ImGuiCol_TitleBg] = ImVec4(0, 0, 0, 1.0f);
                colors[ImGuiCol_TitleBgActive] = ImVec4(0, 0, 0, 1.0f);
                colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0, 0, 0, 1.0f);
            }
            else 
            {
                /// YOUR DEFAULT STYLE PROPERTIES HERE
            }
        }
    }


    namespace Tabs 
    {
        void TABPlayer()
        {
            ImGui::Checkbox("Godmode", &Config.IsGodMode);

            ImGui::Checkbox("InfStamina", &Config.IsInfStamina);

            // "InfAmmo" checkbox hidden - Config.IsInfinAmmo is still read every tick
            // (see the Loops() call below), just decluttered. Only tops up ammo you
            // already own (capped at 99 via the dupe mechanism), can't conjure ammo
            // from nothing. Freeze Slot Count in the Duping tab does the same with
            // more control if needed.

            ImGui::Checkbox("AttackHack", &Config.IsAttackModiler);
            if (Config.IsAttackModiler)
            {
                ImGui::SameLine();
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                ImGui::SliderInt("##AttackMod", &Config.DamageUp, 1, 200000);
            }

            // Same idea as AttackHack, but for Defense - scales the equipped
            // armor's own DefenseValue instead of hooking incoming damage
            // (see SetArmorDefenseBoost in feature.cpp).
            ImGui::Checkbox("DefenseHack (Armor)", &Config.IsArmorDefenseHack);
            if (Config.IsArmorDefenseHack)
            {
                ImGui::SameLine();
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                ImGui::SliderFloat("##ArmorDefenseMod", &Config.ArmorDefenseMultiplier, 1.0f, 100.0f, "%.1fx");
            }
            // "Dump Armor Defense State" moved to the DEBUG tab, with the other Dump buttons.
            //ImGui::Checkbox("DefenseHack", &Config.IsDefuseModiler);

            // "SpeedHack" runs on SpeedHackAllInOne (Walk/Swim/Fly/Custom
            // CharacterMovementComponent fields) instead of the old
            // CustomTimeDilation-based SpeedHack() - that one had the
            // sped-up-cartoon look and didn't affect flying. SpeedHack()
            // itself is still in feature.cpp, just not called from here.
            // Capped at 3.32x, higher causes rubber-banding/desync.
            static bool s_isCapturingSpeedHackKey = false;
            ImGui::Checkbox("SpeedHack", &Config.IsSpeedHack);
            ImGui::SameLine();
            KeybindUI(nullptr, "speedhack", &Config.SpeedHackToggleKey, &s_isCapturingSpeedHackKey);
            if (Config.IsSpeedHack)
            {
                ImGui::SameLine();
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                ImGui::SliderFloat("##SpeedMod", &Config.SpeedModiflers, 1.0f, 3.32f, "%.2f");
            }

            ImGui::Checkbox("Max Weight Hack", &Config.IsMaxWeightHack);
            if (Config.IsMaxWeightHack)
            {
                ImGui::SameLine();
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                ImGui::SliderFloat("##MaxWeight", &Config.MaxWeightValue, 500.0f, 999999.0f);
            }

            // "Add Tech Points"/"Add Ancient Tech Points" used to be here,
            // direct-writing UPalTechnologyData::TechnologyPoint/bossTechnologyPoint.
            // Pulled since both fields are flagged Net/RepNotify in Pal_classes.hpp -
            // same category as bAdmin/UnusedStatusPoint/RemainingBullets, which don't
            // register when written directly via reflection (see the Duping tab
            // writeup / SetInfiniteAmmo comments). Untested, so moved to
            // TABNotWorking on suspicion rather than left presented as working.

            // FLY: the checkbox only ARMS the feature - checking it does NOT start
            // flying by itself. Actual flight only starts/stops when the bound key is
            // pressed in-game while armed (checked in Loops()). Unchecking while
            // actively flying also stops flight immediately, so unarming can't strand
            // the player mid-air.
            static bool s_isCapturingFlyKey = false;
            bool flyCheckboxChanged = ImGui::Checkbox("FLY", &Config.IsFlyEnabled);
            ImGui::SameLine();
            KeybindUI(nullptr, "fly", &Config.FlyToggleKey, &s_isCapturingFlyKey);
            if (flyCheckboxChanged && !Config.IsFlyEnabled && Config.IsToggledFly)
            {
                Config.IsToggledFly = false;
                ExploitFly(false);
            }
            if (Config.IsFlyEnabled)
                ImGui::Text("  Space = up, C = down");

            // Player-only ESP - reuses the existing, working ESP_DEBUG (proper world-to-
            // screen projection, distance-gated), just filtered to APalPlayerCharacter
            // instead of monsters. The original "ESP" checkbox/function is untouched.
            if (ImGui::Checkbox("Player ESP", &Config.IsPlayerESP) && !Config.IsPlayerESP)
                Config.mPlayerESPDistance = 1000.0f;
            if (Config.IsPlayerESP)
            {
                ImGui::SameLine();
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                ImGui::SliderFloat("##PLAYERESPDISTANCE", &Config.mPlayerESPDistance, 1.0f, 10000.0f, "%.0f", ImGuiSliderFlags_AlwaysClamp);
            }

            // Same as Player ESP above, just filtered to APalMonsterCharacter instead -
            // separate from the "DEBUG ESP" checkbox in the WIP tab, which reuses the
            // same underlying ESP_DEBUG but is capped to a 100-unit debug range.
            if (ImGui::Checkbox("Pal ESP", &Config.IsPalESP) && !Config.IsPalESP)
                Config.mPalESPDistance = 1000.0f;
            if (Config.IsPalESP)
            {
                ImGui::SameLine();
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                ImGui::SliderFloat("##PALESPDISTANCE", &Config.mPalESPDistance, 1.0f, 10000.0f, "%.0f", ImGuiSliderFlags_AlwaysClamp);
            }

            ImGui::Separator();
            // "New Name" input + "Set New Name" button hidden, not needed.
            // UpdateCharacterNickName_ToServer is still a real, working call, just not exposed.
            //ImGui::SliderInt("Defense Modifilers", &Config.DefuseUp, 1, 200000);

            // Moved here from the now-empty EXPLOIT tab.
            ImGui::Checkbox("Show Quick Tab", &Config.IsQuick);
            ImGui::Checkbox("Open Manager Menu", &Config.bisOpenManager);
            if (ImGui::Button("Unlock All Effigies", ImVec2(ImGui::GetContentRegionAvail().x - 3, 20)))
                UnlockAllEffigies();
            if (ImGui::Button("Revive/Fill HP", ImVec2(ImGui::GetContentRegionAvail().x - 3, 20)))
                ReviveLocalPlayer();

            // One-shot, rarely-needed button - doesn't need to be prominent.
            if (ImGui::Button("Reveal Whole Map", ImVec2(ImGui::GetContentRegionAvail().x - 3, 20)))
                RevealWholeMap();
        }

        // Emptied out - everything here moved to TABPlayer (Show Quick Tab, Open
        // Manager Menu, Unlock All Effigies, Revive/Fill HP) or TABDatabase (Item
        // Name/Num, Give item). "Show Teleporter Tab" removed entirely - the
        // Teleporter tab is now always shown. Not called from the tab bar anymore.
        void TABExploit()
        {
        }

        // Dupe mechanism: RequestMove_ToServer with a negative Num, moving a
        // slot onto itself - Num=-N grants +N of that item for free.
        // Give/Duplicate/Freeze all route through
        // IncrementInventoryItemCountByIndex, which uses this.
        void TABDuping()
        {
            ImGui::Text("Inventory Slots (Index, Item, Count):");
            if (ImGui::BeginChild("SlotOverviewTesting", ImVec2(0, 120), true))
            {
                for (const auto& line : GetInventorySlotOverview())
                {
                    if (ImGui::Selectable(line.c_str()))
                    {
                        Config.AddItemSlot = atoi(line.c_str() + 1);
                        Config.FreezeSlotIndex = Config.AddItemSlot;
                    }
                }
            }
            ImGui::EndChild();

            ImGui::InputInt("Slot to modify (start 0):", &Config.AddItemSlot);
            ImGui::InputInt("Quantity to add:", &Config.AddItemCount);
            if (ImGui::Button("Give items from slot", ImVec2(ImGui::GetContentRegionAvail().x - 3, 20)))
                IncrementInventoryItemCountByIndex(Config.AddItemCount, Config.AddItemSlot);
            if (ImGui::Button("Duplicate item in slot (doubles the stack)", ImVec2(ImGui::GetContentRegionAvail().x - 3, 20)))
                DuplicateItemInSlot(Config.AddItemSlot);
            if (ImGui::Button("Duplicate ALL slots (doubles every stack)", ImVec2(ImGui::GetContentRegionAvail().x - 3, 20)))
                DuplicateAllSlots();

            // Consumes the REAL item already in the selected slot (Tech Books, Elixirs,
            // etc.) via a single batch of RPCs fired once on click - not an auto-clicker.
            // Uses the same "Slot to modify" selector above.
            ImGui::Text("Use item in selected slot (fires once per click, not an auto-clicker):");
            if (ImGui::Button("Use x1", ImVec2(ImGui::GetContentRegionAvail().x / 3 - 3, 20)))
                UseInventorySlotItemMultiple(Config.AddItemSlot, 1);
            ImGui::SameLine();
            if (ImGui::Button("Use x10", ImVec2(ImGui::GetContentRegionAvail().x / 2 - 3, 20)))
                UseInventorySlotItemMultiple(Config.AddItemSlot, 10);
            ImGui::SameLine();
            if (ImGui::Button("Use x100", ImVec2(ImGui::GetContentRegionAvail().x - 3, 20)))
                UseInventorySlotItemMultiple(Config.AddItemSlot, 100);

            // "Freeze Slot Count"/"Freeze Pal Sphere"/"Force Save Now"/"Save slot name"
            // hidden - superseded by the item dupe above (Duplicate item in slot /
            // Duplicate ALL slots), which gives a real, persistent stack instead of
            // re-topping it up every tick. Nothing here is broken - Config.IsFreezeSlot/
            // IsFreezePalSphere are still read every tick in Loops(), just not shown.
#if 0
            ImGui::Checkbox("Freeze Slot Count", &Config.IsFreezeSlot);
            ImGui::InputInt("Slot to freeze", &Config.FreezeSlotIndex);
            ImGui::InputInt("Freeze at count", &Config.FreezeSlotTarget);

            ImGui::Separator();
            ImGui::InputText("Save slot name", Config.ForceSaveName, sizeof(Config.ForceSaveName));
            if (ImGui::Button("Force Save Now (test slot)", ImVec2(ImGui::GetContentRegionAvail().x - 3, 20)))
                ForceSaveWorld(Config.ForceSaveName);
#endif
        }

        void TABNotWorking()
        {
            // Alternative SpeedHack via MaxWalkSpeed instead of CustomTimeDilation -
            // moves the character faster without the sped-up-cartoon animation
            // look the real SpeedHack (Player tab) has. Separate function/state,
            // can't break the working one.
            if (ImGui::Checkbox("SpeedHack (Clean - MaxWalkSpeed)", &Config.IsSpeedHackClean))
                SpeedHackClean(Config.IsSpeedHackClean, Config.SpeedHackCleanMultiplier);
            if (Config.IsSpeedHackClean)
            {
                ImGui::SameLine();
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                // Capped at 3.32x, higher causes rubber-banding/desync.
                ImGui::SliderFloat("##SpeedHackCleanMult", &Config.SpeedHackCleanMultiplier, 1.0f, 3.32f, "%.2f");
            }

            // The All-In-One (Walk/Swim/Fly/Custom) version graduated to become
            // the real "SpeedHack" in the Player tab, since swimming/climbing/
            // flying need their own speed fields, not just MaxWalkSpeed - no
            // longer duplicated here.

            // ============================================================================
            // Everything below was in this tab and doesn't work, hidden but still
            // compiled into the DLL (see the #if 0 block at the bottom).
            //
            // Can Craft All Items / Can Build All Buildings / Sphere consumption were
            // re-implemented using AOB signatures from a shared Cheat Engine table
            // (author "Byakuran") for an earlier Palworld version. All three still
            // match this build byte-for-byte, which rules out the earlier
            // "admin rights required" theory - these are local/client-side checks,
            // not server-validated Debug_ RPCs - but all three are broken anyway:
            //  - Craft: workbench "max craftable" shows garbage (~2.14 billion /
            //    INT32_MAX), "not enough material" still blocks crafting.
            //  - Build: still requires materials same as before.
            //  - Sphere: still gets consumed on throw.
            // An exact byte-level AOB match doesn't guarantee the function still
            // plays the same logical role in this build - these are apparently
            // either UI-display-only helpers or not the real gameplay gate. All
            // three patches are hard-disabled in Hooking.cpp so they can't
            // corrupt state while doing nothing useful.
            // ============================================================================
            // "Dump Weapon Ammo State"/"Dump Movement Speed State" moved to the
            // DEBUG tab - they don't belong to the "doesn't work" theme of this
            // tab, and living here meant toggling WIP on/off just to reach them.
            // HIDDEN - doesn't work (or unconfirmed/risky), kept for reference only.
            // Still fully compiled into the DLL, just not shown in the UI:
            //  - Force Respawn Nearby Chests/Resources: real Debug_ RPC, never confirmed.
            //  - Magic Purse / Can Craft/Build (old dormant AOB attempts, superseded by
            //    the new ones above) / Debug_ RPC fallbacks: blocked without genuine
            //    admin rights (writing bAdmin directly didn't unlock them either).
            //  - Work Speed Accelerator / Pal Always Rolls Perfect: unconfirmed.
            //  - No Use Bullet (UseBullet ExecFunction hook): risky, same hook category
            //    that froze the game on Pal Sphere throw.
            //  - Craft Speed Hack: target function fully recompiled, not recoverable.
            //  - Weightless (Dodge Only): 3 ambiguous ASM candidates, too risky to patch.
            //  - Instant Chest/Resource Respawn (passive ASM): signature no longer
            //    resolves.
            //  - The RequestConsumeItem/RequestConsumeItem_ForThrowWeapon ExecFunction
            //    no-ops (skip the whole consume call) are still hard-disabled in
            //    Hooking.cpp - freezes the game on Pal Sphere throw.
            //  - Pal Editor (Team): party list comes back empty.
            //  - Player Status Points / Tech Points / Elixirs (5 approaches below): all
            //    blocked by AddItem_ServerInternal only ever producing ghost items. Real
            //    workaround: obtain ONE real Elixir/Tech Book/Ancient Civilization Part
            //    through normal gameplay, then use "Duplicate item in slot" on it in the
            //    Duping tab - it's a real item at that point, so the dupe (and using it
            //    afterward) works normally.
            //  - Add Tech Points / Add Ancient Tech Points (direct field write on
            //    UPalTechnologyData): pulled from the Player tab on suspicion - see
            //    comment above TABPlayer's Max Weight Hack.
            // ============================================================================
#if 0
            ImGui::InputInt("Tech Points", &Config.TechPointsToAdd);
            if (ImGui::Button("Add Tech Points", ImVec2(ImGui::GetContentRegionAvail().x - 3, 20)))
                AddTechPoints(Config.TechPointsToAdd);

            ImGui::InputInt("Ancient Tech Points", &Config.AncientTechPointsToAdd);
            if (ImGui::Button("Add Ancient Tech Points", ImVec2(ImGui::GetContentRegionAvail().x - 3, 20)))
                AddAncientTechPoints(Config.AncientTechPointsToAdd);

            ImGui::SliderFloat("Force Respawn Radius (cm)", &Config.RespawnForceRadius, 500.0f, 20000.0f);
            if (ImGui::Button("Force Respawn Nearby Chests/Resources", ImVec2(ImGui::GetContentRegionAvail().x - 3, 20)))
                ForceRespawnNearby(Config.RespawnForceRadius);
            ImGui::Checkbox("Magic Purse (adds a huge amount of money)", &Config.IsMagicPurse);
            ImGui::Checkbox("Work Speed Accelerator", &Config.IsWorkSpeedAccelerator);
            ImGui::SliderFloat("Work Speed Extra Rate", &Config.WorkSpeedExtraRate, 0.0f, 20.0f);
            ImGui::Checkbox("Pal Always Rolls Perfect Attack/Defense", &Config.IsPalPerfectStatRoll);
            ImGui::Checkbox("No Use Bullet (UseBullet ExecFunction hook)", &Config.IsNoUseBulletHook);
            ImGui::Checkbox("Craft Speed Hack", &Config.IsCraftSpeedHack);
            ImGui::SliderFloat("Craft Speed Multiplier", &Config.CraftSpeedMultiplier, 1.0f, 50.0f);
            ImGui::Checkbox("Instant Chest/Resource Respawn (passive)", &Config.IsInstantMapObjectRespawn);
            ImGui::Checkbox("Weightless (Dodge Only)", &Config.IsWeightlessDodge);
            ImGui::Checkbox("No Consume Sphere/Ammo (ExecFunction hook - FREEZES GAME, do not enable)", &Config.IsNoConsumeSphereAmmo);

            // Debug_SetStatusPoint_ToServer - doesn't work
            ImGui::InputInt("Amount / target level", &Config.StatusPointLevel);
            if (ImGui::Button("HP##dbg", ImVec2(ImGui::GetContentRegionAvail().x / 6 - 3, 20))) SetPlayerStatusPointLevel_DebugRPC(0, Config.StatusPointLevel);
            ImGui::SameLine();
            if (ImGui::Button("Stamina##dbg", ImVec2(ImGui::GetContentRegionAvail().x / 5 - 3, 20))) SetPlayerStatusPointLevel_DebugRPC(1, Config.StatusPointLevel);
            ImGui::SameLine();
            if (ImGui::Button("Weight##dbg", ImVec2(ImGui::GetContentRegionAvail().x / 4 - 3, 20))) SetPlayerStatusPointLevel_DebugRPC(2, Config.StatusPointLevel);
            ImGui::SameLine();
            if (ImGui::Button("Attack##dbg", ImVec2(ImGui::GetContentRegionAvail().x / 3 - 3, 20))) SetPlayerStatusPointLevel_DebugRPC(3, Config.StatusPointLevel);
            ImGui::SameLine();
            if (ImGui::Button("WorkSpeed##dbg", ImVec2(ImGui::GetContentRegionAvail().x / 2 - 3, 20))) SetPlayerStatusPointLevel_DebugRPC(4, Config.StatusPointLevel);
            ImGui::SameLine();
            if (ImGui::Button("Capture##dbg", ImVec2(ImGui::GetContentRegionAvail().x - 3, 20))) SetPlayerStatusPointLevel_DebugRPC(5, Config.StatusPointLevel);

            // AddPlayerStatusPoint_ToServer - doesn't work (0 unused points)
            if (ImGui::Button("HP##add", ImVec2(ImGui::GetContentRegionAvail().x / 6 - 3, 20))) AddPlayerStatusPointReal(0, Config.StatusPointLevel);
            ImGui::SameLine();
            if (ImGui::Button("Stamina##add", ImVec2(ImGui::GetContentRegionAvail().x / 5 - 3, 20))) AddPlayerStatusPointReal(1, Config.StatusPointLevel);
            ImGui::SameLine();
            if (ImGui::Button("Weight##add", ImVec2(ImGui::GetContentRegionAvail().x / 4 - 3, 20))) AddPlayerStatusPointReal(2, Config.StatusPointLevel);
            ImGui::SameLine();
            if (ImGui::Button("Attack##add", ImVec2(ImGui::GetContentRegionAvail().x / 3 - 3, 20))) AddPlayerStatusPointReal(3, Config.StatusPointLevel);
            ImGui::SameLine();
            if (ImGui::Button("WorkSpeed##add", ImVec2(ImGui::GetContentRegionAvail().x / 2 - 3, 20))) AddPlayerStatusPointReal(4, Config.StatusPointLevel);
            ImGui::SameLine();
            if (ImGui::Button("Capture##add", ImVec2(ImGui::GetContentRegionAvail().x - 3, 20))) AddPlayerStatusPointReal(5, Config.StatusPointLevel);

            // Grant unused points then spend - doesn't work (grant is cosmetic-only)
            if (ImGui::Button("HP##grant", ImVec2(ImGui::GetContentRegionAvail().x / 6 - 3, 20))) GrantAndSpendStatusPoint(0, Config.StatusPointLevel);
            ImGui::SameLine();
            if (ImGui::Button("Stamina##grant", ImVec2(ImGui::GetContentRegionAvail().x / 5 - 3, 20))) GrantAndSpendStatusPoint(1, Config.StatusPointLevel);
            ImGui::SameLine();
            if (ImGui::Button("Weight##grant", ImVec2(ImGui::GetContentRegionAvail().x / 4 - 3, 20))) GrantAndSpendStatusPoint(2, Config.StatusPointLevel);
            ImGui::SameLine();
            if (ImGui::Button("Attack##grant", ImVec2(ImGui::GetContentRegionAvail().x / 3 - 3, 20))) GrantAndSpendStatusPoint(3, Config.StatusPointLevel);
            ImGui::SameLine();
            if (ImGui::Button("WorkSpeed##grant", ImVec2(ImGui::GetContentRegionAvail().x / 2 - 3, 20))) GrantAndSpendStatusPoint(4, Config.StatusPointLevel);
            ImGui::SameLine();
            if (ImGui::Button("Capture##grant", ImVec2(ImGui::GetContentRegionAvail().x - 3, 20))) GrantAndSpendStatusPoint(5, Config.StatusPointLevel);

            // Spawn + use Technical Manuals / Ancient Civilization Parts - doesn't
            // work (ghost items, see root cause above)
            if (ImGui::Button("Give+Use High Grade Manual (G1)", ImVec2(ImGui::GetContentRegionAvail().x - 3, 20))) GiveAndUseTechBook(1);
            if (ImGui::Button("Give+Use Innovative Manual (G2)", ImVec2(ImGui::GetContentRegionAvail().x - 3, 20))) GiveAndUseTechBook(2);
            if (ImGui::Button("Give+Use Future Manual (G3)", ImVec2(ImGui::GetContentRegionAvail().x - 3, 20))) GiveAndUseTechBook(3);
            ImGui::InputInt("Amount##ancientparts", &Config.AddItemCount);
            if (ImGui::Button("Give+Use Ancient Civilization Parts", ImVec2(ImGui::GetContentRegionAvail().x - 3, 20))) GiveAndUseAncientCivilizationParts(Config.AddItemCount);

            // Elixirs - doesn't work (ghost items, see root cause above)
            ImGui::InputInt("Tier (1 or 2)", &Config.ElixirTier);
            if (ImGui::Button("HP##elixir", ImVec2(ImGui::GetContentRegionAvail().x / 5 - 3, 20))) GiveAndUseElixir(0, Config.ElixirTier);
            ImGui::SameLine();
            if (ImGui::Button("Stamina##elixir", ImVec2(ImGui::GetContentRegionAvail().x / 4 - 3, 20))) GiveAndUseElixir(1, Config.ElixirTier);
            ImGui::SameLine();
            if (ImGui::Button("Weight##elixir", ImVec2(ImGui::GetContentRegionAvail().x / 3 - 3, 20))) GiveAndUseElixir(2, Config.ElixirTier);
            ImGui::SameLine();
            if (ImGui::Button("Attack##elixir", ImVec2(ImGui::GetContentRegionAvail().x / 2 - 3, 20))) GiveAndUseElixir(3, Config.ElixirTier);
            ImGui::SameLine();
            if (ImGui::Button("WorkSpeed##elixir", ImVec2(ImGui::GetContentRegionAvail().x - 3, 20))) GiveAndUseElixir(4, Config.ElixirTier);

            ImGui::Text("Pal Editor (Team):");
            if (ImGui::BeginChild("PartyPalOverview", ImVec2(0, 100), true))
            {
                auto overview = GetPartyPalOverview();
                for (size_t i = 0; i < overview.size(); i++)
                {
                    bool selected = (Config.SelectedPartyPalIndex == (int)i);
                    if (ImGui::Selectable(overview[i].c_str(), selected))
                    {
                        Config.SelectedPartyPalIndex = (int)i;
                        LoadSelectedPartyPalStats(Config.SelectedPartyPalIndex);
                    }
                }
            }
            ImGui::EndChild();

            if (Config.SelectedPartyPalIndex >= 0)
            {
                if (ImGui::Button("Full Heal", ImVec2(ImGui::GetContentRegionAvail().x - 3, 20)))
                    FullHealPartyPal(Config.SelectedPartyPalIndex);
                ImGui::InputInt("Level", &Config.EditPalLevel);
                ImGui::InputInt("Rank (0-4 stars)", &Config.EditPalRank);
                ImGui::InputInt("Talent HP", &Config.EditPalTalentHP);
                ImGui::InputInt("Talent Melee", &Config.EditPalTalentMelee);
                ImGui::InputInt("Talent Shot", &Config.EditPalTalentShot);
                ImGui::InputInt("Talent Defense", &Config.EditPalTalentDefense);
                if (ImGui::Button("Apply Changes", ImVec2(ImGui::GetContentRegionAvail().x - 3, 25)))
                    ApplySelectedPartyPalStats(Config.SelectedPartyPalIndex);
            }
#endif
        }

        void TABConfig()
        {
            
            ImGui::Text("PalWorld Menu");
            ImGui::Text("VERSION: v1.2.3");

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            if (ImGui::Button("UNHOOK DLL", ImVec2(ImGui::GetContentRegionAvail().x - 3, 20))) {
#if DEBUG
                g_Console->printdbg("\n\n[+] UNHOOK INITIALIZED [+]\n\n", Console::Colors::red);

#endif
                g_KillSwitch = TRUE;
            }
        }
        
        void TABDatabase()
        {
            // Moved here from the now-empty EXPLOIT tab - clicking an item button below
            // fills Item Name (via strcpy_s further down), so browse-then-give works
            // without switching tabs.
            ImGui::InputText("Item Name", Config.ItemName, sizeof(Config.ItemName));
            ImGui::InputInt("Item Num", &Config.Item);
            if (ImGui::Button("Give item", ImVec2(ImGui::GetContentRegionAvail().x - 3, 20)))
            {
                SDK::APalPlayerCharacter* p_appc = Config.GetPalPlayerCharacter();
                SDK::APalPlayerState* p_apps = Config.GetPalPlayerState();
                if (p_appc && p_apps)
                {
                    SDK::UPalPlayerInventoryData* InventoryData = Config.GetPalPlayerCharacter()->GetPalPlayerController()->GetPalPlayerState()->GetInventoryData();
                    if (InventoryData && (Config.ItemName != NULL))
                    {
                        g_Console->printdbg("\n\n[+] ItemName: %s [+]\n\n", Console::Colors::green, Config.ItemName);
                        AddItemToInventoryByName(InventoryData, Config.ItemName, Config.Item);
                    }
                }
            }
            ImGui::Separator();

            //ImGui::Checkbox("IsItems", &Config.matchDbItems);

            ImGui::InputText("Filter", Config.inputTextBuffer, sizeof(Config.inputTextBuffer), ImGuiInputTextFlags_CallbackCharFilter, InputTextCallback);

            Config.Update(Config.inputTextBuffer);

            const auto& filteredItems = Config.GetFilteredItems();
            const auto& nameMap = GetItemDisplayNameMap();

            for (const auto& itemName : filteredItems) {
                auto it = nameMap.find(itemName);
                // "##itemName" as the hidden ID suffix keeps every button's ImGui ID
                // unique even if two items happen to share a display name - the raw
                // ID underneath is always unique.
                std::string label = (it != nameMap.end() ? it->second : itemName) + "##" + itemName;
                if (ImGui::Button(label.c_str()))
                {
                    strcpy_s(Config.ItemName, itemName.c_str());
                }
            }
        }
        
        void TABTeleporter()
        {
            ImGui::Checkbox("SafeTeleport", &Config.IsSafe);
            if (ImGui::Button("Home", ImVec2(ImGui::GetContentRegionAvail().x - 3, 20)))
                RespawnLocalPlayer(Config.IsSafe);

            ImGui::InputFloat3("Pos",Config.Pos);
            ImGui::SameLine();
            if (ImGui::Button("TP", ImVec2(ImGui::GetContentRegionAvail().x - 3, 20)))
            {
                SDK::FVector vector = { Config.Pos[0],Config.Pos[1],Config.Pos[2] };
                AnyWhereTP(vector, Config.IsSafe);
            }

            ImGui::Separator();
            ImGui::Columns(3, "TeleportColumns", true);

            // No per-column Separator() below headers - Separator() spans all
            // columns by default and throws off each column's row-height
            // tracking, which pushed "Active Dungeons" a row lower than the
            // other two headers. The column border lines (border=true above)
            // already separate the columns visually.
            ImGui::Text("Boss Locations");
            int bossIdx = 0;
            for (const auto& pair : database::locationMap)
            {
                const std::string& locationName = pair.first;
                std::string label = locationName + "##boss" + std::to_string(bossIdx++);
                if (ImGui::Button(label.c_str(), ImVec2(ImGui::GetColumnWidth() - 10, 0)))
                {
                    SDK::FVector location = SDK::FVector(pair.second[0], pair.second[1], pair.second[2]);
                    AnyWhereTP(location, Config.IsSafe);
                }
            }

            // Real fast-travel points, scanned live from the currently loaded world -
            // see GetFastTravelPoints() in feature.cpp for why this exists alongside
            // the hardcoded list above. Only populated once a world is actually loaded.
            ImGui::NextColumn();
            ImGui::Text("Fast Travel Points");
            int ftpIdx = 0;
            for (const auto& point : GetFastTravelPoints())
            {
                std::string label = point.first + "##ftp" + std::to_string(ftpIdx++);
                if (ImGui::Button(label.c_str(), ImVec2(ImGui::GetColumnWidth() - 10, 0)))
                {
                    SDK::FVector location = point.second;
                    AnyWhereTP(location, Config.IsSafe);
                }
            }

            // Dungeon entrances, scanned live - see GetActiveDungeonEntrances() in
            // feature.cpp. Lists every entrance found; ones with a raid-style
            // active instance get the real dungeon name/level + countdown, the
            // rest just show as "Dungeon Entrance".
            ImGui::NextColumn();
            ImGui::Text("Dungeon Entrances");
            int dungeonIdx = 0;
            for (const auto& dungeon : GetActiveDungeonEntrances())
            {
                std::string label = dungeon.first + "##dungeon" + std::to_string(dungeonIdx++);
                if (ImGui::Button(label.c_str(), ImVec2(ImGui::GetColumnWidth() - 10, 0)))
                {
                    SDK::FVector location = dungeon.second;
                    AnyWhereTP(location, Config.IsSafe);
                }
            }

            ImGui::Columns(1);
        }

        void TABItemSpawner()
        {
            static int num_to_add = 1;
            static int category = 0;

            ImGui::InputInt("Num To Add", &num_to_add);

            ImGui::Combo("Item Category", &category, "All\0Accessories\0Ammo\0Armor\0Crafting Materials\0Eggs\0Food\0Hats\0Medicine\0Money\0Other\0Pal Spheres\0Seeds\0Tools\0Weapons\0");

            std::vector<const char*> list;

            switch (category)
            {
                case 1:
                    list = itemlist::accessories;
                    break;
                case 2:
                    list = itemlist::ammo;
                    break;
                case 3:
                    list = itemlist::armor;
                    break;
                case 4:
                    list = itemlist::craftingmaterials;
                    break;
                case 5:
                    list = itemlist::eggs;
                    break;
                case 6:
                    list = itemlist::food;
                    break;
                case 7:
                    list = itemlist::hats;
                    break;
                case 8:
                    list = itemlist::medicine;
                    break;
                case 9:
                    list = itemlist::money;
                    break;
                case 10:
                    list = itemlist::other;
                    break;
                case 11:
                    list = itemlist::palspheres;
                    break;
                case 12:
                    list = itemlist::seeds;
                    break;
                case 13:
                    list = itemlist::tools;
                    break;
                case 14:
                    list = itemlist::weapons;
                    break;
                default: // 0 = All
                    for (auto* item : itemlist::accessories) list.push_back(item);
                    for (auto* item : itemlist::ammo) list.push_back(item);
                    for (auto* item : itemlist::armor) list.push_back(item);
                    for (auto* item : itemlist::craftingmaterials) list.push_back(item);
                    for (auto* item : itemlist::eggs) list.push_back(item);
                    for (auto* item : itemlist::food) list.push_back(item);
                    for (auto* item : itemlist::hats) list.push_back(item);
                    for (auto* item : itemlist::medicine) list.push_back(item);
                    for (auto* item : itemlist::money) list.push_back(item);
                    for (auto* item : itemlist::other) list.push_back(item);
                    for (auto* item : itemlist::palspheres) list.push_back(item);
                    for (auto* item : itemlist::seeds) list.push_back(item);
                    for (auto* item : itemlist::tools) list.push_back(item);
                    for (auto* item : itemlist::weapons) list.push_back(item);
            }

            int cur_size = 0;

            static char item_search[100];

            ImGui::InputText("Search", item_search, IM_ARRAYSIZE(item_search));

            for (const auto& item : list) {
                std::istringstream ss(item);
                std::string left_text, right_text;

                std::getline(ss, left_text, '|');
                std::getline(ss, right_text);

                auto right_to_lower = right_text;
                std::string item_search_to_lower = item_search;

                std::transform(right_to_lower.begin(), right_to_lower.end(), right_to_lower.begin(), ::tolower);
                std::transform(item_search_to_lower.begin(), item_search_to_lower.end(), item_search_to_lower.begin(), ::tolower);

                if (item_search[0] != '\0' && (right_to_lower.find(item_search_to_lower) == std::string::npos))
                    continue;

                if (cur_size != 0 && cur_size < 20)
                {
                    ImGui::SameLine();
                }
                else if (cur_size != 0)
                {
                    cur_size = 0;
                }

                cur_size += right_text.length();

                ImGui::PushID(item);
                if (ImGui::Button(right_text.c_str()))
                {
                    SDK::UPalPlayerInventoryData* InventoryData = Config.GetPalPlayerCharacter()->GetPalPlayerController()->GetPalPlayerState()->GetInventoryData();
                    AddItemToInventoryByName(InventoryData, (char*)left_text.c_str(), num_to_add);
                }
                ImGui::PopID();
            }
        }

        void TABQuick()
        {
            if (ImGui::Button("Basic Items stack", ImVec2(ImGui::GetContentRegionAvail().x - 3, 20)))
                SpawnMultiple_ItemsToInventory(config::QuickItemSet::basic_items_stackable);

            if (ImGui::Button("Basic Items single", ImVec2(ImGui::GetContentRegionAvail().x - 3, 20)))
                SpawnMultiple_ItemsToInventory(config::QuickItemSet::basic_items_single);

            if (ImGui::Button("Unlock Pal skills", ImVec2(ImGui::GetContentRegionAvail().x - 3, 20)))
                SpawnMultiple_ItemsToInventory(config::QuickItemSet::pal_unlock_skills);

            if (ImGui::Button("Spheres", ImVec2(ImGui::GetContentRegionAvail().x - 3, 20)))
                SpawnMultiple_ItemsToInventory(config::QuickItemSet::spheres);

            if (ImGui::Button("Tools", ImVec2(ImGui::GetContentRegionAvail().x - 3, 20)))
                    SpawnMultiple_ItemsToInventory(config::QuickItemSet::tools);
        }
        
        void TABDebug()
        {
            if (ImGui::Button("Dump Weapon Ammo State to Console", ImVec2(ImGui::GetContentRegionAvail().x - 3, 20)))
                DumpWeaponAmmoState();
            if (ImGui::Button("Dump Movement Speed State to Console", ImVec2(ImGui::GetContentRegionAvail().x - 3, 20)))
                DumpMovementSpeedState();
            if (ImGui::Button("Dump Armor Defense State to Console", ImVec2(ImGui::GetContentRegionAvail().x - 3, 20)))
                DumpArmorDefenseState();
            ImGui::Separator();

            if (ImGui::Checkbox("DEBUG ESP", &Config.isDebugESP) && !Config.isDebugESP)
                Config.mDebugESPDistance = 10.f;
            if (Config.isDebugESP)
            {
                ImGui::SameLine();
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                ImGui::SliderFloat("##DISTANCE", &Config.mDebugESPDistance, 1.0f, 100.f, "%.0f", ImGuiSliderFlags_AlwaysClamp);
            }

            if (ImGui::Checkbox("TELEPORT PALS TO XHAIR", &Config.IsTeleportAllToXhair) && !Config.IsTeleportAllToXhair)
                Config.mDebugEntCapDistance = 10.f;
            if (Config.IsTeleportAllToXhair)
            {
                ImGui::SameLine();
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                ImGui::SliderFloat("##ENT_CAP_DISTANCE", &Config.mDebugEntCapDistance, 1.0f, 100.f, "%.0f", ImGuiSliderFlags_AlwaysClamp);
            }
            
            if (ImGui::Checkbox("DEATH AURA", &Config.IsDeathAura) && !Config.IsDeathAura)
            {
                Config.mDeathAuraDistance = 10.0f;
                Config.mDeathAuraAmount = 1;
            }
            if (Config.IsDeathAura)
            {
                ImGui::SameLine();
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * .7);
                ImGui::SliderFloat("##AURA_DISTANCE", &Config.mDeathAuraDistance, 1.0f, 100.f, "%.0f", ImGuiSliderFlags_AlwaysClamp);
                ImGui::SameLine();
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                ImGui::SliderInt("##AURA_DMG", &Config.mDeathAuraAmount, 100, 1000, "%d", ImGuiSliderFlags_AlwaysClamp);
            }

            if (ImGui::Button("PRINT ENGINE GLOBALS", ImVec2(ImGui::GetContentRegionAvail().x - 3, 20)))
            {

                g_Console->printdbg("[+] [UNREAL ENGINE GLOBALS]\n"
                    "UWorld:\t\t\t0x%llX\n"
                    "ULocalPlayer:\t\t0x%llX\n"
                    "APalPlayerController:\t0x%llX\n"
                    "APalPlayerCharacter:\t0x%llX\n"
                    "APalPlayerState:\t0x%llX\n"
                    "UPalPlayerInventory:\t0x%llX\n"
                    "APalWeaponBase:\t\t0x%llX\n",
                    Console::Colors::yellow, 
                    Config.gWorld,
                    Config.GetLocalPlayer(),
                    Config.GetPalPlayerController(),
                    Config.GetPalPlayerCharacter(),
                    Config.GetPalPlayerState(),
                    Config.GetInventoryComponent(),
                    Config.GetPlayerEquippedWeapon()
                );
                
            }

            //  Get Function Pointer Offset
            ImGui::InputTextWithHint("##INPUT", "INPUT GOBJECT fn NAME", inputBuffer_getFnAddr, 100);
            ImGui::SameLine();
            if (ImGui::Button("GET fn", ImVec2(ImGui::GetContentRegionAvail().x, 20)))
            {
                std::string input = inputBuffer_getFnAddr;
                SDK::UFunction* object = SDK::UObject::FindObject<SDK::UFunction>(input);
                if (object)
                {
                    static __int64 dwHandle = reinterpret_cast<__int64>(GetModuleHandle(0));
                    void* fnAddr = object->ExecFunction;
                    unsigned __int64 fnOffset = (reinterpret_cast<__int64>(fnAddr) - dwHandle);
                    g_Console->printdbg("[+] Found [%s] -> 0x%llX\n", Console::Colors::yellow, input.c_str(), fnOffset);
                }
                else
                    g_Console->printdbg("[!] OBJECT [%s] NOT FOUND!\n", Console::Colors::red, input.c_str());
                memset(inputBuffer_getFnAddr, 0, 100);
            }


            //  Get Class pointer by name
            ImGui::InputTextWithHint("##INPUT_GETCLASS", "INPUT OBJECT CLASS NAME", inputBuffer_getClass, 100);
            ImGui::SameLine();
            if (ImGui::Button("GET CLASS", ImVec2(ImGui::GetContentRegionAvail().x, 20)))
            {
                std::string input = inputBuffer_getClass;
                SDK::UClass* czClass = SDK::UObject::FindObject<SDK::UClass>(input.c_str());
                if (czClass)
                {
                    static __int64 dwHandle = reinterpret_cast<__int64>(GetModuleHandle(0));
                    g_Console->printdbg("[+] Found [%s] -> 0x%llX\n", Console::Colors::yellow, input.c_str(), czClass->Class);
                }
                else
                    g_Console->printdbg("[!] CLASS [%s] NOT FOUND!\n", Console::Colors::red, input.c_str());

            }

            //  Waypoints
            ImGui::InputTextWithHint("##INPUT_SETWAYPOINT", "CUSTOM WAYPOINT NAME", inputBuffer_setWaypoint, 32);
            ImGui::SameLine();
            if (ImGui::Button("SET", ImVec2(ImGui::GetContentRegionAvail().x, 20)))
            {
                std::string wpName = inputBuffer_setWaypoint;
                if (wpName.size() > 0)
                {
                    AddWaypointLocation(wpName);
                    memset(inputBuffer_setWaypoint, 0, 32);
                }
            }
            if (Config.db_waypoints.size() > 0)
            {
                if (ImGui::BeginChild("##CHILD_WAYPOINTS", { 0.0f, 100.f }))
                {
                    DWORD index = -1;
                    for (auto waypoint : Config.db_waypoints)
                    {
                        index++;
                        ImGui::PushID(index);
                        //  ImGui::Checkbox("SHOW", &waypoint.bIsShown);
                        //  ImGui::SameLine();
                        if (ImGui::Button(waypoint.waypointName.c_str(), ImVec2(ImGui::GetContentRegionAvail().x, 20)))
                            AnyWhereTP(waypoint.waypointLocation, false);
                        ImGui::PopID();
                    }

                    ImGui::EndChild();
                }
            }
        }
	}

	void Menu::Draw()
	{

		if (g_GameVariables->m_ShowMenu)
			MainMenu();

		if (g_GameVariables->m_ShowHud)
			HUD(&g_GameVariables->m_ShowHud);

		if (g_GameVariables->m_ShowDemo)
			ImGui::ShowDemoWindow();
	}

    void Menu::ManagerMenu()
    {
        if (!ImGui::Begin("Manager", &g_GameVariables->m_ShowMenu, 96))
        {
            ImGui::End();
            return;
        }

        
        if (Config.gWorld)
        {
            ImGui::Checkbox("filterPlayer", &Config.filterPlayer);
            SDK::TArray<SDK::AActor*> T = Config.GetUWorld()->PersistentLevel->Actors;
            for (int i = 0; i < T.Num(); i++)
            {
                if (!T[i])
                    continue;

                if (!T[i]->IsA(SDK::APalCharacter::StaticClass()))
                    continue;

                SDK::APalCharacter* Character = (SDK::APalCharacter*)T[i];
                SDK::FString name;
                if (Config.filterPlayer)
                {
                    if (!T[i]->IsA(SDK::APalPlayerCharacter::StaticClass()))
                        continue;
                }
                if (T[i]->IsA(SDK::APalPlayerCharacter::StaticClass()))
                {
                    if (!Character)
                        continue;

                    Character->CharacterParameterComponent->GetNickname(&name);
                }
                else
                {
                    SDK::UKismetStringLibrary* lib = SDK::UKismetStringLibrary::GetDefaultObj();
                    if (!Character)
                        continue;

                    std::string s = Character->GetFullName();
                    size_t firstUnderscorePos = s.find('_');

                    if (firstUnderscorePos != std::string::npos) 
                    {
                        std::string result = s.substr(firstUnderscorePos + 1);

                        size_t secondUnderscorePos = result.find('_');

                        if (secondUnderscorePos != std::string::npos) {
                            result = result.substr(0, secondUnderscorePos);
                        }
                        wchar_t  ws[255];
                        swprintf(ws, 255, L"%hs", result.c_str());
                        name = SDK::FString(ws);
                    }
                }
                ImGui::Text(name.ToString().c_str());
                ImGui::SameLine();
                ImGui::PushID(i);
                if (ImGui::Button("Kill"))
                {
                    if (T[i]->IsA(SDK::APalCharacter::StaticClass()))
                        SendDamageToActor(Character, INT_MAX);
                }
                ImGui::SameLine();
                if (ImGui::Button("TP"))
                {
                    if (Config.GetPalPlayerCharacter() != NULL)
                    {
                        if (Character)
                        {
                            SDK::FVector vector = Character->K2_GetActorLocation();
                            AnyWhereTP(vector, Config.IsSafe);
                        }
                    }
                }

                /*if (Character->IsA(SDK::APalPlayerCharacter::StaticClass()))
                {
                    ImGui::SameLine();
                    if (ImGui::Button("Boss"))
                    {
                        if (Config.GetPalPlayerCharacter() != NULL)
                        {
                            auto controller = Config.GetPalPlayerCharacter()->GetPalPlayerController();
                            if (controller != NULL)
                            {
                                controller->Transmitter->BossBattle->RequestBossBattleEntry_ToServer(SDK::EPalBossType::ElectricBoss, (SDK::APalPlayerCharacter*)Character);
                                controller->Transmitter->BossBattle->RequestBossBattleStart_ToServer(SDK::EPalBossType::ElectricBoss, (SDK::APalPlayerCharacter*)Character);
                            }
                        }
                    }
                }*/
                if (Character->IsA(SDK::APalPlayerCharacter::StaticClass()))
                {
                    /*ImGui::SameLine();
                    if ( ImGui::Button( "Join Guild" ) )
                    {
                        ForceJoinGuild( (SDK::APalPlayerCharacter*)Character );
                    }*/
                    ImGui::SameLine();
                    if (ImGui::Button("MaskIt"))
                    {
                        if (Config.GetPalPlayerCharacter() != NULL)
                        {
                            auto controller = Config.GetPalPlayerCharacter()->GetPalPlayerController();
                            if (controller != NULL)
                            {
                                auto player = (SDK::APalPlayerCharacter*)Character;
                                SDK::FString fakename;
                                player->CharacterParameterComponent->GetNickname(&fakename);
                                Config.GetPalPlayerCharacter()->GetPalPlayerController()->UpdateCharacterNickName_ToServer(Config.GetPalPlayerCharacter()->CharacterParameterComponent->IndividualHandle->ID, fakename);
                            }
                        }
                    }
                }
                ImGui::PopID();
            }

        }

        if (Config.GetUWorld() != NULL)
        {
        }
        
        ImGui::End();
    }
	
    void Menu::MainMenu()
	{
        if (!g_GameVariables->m_ShowDemo)
            Styles::InitStyle();

        if (g_Menu->dbg_RAINBOW_THEME) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(g_Menu->dbg_RAINBOW));
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(g_Menu->dbg_RAINBOW));
            ImGui::PushStyleColor(ImGuiCol_BorderShadow, ImVec4(g_Menu->dbg_RAINBOW));
        }
        // The window flags (96 = AlwaysAutoResize | NoCollapse) mean it otherwise grows
        // to fit whatever tab is open - the Database tab's long item-button list made
        // the whole window balloon to fill (and exceed) the screen. Capping the height
        // here keeps AutoResize's width-fitting behavior while forcing a scrollbar once
        // content overflows the max height, instead of the window itself growing.
        ImGui::SetNextWindowSizeConstraints(ImVec2(0, 0), ImVec2(FLT_MAX, 700.0f));
        if (!ImGui::Begin("PalWorld", &g_GameVariables->m_ShowMenu, 96))
        {
            ImGui::End();
            return;
        }
        if (g_Menu->dbg_RAINBOW_THEME) {
            ImGui::PopStyleColor();
            ImGui::PopStyleColor();
            ImGui::PopStyleColor();
        }
        
        ImGuiContext* pImGui = GImGui;

        //  Display Menu Content
        //Tabs::TABMain();

        //ImGui::Text("Testing some case...");

        if (ImGui::BeginTabBar("##tabs", ImGuiTabBarFlags_None))
        {
            if (ImGui::BeginTabItem("Player"))
            {
                Tabs::TABPlayer();
                ImGui::EndTabItem();
            }
            // "EXPLOIT" tab hidden - it's empty now, everything moved to Player/Database.
            /*
            if (ImGui::BeginTabItem("EXPLOIT"))
            {
                Tabs::TABExploit();
                ImGui::EndTabItem();
            }
            */
            if (ImGui::BeginTabItem("Duping"))
            {
                Tabs::TABDuping();
                ImGui::EndTabItem();
            }
            // "WIP" tab hidden - still fully compiled, just not shown. The Dump
            // buttons that used to live here moved to the DEBUG tab.
            /*
            if (ImGui::BeginTabItem("WIP"))
            {
                Tabs::TABNotWorking();
                ImGui::EndTabItem();
            }
            */
            if (ImGui::BeginTabItem("Database"))
            {
                Tabs::TABDatabase();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Item Spawner"))
            {
                Tabs::TABItemSpawner();
                ImGui::EndTabItem();
            }
            // "CONFIG" tab hidden - also hides the "UNHOOK DLL" button, the only
            // thing in this tab.
            /*
            if (ImGui::BeginTabItem("CONFIG"))
            {
                Tabs::TABConfig();
                ImGui::EndTabItem();
            }
            */
#if DEBUG
            if (ImGui::BeginTabItem("DEBUG"))
            {
                Tabs::TABDebug();
                ImGui::EndTabItem();
            }
#endif
            if (Config.IsQuick && ImGui::BeginTabItem("Quick"))
            {
                Tabs::TABQuick();
                ImGui::EndTabItem();
            }
            // Always shown now - "Show Teleporter Tab" checkbox (Config.bisTeleporter)
            // removed along with the rest of the emptied-out EXPLOIT tab.
            if (ImGui::BeginTabItem("Teleporter"))
            {
                Tabs::TABTeleporter();
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
        ImGui::End();



        if (Config.bisOpenManager)
            ManagerMenu();
	}

	void Menu::HUD(bool* p_open)
	{
        
        ImGui::SetNextWindowPos(g_D3D11Window->pViewport->WorkPos);
        ImGui::SetNextWindowSize(g_D3D11Window->pViewport->WorkSize);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, NULL);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.00f, 0.00f, 0.00f, 0.00f));
        if (!ImGui::Begin("##HUDWINDOW", (bool*)true, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoInputs))
        {
            ImGui::PopStyleColor();
            ImGui::PopStyleVar();
            ImGui::End();
            return;
        }
        ImGui::PopStyleColor();
        ImGui::PopStyleVar();

        auto ImDraw = ImGui::GetWindowDrawList();
        auto draw_size = g_D3D11Window->pViewport->WorkSize;
        auto center = ImVec2({ draw_size.x * .5f, draw_size.y * .5f });
        auto top_center = ImVec2({ draw_size.x * .5f, draw_size.y * 0.0f });
        
        //  Watermark
        //ImDraw->AddText(top_center, g_Menu->dbg_RAINBOW, "PalWorld-NetCrack");

        if (Config.IsESP)
            ESP();

        if (Config.isDebugESP)
            ESP_DEBUG(Config.mDebugESPDistance, ImVec4(1.0f, 0.0f, 0.0f, 1.0f), SDK::APalMonsterCharacter::StaticClass());

        if (Config.IsPlayerESP)
            ESP_DEBUG(Config.mPlayerESPDistance, ImVec4(0.0f, 1.0f, 1.0f, 1.0f), SDK::APalPlayerCharacter::StaticClass());

        if (Config.IsPalESP)
            ESP_DEBUG(Config.mPalESPDistance, ImVec4(1.0f, 0.65f, 0.0f, 1.0f), SDK::APalMonsterCharacter::StaticClass());

        if (Config.db_waypoints.size() > 0)
            RenderWaypointsToScreen();

        ImGui::End();
	}

    void Menu::Loops()
    {
        //  Respawn
        if ((GetAsyncKeyState(VK_F5) & 1))
            RespawnLocalPlayer(Config.IsSafe);

        //  Revive Player
        if ((GetAsyncKeyState(VK_F6) & 1))
            ReviveLocalPlayer();

        //  Toggle SpeedHack/AttackHack (user-bound keys, unbound by default)
        if (Config.SpeedHackToggleKey != 0 && (GetAsyncKeyState(Config.SpeedHackToggleKey) & 1))
            Config.IsSpeedHack = !Config.IsSpeedHack;
        if (Config.AttackHackToggleKey != 0 && (GetAsyncKeyState(Config.AttackHackToggleKey) & 1))
            Config.IsAttackModiler = !Config.IsAttackModiler;

        //  Toggle actual flight (only does anything while FLY is armed/checked)
        if (Config.IsFlyEnabled && Config.FlyToggleKey != 0 && (GetAsyncKeyState(Config.FlyToggleKey) & 1))
        {
            Config.IsToggledFly = !Config.IsToggledFly;
            ExploitFly(Config.IsToggledFly);
        }

        // SpeedHack() (CustomTimeDilation) deactivated - SpeedHack now runs on
        // SpeedHackAllInOne instead, see comment in TABPlayer.
        // Called unconditionally (not gated by "if IsSpeedHack") so the restore
        // branch (bEnable=false) actually runs when the checkbox/hotkey turns it
        // off - previously it never did, leaving Walk/Swim/Fly/Custom speed
        // stuck at the scaled value forever once enabled.
        SpeedHackAllInOne(Config.IsSpeedHack, Config.SpeedModiflers);

        if (Config.IsSpeedHackClean)
            SpeedHackClean(true, Config.SpeedHackCleanMultiplier);

        // SetPlayerAttackParam(Config.DamageUp) used to be called here, but AttackUp
        // is Transient and gets recalculated every tick, so a direct write never
        // sticks. InstallDamageMultiplierHook (Hooking.cpp) already reads
        // Config.IsAttackModiler/Config.DamageUp at the native damage-calc
        // instruction instead, so AttackHack still works via that path - the
        // redundant dead call here was removed.

        //
        if (Config.IsDefuseModiler)
            SetPlayerDefenseParam(Config.DefuseUp);

        // Called unconditionally, same restore reasoning as SpeedHackAllInOne above.
        SetArmorDefenseBoost(Config.ArmorDefenseMultiplier, !Config.IsArmorDefenseHack);

        //  
        if (Config.IsInfStamina)
            ResetStamina();

        if (Config.IsTeleportAllToXhair)
            TeleportAllPalsToCrosshair(Config.mDebugEntCapDistance);

        if (Config.IsDeathAura)
            DeathAura(Config.mDeathAuraAmount, Config.mDeathAuraDistance, true);

        //  
        //  SetDemiGodMode(Config.IsMuteki);

        if (Config.IsGodMode)
            SetPlayerHealth(0.9f);

        if (Config.IsToggledFly)
            UpdateFlyVerticalMovement();

        SetInfiniteAmmo(Config.IsInfinAmmo);
        SetUnlimitedAmmoNative(Config.IsInfinAmmo);

        // Both called unconditionally, same reasoning as SpeedHackAllInOne above -
        // SetCraftingSpeed/SetMaxInventoryWeight already had (or now have) a real
        // restore path, it just never ran because these were gated by "if Is*Hack".
        SetCraftingSpeed(Config.CraftSpeedMultiplier, !Config.IsCraftSpeedHack);
        SetMaxInventoryWeight(Config.MaxWeightValue, !Config.IsMaxWeightHack);

        SetInstantMapObjectRespawn(Config.IsInstantMapObjectRespawn);
        SetCanCraftAllItems(Config.IsCanCraftAllItems);
        SetCanBuildAllBuildings(Config.IsCanBuildAllBuildings);
        SetWeightlessDodge(Config.IsWeightlessDodge);

        if (Config.IsFreezeSlot)
            FreezeSlotItemCount(Config.FreezeSlotIndex, Config.FreezeSlotTarget);

        if (Config.IsFreezePalSphere)
            FreezeAllPalSpheres(Config.PalSphereFreezeTarget);

        SetNoConsumeSphereAndAmmo(Config.IsNoConsumeSphereAmmo);
        SetNoUseBulletHook(Config.IsNoUseBulletHook);
        SetMagicPurse(Config.IsMagicPurse);
        SetPalPerfectStatRoll(Config.IsPalPerfectStatRoll);
        SetWorkSpeedAccelerator(Config.IsWorkSpeedAccelerator);
    }
}