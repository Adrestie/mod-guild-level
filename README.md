# mod-guild-level

**WORK IN PROGRESS**

An AzerothCore module (WoW 3.3.5a) that brings the guild levels of Cataclysm and builds a guild house on them: members who play together earn guild experience, unlocking perks, a guild house and guild talents.

## Key features

- Levels 1-25: passive perks for every member (experience, honor, mount speed, cheaper vendors...).
- Active perks at levels 21-25: summon or resurrect the raid, a guild bank chest, a supplier, the way to the Guild House.
- Paragon levels past 25, the first ones unlocking the Guild House features.
- Guild talents from Paragon 10, raising the members' stats.
- Guild House decoration: 2443 objects, from quests, dungeons and raids, placed in a Customization mode.
- Texts in English, French, German, Spanish and Russian.

## Dependencies

| Dependency | For |
|---|---|
| [AzerothCore](https://github.com/azerothcore/azerothcore-wotlk) (WotLK 3.3.5a) | the server |
| [mod-ale](https://github.com/azerothcore/mod-ale) | the windows (server side); without it, commands only |
| [AIO](https://github.com/Rochet2/AIO) | the windows: `AIO_Server` in `lua_scripts`, and the `AIO_Client` addon on every client |
| [WarcraftXL](https://github.com/WarcraftXL/wxl-core) and its Modern M2 plugin | the client draws the furniture and the level-up effects |
| [mod-forever-ui](https://github.com/Adrestie/mod-forever-ui) | optional: its look, and talents and Collection in its guild page |

## How to install

1. Download `installer.exe` from the [WoW-mods-installer releases](https://github.com/Adrestie/WoW-mods-installer/releases).
2. Stop the worldserver and close the game.
3. Run `installer.exe`, give it this folder and the folders it asks for, click **Check**, then **Install**.
4. Rebuild the worldserver. On its next start, the core applies the module's SQL.
5. The installer writes the client patch into `Data\patch-Z.MPQ` and the `wxl-guildhouse` extension into the game's `Extensions`: give both to the other players.

## How to update

Same steps with the new version's folder, choosing **Update** instead of **Install**. The guilds' levels, talents, decoration stock and placed furniture, and your `mod-guild-level.conf` settings, are kept. Rebuild the worldserver.

## How to uninstall

Same steps, choosing **Remove**. Everything goes, database included: every guild loses its level and its house. Rebuild the worldserver.

## Credits

- [wxl-housing](https://github.com/sogyboi/wxl-housing) by sogyboi, which the `wxl-guildhouse` extension forks.
- [WarcraftXL](https://github.com/WarcraftXL/wxl-core), ALE and AIO (Rochet2), on which it builds.

## Configuration

`mod-guild-level.conf` sets:

- the module on or off, the members needed together, and the experience rates (kills, quests, bosses by family);
- the level and Paragon curves, and the highest Paragon level;
- each passive perk, and the level, cooldown and duration of each active perk;
- the Guild House, its decoration, and the drop chances of decoration objects;
- the guild talents: when they start, points per level, tier thresholds and reset cost.

## Commands

| Command | |
|---|---|
| `.gperk raid \| rez \| bank \| vendor \| home \| leave \| list` | the active perks |
| `/gtalent` | the guild talent window |
| `/gdeco` | the Customization mode of the Guild House |
| `/gcoll` | the Collection (also `/gcollection`) |
| `.ghouse begin \| place \| cancel \| remove` | placing furniture |
| `.guildlevel info \| setlevel \| setparagon \| addxp [guild]` | game master: a guild's level |
| `.ghouse give <object> <count> [guild]` | game master: objects for a guild's stock |
