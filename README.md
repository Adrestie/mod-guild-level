# mod-guild-level

An AzerothCore module (WoW 3.3.5a) that brings the guild levels of Cataclysm
and builds a guild house on them. Members who play together earn guild
experience; the guild climbs 25 levels and 9 Paragon levels, unlocking perks,
talents, a house of its own and, at Paragon 9, the furniture to decorate it.

## Features

* **Levels 1-25:** passive perks for every member (experience, honor, mount speed, cheaper vendors...).
* **Active perks 21-25:** summon or resurrect the raid, a guild bank chest, a supplier, the way home.
* **Paragon levels:** past level 25, the guild climbs 9 Paragon levels.
* **Guild talents:** from level 10, a point per level, a tree of bonuses for every member.
* **Paragon 1-8:** a house in the faction's PvP hall, one phase per guild, with its services.
* **Customization (Paragon 9):** a 3D editor to place, turn and size 2443 objects of the modern housing.
* **Collection:** what the guild owns, what it lacks and where to find it.
* **Drops and quests:** dungeon and raid bosses (by theme or by boss) and 800 quests give the objects.
* **Languages:** English, French, German, Spanish (esES) and Russian.

## Main commands

| Command | Effect |
|---|---|
| `.gperk raid\|rez\|bank\|vendor\|home\|leave\|list` | the active perks |
| `.gtalent` | the talent window |
| `/gdeco`, `/gcoll` | the Customization mode, the Collection |
| `.guildlevel info\|setlevel\|setparagon\|addxp [$guild]` | GM: a guild's level |
| `.ghouse give $object $count [$guild]` | GM: objects for a guild's stock |

## Dependencies

| What | Role |
|---|---|
| AzerothCore WotLK | the server the module is compiled into (required) |
| [ALE](https://github.com/azerothcore/mod-ale) + [AIO](https://github.com/Rochet2/AIO) | the windows (server and client); without them, commands only |
| [WarcraftXL](https://github.com/WarcraftXL/wxl-core) + its Modern M2 plugin | the client draws the furniture and the level-up effects |
| `wxl-guildhouse` (`client/dist`) | the client extension of the Customization editor |
| [ForeverUI](https://github.com/Adrestie/mod-forever-ui) | optional: its look, talents and Collection in its guild page |

## Installation

Point the [WoW-mods](https://github.com/Adrestie/WoW-mods) installer at this
folder: it installs the module, SQL, configuration, Lua scripts, client patch
and extension. Manual steps, options, ID ranges (92000-94599) and in-game
tests: [docs/GUIDE.md](docs/GUIDE.md).

## Credits

* Blizzard Entertainment: the game, its models, textures, icons and names (not affiliated).
* [AzerothCore](https://github.com/azerothcore/azerothcore-wotlk), ALE, AIO (Rochet2), WarcraftXL.
* [wxl-housing](https://github.com/sogyboi/wxl-housing) by sogyboi, which `wxl-guildhouse` forks.
* [Dear ImGui](https://github.com/ocornut/imgui), [ImGuizmo](https://github.com/CedricGuillemet/ImGuizmo), wow.export, [wago.tools](https://wago.tools), m2port ([mod-limit-break](https://github.com/Adrestie/mod-limit-break)), WebWowViewerCpp.

## Licence

The module: GPL-2.0-or-later ([LICENSE](LICENSE)). The extension
`wxl-guildhouse`: GPL-3.0-or-later, ImGuizmo MIT. Game assets belong to
Blizzard Entertainment.
