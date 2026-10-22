# mod-guild-level: the detailed guide

The full reference of the module: every feature, the GM commands, the options, the
ID ranges, the manual installation and the in-game tests. The short overview is
the [README](../README.md).

An AzerothCore module (WoW 3.3.5a) that brings the guild levels of Cataclysm,
and builds a guild house on them:

* **Guild experience and levels:** members of a guild who play together earn
  guild experience; the guild climbs **25 levels**, then **Paragon levels**
  (99 by default, the first 9 unlocking the Guild House features),
  announced to its members with a level-up toast and shown on a bar in the
  guild window.
* **Perks:** levels 1 to 20 give passive perks to every member (experience,
  honor, reputation, mount speed, cheaper vendors...), levels 21 to 25 active
  perks (summon the raid, resurrect it, a guild bank chest, a guild supplier,
  the way home).
* **Guild talents:** from Paragon 10, every Paragon level gives the guild a
  talent point, spent by its officers in a tree of statistics and
  bonuses that every member gets.
* **The guild house:** from level 25, the guild has its own house in its
  faction's PvP hall (Champions' Hall, Hall of Legends), one phase per guild;
  Paragon 2 to 8 bring its services (supplier, banker, trainers, auction
  house, portals).
* **Customization (Paragon 9):** the members decorate the house with the
  furniture of the modern housing: **2443 objects** in seven families,
  dropped by dungeon and raid bosses (by theme and by boss) and given by
  **800 quests**, placed, turned and sized with a 3D editor, and listed in a
  Collection window that tells where each one is found.

The texts, the interface and the objects' names are in English, French,
German, Spanish (esES) and Russian.

The module owns the numbers **92000-94599** of the [WoW-mods registry](https://github.com/Adrestie/WoW-mods/blob/main/ID_RANGES.md):
spell 92000 is the perks' aura, creature 92001 the guild supplier, displays
92010-92011 and gameobjects 920000-920001 the Guild Chest, gameobject 920002
the house's portal, spells 92021-92071 the talents, 92080-92081 the
Customization objects' use and loot mark; the Customization objects are
displays and items 92100-94599 (92100 + their rank in
`tools/decor/catalog.json`; the objects taken out leave their number unused),
gameobjects from 920100.

**A client patch** (the WoW-mods installer puts it in place): the
Customization objects' models, icons and display rows, the halls' terrain and
light, the level-up effects, and the Guild Chest of `.gperk bank`,
Cataclysm's Mobile Banking chest, converted for 3.3.5a by
`tools/convert_guild_chest.py` from what wow.export exports of a modern
client. The chest loses what 3.3.5a cannot draw: the guild's own colours and
emblem on the banner (baked blue for the Alliance, red for the Horde) and the
reflections of the metal.

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
* **Paragon levels**, past level 25, up to `GuildLevel.Paragon.Max` (99 by
  default): `GuildLevel.Curve.Paragon`. By default 1,500,000 for Paragon 1 up to
  4,000,000 for Paragon 9, about 23 million in all, and 4,000,000 for each one
  after it. The first 9 unlock the Guild House features.
* At the last Paragon level the guild earns nothing more; its lifetime total
  still counts.

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
| 25 | `.gperk home` | Into the guild house, once the guild has one (level 25, see [The guild house](#the-guild-house)). `.gperk leave` goes back. | 1 h |

`.gperk list` shows the active perks of your guild: locked, ready, or the time
left. A game master resets every cooldown of a character with
`.gperk reset [$character]` (named, else the selected player, else himself;
offline characters too). The Guild Supplier sells the reagents of every class at their usual
prices: Arcane Powder, Runes of Teleportation and of Portals, Light Feather,
Symbols of Kings and of Divinity, Holy, Sacred and Devout Candles, Ankh,
Infernal Stone, Demonic Figurine, Corpse Dust, Flash Powder, Maple and
Ironwood Seeds, Wild Quillvine, Wild Spineleaf and Starleaf Seed.

## Guild talents

From Paragon 10 (`GuildLevel.Talents.FromParagon`), every Paragon level gives
the guild a talent point (`PointsPerLevel`); a guild that had spent more points
than it earns gets them all back, free, at the next startup. The guild master and
the officers (ranks up to `GuildLevel.Talents.Rank`) spend them in a tree of
nodes: statistics (stamina, strength, critical strike rating...) that stack
point after point, and unique bonuses. A node is bought when it starts the
tree or is linked to a node that holds a point; tiers 2 and 3 open once the
guild has spent 10 and 25 points. Resetting gives every point back and costs
gold from the guild bank (5000 gold the first time, 10 % more each reset, 10 %
less each week without one).

Every member of the guild gets the effects; the guild's aura lists them in its
tooltip. The tree is drawn by a game master in the talent window's edit mode
and kept in the world database. `.gtalent` opens the window (a side tab of
ForeverUI's guild page when it is installed, a button on the stock guild
window otherwise).

## The guild house

From **level 25**, a guild has a house: **Champions' Hall** (map 449) for an
Alliance guild, the **Hall of Legends** (map 450) for a Horde one -- the two PvP
halls the game left empty in 2.0. Members go in with `.gperk home` (the level
25 perk, 1 h cooldown) and out with `.gperk leave`, back where they came from.

**Every guild has its own house, by phases.** An instance per guild is not
possible without changing the core: it refuses a second player into an
instance someone of another group is in. So the halls stay open maps, and a
guild whose house is open owns one of the hall's 31 phases: its members, its
creatures and its objects live in that phase only, and no guild sees another.
The phase goes back to the pool once the house has been empty for
`GuildLevel.House.EmptyDelay` seconds; what stands in it is in the database and
comes back. Up to 31 houses can be open at once per hall, any number of guilds
in all; a 32nd guild is asked to try again a few minutes later.

**What stands in it** is in the world table `mod_guild_level_house_spawn`, one
row per creature or object, each unlocked at a Paragon level: placed when the
house opens, and at once in an open house when the guild reaches that level.
Rows under id 1000 are the module's; a server adds its own above.

| Paragon | What appears |
|---|---|
| 0 (level 25) | a mailbox by the entrance |
| 2 | the Guild Supplier (reagents of every class, repairs) |
| 3 | a banker of the faction's capital (Olivia Burnside / Karus) |
| 4 | nobody (the Guild Supplier repairs) |
| 5 | the ten class trainers of the faction's capital (the Death Knight one from Acherus) and a stable master |
| 6 | the fourteen Grand Master profession trainers of Dalaran, who teach every recipe of Vanilla, Burning Crusade and Wrath, with a forge, an anvil and a cooking fire |
| 7 | two auctioneers of the faction: its own auction house |
| 8 | portals to the four capitals of the faction, Shattrath, Dalaran and the Caverns of Time (gameobject 920002 of the module: the game has no such portal) |

**The layout** was proposed from the walkable floor of each hall (the
server's mmaps). Champions' Hall, a long room: services by the entrance, class
trainers along the north wall, profession trainers along the south wall,
auctioneers and portals on the dais at the far end. Hall of Legends, a round
room: services by the entrance, class trainers around the west of the ring,
profession trainers around the east, auctioneers facing the entrance, portals
in a circle at the centre. A GM adjusts it from inside a hall:

| Command | Effect |
|---|---|
| `.guildlevel house list` | Every spawn of the hall: id, Paragon level, kind, entry, position. |
| `.guildlevel house move $id` | Moves the spawn where the GM stands, facing his way. |
| `.guildlevel house add $paragon creature\|object $entry` | Adds a spawn there (ids from 1000). |
| `.guildlevel house del $id` | Removes the spawn. |

Every open house of the hall follows at once.

Someone who is not a member (or whose guild has no house) and ends up in a
hall -- a GM teleport, a login after leaving the guild -- is sent back. Game
masters keep their own phase there (`.modify phase` shows a house).

## Customization (Paragon 9)

At **Paragon 9** the guild decorates its house with the furniture of the modern
housing: **2443 objects** in seven families (furniture, lights, decoration,
plants, structure, utility, miscellaneous), the models of Midnight's housing
converted for 3.3.5a (`tools/decor`). Every guild sees only its own objects, in
its own phase of the hall.

**Placing.** A member of a rank allowed to decorate (`GuildLevel.House.Decor.Rank`),
inside the house, presses **Customization** at the top of the screen (or types
`/gdeco`): the usual interface gives way to the guild's objects (left) and the
edit bar (bottom).

| Action | How |
|---|---|
| Take a new object | click it in the list (hovering shows it turning above the list); it follows the cursor on the hall's floors and walls, the wheel turns it, a left click sets it down |
| Take a placed object | the objects within 30 yards and in the same room are outlined in blue (lighter under the cursor): click one |
| Move, turn, size | the gizmo (keys 1, 2, 3); turning around the world's axes or the object's own, and back to its first rotation; size from 0.1 to 2 |
| Place | Place, or a click away from the object and its gizmo |
| Back to the stock | right click on a placed object (it turns red), right click again |
| Leave | Leave, or Esc |

One member at a time handles an object; an object is placed at most as many
times as the guild owns it, and taken away it goes back to the stock. A guild
disbanded loses its stock and its placed objects.

**Getting objects.** Each object is an item (rare, **not bound: traded and
sold**, 50 silver), used to add the object to the guild's stock:

* **Bosses** of dungeons and raids drop objects (`DropChance`, 10%) for a guild
  of the Customization's Paragon level there in numbers: 4 members in a
  dungeon, 8 in a 10-player raid, 20 in a 25-player one. A drop gives 1 object
  in a dungeon, 2 in a 10-player raid, 3 in a 25-player one (`Drops.*`): one
  member each. Only that guild's members see them in the loot. Each one: a boss's own objects first (`tools/decor/loot.json`,
  `bosses`), else, 60% of the time (`ThemeChance`), one of the instance's theme
  (blood elves in Magisters' Terrace, dragons in Onyxia's Lair...), else one of
  the objects of no theme.
* **800 quests** of Vanilla, Burning Crusade and Wrath, all open to both
  factions, each give one object to a member of a guild who hands it in (by
  mail when the bags are full). No test, disabled, raid, daily, weekly,
  repeatable, class, profession or holiday quest. The quests of a theme's
  zones give that theme's objects first (`tools/decor/build_quests.py`).

**Collection.** A side tab of ForeverUI's guild window (beside the talents'),
or without ForeverUI a window of its own (the **Collection** button of the
guild window); `/gcoll` and the Customization panel's **Collection** button
open it too: every object, owned (with how many, how many placed)
or missing, by family, with a search; hovering one shows it turning and where
it is found (its quest, its bosses, the dungeons and raids of its theme).

**Languages.** The objects' names are the housing's own in English, French,
German, Spanish (esES) and Russian (`tools/decor/build_locales.py`), as are the
module's texts and the interface.

**What it needs on the client:** [WarcraftXL](https://github.com/WarcraftXL/wxl-core)
and its extension **wxl-guildhouse** (`client/dist/wxl-guildhouse`, sources in
`client/wxl-guildhouse`): the preview of an object in hand, the outlines, the
gizmo and the cursor's ray on the hall are its work. Without it the
Customization mode says so and stays closed; the objects placed are still seen
by everyone, and the Collection works. The models, icons and display rows go
into the game's archives with the rest of the client patch.

| Command | Effect |
|---|---|
| `.ghouse give $object $count [$guild]` | GM (2): adds (or, negative, takes away) objects to a guild's stock; 0 for every object. |
| `.ghouse begin\|place\|cancel\|remove ...` | What the Customization window does for the member (any player, with the rules above). |

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
Spanish (esES) and Russian, English for the others.

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
| `GuildLevel.Paragon.Max` | 99 | The last Paragon level. |
| `GuildLevel.Curve.Paragon` | 9 amounts | Cost of each Paragon level (the last amount for the ones after). |
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

| `GuildLevel.House.Enable` | 1 | 0 = no guild house. |
| `GuildLevel.House.Paragon` | 0 | The Paragon level that opens the house, past level 25 (0: level 25 itself). |
| `GuildLevel.House.EmptyDelay` | 60 | Seconds an empty house keeps its phase. |
| `GuildLevel.House.Decor.Enable` | 1 | 0 = no Customization (no drop, no quest object, no edit). |
| `GuildLevel.House.Decor.Paragon` | 9 | The Paragon level that opens it. |
| `GuildLevel.House.Decor.Rank` | 1 | Highest guild rank id allowed to decorate (0: the guild master only). |
| `GuildLevel.House.Decor.DropChance` | 10 | % for a boss to drop an object for a guild there in numbers. |
| `GuildLevel.House.Decor.ThemeChance` | 60 | % for each object to be of the instance's theme (else one of no theme). |
| `GuildLevel.House.Decor.Drops.Dungeon` / `.Raid10` / `.Raid25` | 1 / 2 / 3 | Objects a drop gives, one member each. |
| `GuildLevel.House.Decor.Members.Dungeon` / `.Raid10` / `.Raid25` | 4 / 8 / 20 | Guildmates needed at the kill. |
| `GuildLevel.Active.<Perk>.Level` | 21 to 25 | Guild level of each active perk (Raid, Rez, Bank, Vendor, Home). |
| `GuildLevel.Active.<Perk>.Cooldown` | 3600 (Rez 600, Home 0) | Seconds; 0 = none. |
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

## Dependencies

### Server, required

| What | What for |
|---|---|
| AzerothCore WotLK (3.3.5a) | the C++ module is compiled into it: guild experience, levels, perks, talents, the guild house, the Customization's objects, drops and quest rewards |

### Server and client, required for the interface

| What | What for | Without it |
|---|---|---|
| [ALE](https://github.com/azerothcore/mod-ale) (the AzerothCore Lua Engine, formerly mod-eluna) | runs the server side of the windows (`data/lua/GuildLevel` goes into `lua_scripts`) | no window: experience, levels, perks, talents and the house work the same, through the commands (`.gtalent`, `.ghouse`) |
| [AIO](https://github.com/Rochet2/AIO) | sends the windows to the players: the server side next to ALE, the `AIO_Client` addon on every client | as above |

The windows: the level bar in the guild window, the level-up toast, the talent
window, the Customization mode and the Collection window.

### Client, required for the Customization and the effects

| What | What for | Without it |
|---|---|---|
| [WarcraftXL](https://github.com/WarcraftXL/wxl-core) | loads the client extensions below | as below |
| WarcraftXL's Modern M2 plugin (`wxl-modern-m2`) | draws the modern (MD21) models: the 2443 Customization objects and the level-up effects | those models are not drawn |
| the extension `wxl-guildhouse` (this module's, `client/dist/wxl-guildhouse`, sources in `client/wxl-guildhouse`) | the Customization mode's editor: the object in hand, the outlines, the 3D gizmo, the cursor's ray on the hall | the Customization mode says so and stays closed; placed objects are still seen by everyone, the Collection works |
| the client patch (`data/art`, the rows of `data/dbc`) | models, icons, display rows, the halls' terrain and light, the Guild Chest | missing models and icons |

### Optional

| What | What for |
|---|---|
| [ForeverUI](https://github.com/Adrestie/mod-forever-ui) | its look for the Customization windows, and the talent window and the Collection as side tabs of its guild page |

### The installer

The [WoW-mods](https://github.com/Adrestie/WoW-mods) installer puts everything
in place from this folder: the module into the server's sources, the SQL, the
configuration, the Lua scripts, the client patch (written into the game's last
patch archive, `Data\patch-Z.MPQ` and its language twins) and the extension
into the game's `Extensions` folder. It needs the game's Wow.exe to load
WarcraftXL for the extension, and may patch Wow.exe so that it reads the
custom archives (`--patch-wow-exe`).

## Installation

With the WoW-mods installer: point it at this folder. By hand:

1. Copy the module to `modules/mod-guild-level` in the AzerothCore sources,
   then re-run CMake and rebuild.
2. Apply `data/sql/characters` to the characters database and `data/sql/world`
   to the world database, or let the core updater do it. Every file can be run
   again safely.
3. Copy `conf/mod-guild-level.conf.dist` to the server's `configs/modules`
   folder as `mod-guild-level.conf`.
4. For the interface: copy `data/lua/GuildLevel` into the ALE scripts folder
   (`lua_scripts/GuildLevel`), with AIO installed on the server and the
   `AIO_Client` addon on every client.
5. For the client: write `data/art` into a patch archive the game reads last
   (`Data\patch-Z.MPQ`), with the rows of `data/dbc` added to the game's DBC
   files of the same names, and copy `client/dist/wxl-guildhouse` into the
   game's `Extensions` folder (WarcraftXL installed).

The WoW-mods installer does all of it, and leaves the extension's sources
(`client`) out of the server's modules folder: they are a Windows client DLL,
not server code.

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
41. **House:** a guild at level 24: `.gperk home` answers that the house opens
    at level 25, no cooldown. At level 25, `.gperk home` takes an
    Alliance member to Champions' Hall, a Horde member to the Hall of Legends,
    with a mailbox near the entrance; `.gperk leave` takes him back where he
    was.
42. **Cooldowns kept:** use `.gperk bank`, restart the worldserver, `.gperk
    list`: the time left has gone on counting. The row is in
    `mod_guild_level_perk_cooldown`.
43. **Others:** a player out of a guild gets "You are not in a guild".
44. **Reset:** after using `.gperk bank`, a GM types `.gperk reset <name>`:
    the answer names the character, `.gperk list` shows the perk ready again,
    and his rows are gone from `mod_guild_level_perk_cooldown`. A player
    without GM rights cannot use it.

### Guild house

45. **Own house:** two guilds at level 25, one member of each goes home: each
    sees his mailbox, neither sees the other. Two members of the same guild,
    not grouped, both go home: they see each other.
46. **Strangers:** a player of another guild, or none, teleported into the hall
    by a GM, is sent back with "This is not your guild's house".
47. **Leaving the guild:** a member inside is removed from the guild: he is
    sent out at once.
48. **Paragon change:** with a member inside, `.guildlevel setparagon 0` then
    `setparagon 1`: the mailbox goes, then comes back.
49. **Empty house:** everyone leaves; a minute later the phase is free again
    (a GM with `.modify phase` sees nothing there). Going back in brings the
    mailbox back.
50. **Paragon 2 to 8:** a guild at Paragon 8: going home shows the supplier,
    banker by the entrance, the class trainers and stable master,
    the profession trainers with forge, anvil and fire, two auctioneers and
    seven portals. At Paragon 3, only what Paragon 1 to 3 unlock. Each NPC
    works: training, bank, auction house of the faction, repairs; each portal
    goes where it says (the Caverns of Time one to Tanaris).
51. **Layout:** a GM in the hall: `.guildlevel house list`, then
    `.guildlevel house move <id>` somewhere else: the NPC moves in every open
    house of the hall. `add` and `del` likewise.
52. **Logout inside:** a member logs out in the house and back in: he is in
    his house again, in its phase.

### Customization

53. **Way in:** a guild at Paragon 9 (`.guildlevel setparagon 9`), an officer
    goes home: the Customization button shows at the top of the screen, at
    login in the house as on arrival. A member of a lower rank, or a guild at
    Paragon 8, gets the reason instead.
54. **Stock:** `.ghouse give 494 4`: four Sturdy Wooden Chairs in the list.
    Place them: the fifth cannot be. Right click twice on one: back to the
    stock, 1 left to place.
55. **Edit:** take a placed chair, move, turn (World, then Local, then back to
    its first rotation) and size it, click away: it stays as left, and another
    member of the guild sees it there after relog; size is kept.
56. **Item:** `.additem 92100`: the item is not bound, sells for 50 silver,
    and used by a member of a Paragon 9 guild it joins the stock ("goes into
    your guild's objects"). Used outside a guild: refused.
57. **Quest:** a member of a guild hands in a quest of
    `mod_guild_level_decor_quest`: its object comes into the bags (by mail
    with full bags), with a message in the player's language.
58. **Collection:** `/gcoll`: owned and missing objects, families, search;
    hovering a missing one shows its quest or its dungeons.
59. **Disband:** the guild disbanded: its stock and placed objects are gone
    and its phase is freed.

## Credits

* **Blizzard Entertainment** owns World of Warcraft and everything taken from
  it here: the models, textures, icons and names of the modern housing's
  furniture and their official translations, the Guild Chest, the level-up
  effects and the halls. This module is not affiliated with Blizzard.
* [AzerothCore](https://github.com/azerothcore/azerothcore-wotlk), the server the module is
  compiled into.
* [ALE](https://github.com/azerothcore/mod-ale), the AzerothCore Lua Engine
  (formerly mod-eluna), and [AIO](https://github.com/Rochet2/AIO) by Rochet2,
  which carry the windows.
* [WarcraftXL](https://github.com/WarcraftXL/wxl-core) and its Modern M2
  plugin, which load the extension and draw the modern models.
* [wxl-housing](https://github.com/sogyboi/wxl-housing) by sogyboi: the
  extension `wxl-guildhouse` is a fork of it (the spawn and chunk attachment of
  the objects, the ImGui overlay's host), under its licence, GPL-3.0-or-later.
* [Dear ImGui](https://github.com/ocornut/imgui) by Omar Cornut and
  [ImGuizmo](https://github.com/CedricGuillemet/ImGuizmo) by Cedric Guillemet
  and contributors (MIT), for the gizmo.
* wow.export, through which the furniture, the Guild Chest and the effects were
  exported from the modern client.
* [wago.tools](https://wago.tools), whose tables of the modern client gave the
  objects' names in each language, their families and the dungeons' and zones'
  names (`tools/decor/build_locales.py`).
* m2port of [mod-limit-break](https://github.com/Adrestie/mod-limit-break),
  which lays the modern models out for the client; its table of the modern
  shaders' texture coordinates comes from WebWowViewerCpp.
* [ForeverUI](https://github.com/Adrestie/mod-forever-ui), whose look and guild
  page the windows use when it is installed.

## Licence

The module (server code, SQL, Lua, tools): GPL-2.0-or-later, the licence of
AzerothCore it is compiled into. See [LICENSE](../LICENSE).

The client extension `wxl-guildhouse` (`client/wxl-guildhouse`,
`client/dist/wxl-guildhouse`): GPL-3.0-or-later, the licence of wxl-housing it
is forked from; ImGuizmo (`client/wxl-guildhouse/third_party/imguizmo`) keeps
its MIT licence. See [client/wxl-guildhouse/LICENSE](../client/wxl-guildhouse/LICENSE).

The game's assets (models, textures, icons, names) belong to Blizzard
Entertainment and are not covered by these licences.
