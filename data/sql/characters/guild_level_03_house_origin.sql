-- mod-guild-level — where each character came from before his guild house:
-- the spot he left, or the front of the entrance of the dungeon or raid he was
-- in. The house's spell and the halls' way out send him back there.
-- Deleted with the character, kept when the module is updated.
--
-- Idempotent: the file may be replayed, it creates nothing that already exists.

CREATE TABLE IF NOT EXISTS `mod_guild_level_house_origin` (
  `guid` INT UNSIGNED NOT NULL COMMENT 'characters.guid',
  `map` SMALLINT UNSIGNED NOT NULL,
  `x` FLOAT NOT NULL,
  `y` FLOAT NOT NULL,
  `z` FLOAT NOT NULL,
  `o` FLOAT NOT NULL,
  PRIMARY KEY (`guid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci
  COMMENT='GuildLevel: the way back out of the guild house of each character';
