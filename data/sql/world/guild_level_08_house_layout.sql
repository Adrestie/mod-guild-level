-- mod-guild-level — the layout of the guild houses (Paragon 2 to 8).
--
-- Proposed from the walkable floor of each hall (the server's mmaps), every
-- spot facing the centre of the room. The Alliance hall gives each wing its
-- trade: the class trainers in the town hall, the crafts with the forge and
-- the anvil in the second guild house, cooking, first aid, fishing and the
-- auctioneers in the inn, the portals upstairs in the first guild house; the
-- barracks, the largest wing, stays empty. To adjust in game with
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
(3, 449, 2, 0, 92001, 3.50, 3.50, 0.55, 2.521, 'Paragon 2: Guild Supplier'),
(4, 449, 3, 0, 2455, -6.50, 5.50, -0.05, 0.077, 'Paragon 3: Banker'),
(6, 449, 5, 0, 914, 0.50, 46.50, 1.35, 0.093, 'Paragon 5: Warrior trainer'),
(7, 449, 5, 0, 928, -4.50, 41.50, 1.55, 0.389, 'Paragon 5: Paladin trainer'),
(8, 449, 5, 0, 5515, 10.50, 41.50, 1.35, 1.687, 'Paragon 5: Hunter trainer'),
(9, 449, 5, 0, 918, 16.50, 41.50, 1.65, 2.421, 'Paragon 5: Rogue trainer'),
(10, 449, 5, 0, 376, 21.50, 41.50, 1.35, 2.676, 'Paragon 5: Priest trainer'),
(11, 449, 5, 0, 28474, 21.50, 47.50, 1.65, 3.153, 'Paragon 5: Death Knight trainer'),
(12, 449, 5, 0, 20407, 20.50, 53.50, 1.65, 3.663, 'Paragon 5: Shaman trainer'),
(13, 449, 5, 0, 331, 14.50, 52.50, 1.35, 3.973, 'Paragon 5: Mage trainer'),
(14, 449, 5, 0, 461, 9.50, 53.50, 1.35, 4.764, 'Paragon 5: Warlock trainer'),
(15, 449, 5, 0, 5504, 3.50, 53.50, 1.35, 5.512, 'Paragon 5: Druid trainer'),
(16, 449, 5, 0, 9977, -1.50, 52.50, 1.75, 5.857, 'Paragon 5: Stable Master'),
(17, 449, 6, 0, 28703, 16.50, 30.50, 1.35, 0.278, 'Paragon 6: Alchemy trainer'),
(18, 449, 6, 0, 28694, 27.50, 31.50, 1.85, 0.520, 'Paragon 6: Blacksmithing trainer'),
(19, 449, 6, 0, 28693, 18.50, 22.50, 1.35, 0.678, 'Paragon 6: Enchanting trainer'),
(20, 449, 6, 0, 28697, 25.50, 23.50, 1.85, 0.913, 'Paragon 6: Engineering trainer'),
(21, 449, 6, 0, 28704, 30.50, 21.50, 1.85, 1.267, 'Paragon 6: Herbalism trainer'),
(22, 449, 6, 0, 28702, 33.50, 23.50, 1.85, 1.451, 'Paragon 6: Inscription trainer'),
(23, 449, 6, 0, 28701, 37.50, 32.50, 2.45, 2.225, 'Paragon 6: Jewelcrafting trainer'),
(24, 449, 6, 0, 28700, 42.50, 36.50, 2.15, 3.237, 'Paragon 6: Leatherworking trainer'),
(25, 449, 6, 0, 28698, 52.50, 44.50, 2.45, 3.603, 'Paragon 6: Mining trainer'),
(26, 449, 6, 0, 28696, 44.50, 41.50, 2.55, 3.682, 'Paragon 6: Skinning trainer'),
(27, 449, 6, 0, 28699, 46.50, 44.50, 1.65, 3.789, 'Paragon 6: Tailoring trainer'),
(28, 449, 6, 1, 1685, 51.50, 54.50, 1.35, 3.989, 'Paragon 6: Forge'),
(29, 449, 6, 1, 1744, 43.50, 49.50, 1.65, 4.157, 'Paragon 6: Anvil'),
(30, 449, 6, 0, 28705, 20.50, 1.50, 0.05, 0.188, 'Paragon 6: Cooking trainer'),
(31, 449, 6, 0, 28706, 21.50, -7.50, 0.55, 0.865, 'Paragon 6: First Aid trainer'),
(32, 449, 6, 0, 28742, 33.50, -3.50, -0.55, 1.935, 'Paragon 6: Fishing trainer'),
(33, 449, 6, 1, 1798, 45.50, 4.50, 0.05, 3.212, 'Paragon 6: Cooking fire'),
(34, 449, 7, 0, 8670, 39.50, 11.50, 0.45, 3.889, 'Paragon 7: Auctioneer'),
(35, 449, 7, 0, 8719, 23.50, 11.50, 0.35, 5.453, 'Paragon 7: Auctioneer'),
(36, 449, 8, 1, 176296, -20.50, -40.50, 5.65, 0.074, 'Paragon 8: Portal to Stormwind'),
(37, 449, 8, 1, 176497, -16.50, -49.50, 5.65, 0.727, 'Paragon 8: Portal to Ironforge'),
(38, 449, 8, 1, 176498, -5.50, -45.50, 5.65, 1.504, 'Paragon 8: Portal to Darnassus'),
(39, 449, 8, 1, 182351, 4.50, -50.50, 5.75, 2.281, 'Paragon 8: Portal to Exodar'),
(40, 449, 8, 1, 183384, 4.50, -41.50, 6.25, 2.921, 'Paragon 8: Portal to Shattrath'),
(41, 449, 8, 1, 191164, -3.50, -32.50, 5.65, 4.485, 'Paragon 8: Portal to Dalaran'),
(42, 449, 8, 1, 920002, -12.50, -34.50, 5.65, 5.703, 'Paragon 8: Portal to Caverns of Time'),
(43, 450, 2, 0, 92001, 236.50, 75.50, 25.45, 0.983, 'Paragon 2: Guild Supplier'),
(44, 450, 3, 0, 3309, 231.50, 86.50, 25.65, 6.242, 'Paragon 3: Banker'),
(46, 450, 5, 0, 3353, 230.50, 94.50, 26.05, 5.704, 'Paragon 5: Warrior trainer'),
(47, 450, 5, 0, 23128, 232.50, 96.50, 25.75, 5.521, 'Paragon 5: Paladin trainer'),
(48, 450, 5, 0, 3406, 233.50, 98.50, 25.75, 5.387, 'Paragon 5: Hunter trainer'),
(49, 450, 5, 0, 3327, 236.50, 99.50, 26.05, 5.191, 'Paragon 5: Rogue trainer'),
(50, 450, 5, 0, 5994, 238.50, 100.50, 26.05, 5.044, 'Paragon 5: Priest trainer'),
(51, 450, 5, 0, 28474, 240.50, 101.50, 25.85, 4.904, 'Paragon 5: Death Knight trainer'),
(52, 450, 5, 0, 3344, 243.50, 101.50, 25.85, 4.712, 'Paragon 5: Shaman trainer'),
(53, 450, 5, 0, 5882, 245.50, 101.50, 25.85, 4.584, 'Paragon 5: Mage trainer'),
(54, 450, 5, 0, 3324, 248.50, 100.50, 25.85, 4.380, 'Paragon 5: Warlock trainer'),
(55, 450, 5, 0, 3033, 250.50, 99.50, 25.85, 4.234, 'Paragon 5: Druid trainer'),
(56, 450, 5, 0, 10054, 252.50, 98.50, 25.85, 4.088, 'Paragon 5: Stable Master'),
(57, 450, 6, 0, 28703, 243.50, 70.50, 25.85, 1.571, 'Paragon 6: Alchemy trainer'),
(58, 450, 6, 0, 28694, 245.50, 70.50, 25.85, 1.699, 'Paragon 6: Blacksmithing trainer'),
(59, 450, 6, 0, 28693, 247.50, 71.50, 25.85, 1.840, 'Paragon 6: Enchanting trainer'),
(60, 450, 6, 0, 28697, 249.50, 71.50, 26.05, 1.963, 'Paragon 6: Engineering trainer'),
(61, 450, 6, 0, 28704, 251.50, 72.50, 26.05, 2.106, 'Paragon 6: Herbalism trainer'),
(62, 450, 6, 0, 28702, 253.50, 74.50, 26.25, 2.287, 'Paragon 6: Inscription trainer'),
(63, 450, 6, 0, 28701, 255.50, 75.50, 26.25, 2.423, 'Paragon 6: Jewelcrafting trainer'),
(64, 450, 6, 0, 28700, 256.50, 77.50, 25.85, 2.563, 'Paragon 6: Leatherworking trainer'),
(65, 450, 6, 0, 28698, 257.50, 79.50, 25.85, 2.707, 'Paragon 6: Mining trainer'),
(66, 450, 6, 0, 28696, 258.50, 81.50, 25.85, 2.850, 'Paragon 6: Skinning trainer'),
(67, 450, 6, 0, 28699, 258.50, 83.50, 25.85, 2.976, 'Paragon 6: Tailoring trainer'),
(68, 450, 6, 0, 29631, 258.50, 85.50, 25.85, 3.108, 'Paragon 6: Cooking trainer'),
(69, 450, 6, 0, 28706, 258.50, 87.50, 25.85, 3.241, 'Paragon 6: First Aid trainer'),
(70, 450, 6, 0, 28742, 258.50, 89.50, 25.85, 3.371, 'Paragon 6: Fishing trainer'),
(71, 450, 6, 1, 1685, 249.50, 77.50, 25.65, 2.185, 'Paragon 6: Forge'),
(72, 450, 6, 1, 1744, 251.50, 80.50, 25.45, 2.539, 'Paragon 6: Anvil'),
(73, 450, 6, 1, 1798, 253.50, 84.50, 25.35, 2.993, 'Paragon 6: Cooking fire'),
(74, 450, 7, 0, 8673, 256.50, 91.50, 25.85, 3.542, 'Paragon 7: Auctioneer'),
(75, 450, 7, 0, 8724, 255.50, 93.50, 25.85, 3.700, 'Paragon 7: Auctioneer'),
(76, 450, 8, 1, 176499, 238.50, 83.50, 25.45, 3.632, 'Paragon 8: Portal to Orgrimmar'),
(77, 450, 8, 1, 176500, 242.50, 80.50, 25.45, 4.504, 'Paragon 8: Portal to Thunder Bluff'),
(78, 450, 8, 1, 176501, 247.50, 81.50, 25.45, 5.390, 'Paragon 8: Portal to Undercity'),
(79, 450, 8, 1, 182352, 249.50, 86.50, 25.35, 6.258, 'Paragon 8: Portal to Silvermoon'),
(80, 450, 8, 1, 184594, 247.50, 90.50, 25.35, 1.009, 'Paragon 8: Portal to Shattrath'),
(81, 450, 8, 1, 191164, 241.50, 91.50, 25.35, 1.734, 'Paragon 8: Portal to Dalaran'),
(82, 450, 8, 1, 920002, 238.50, 88.50, 25.35, 2.760, 'Paragon 8: Portal to Caverns of Time');

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
