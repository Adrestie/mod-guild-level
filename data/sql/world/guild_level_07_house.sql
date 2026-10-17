-- mod-guild-level — the guild house (Paragon 1 and up).
--
-- Champions' Hall (map 449) for the Alliance, Hall of Legends (map 450) for
-- the Horde: one phase of the hall per open guild house (src/GuildHouse.h).
--
-- Idempotent: the table is created once, its rows and the texts are written
-- again.

-- WHAT STANDS IN A HOUSE, unlocked Paragon level by Paragon level. kind 0 is
-- a creature, 1 a gameobject. Each guild's copy is placed in its own phase
-- when its house opens. Rows of the module only: a server may add its own
-- above id 1000.
CREATE TABLE IF NOT EXISTS `mod_guild_level_house_spawn` (
  `id` INT UNSIGNED NOT NULL,
  `map` SMALLINT UNSIGNED NOT NULL COMMENT '449 Alliance, 450 Horde',
  `paragon` TINYINT UNSIGNED NOT NULL DEFAULT 1 COMMENT 'Paragon level that unlocks it',
  `kind` TINYINT UNSIGNED NOT NULL DEFAULT 0 COMMENT '0 creature, 1 gameobject',
  `entry` INT UNSIGNED NOT NULL,
  `x` FLOAT NOT NULL,
  `y` FLOAT NOT NULL,
  `z` FLOAT NOT NULL,
  `o` FLOAT NOT NULL DEFAULT 0,
  `comment` VARCHAR(100) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci
  COMMENT='GuildLevel: creatures and objects of the guild houses';

DELETE FROM `mod_guild_level_house_spawn` WHERE `id` < 1000;
INSERT INTO `mod_guild_level_house_spawn` (`id`, `map`, `paragon`, `kind`, `entry`, `x`, `y`, `z`, `o`, `comment`) VALUES
(1, 449, 1, 1, 32349, -3.5, 4.4, -0.256, 0, 'Paragon 1: mailbox, Alliance'),
(2, 450, 1, 1, 32349, 218.0, 72.5, 25.721, 0.485, 'Paragon 1: mailbox, Horde');

-- THE TEXTS of the house. {} in 145: the Paragon level that opens it.
DELETE FROM `module_string` WHERE `module` = 'mod-guild-level' AND `id` BETWEEN 145 AND 148;
INSERT INTO `module_string` (`module`, `id`, `string`) VALUES
('mod-guild-level', 145, 'Your guild opens its house at Paragon level {}.'),
('mod-guild-level', 146, 'Every guild house of your hall is open right now: try again in a few minutes.'),
('mod-guild-level', 147, 'You are not in a guild house.'),
('mod-guild-level', 148, 'This is not your guild''s house.');

DELETE FROM `module_string_locale` WHERE `module` = 'mod-guild-level' AND `id` BETWEEN 145 AND 148;
INSERT INTO `module_string_locale` (`module`, `id`, `locale`, `string`) VALUES
('mod-guild-level', 145, 'frFR', 'Votre guilde ouvre sa maison au niveau de Parangon {}.'),
('mod-guild-level', 146, 'frFR', 'Toutes les maisons de guilde de votre hall sont ouvertes en ce moment : réessayez dans quelques minutes.'),
('mod-guild-level', 147, 'frFR', 'Vous n''êtes pas dans une maison de guilde.'),
('mod-guild-level', 148, 'frFR', 'Ce n''est pas la maison de votre guilde.'),
('mod-guild-level', 145, 'deDE', 'Eure Gilde eröffnet ihr Haus auf Paragonstufe {}.'),
('mod-guild-level', 146, 'deDE', 'Alle Gildenhäuser Eurer Halle sind gerade geöffnet: Versucht es in ein paar Minuten erneut.'),
('mod-guild-level', 147, 'deDE', 'Ihr seid in keinem Gildenhaus.'),
('mod-guild-level', 148, 'deDE', 'Das ist nicht das Haus Eurer Gilde.'),
('mod-guild-level', 145, 'esES', 'Tu hermandad abre su casa en el nivel de Paragón {}.'),
('mod-guild-level', 146, 'esES', 'Todas las casas de hermandad de tu salón están abiertas ahora mismo: inténtalo de nuevo en unos minutos.'),
('mod-guild-level', 147, 'esES', 'No estás en una casa de hermandad.'),
('mod-guild-level', 148, 'esES', 'Esta no es la casa de tu hermandad.'),
('mod-guild-level', 145, 'esMX', 'Tu hermandad abre su casa en el nivel de Paragón {}.'),
('mod-guild-level', 146, 'esMX', 'Todas las casas de hermandad de tu salón están abiertas ahora mismo: inténtalo de nuevo en unos minutos.'),
('mod-guild-level', 147, 'esMX', 'No estás en una casa de hermandad.'),
('mod-guild-level', 148, 'esMX', 'Esta no es la casa de tu hermandad.'),
('mod-guild-level', 145, 'ruRU', 'Гильдия открывает свой дом на {}-м уровне Парагона.'),
('mod-guild-level', 146, 'ruRU', 'Все дома гильдий вашего зала сейчас открыты: попробуйте снова через несколько минут.'),
('mod-guild-level', 147, 'ruRU', 'Вы не в доме гильдии.'),
('mod-guild-level', 148, 'ruRU', 'Это не дом вашей гильдии.');

DELETE FROM `command` WHERE `name` IN ('gperk home', 'gperk leave');
INSERT INTO `command` (`name`, `security`, `help`) VALUES
('gperk home', 0, 'Syntax: .gperk home\n\nGuild level 25, and a guild house (Paragon 1): into your guild''s house, Champions'' Hall for the Alliance, Hall of Legends for the Horde. Out of combat, not in a battleground or an arena. 1 hour cooldown.'),
('gperk leave', 0, 'Syntax: .gperk leave\n\nOut of the guild house, back where you came from. No cooldown.');
