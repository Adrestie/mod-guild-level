-- mod-guild-level — the Customization objects of each guild (src/GuildDecor.h):
-- how many of each object it owns, and the ones placed in its house. Deleted
-- with the guild, kept when the module is updated.
--
-- Idempotent: the file may be replayed, it creates nothing that already exists.

CREATE TABLE IF NOT EXISTS `mod_guild_level_decor_stock` (
  `guildid` INT UNSIGNED NOT NULL,
  `decor` INT UNSIGNED NOT NULL COMMENT 'mod_guild_level_decor (world)',
  `owned` INT UNSIGNED NOT NULL DEFAULT 0,
  PRIMARY KEY (`guildid`, `decor`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci
  COMMENT='GuildLevel: the Customization objects each guild owns';

CREATE TABLE IF NOT EXISTS `mod_guild_level_decor_placed` (
  `id` INT UNSIGNED NOT NULL,
  `guildid` INT UNSIGNED NOT NULL,
  `decor` INT UNSIGNED NOT NULL COMMENT 'mod_guild_level_decor (world)',
  `map` SMALLINT UNSIGNED NOT NULL COMMENT '449 Alliance, 450 Horde',
  `x` FLOAT NOT NULL,
  `y` FLOAT NOT NULL,
  `z` FLOAT NOT NULL,
  `qx` FLOAT NOT NULL DEFAULT 0,
  `qy` FLOAT NOT NULL DEFAULT 0,
  `qz` FLOAT NOT NULL DEFAULT 0,
  `qw` FLOAT NOT NULL DEFAULT 1,
  `scale` FLOAT NOT NULL DEFAULT 1,
  PRIMARY KEY (`id`),
  KEY `guildid` (`guildid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci
  COMMENT='GuildLevel: the Customization objects placed in each guild house';
