-- mod-guild-level — the guild house opens at guild level 25 (GuildLevel.House.Paragon = 0).
--
-- The house's mailbox, unlocked at Paragon 1 before, stands from level 25 on. Only the module's two
-- rows: what stands in a house may have been moved in the game, so the layout is not written again.
-- Idempotent.

UPDATE `mod_guild_level_house_spawn` SET `paragon` = 0, `comment` = REPLACE(`comment`, 'Paragon 1:', 'Level 25:')
WHERE `id` IN (1, 2) AND `paragon` = 1;

-- 149: the house is not open yet ({} the guild level).
DELETE FROM `module_string` WHERE `module` = 'mod-guild-level' AND `id` = 149;
INSERT INTO `module_string` (`module`, `id`, `string`) VALUES
('mod-guild-level', 149, 'Your guild opens its house at level {}.');

DELETE FROM `module_string_locale` WHERE `module` = 'mod-guild-level' AND `id` = 149;
INSERT INTO `module_string_locale` (`module`, `id`, `locale`, `string`) VALUES
('mod-guild-level', 149, 'frFR', 'Votre guilde ouvre sa maison au niveau {}.'),
('mod-guild-level', 149, 'deDE', 'Eure Gilde eröffnet ihr Haus auf Stufe {}.'),
('mod-guild-level', 149, 'esES', 'Tu hermandad abre su casa en el nivel {}.'),
('mod-guild-level', 149, 'ruRU', 'Гильдия открывает свой дом на {}-м уровне.');

UPDATE `command` SET `help` = REPLACE(`help`, 'Guild level 25, and a guild house (Paragon 1):', 'Guild level 25:')
WHERE `name` = 'gperk home';
