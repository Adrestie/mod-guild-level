-- mod-guild-level — `.gperk reset`, a GM's reset of the active perks' cooldowns.
--
-- Idempotent: every row is deleted before being written again.

DELETE FROM `module_string` WHERE `module` = 'mod-guild-level' AND `id` = 144;
INSERT INTO `module_string` (`module`, `id`, `string`) VALUES
('mod-guild-level', 144, 'Cooldowns of the active perks reset for {}.');

DELETE FROM `module_string_locale` WHERE `module` = 'mod-guild-level' AND `id` = 144;
INSERT INTO `module_string_locale` (`module`, `id`, `locale`, `string`) VALUES
('mod-guild-level', 144, 'frFR', 'Délais des bonus actifs réinitialisés pour {}.'),
('mod-guild-level', 144, 'deDE', 'Abklingzeiten der aktiven Boni für {} zurückgesetzt.'),
('mod-guild-level', 144, 'esES', 'Tiempos de reutilización de las ventajas activas reiniciados para {}.'),
('mod-guild-level', 144, 'ruRU', 'Время восстановления активных бонусов сброшено для {}.');

DELETE FROM `command` WHERE `name` = 'gperk reset';
INSERT INTO `command` (`name`, `security`, `help`) VALUES
('gperk reset', 2, 'Syntax: .gperk reset [$character]\n\nGM. Every cooldown of the active perks of the character named, else of the selected player, else your own, back to ready. Works on an offline character too.');
