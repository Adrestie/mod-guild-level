-- mod-guild-level — the cooldowns of the active perks (.gperk), per character.
--
-- `perk`: 0 raid, 1 rez, 2 bank, 3 vendor, 4 home. `ready_at`: the time the
-- perk is ready again, in Unix seconds. A row past its time means nothing.
-- Deleted with the character, kept when the module is updated.
--
-- Idempotent: the file may be replayed, it creates nothing that already exists.

CREATE TABLE IF NOT EXISTS `mod_guild_level_perk_cooldown` (
  `guid` INT UNSIGNED NOT NULL COMMENT 'characters.guid',
  `perk` TINYINT UNSIGNED NOT NULL COMMENT '0 raid, 1 rez, 2 bank, 3 vendor, 4 home',
  `ready_at` INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'Unix time the perk is ready again',
  PRIMARY KEY (`guid`, `perk`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci
  COMMENT='GuildLevel: cooldowns of the active perks of each character';
