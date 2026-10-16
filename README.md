# mod-guild-level

An AzerothCore module (WoW 3.3.5a) that brings the guild levels of Cataclysm:
members of a guild who play together earn guild experience, and the guild
climbs **25 levels**, then **9 Paragon levels**.

> **Status: step 3 of the roadmap.** Guild experience, levels, Paragon levels,
> announcements, GM commands, the passive perks of levels 1 to 20 and the
> active perks of levels 21 to 24 work (see [Perks](#perks-of-levels-1-to-20)
> and [Active perks](#active-perks-of-levels-21-to-25)). The level 25 perk, the
> guild talents, the guild house and its Customization mode are planned (see
> [Roadmap](#roadmap)) and not built yet.

No client patch: core hooks, one characters table, texts in `module_string`,
and one hidden server-side spell. The module owns the tranches **92-93**
(92000-93999) of the [WoW-mods registry](https://github.com/Adrestie/WoW-mods/blob/main/ID_RANGES.md):
spell 92000 is the perks' aura, creature 92001 the guild supplier, displays
92010-92011 and gameobjects 920000-920001 the Guild Chest; the rest is kept for
the guild house and the talents to come.

**A client patch for one thing**: the Guild Chest of `.gperk bank` is
Cataclysm's Mobile Banking chest, converted for 3.3.5a by
`tools/convert_guild_chest.py` from what wow.export exports of a modern
client. Its files (`data/art/World/GuildLevel/GuildChest`) and its display rows
(`data/dbc/guild_level_GameObjectDisplayInfo.dbc`) go into the game client with
the WoW-mods installer. The model loses what 3.3.5a cannot draw: the guild's
own colours and emblem on the banner (baked blue for the Alliance, red for the
Horde) and the reflections of the metal.

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

## Perks of levels 1 to 20

Every member of a guild gets the perks of its **current level**, alone or in a
group, from the moment the level is reached or the member joins; leaving the
guild takes them away. The "more" perks of levels 11 to 19 add up with those of
levels 1 to 9. A Paragon guild has them all.

| Levels | Perk | Total | How |
|---|---|---|---|
| 1, 11 | Experience, from every source | +5%, then +15% | hook on the experience given |
| 2, 12 | Durability mended: 1 point every minute on a damaged piece worn | 1 piece, then 2 | player tick |
| 3, 13 | Honor, from every source | +5%, then +15% | hidden aura (honor gain) |
| 4, 14 | Hearthstone cooldown | -5 min, then -15 min | spell script on the Hearthstone |
| 5, 15 | Lucky gathering: 10% chance per node or skinned corpse | +1 item, then +3 | hook on the loot |
| 6, 16 | Vendor prices and repairs | -5%, then -10% | hook on the price discount |
| 7, 17 | Gold looted, added to the guild bank | 5%, then 10% | hook on the loot money |
| 8, 18 | Mounted speed, ground and flying | +5%, then +10% | hidden aura (mounted speed) |
| 9, 19 | Reputation gained | +5%, then +10% | hook on the reputation given |
| 10 | Resurrected by a guildmate | full health and mana | hook on the resurrection |
| 20 | Flasks, elixirs and food buffs | +30 min | hook on the aura applied |

Details:

* **Experience** applies to kills, quests, exploration and battlegrounds. It
  does not feed guild experience, which is read from the kill itself.
* **Honor** and **mounted speed** ride on spell 92000, a passive, hidden aura
  kept through death, whose amounts the module sets from the guild's level.
  Mounted speed multiplies with everything else (riding skill, Crusader Aura,
  Riding Crop...).
* **Hearthstone**: the cooldown is shortened right after the cast (spell 8690,
  `spell_guild_level_hearthstone`).
* **Gathering**: mining, herbalism and skinning. One roll per node or corpse,
  on the first item taken (the ore, the herb, the leather), which then comes
  with 1 more (3 more from level 15). A green line tells the player.
* **Vendors**: the price of everything bought from a vendor, and repairs.
  Trainers and flight masters keep their prices.
* **Gold to the bank**: the gold looted from creatures and chests is **added**
  to the guild bank of the looter: the looters keep all of theirs. A group's
  pile counts once, for the guild of the one who takes it.
* **Reputation**: gains only, from kills and quests; a loss stays a loss.
* **Resurrection**: when a player accepts a resurrection offered by a member of
  his guild (a resurrection spell; a Soulstone is the player's own), he comes
  back with full health and mana.
* **Consumables**: flasks and elixirs (told by the item that cast them) and
  food buffs (the "Well Fed" auras) last 30 more minutes, once per
  application.

* **Durability**: the core has no hook where durability is lost, and the
  module never changes the core. So the perk mends instead: every minute
  (`GuildLevel.Perk.Durability.Interval`), a damaged piece worn, drawn at
  random, gets 1 point of durability back; 2 different pieces from level 12.
  A broken piece mended this way gets its stats back. Nothing happens while
  the player is dead.

## Active perks of levels 21 to 25

Members of a guild of the level asked use them with **player commands**, no
client patch needed (buttons in an interface will come later and call the same
thing). Each perk has its own **cooldown per character**, kept in
`mod_guild_level_perk_cooldown` (characters database) across restarts, and
deleted with the character. A perk that does nothing (no one to summon, no
one dead, nothing could be placed) starts no cooldown.

| Level | Command | Perk | Cooldown |
|---|---|---|---|
| 21 | `.gperk raid` | Every member of the group or raid gets a summon to accept, as from a summoning stone. Out of combat, not in a battleground or an arena; dead members and members in a battleground are left out. | 1 h |
| 22 | `.gperk rez` | Every dead member of the group on the same map and instance gets a resurrection to accept, where the player stands, with 35% of their health and mana (full for guildmates, thanks to the level 10 perk). Out of combat. | 10 min |
| 23 | `.gperk bank` | The Guild Chest of Cataclysm's Mobile Banking stands in front of the player for 5 minutes: blue banner for the Alliance (gameobject 920000), red for the Horde (920001). Not in a battleground or an arena. | 1 h |
| 24 | `.gperk vendor` | The Guild Supplier (creature 92001), a reagent vendor who repairs, waits in front of the player for 5 minutes. Not in a battleground or an arena. | 1 h |
| 25 | `.gperk home` | To the guild house: **it comes with the guild house**. For now the command says so and starts no cooldown. | 1 h |

`.gperk list` shows the active perks of your guild: locked, ready, or the time
left. A game master resets every cooldown of a character with
`.gperk reset [$character]` (named, else the selected player, else himself;
offline characters too). The Guild Supplier sells the reagents of every class at their usual
prices: Arcane Powder, Runes of Teleportation and of Portals, Light Feather,
Symbols of Kings and of Divinity, Holy, Sacred and Devout Candles, Ankh,
Infernal Stone, Demonic Figurine, Corpse Dust, Flash Powder, Maple and
Ironwood Seeds, Wild Quillvine, Wild Spineleaf and Starleaf Seed.

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
| `.guildlevel perks [$guild]` | The perks the guild's members get at its current level, then its active perks: locked, ready, or the cooldown left of the selected player (else your own) when he belongs to the guild. |

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
| `GuildLevel.SaveInterval` | 10 | Seconds between two saves. |
| `GuildLevel.Perks.Enable` | 1 | 0 = no perk at all. |
| `GuildLevel.Perk.Experience` | `1:5 11:10` | % experience. |
| `GuildLevel.Perk.Durability` | `2:1 12:1` | Damaged pieces worn mended by 1 point per period. |
| `GuildLevel.Perk.Durability.Interval` | 60 | Seconds between two mendings. |
| `GuildLevel.Perk.Honor` | `3:5 13:10` | % honor. |
| `GuildLevel.Perk.Hearthstone` | `4:5 14:10` | Minutes off the Hearthstone cooldown. |
| `GuildLevel.Perk.Gathering` | `5:1 15:2` | Items more on a lucky gathering. |
| `GuildLevel.Perk.Gathering.Chance` | 10 | % chance of a lucky gathering. |
| `GuildLevel.Perk.Vendor` | `6:5 16:5` | % off vendor prices and repairs. |
| `GuildLevel.Perk.GoldToBank` | `7:5 17:5` | % of looted gold added to the guild bank. |
| `GuildLevel.Perk.MountSpeed` | `8:5 18:5` | % mounted speed. |
| `GuildLevel.Perk.Reputation` | `9:5 19:5` | % reputation. |
| `GuildLevel.Perk.Resurrection` | `10:1` | Above 0: full resurrection by a guildmate. |
| `GuildLevel.Perk.Consumables` | `20:30` | Minutes added to flasks, elixirs and food buffs. |

| `GuildLevel.Active.<Perk>.Level` | 21 to 25 | Guild level of each active perk (Raid, Rez, Bank, Vendor, Home). |
| `GuildLevel.Active.<Perk>.Cooldown` | 3600 (Rez 600) | Seconds; 0 = none. |
| `GuildLevel.Active.Rez.Percent` | 35 | Health and mana of the resurrection offered. |
| `GuildLevel.Active.Bank.Duration` | 300 | Seconds the chest stays. |
| `GuildLevel.Active.Bank.Entry.Alliance` / `.Horde` | 920000 / 920001 | The chest of each faction (any gameobject of the guild bank type; 187294 is the game's Guild Vault). |
| `GuildLevel.Active.Vendor.Duration` / `.Entry` | 300 / 92001 | Seconds the supplier stays; its creature. |

A perk is a list of `level:amount` steps that add up once reached: `1:5 11:10`
is +5 from level 1, +15 from level 11. `""` turns a perk off. After
`.reload config`, the members online get the new amounts at once.

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
16. **Persistence:** earn some experience, wait ten seconds
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

### Perks

Same guild, level 20 characters. Before each test, set the guild's level with
`.guildlevel setlevel N` and check `.guildlevel perks`; set it back to 0 for
the comparison. Numbers are for the default configuration.

21. **List:** `.guildlevel perks` at level 0 shows every perk at 0 and "no";
    at level 20: experience +15%, honor +15%, reputation +10%, mounted speed
    +10%, vendors -10%, gold 10%, Hearthstone -15 min, gathering 10% for 3
    more, resurrection "yes", consumables +30 min, durability 2 items every
    60 seconds.
22. **Experience:** a solo kill worth N experience gives N at level 0,
    N x 1.05 at level 1, N x 1.15 at level 11 (rested experience aside). A
    quest turned in gives its experience x 1.15 at level 11.
23. **Honor:** at level 3, an honorable kill or a battleground bonus gives 5%
    more honor than at level 0; 15% more at level 13.
24. **Hearthstone:** after use, the Hearthstone's cooldown is 30 minutes at
    level 0, 25 at level 4, 15 at level 14.
25. **Gathering:** at level 5, gather about 30 nodes: about 1 in 10 gives one
    more ore or herb, with the green "Bountiful gathering" line. At level 15,
    3 more. `GuildLevel.Perk.Gathering.Chance = 100` makes it every time.
26. **Vendors:** at level 6, an item worth 1 gold at a neutral vendor costs
    95 silver; at level 16, 90 silver. Repairs are 10% cheaper. A trainer's
    prices do not change.
27. **Gold to the bank:** at level 7, loot a creature dropping G copper: the
    looter gets G, the guild bank grows by 5% of G (10% at level 17).
28. **Mounted speed:** at level 8, a 60% ground mount runs at 168% instead of
    160% (x 1.05); at level 18, 176%.
29. **Reputation:** at level 9, a kill worth 10 reputation gives 10.5; at
    level 19, 11. A reputation loss is unchanged.
30. **Resurrection:** at level 10, a guildmate resurrects a dead member, who
    comes back with full health and mana (the spell's own amount at level 9).
31. **Consumables:** at level 20, a flask lasts 1 hour 30 instead of 1 hour;
    an elixir and a food buff ("Well Fed") last 30 minutes more. Relogging does
    not add another 30 minutes.
32. **Level change:** with a member online, `.guildlevel setlevel 8` then
    `setlevel 0`: the mounted speed changes at once, without relogging.
33. **Joining and leaving:** in a level 20 guild, `.guild uninvite` a member:
    his perks stop at once (mounted speed back to normal); invited again, they
    come back.
34. **Durability:** at level 2, wear down the gear (die once, or
    `.damage` yourself until the pieces lose points), note the durability of
    each piece worn, wait 5 minutes: 5 points have come back in all, one per
    minute, spread over random pieces. At level 12, 10 points in 5 minutes,
    never twice the same piece in the same minute. At level 1, none. A
    broken piece (0) mended to 1 gets its stats back.
35. **Switch:** `GuildLevel.Perks.Enable = 0` and `.reload config`: every perk
    stops, and `.guildlevel perks` says so.

### Active perks

A guild at level 20, then 25 (`.guildlevel setlevel`); two or three members
in a group, characters with a client to accept summons and resurrections.

36. **Locked:** at level 20, `.gperk raid` answers that the guild must be level
    21; `.gperk list` shows the five perks "locked". `.guildlevel perks` shows
    the same list under the passive perks.
37. **Summon:** at level 21, members far away (another zone, another continent)
    get the summon window when `.gperk raid` is typed; accepting brings them
    next to the player. The answer counts the summons sent. Typed again: "not
    ready, 59 min … s left". In combat, or in a battleground: refused, no
    cooldown. Alone, out of a group: refused.
38. **Resurrection:** at level 22, a member of the group dies; out of combat,
    `.gperk rez` offers him a resurrection; accepted, he comes back next to the
    player, with full health and mana if he is a guildmate (35% otherwise).
    Cooldown 10 min. With no one dead: refused, no cooldown.
39. **Chest:** at level 23, `.gperk bank`: the Guild Chest of Cataclysm stands
    two yards in front of the player, with a blue banner for an Alliance
    player, red for a Horde one; right-click opens the guild bank. After 5
    minutes it disappears. Cooldown 1 h. Without the client files of the
    module, the client draws nothing where the chest stands.
40. **Supplier:** at level 24, `.gperk vendor`: the Guild Supplier stands in
    front of the player, sells the 19 reagents and repairs. After 5 minutes he
    disappears. Cooldown 1 h.
41. **House:** at level 25, `.gperk home` answers that the perk comes with the
    guild house; no cooldown starts.
42. **Cooldowns kept:** use `.gperk bank`, restart the worldserver, `.gperk
    list`: the time left has gone on counting. The row is in
    `mod_guild_level_perk_cooldown`.
43. **Others:** a player out of a guild gets "You are not in a guild".
44. **Reset:** after using `.gperk bank`, a GM types `.gperk reset <name>`:
    the answer names the character, `.gperk list` shows the perk ready again,
    and his rows are gone from `mod_guild_level_perk_cooldown`. A player
    without GM rights cannot use it.

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
