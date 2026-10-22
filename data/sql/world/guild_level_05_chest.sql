-- mod-guild-level — the Guild Chest summoned by `.gperk bank`.
--
-- Cataclysm's Mobile Banking chest, converted for 3.3.5a by
-- tools/convert_guild_chest.py: the client files are under
-- data/art/World/GuildLevel/GuildChest, the client's display rows in
-- data/dbc/guild_level_GameObjectDisplayInfo.dbc. This file gives the server
-- the same display rows, and the chests themselves.
--
--   display 92010 / gameobject 920000   the Alliance chest (blue banner)
--   display 92011 / gameobject 920001   the Horde chest (red banner)
--
-- Tranche 92 of the WoW-mods registry (gameobjects at tranche x 10).
-- Idempotent: every row is deleted before being written again.

DELETE FROM `gameobjectdisplayinfo_dbc` WHERE `ID` IN (92010, 92011);
INSERT INTO `gameobjectdisplayinfo_dbc` (`ID`, `ModelName`, `GeoBoxMinX`, `GeoBoxMinY`, `GeoBoxMinZ`, `GeoBoxMaxX`, `GeoBoxMaxY`, `GeoBoxMaxZ`) VALUES
(92010, 'World\\GuildLevel\\GuildChest\\GuildChest_Alliance01.mdx', -0.76182, -1.23147, -0.00131, 1.87450, 1.23150, 1.92765),
(92011, 'World\\GuildLevel\\GuildChest\\GuildChest_Horde01.mdx', -0.77, -1.32, -0.01, 2.06, 1.32, 1.96);

-- The guild bank type (34) of the game's Guild Vault (187294), with the chest
-- as its look.
DROP TEMPORARY TABLE IF EXISTS `guild_level_chest_clone`;
CREATE TEMPORARY TABLE `guild_level_chest_clone` AS
    SELECT * FROM `gameobject_template` WHERE `entry` = 187294;
DELETE FROM `gameobject_template` WHERE `entry` IN (920000, 920001);
UPDATE `guild_level_chest_clone` SET `entry` = 920000, `name` = 'Guild Chest', `displayId` = 92010, `ScriptName` = '', `AIName` = '';
INSERT INTO `gameobject_template` SELECT * FROM `guild_level_chest_clone`;
UPDATE `guild_level_chest_clone` SET `entry` = 920001, `displayId` = 92011;
INSERT INTO `gameobject_template` SELECT * FROM `guild_level_chest_clone`;
DROP TEMPORARY TABLE IF EXISTS `guild_level_chest_clone`;

DELETE FROM `gameobject_template_locale` WHERE `entry` IN (920000, 920001);
INSERT INTO `gameobject_template_locale` (`entry`, `locale`, `name`, `castBarCaption`, `VerifiedBuild`) VALUES
(920000, 'frFR', 'Coffre de guilde', '', 0), (920001, 'frFR', 'Coffre de guilde', '', 0),
(920000, 'deDE', 'Gildentruhe', '', 0),      (920001, 'deDE', 'Gildentruhe', '', 0),
(920000, 'esES', 'Cofre de hermandad', '', 0), (920001, 'esES', 'Cofre de hermandad', '', 0),
(920000, 'ruRU', 'Сундук гильдии', '', 0),    (920001, 'ruRU', 'Сундук гильдии', '', 0);
