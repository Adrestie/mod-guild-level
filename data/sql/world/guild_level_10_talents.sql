-- mod-guild-level — the guild talents (.gtalent).
--
-- THE TREE: drawn in game by an administrator, in the talent window's edit
-- mode, which saves every change here at once. These tables are created once
-- and never emptied by this file: replaying it, or updating the module, keeps
-- the tree.
--
--   `effect`  what a point gives (GuildTalentEffect, src/GuildTalents.h):
--             1-5 Stamina, Strength, Agility, Intellect, Spirit;
--             10 crit, 11 haste, 12 hit, 13 expertise, 14 armor penetration
--             (ratings), 15 attack power, 16 spell power, 17 mana per 5 sec.,
--             18 defense, 19 dodge, 20 parry, 21 block, 22 resilience
--             (ratings), 23 armor, 24 spell penetration, 25 resistance to all
--             schools, 26 health, 27 mana, 28 healing power, 29 spell damage;
--             40 % damage done, 41 % healing done, 42 % less damage taken,
--             43 % maximum health, 44 % run speed, 45 % experience, 46 % honor,
--             47 % reputation, 48 % mounted speed, 49 % off vendors and
--             repairs, 50 % of looted gold to the guild bank, 51 % chance of a
--             lucky gathering, 52 minutes on flasks, elixirs and food,
--             53 minutes off the Hearthstone, 54 % critical damage;
--             60 the origin: no effect, always held, the tree starts from it;
--             with a chance and an internal cooldown: 61 a hit ignores % of
--             armor (3 s), 62 a hit gives % damage (10 s), 63 a melee hit
--             gives extra attacks, 64 a block gives a shield of % of maximum
--             health, 65 a parry strikes back for % of maximum health, 66 a
--             dodge gives % less damage taken (5 s, up to 5 times), 67 a spell
--             makes the next one free, 68 instant, 69 a hit or a spell gives
--             rage, energy or runic power (a fifth of it as % of mana).
--   `amount`  per point; `max_points` how many points the node takes.
--   `chance`  % (effects 61-69), `cooldown` seconds between two procs.
--   `tier`    1 to 3: the points the guild must have spent first
--             (GuildLevel.Talents.Tier2.Points, Tier3.Points).
--   `start`   1: bought without a neighbour.
--   `icon`    a texture path; empty, the effect's own.
--
-- Links are undirected: one row per pair, node_a < node_b.

CREATE TABLE IF NOT EXISTS `mod_guild_level_talent_node` (
  `id` INT UNSIGNED NOT NULL,
  `x` INT NOT NULL DEFAULT 0 COMMENT 'grid column',
  `y` INT NOT NULL DEFAULT 0 COMMENT 'grid row, downward',
  `effect` TINYINT UNSIGNED NOT NULL,
  `amount` FLOAT NOT NULL DEFAULT 0 COMMENT 'per point',
  `max_points` TINYINT UNSIGNED NOT NULL DEFAULT 1,
  `chance` FLOAT NOT NULL DEFAULT 0 COMMENT '%, the effects that proc',
  `cooldown` INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'seconds between two procs',
  `tier` TINYINT UNSIGNED NOT NULL DEFAULT 1,
  `start` TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `icon` VARCHAR(255) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci
  COMMENT='GuildLevel: the nodes of the guild talent tree';

CREATE TABLE IF NOT EXISTS `mod_guild_level_talent_link` (
  `node_a` INT UNSIGNED NOT NULL,
  `node_b` INT UNSIGNED NOT NULL,
  PRIMARY KEY (`node_a`, `node_b`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci
  COMMENT='GuildLevel: the links of the guild talent tree';

-- THE COMMANDS' HELP.
DELETE FROM `command` WHERE `name` = 'gtalent' OR `name` LIKE 'gtalent %';
INSERT INTO `command` (`name`, `security`, `help`) VALUES
('gtalent', 0, 'Syntax: .gtalent\n\nThe talent points of your guild and what it bought. The talent window (the button of the guild window) does the same and more.'),
('gtalent buy', 0, 'Syntax: .gtalent buy $node\n\nPuts a point of your guild into the node: the guild master and the officers only.'),
('gtalent reset', 0, 'Syntax: .gtalent reset [confirm]\n\nThe price of a reset; with confirm, gives back every point of your guild, paid from the guild bank. The guild master and the officers only.'),
('gtalent node add', 3, 'Syntax: .gtalent node add $x $y $effect $amount $maxpoints [$chance $cooldown $tier]\n\nA new node of the tree (effects: see data/sql/world/guild_level_10_talents.sql).'),
('gtalent node set', 3, 'Syntax: .gtalent node set $id $effect $amount $maxpoints [$chance $cooldown $tier]\n\nChanges what a node gives.'),
('gtalent node move', 3, 'Syntax: .gtalent node move $id $x $y'),
('gtalent node start', 3, 'Syntax: .gtalent node start $id 0|1\n\nWhether the node can be bought without a neighbour.'),
('gtalent node icon', 3, 'Syntax: .gtalent node icon $id [$texture]\n\nThe node''s icon; none, the effect''s own.'),
('gtalent node delete', 3, 'Syntax: .gtalent node delete $id\n\nRemoves the node and its links; the guilds get its points back.'),
('gtalent link', 3, 'Syntax: .gtalent link $node $node\n\nLinks two nodes, or unlinks them.'),
('gtalent reload', 3, 'Syntax: .gtalent reload\n\nReads the tree again from the world database.'),
('gtalent clear', 2, 'Syntax: .gtalent clear [$guild]\n\nGives a guild back every talent point, for free.');

-- THE TEXTS.
DELETE FROM `module_string` WHERE `module` = 'mod-guild-level' AND `id` BETWEEN 150 AND 166;
INSERT INTO `module_string` (`module`, `id`, `string`) VALUES
('mod-guild-level', 150, 'The guild talents are disabled.'),
('mod-guild-level', 151, 'Only the guild master and the officers (ranks up to {}) spend the guild''s talent points.'),
('mod-guild-level', 152, 'There is no such talent.'),
('mod-guild-level', 153, 'This talent holds all the points it can.'),
('mod-guild-level', 154, 'This talent must be linked to a talent that holds a point.'),
('mod-guild-level', 155, 'Your guild has no talent point left.'),
('mod-guild-level', 156, 'Talent {}: {}/{}.'),
('mod-guild-level', 157, 'Resetting the guild''s talents costs {} gold from the guild bank. Type .gtalent reset confirm to do it.'),
('mod-guild-level', 158, 'The guild''s talents are reset, for {} gold.'),
('mod-guild-level', 159, 'The guild bank does not hold the {} gold of a reset.'),
('mod-guild-level', 160, 'Your guild has not spent any talent point.'),
('mod-guild-level', 161, 'Guild talents: {} points earned, {} spent, {} left.'),
('mod-guild-level', 162, '  talent {}: {}/{}, +{} {}'),
('mod-guild-level', 163, 'A reset costs {} gold.'),
('mod-guild-level', 164, 'Talent {} saved.'),
('mod-guild-level', 165, 'Nothing changed: unknown talent or values that do not hold.'),
('mod-guild-level', 166, 'Your guild has not spent enough points yet for this tier of talents.');

-- frFR only: the other languages are guild_level_18's, which a re-run of this file must not wipe.
DELETE FROM `module_string_locale` WHERE `module` = 'mod-guild-level' AND `id` BETWEEN 150 AND 166 AND `locale` = 'frFR';
INSERT INTO `module_string_locale` (`module`, `id`, `locale`, `string`) VALUES
('mod-guild-level', 150, 'frFR', 'Les talents de guilde sont désactivés.'),
('mod-guild-level', 151, 'frFR', 'Seuls le maître de guilde et les officiers (rangs jusqu''à {}) dépensent les points de talent de la guilde.'),
('mod-guild-level', 152, 'frFR', 'Ce talent n''existe pas.'),
('mod-guild-level', 153, 'frFR', 'Ce talent a déjà tous ses points.'),
('mod-guild-level', 154, 'frFR', 'Ce talent doit être relié à un talent qui a au moins un point.'),
('mod-guild-level', 155, 'frFR', 'Votre guilde n''a plus de point de talent.'),
('mod-guild-level', 156, 'frFR', 'Talent {} : {}/{}.'),
('mod-guild-level', 157, 'frFR', 'Réinitialiser les talents de la guilde coûte {} po, prises dans la banque de guilde. Tapez .gtalent reset confirm pour le faire.'),
('mod-guild-level', 158, 'frFR', 'Les talents de la guilde sont réinitialisés, pour {} po.'),
('mod-guild-level', 159, 'frFR', 'La banque de guilde n''a pas les {} po d''une réinitialisation.'),
('mod-guild-level', 160, 'frFR', 'Votre guilde n''a dépensé aucun point de talent.'),
('mod-guild-level', 161, 'frFR', 'Talents de guilde : {} points gagnés, {} dépensés, {} restants.'),
('mod-guild-level', 162, 'frFR', '  talent {} : {}/{}, +{} {}'),
('mod-guild-level', 163, 'frFR', 'Une réinitialisation coûte {} po.'),
('mod-guild-level', 164, 'frFR', 'Talent {} enregistré.'),
('mod-guild-level', 165, 'frFR', 'Rien de changé : talent inconnu ou valeurs incorrectes.'),
('mod-guild-level', 166, 'frFR', 'Votre guilde n''a pas encore dépensé assez de points pour ce palier de talents.');
