-- mod-guild-level — characters schema.
--
-- The level of every guild that ever earned guild experience. A guild without
-- a row is level 0. `level` goes from 0 to 25; past 25, `paragon` from 0 to 9.
-- `xp` is the progress toward the next level (or the next Paragon level), and
-- stays 0 at Paragon 9; `total_xp` counts everything the guild ever earned.
--
-- The row goes with its guild when it is disbanded.
--
-- Idempotent: the file may be replayed, it creates nothing that already exists.

CREATE TABLE IF NOT EXISTS `mod_guild_level` (
  `guildid` INT UNSIGNED NOT NULL COMMENT 'guild.guildid',
  `level` TINYINT UNSIGNED NOT NULL DEFAULT 0 COMMENT '0 to 25',
  `paragon` TINYINT UNSIGNED NOT NULL DEFAULT 0 COMMENT '0 to 9, past level 25',
  `xp` BIGINT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'toward the next level or Paragon level',
  `total_xp` BIGINT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'everything ever earned',
  PRIMARY KEY (`guildid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci
  COMMENT='GuildLevel: level, Paragon level and experience of each guild';
