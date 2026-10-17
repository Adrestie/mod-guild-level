-- mod-guild-level — the layout of the guild houses (Paragon 2 to 8).
--
-- Proposed from the walkable floor of each hall (the server's mmaps), every
-- spot facing the centre of the room. The Alliance hall gives each wing its
-- trade: the class trainers in the town hall, the crafts with the forge and
-- the anvil in the second guild house, cooking, first aid, fishing and the
-- auctioneers in the inn, the portals upstairs in the first guild house; the
-- barracks, the largest wing, stays empty; the spots were then set in game
-- by hand. To adjust in game with
-- `.guildlevel house list | move | add | del` (rows from id 1000 are a
-- server's own and survive updates).
--
--   Paragon 2  the Guild Supplier (reagents, repairs)
--   Paragon 3  a banker of the faction's capital
--   Paragon 4  (no one: the Guild Supplier repairs)
--   Paragon 5  the class trainers (the capital's; Death Knight from Acherus)
--              and a stable master
--   Paragon 6  the Grand Master trainers of Dalaran, every profession, with
--              a forge, an anvil and a cooking fire
--   Paragon 7  two auctioneers of the faction (its auction house)
--   Paragon 8  portals: the four capitals of the faction, Shattrath, Dalaran
--              and the Caverns of Time
--
-- Idempotent: the module's rows (ids under 1000) are written again.

-- THE PORTAL TO THE CAVERNS OF TIME, which the game has not: the Dalaran
-- portal's look, casting Teleport to Caverns of Time (46343). Gameobject
-- 920002, tranche 92 of the WoW-mods registry.
DROP TEMPORARY TABLE IF EXISTS `guild_level_portal_clone`;
CREATE TEMPORARY TABLE `guild_level_portal_clone` AS
    SELECT * FROM `gameobject_template` WHERE `entry` = 191164;
UPDATE `guild_level_portal_clone` SET `entry` = 920002, `name` = 'Portal to the Caverns of Time', `Data0` = 46343,
    `ScriptName` = '', `AIName` = '';
DELETE FROM `gameobject_template` WHERE `entry` = 920002;
INSERT INTO `gameobject_template` SELECT * FROM `guild_level_portal_clone`;
DROP TEMPORARY TABLE IF EXISTS `guild_level_portal_clone`;

DELETE FROM `gameobject_template_locale` WHERE `entry` = 920002;
INSERT INTO `gameobject_template_locale` (`entry`, `locale`, `name`, `castBarCaption`, `VerifiedBuild`) VALUES
(920002, 'frFR', 'Portail vers les Grottes du temps', '', 0),
(920002, 'deDE', 'Portal zu den Höhlen der Zeit', '', 0),
(920002, 'esES', 'Portal a las Cavernas del Tiempo', '', 0),
(920002, 'esMX', 'Portal a las Cavernas del Tiempo', '', 0),
(920002, 'ruRU', 'Портал в Пещеры Времени', '', 0);

DELETE FROM `mod_guild_level_house_spawn` WHERE `id` BETWEEN 3 AND 999;
INSERT INTO `mod_guild_level_house_spawn` (`id`, `map`, `paragon`, `kind`, `entry`, `x`, `y`, `z`, `o`, `comment`) VALUES
(3, 449, 2, 0, 92001, -6.80, 7.45, -0.26, 0.099, 'Paragon 2: Guild Supplier'),
(4, 449, 3, 0, 2455, -6.50, 5.50, -0.05, 0.077, 'Paragon 3: Banker'),
(6, 449, 5, 0, 914, -5.74, 46.84, 1.06, 0.085, 'Paragon 5: Warrior trainer'),
(7, 449, 5, 0, 928, -5.83, 43.37, 1.06, 0.085, 'Paragon 5: Paladin trainer'),
(8, 449, 5, 0, 5515, -3.58, 52.86, 1.06, 5.106, 'Paragon 5: Hunter trainer'),
(9, 449, 5, 0, 918, 17.47, 41.27, 1.06, 1.714, 'Paragon 5: Rogue trainer'),
(10, 449, 5, 0, 376, 21.50, 41.50, 1.35, 2.676, 'Paragon 5: Priest trainer'),
(11, 449, 5, 0, 28474, 21.50, 47.50, 1.65, 3.153, 'Paragon 5: Death Knight trainer'),
(12, 449, 5, 0, 20407, 20.50, 53.50, 1.65, 3.663, 'Paragon 5: Shaman trainer'),
(13, 449, 5, 0, 331, 16.93, 54.51, 1.06, 4.524, 'Paragon 5: Mage trainer'),
(14, 449, 5, 0, 461, 4.52, 40.93, 1.06, 1.743, 'Paragon 5: Warlock trainer'),
(15, 449, 5, 0, 5504, 1.47, 53.61, 1.06, 4.715, 'Paragon 5: Druid trainer'),
(16, 449, 5, 0, 9977, -4.69, 52.39, 1.06, 5.683, 'Paragon 5: Stable Master'),
(17, 449, 6, 0, 28703, 29.15, 21.17, -7.75, 1.716, 'Paragon 6: Alchemy trainer'),
(18, 449, 6, 0, 28694, 53.32, 50.36, 1.19, 3.181, 'Paragon 6: Blacksmithing trainer'),
(19, 449, 6, 0, 28693, 23.57, 31.11, -7.75, 4.722, 'Paragon 6: Enchanting trainer'),
(20, 449, 6, 0, 28697, 46.51, 33.46, -13.24, 1.627, 'Paragon 6: Engineering trainer'),
(21, 449, 6, 0, 28704, 39.92, 21.47, -6.08, 1.669, 'Paragon 6: Herbalism trainer'),
(22, 449, 6, 0, 28702, 54.33, 50.55, -5.25, 3.166, 'Paragon 6: Inscription trainer'),
(23, 449, 6, 0, 28701, 43.08, 50.05, -5.25, 6.257, 'Paragon 6: Jewelcrafting trainer'),
(24, 449, 6, 0, 28700, 42.23, 50.25, 1.19, 0.021, 'Paragon 6: Leatherworking trainer'),
(25, 449, 6, 0, 28698, 47.32, 21.79, -6.08, 1.576, 'Paragon 6: Mining trainer'),
(26, 449, 6, 0, 28696, 44.14, 21.62, -6.08, 1.719, 'Paragon 6: Skinning trainer'),
(27, 449, 6, 0, 28699, 17.42, 25.74, -7.75, 0.117, 'Paragon 6: Tailoring trainer'),
(28, 449, 6, 1, 1685, 52.92, 53.39, 1.18, 3.397, 'Paragon 6: Forge'),
(29, 449, 6, 1, 1744, 51.22, 55.04, 1.19, 4.373, 'Paragon 6: Anvil'),
(30, 449, 6, 0, 28705, 34.31, 42.30, -6.08, 5.541, 'Paragon 6: Cooking trainer'),
(31, 449, 6, 0, 28706, 49.59, 34.50, -6.08, 3.072, 'Paragon 6: First Aid trainer'),
(32, 449, 6, 0, 28742, 38.53, 43.05, -6.08, 3.895, 'Paragon 6: Fishing trainer'),
(33, 449, 6, 1, 1798, 36.08, 40.79, -6.08, 5.562, 'Paragon 6: Cooking fire'),
(34, 449, 7, 0, 8670, 0.63, 32.30, 1.09, 4.756, 'Paragon 7: Auctioneer'),
(35, 449, 7, 0, 8719, -1.32, 32.32, 1.10, 4.666, 'Paragon 7: Auctioneer'),
(36, 449, 8, 1, 176296, -14.90, 20.43, 1.06, 3.207, 'Paragon 8: Portal to Stormwind'),
(37, 449, 8, 1, 176497, -33.42, 20.11, 1.06, 0.096, 'Paragon 8: Portal to Ironforge'),
(38, 449, 8, 1, 176498, -34.85, 7.40, 2.35, 6.251, 'Paragon 8: Portal to Darnassus'),
(39, 449, 8, 1, 182351, -14.85, 7.16, 2.35, 3.136, 'Paragon 8: Portal to Exodar'),
(40, 449, 8, 1, 183384, -20.30, 1.18, 2.35, 1.655, 'Paragon 8: Portal to Shattrath'),
(41, 449, 8, 1, 191164, -24.32, 10.90, 2.35, 1.592, 'Paragon 8: Portal to Dalaran'),
(42, 449, 8, 1, 920002, -28.68, 0.53, 2.35, 1.494, 'Paragon 8: Portal to Caverns of Time'),
(43, 450, 2, 0, 92001, 236.50, 75.50, 25.45, 0.983, 'Paragon 2: Guild Supplier'),
(44, 450, 3, 0, 3309, 231.50, 86.50, 25.65, 6.242, 'Paragon 3: Banker'),
(46, 450, 5, 0, 3353, 235.50, 137.50, 26.15, 0.608, 'Paragon 5: Warrior trainer'),
(47, 450, 5, 0, 23128, 240.50, 120.50, 26.15, 1.656, 'Paragon 5: Paladin trainer'),
(48, 450, 5, 0, 3406, 244.50, 123.50, 26.15, 1.904, 'Paragon 5: Hunter trainer'),
(49, 450, 5, 0, 3327, 247.50, 137.50, 26.15, 2.879, 'Paragon 5: Rogue trainer'),
(50, 450, 5, 0, 5994, 258.50, 142.50, 26.15, 3.277, 'Paragon 5: Priest trainer'),
(51, 450, 5, 0, 28474, 251.50, 143.50, 26.15, 3.424, 'Paragon 5: Death Knight trainer'),
(52, 450, 5, 0, 3344, 243.50, 150.50, 25.75, 4.301, 'Paragon 5: Shaman trainer'),
(53, 450, 5, 0, 5882, 236.50, 146.50, 26.15, 5.051, 'Paragon 5: Mage trainer'),
(54, 450, 5, 0, 3324, 227.50, 153.50, 26.15, 5.405, 'Paragon 5: Warlock trainer'),
(55, 450, 5, 0, 3033, 221.50, 150.50, 26.15, 5.732, 'Paragon 5: Druid trainer'),
(56, 450, 5, 0, 10054, 228.50, 143.50, 26.15, 5.942, 'Paragon 5: Stable Master'),
(57, 450, 6, 0, 28703, 277.50, 81.50, 26.25, 0.498, 'Paragon 6: Alchemy trainer'),
(58, 450, 6, 0, 28694, 278.50, 78.50, 26.15, 0.819, 'Paragon 6: Blacksmithing trainer'),
(59, 450, 6, 0, 28693, 282.50, 75.50, 26.15, 1.309, 'Paragon 6: Enchanting trainer'),
(60, 450, 6, 0, 28697, 287.50, 77.50, 26.25, 1.840, 'Paragon 6: Engineering trainer'),
(61, 450, 6, 0, 28704, 292.50, 76.50, 25.95, 2.239, 'Paragon 6: Herbalism trainer'),
(62, 450, 6, 0, 28702, 288.50, 83.50, 26.25, 2.547, 'Paragon 6: Inscription trainer'),
(63, 450, 6, 0, 28701, 294.50, 81.50, 26.15, 2.715, 'Paragon 6: Jewelcrafting trainer'),
(64, 450, 6, 0, 28700, 290.50, 89.50, 26.65, 3.767, 'Paragon 6: Leatherworking trainer'),
(65, 450, 6, 0, 28698, 287.50, 88.50, 26.65, 4.032, 'Paragon 6: Mining trainer'),
(66, 450, 6, 0, 28696, 287.50, 95.50, 26.65, 4.486, 'Paragon 6: Skinning trainer'),
(67, 450, 6, 0, 28699, 284.50, 88.50, 26.65, 4.971, 'Paragon 6: Tailoring trainer'),
(68, 450, 6, 0, 29631, 264.50, 110.50, 25.55, 0.632, 'Paragon 6: Cooking trainer'),
(69, 450, 6, 0, 28706, 276.50, 104.50, 25.55, 1.744, 'Paragon 6: First Aid trainer'),
(70, 450, 6, 0, 28742, 289.50, 110.50, 25.55, 2.707, 'Paragon 6: Fishing trainer'),
(71, 450, 6, 1, 1685, 278.50, 90.50, 26.65, 5.665, 'Paragon 6: Forge'),
(72, 450, 6, 1, 1744, 279.50, 87.50, 26.65, 5.980, 'Paragon 6: Anvil'),
(73, 450, 6, 1, 1798, 283.50, 123.50, 25.75, 3.707, 'Paragon 6: Cooking fire'),
(74, 450, 7, 0, 8673, 270.50, 133.50, 25.75, 4.941, 'Paragon 7: Auctioneer'),
(75, 450, 7, 0, 8724, 261.50, 124.50, 25.95, 5.786, 'Paragon 7: Auctioneer'),
(76, 450, 8, 1, 176499, 203.50, 113.50, -2.85, 0.222, 'Paragon 8: Portal to Orgrimmar'),
(77, 450, 8, 1, 176500, 205.50, 105.50, -2.55, 0.969, 'Paragon 8: Portal to Thunder Bluff'),
(78, 450, 8, 1, 176501, 213.50, 106.50, -2.95, 1.696, 'Paragon 8: Portal to Undercity'),
(79, 450, 8, 1, 182352, 219.50, 112.50, -2.65, 2.743, 'Paragon 8: Portal to Silvermoon'),
(80, 450, 8, 1, 184594, 224.50, 119.50, -2.65, 3.460, 'Paragon 8: Portal to Shattrath'),
(81, 450, 8, 1, 191164, 216.50, 124.50, -2.95, 4.282, 'Paragon 8: Portal to Dalaran'),
(82, 450, 8, 1, 920002, 206.50, 123.50, -2.95, 5.345, 'Paragon 8: Portal to Caverns of Time');

DELETE FROM `command` WHERE `name` = 'guildlevel house' OR `name` LIKE 'guildlevel house %';
INSERT INTO `command` (`name`, `security`, `help`) VALUES
('guildlevel house', 2, 'Syntax: .guildlevel house $subcommand

The layout of the guild houses, typed from inside a hall: what stands in them, Paragon level by Paragon level. Every open house of the hall follows at once.'),
('guildlevel house list', 2, 'Syntax: .guildlevel house list

Every spawn of the hall you stand in: id, Paragon level, creature or object, entry, position.'),
('guildlevel house move', 2, 'Syntax: .guildlevel house move $id

Moves that spawn where you stand, facing your way.'),
('guildlevel house add', 2, 'Syntax: .guildlevel house add $paragon creature|object $entry

Adds a spawn where you stand, unlocked at that Paragon level. Its id starts at 1000.'),
('guildlevel house del', 2, 'Syntax: .guildlevel house del $id

Removes that spawn from every house.');
