-- mod-guild-level — the help of the commands (.help guildlevel).
--
-- Idempotent: the file may be replayed.

DELETE FROM `command` WHERE `name` = 'guildlevel' OR `name` LIKE 'guildlevel %';
INSERT INTO `command` (`name`, `security`, `help`) VALUES
('guildlevel', 2, 'Syntax: .guildlevel $subcommand\n\nThe level of a guild. Each subcommand ends with an optional guild: its name (spaces allowed) or its id. Without it, the guild of the selected player, else your own.'),
('guildlevel info', 2, 'Syntax: .guildlevel info [$guild]\n\nLevel, Paragon level and experience of the guild.'),
('guildlevel setlevel', 2, 'Syntax: .guildlevel setlevel $level [$guild]\n\nSets the guild to level $level (0 to 25), Paragon 0, with an empty experience bar. Not announced.'),
('guildlevel setparagon', 2, 'Syntax: .guildlevel setparagon $paragon [$guild]\n\nSets the guild to level 25 and Paragon level $paragon (0 to 9), with an empty experience bar. Not announced.'),
('guildlevel setxp', 2, 'Syntax: .guildlevel setxp $xp [$guild]\n\nSets the experience toward the next level, kept below what that level needs.'),
('guildlevel addxp', 2, 'Syntax: .guildlevel addxp $xp [$guild]\n\nGives the guild $xp experience as if it had earned it: the levels it climbs are announced to its members.');
