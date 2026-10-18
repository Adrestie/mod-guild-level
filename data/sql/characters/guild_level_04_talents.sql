-- mod-guild-level — the guild talents: the points each guild put in each node
-- of the tree (mod_guild_level_talent_node, world database), and the price of
-- its next reset. Deleted with the guild, kept when the module is updated.
--
-- `next_price`: the gold the next reset costs, as of `last_reset` (Unix time);
-- each full week since takes GuildLevel.Talents.Reset.Decrease % of the base
-- cost off, down to the base cost. No row: the base cost.
--
-- Idempotent: the file may be replayed, it creates nothing that already exists.

CREATE TABLE IF NOT EXISTS `mod_guild_level_talent` (
  `guildid` INT UNSIGNED NOT NULL COMMENT 'guild.guildid',
  `node` INT UNSIGNED NOT NULL COMMENT 'mod_guild_level_talent_node.id (world)',
  `points` TINYINT UNSIGNED NOT NULL DEFAULT 0,
  PRIMARY KEY (`guildid`, `node`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci
  COMMENT='GuildLevel: points of each guild in its talent tree';

CREATE TABLE IF NOT EXISTS `mod_guild_level_talent_reset` (
  `guildid` INT UNSIGNED NOT NULL COMMENT 'guild.guildid',
  `next_price` INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'gold, as of last_reset',
  `last_reset` INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'Unix time',
  PRIMARY KEY (`guildid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci
  COMMENT='GuildLevel: price of the next talent reset of each guild';
