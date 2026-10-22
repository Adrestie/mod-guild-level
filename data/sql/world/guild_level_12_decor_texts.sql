-- mod-guild-level — the texts and commands of the Customization mode (Paragon 7).
--
-- Idempotent: every row is deleted before being written again.

DELETE FROM `command` WHERE `name` = 'ghouse' OR `name` LIKE 'ghouse %';
INSERT INTO `command` (`name`, `security`, `help`) VALUES
('ghouse begin', 0, 'Syntax: .ghouse begin $placed\n\nTakes a placed object of your guild house in hand: no one else sees it or touches it until you place it or cancel. The Customization window does it for you.'),
('ghouse place', 0, 'Syntax: .ghouse place $placed $object $x $y $z $qx $qy $qz $qw $scale\n\nPuts the object in hand back (or, with 0 for $placed, a new one of the guild''s stock) at that position, rotation (a quaternion) and size.'),
('ghouse cancel', 0, 'Syntax: .ghouse cancel $placed\n\nPuts the object in hand back as it was.'),
('ghouse remove', 0, 'Syntax: .ghouse remove $placed\n\nTakes a placed object away, back to the guild''s stock.'),
('ghouse give', 2, 'Syntax: .ghouse give $object $count [$guild]\n\nAdds $count (or takes away, negative) of an object to the stock of a guild (its name or id; else the selected player''s, else yours). 0 for $object: every object.');

DELETE FROM `module_string` WHERE `module` = 'mod-guild-level' AND `id` BETWEEN 170 AND 189;
INSERT INTO `module_string` (`module`, `id`, `string`) VALUES
('mod-guild-level', 170, 'Your guild decorates its house from Paragon level {}.'),
('mod-guild-level', 171, 'Only the guild ranks up to {} may decorate the house (0 is the guild master).'),
('mod-guild-level', 172, 'There is no such object.'),
('mod-guild-level', 173, 'Your guild has none of this object left to place.'),
('mod-guild-level', 174, 'Another member is moving this object.'),
('mod-guild-level', 175, 'There is no such object in your guild house.'),
('mod-guild-level', 176, 'This object cannot be placed there.'),
('mod-guild-level', 177, '{} of object {} (0: every object) given to the guild {}.');

DELETE FROM `module_string_locale` WHERE `module` = 'mod-guild-level' AND `id` BETWEEN 170 AND 189;
INSERT INTO `module_string_locale` (`module`, `id`, `locale`, `string`) VALUES
('mod-guild-level', 170, 'frFR', 'Votre guilde décore sa maison à partir du niveau de Parangon {}.'),
('mod-guild-level', 171, 'frFR', 'Seuls les rangs de guilde jusqu''à {} peuvent décorer la maison (0 est le maître de guilde).'),
('mod-guild-level', 172, 'frFR', 'Cet objet n''existe pas.'),
('mod-guild-level', 173, 'frFR', 'Votre guilde n''a plus de cet objet à placer.'),
('mod-guild-level', 174, 'frFR', 'Un autre membre déplace cet objet.'),
('mod-guild-level', 175, 'frFR', 'Cet objet n''est pas dans votre maison de guilde.'),
('mod-guild-level', 176, 'frFR', 'Cet objet ne peut pas être placé ici.'),
('mod-guild-level', 177, 'frFR', '{} de l''objet {} (0 : tous les objets) donnés à la guilde {}.');
