# mod-guild-level

An AzerothCore module (WoW 3.3.5a) that brings the guild levels of Cataclysm:
members of a guild who play together earn guild experience, and the guild
climbs **25 levels**, then **9 Paragon levels**.

> **Status: step 1 of the roadmap.** Guild experience, levels, Paragon levels,
> announcements and GM commands work. The perks of the levels, the guild
> talents, the guild house and its Customization mode are planned (see
> [Roadmap](#roadmap)) and not built yet.

No client patch and no new identifiers yet: core hooks, one characters table,
texts in `module_string`. The module owns the tranches **92-93**
(92000-93999) of the [WoW-mods registry](https://github.com/Adrestie/WoW-mods/blob/main/ID_RANGES.md)
for the guild house, the perks and the talents to come.

## Guild experience

When a monster dies, every member of the group it rewards who belongs to the
killer's guild brings that guild **the experience the kill is worth to him
alone**. The more of the guild in the group, the more it earns: a monster worth
10 experience earns the guild 20 with two guildmates in the group, 50 with five.

* **Who counts:** the members the core rewards for the kill (the killer, and
  the group within reward distance), alive, of the same guild.
* **At least two:** nothing is earned with fewer than
  `GuildLevel.MinMembers` guildmates at the kill (2 by default). A guild
  member playing alone, or with players of other guilds only, earns nothing
  for his guild.
* **What each one brings:** the experience the kill would give him playing
  alone, before the group splits it and before any personal bonus (rested
  experience, heirlooms...). The server's kill rate is included, and so is the
  core's cut for monsters with little health. A monster gray to a member brings
  nothing from him.
* **Every level counts:** a character at the maximum level brings his share
  too.
* Only creatures: player kills in battlegrounds and quests earn nothing.

Everything earned is multiplied by `GuildLevel.Rate`.

## Levels

A new guild is level 0. Each step costs the amount given by the curves of the
configuration, and the bar starts again from zero at each step:

* **Levels 1 to 25:** `GuildLevel.Curve.Levels`, 25 amounts. By default 20,000
  for level 1 up to 1,300,000 for level 25, about 11 million in all.
* **Paragon 1 to 9**, past level 25: `GuildLevel.Curve.Paragon`, 9 amounts. By
  default 1,500,000 up to 4,000,000, about 23 million in all.
* At Paragon 9 the guild earns nothing more; its lifetime total still counts.

For scale: a level 80 monster is worth about 500 experience to a level 80
character, so five guildmates bring their guild about 2,500 per kill.

Each level and Paragon level reached is told to every member of the guild
online, in the green of the guild channel and in his own language:
`[Guild] Your guild has reached level 3!`. Level 25 and Paragon 9 have a
message of their own.

The guild's experience is kept in memory and written to `mod_guild_level`
(characters database) every `GuildLevel.SaveInterval` seconds, at once when a
level is reached or changed by a GM, and on shutdown. The row is deleted with
its guild.

## GM commands

Every command ends with an optional guild: its **name** (spaces allowed, no
quotes) or its **id**. Without it, the command acts on the guild of the
selected player, else on your own. They work from the console too, with a
guild named.

| Command | Effect |
|---|---|
| `.guildlevel info [$guild]` | Level, Paragon level, experience toward the next step, lifetime experience. |
| `.guildlevel setlevel $level [$guild]` | Sets the level (0 to 25), Paragon 0, empty bar. Not announced. |
| `.guildlevel setparagon $paragon [$guild]` | Sets level 25 and the Paragon level (0 to 9), empty bar. Not announced. |
| `.guildlevel setxp $xp [$guild]` | Sets the experience toward the next step, kept below what it costs. |
| `.guildlevel addxp $xp [$guild]` | Gives experience as if earned: the levels climbed are announced. |

All need the game master level (2). The texts are in English, French, German,
Spanish (both) and Russian, English for the others.

## Configuration

Everything is set in `conf/mod-guild-level.conf.dist`, and `.reload config`
applies changes. A curve made shorter makes guilds with more experience than
their step costs climb at once, without announcement.

| Option | Default | |
|---|---|---|
| `GuildLevel.Enable` | 1 | 0 = no guild earns experience (levels are kept). |
| `GuildLevel.MinMembers` | 2 | Guildmates needed at the kill. |
| `GuildLevel.Rate` | 1.0 | Multiplies everything earned. |
| `GuildLevel.Curve.Levels` | 25 amounts | Cost of levels 1 to 25. |
| `GuildLevel.Curve.Paragon` | 9 amounts | Cost of Paragon 1 to 9. |
| `GuildLevel.Announce` | 1 | Announce each level to the guild. |
| `GuildLevel.SaveInterval` | 60 | Seconds between two saves. |

## For the other modules

`GuildLevelMgr::Get(guildId)` returns the level, the Paragon level and the
experience of a guild, from any thread; a guild the module never heard of is
level 0. The next steps of this module read the level there.

## Installation

With the WoW-mods installer: point it at this folder. By hand:

1. Copy the module to `modules/mod-guild-level` in the AzerothCore sources,
   then re-run CMake and rebuild.
2. Apply `data/sql/characters` to the characters database and `data/sql/world`
   to the world database, or let the core updater do it. Every file can be run
   again safely.
3. Copy `conf/mod-guild-level.conf.dist` to the server's `configs/modules`
   folder as `mod-guild-level.conf`.

## In-game tests

Use a GM character and two or three test characters of one guild (call it
`Test Guild`), all able to group together, below the maximum level, with a
spot of monsters that are not gray to them. Run `.reload config` after each
configuration change.

1. **Startup:** the worldserver starts without errors from the module, and the
   log reads `mod-guild-level: loaded N guild levels`.
2. **Info:** `.guildlevel info Test Guild` answers level 0/25, Paragon 0/9,
   experience 0 / 20000, lifetime 0.
3. **Alone:** one member kills a monster alone. `.guildlevel info` shows no
   change.
4. **With a stranger:** the member groups with a character of another guild
   (or none) and they kill a monster. No change.
5. **Two guildmates:** two members of the guild group and kill a monster worth
   about N experience to each when alone. The guild earns about 2 x N. Check
   the lifetime experience with `.guildlevel info`.
6. **Three guildmates:** the guild earns about 3 x N for the same kind of
   monster.
7. **Out of range or dead:** a third member stays far away (beyond reward
   distance), or lies dead: he does not count, and with only one other
   guildmate left, nothing is earned.
8. **Gray monster:** kill monsters gray to everyone: nothing is earned.
9. **Level up:** `.guildlevel setxp 19990 Test Guild`, then kill a monster with
   two guildmates. Every member online reads, in green,
   `[Guild] Your guild has reached level 1!`, and `.guildlevel info` shows
   level 1 with the leftover on the bar.
10. **Several levels at once:** `.guildlevel addxp 100000 Test Guild`: one
    announcement per level climbed (levels 2 and 3 by default), and the
    leftover on the bar.
11. **Level 25:** `.guildlevel setlevel 24`, then `.guildlevel addxp 1300000`:
    the level 25 message tells the experience now goes to Paragon levels.
12. **Paragon:** `.guildlevel addxp 1500000`: `[Guild] Your guild has reached
    Paragon level 1!`.
13. **Paragon 9:** `.guildlevel setparagon 8`, then `.guildlevel addxp
    4000000`: the Paragon 9 message. `.guildlevel info` answers that every
    level is reached; more kills only raise the lifetime total.
14. **Limits:** `.guildlevel setlevel 26` and `.guildlevel setparagon 10` are
    refused with the maximum. `.guildlevel setxp 999999999` stops one below
    the cost of the step.
15. **Which guild:** select a member of the guild and type `.guildlevel info`
    without a name: the guild of the selected player. Without a selection, your
    own guild. From the console, `.guildlevel info Test Guild` and
    `.guildlevel info <guild id>` both answer.
16. **Persistence:** earn some experience, wait one minute
    (`GuildLevel.SaveInterval`), and look at the guild's row in
    `mod_guild_level`. Restart the worldserver: `.guildlevel info` shows the
    same values.
17. **Disband:** disband a guild that earned experience: its row is gone from
    `mod_guild_level`.
18. **Settings:** `GuildLevel.MinMembers = 1` lets a member alone earn;
    `GuildLevel.Rate = 2` doubles what is earned; `GuildLevel.Enable = 0` stops
    every gain, and `.guildlevel info` says so; `GuildLevel.Announce = 0`
    silences the level messages.
19. **Curves:** set `GuildLevel.Curve.Levels = "1000"`: every level costs 1,000
    (a short list repeats its last amount). A guild that had more experience
    on its bar climbs at once, without announcement. An unreadable curve
    (`"abc"`) logs an error and keeps the built-in one.
20. **Locales:** on a French client, the announcement reads
    `[Guilde] Votre guilde atteint le niveau … !`, and the commands answer in
    French.

## Roadmap

The steps to come, in the order of the original concept.

### An interface

A window of its own, or a page in
[ForeverUI](https://github.com/Adrestie/mod-forever-ui) when it is installed.

### Perks of levels 1 to 25

1. 5% more experience.
2. 5% less durability loss.
3. 5% more honor.
4. Hearthstone cooldown 5 minutes shorter.
5. Sometimes 1 more material when gathering.
6. Vendor prices 5% lower.
7. 5% of the gold looted goes to the guild bank.
8. Mounted speed 5% higher.
9. 5% more reputation.
10. Being resurrected by a guild member restores all health and mana.
11. 10% more experience (on top of level 1).
12. 15% less durability loss (on top of level 2).
13. 10% more honor (on top of level 3).
14. Hearthstone cooldown 10 more minutes shorter.
15. Sometimes 2 more materials when gathering.
16. Vendor prices 5% lower again.
17. 5% more of the gold looted goes to the guild bank.
18. Mounted speed 5% higher again.
19. 5% more reputation again.
20. Consumable effects last 30 minutes longer.
21. Teleport the raid members to you (1 hour cooldown).
22. Resurrect the raid members (out of combat only).
23. Summon a temporary guild bank (1 hour cooldown).
24. Summon a temporary reagent vendor and repairer (1 hour cooldown).
25. Teleport to the guild house.

### Paragon levels: the guild house

Paragon levels give the guild a house and reward its members who play
together:

1. The guild house, a mailbox, and the spell to teleport to the house.
2. A reagent vendor.
3. A banker.
4. A repairer.
5. Class trainers and a stable master.
6. Profession trainers with every recipe of the Vanilla, Burning Crusade and
   Wrath trainers, a forge, an anvil and the other objects crafting needs.
7. An auction house.
8. Portals to Orgrimmar / Stormwind, Thunder Bluff / Ironforge, Undercity /
   Darnassus, the Exodar / Silvermoon, the Caverns of Time, Shattrath and
   Dalaran.
9. The Customization mode.

### Guild talents, from level 10

From level 10 the guild earns talent points and unlocks its talent tree, laid
out like the grid of [mod-spheregrid](https://github.com/Adrestie/mod-spheregrid).
The guild master and the officers spend the points; resetting them costs gold
from the guild bank. Some nodes stack (statistics: +10 stamina, +10 strength,
+5 critical strike rating...), others are unique.

### Customization mode (Paragon 9)

Members decorate the guild house with objects found in dungeons and raids. The
guild ranks allowed to do so are set by the guild.

* **Drops:** customization objects only drop for a guild that unlocked the
  mode, and only in a group of 4 guildmates in a 5-player dungeon, 8 in a
  10-player raid, 20 in a 25-player raid. They can drop many times and be
  looted by any guildmate; in a 25-player raid only the 20 guildmates see them
  in their loot window.
* **Collection:** a looted object is used to add it to the guild's objects. A
  window lists the objects the guild owns and the ones it lacks, with a tooltip
  telling where to find them.
* **Placing:** a member with the right, inside the house, enters the
  Customization mode: the usual interface gives way to the Customization one.
  He drags an object from the guild's list into the world, selects a placed
  object to move, rotate and scale it with a 3D gizmo, and removes it with a
  right-click.
* **Rules:** one member at a time handles a given object. An object is placed
  at most as many times as the guild owns it (4 chairs looted, 4 chairs
  placed). Removing one gives it back to the guild's stock (3 chairs placed and
  1 in stock: removing one leaves 2 in stock).

## Licence

GPL-2.0-or-later, the licence of AzerothCore it is compiled into. See
[LICENSE](LICENSE).
