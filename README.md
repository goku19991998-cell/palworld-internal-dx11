# PalWorld-NetCrack
Internal ImGui trainer for Palworld singleplayer, built on the DX11-Internal-Base template.

[COLOR="Lime"][B]Confirmed Working Features (singleplayer):[/B][/COLOR]

[LIST]
[*]Attack Hack (Damage Multiplier)
[*]Defense Hack (Armor Multiplier)
[*]Speed Hack (Walk/Swim/Fly/Custom, incl. while riding a flying Pal)
[*]Infinite Stamina
[*][COLOR="Red"]Infinite Ammo -> DON'T WORK, forgot to hide it[/COLOR]
[*]FAKE God Mode
[*]Max Weight Hack
[*]FLY (Toggle Fly + vertical movement)
[*]Player ESP / Pal ESP
[*]Teleporter (Home, manual coordinate TP, Boss Locations, live Fast Travel Points, live Dungeon Entrances)
[*]Player Modifiers (Revive, Unlock All Effigies, Reveal Whole Map)
[*]Item Spawner & Quick-Tab Items (Use moderate amounts) -> ghost items
[*]WORKING Item Duplication (Give / Duplicate item in slot / Duplicate ALL slots / Use item x1/x10/x100)
[*]Manager Menu (list all characters, Kill / TP / MaskIt nickname spoof)
[*]Debug ESP, Teleport Pals to Crosshair, Death Aura, Custom Waypoints (DEBUG tab)
[/LIST]

## AOBS
> GObjects: `48 8B 05 ? ? ? ? 48 8B 0C C8 4C 8D 04 D1 EB 03`  
> FNames: `48 8D 05 ? ? ? ? EB 13 48 8D 0D ? ? ? ? E8 ? ? ? ? C6 05 ? ? ? ? ? 0F 10`  
> GWorld: `48 8B 1D ?? ?? ?? ?? 48 85 DB 74 33 41 B0`  

## External Library Credits
[Dear ImGui](https://github.com/ocornut/imgui)  
[MinHook](https://github.com/TsudaKageyu/minhook)  
[Dumper7](https://github.com/Encryqed/Dumper-7)  
[DX11-Internal-Base](https://github.com/NightFyre/DX11-ImGui-Internal-Hook)  
