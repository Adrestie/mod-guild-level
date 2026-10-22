-- mod-guild-level — the active perks of levels 21 to 25 (.gperk).
--
-- Idempotent: every row is deleted before being written again.

-- THE GUILD SUPPLIER, creature 92001 (tranche 92 of the WoW-mods registry),
-- summoned for a while by `.gperk vendor`: a reagent vendor who repairs.
-- Cloned from the World Reagent Vendor (5062), model included; vendor and
-- repair flags (128 | 4096), friendly to all (35), level 80.
DROP TEMPORARY TABLE IF EXISTS `guild_level_supplier_clone`;
CREATE TEMPORARY TABLE `guild_level_supplier_clone` AS
    SELECT * FROM `creature_template` WHERE `entry` = 5062;
UPDATE `guild_level_supplier_clone` SET `entry` = 92001, `name` = 'Guild Supplier',
    `subname` = 'Reagents and Repairs', `IconName` = '', `faction` = 35, `npcflag` = 4224,
    `minlevel` = 80, `maxlevel` = 80, `exp` = 2, `AIName` = '', `ScriptName` = '',
    `lootid` = 0, `pickpocketloot` = 0, `skinloot` = 0, `mingold` = 0, `maxgold` = 0;
DELETE FROM `creature_template` WHERE `entry` = 92001;
INSERT INTO `creature_template` SELECT * FROM `guild_level_supplier_clone`;
DROP TEMPORARY TABLE IF EXISTS `guild_level_supplier_clone`;

DELETE FROM `creature_template_model` WHERE `CreatureID` = 92001;
INSERT INTO `creature_template_model` (`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`, `VerifiedBuild`)
    SELECT 92001, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`, 0
    FROM `creature_template_model` WHERE `CreatureID` = 5062;

DELETE FROM `creature_template_locale` WHERE `entry` = 92001;
INSERT INTO `creature_template_locale` (`entry`, `locale`, `Name`, `Title`, `VerifiedBuild`) VALUES
(92001, 'frFR', 'Fournisseur de la guilde', 'Composants et réparations', 0),
(92001, 'deDE', 'Gildenlieferant', 'Reagenzien und Reparaturen', 0),
(92001, 'esES', 'Proveedor de la hermandad', 'Componentes y reparaciones', 0),
(92001, 'ruRU', 'Поставщик гильдии', 'Реагенты и ремонт', 0);

-- The reagents of every class, unlimited, at their usual prices.
DELETE FROM `npc_vendor` WHERE `entry` = 92001;
INSERT INTO `npc_vendor` (`entry`, `slot`, `item`, `maxcount`, `incrtime`, `ExtendedCost`, `VerifiedBuild`) VALUES
(92001,  0, 17020, 0, 0, 0, 0),   -- Arcane Powder
(92001,  1, 17031, 0, 0, 0, 0),   -- Rune of Teleportation
(92001,  2, 17032, 0, 0, 0, 0),   -- Rune of Portals
(92001,  3, 17056, 0, 0, 0, 0),   -- Light Feather
(92001,  4, 21177, 0, 0, 0, 0),   -- Symbol of Kings
(92001,  5, 17033, 0, 0, 0, 0),   -- Symbol of Divinity
(92001,  6, 17028, 0, 0, 0, 0),   -- Holy Candle
(92001,  7, 17029, 0, 0, 0, 0),   -- Sacred Candle
(92001,  8, 44615, 0, 0, 0, 0),   -- Devout Candle
(92001,  9, 17030, 0, 0, 0, 0),   -- Ankh
(92001, 10,  5565, 0, 0, 0, 0),   -- Infernal Stone
(92001, 11, 16583, 0, 0, 0, 0),   -- Demonic Figurine
(92001, 12, 37201, 0, 0, 0, 0),   -- Corpse Dust
(92001, 13,  5140, 0, 0, 0, 0),   -- Flash Powder
(92001, 14, 17034, 0, 0, 0, 0),   -- Maple Seed
(92001, 15, 17038, 0, 0, 0, 0),   -- Ironwood Seed
(92001, 16, 22148, 0, 0, 0, 0),   -- Wild Quillvine
(92001, 17, 44605, 0, 0, 0, 0),   -- Wild Spineleaf
(92001, 18, 44614, 0, 0, 0, 0);   -- Starleaf Seed

-- THE TEXTS of the active perks. {} placeholders, in order:
--   120 level needed; 121 minutes, seconds left; 124 / 125 members;
--   128 / 129 minutes; 134 name, subcommand, level, state; 137 minutes, seconds.
--   138 to 142: the names of the perks (raid, rez, bank, vendor, home).
DELETE FROM `module_string` WHERE `module` = 'mod-guild-level' AND `id` BETWEEN 120 AND 143;
INSERT INTO `module_string` (`module`, `id`, `string`) VALUES
('mod-guild-level', 120, 'Your guild must be level {} for this perk.'),
('mod-guild-level', 121, 'This perk is not ready: {} min {} s left.'),
('mod-guild-level', 122, 'Not while in combat.'),
('mod-guild-level', 123, 'Not in a battleground or an arena.'),
('mod-guild-level', 124, 'Summon sent to {} member(s) of your group.'),
('mod-guild-level', 125, 'Resurrection offered to {} dead member(s) of your group.'),
('mod-guild-level', 126, 'No member of your group to summon.'),
('mod-guild-level', 127, 'No dead member of your group here.'),
('mod-guild-level', 128, 'A guild vault stands here for {} min.'),
('mod-guild-level', 129, 'A guild supplier waits here for {} min.'),
('mod-guild-level', 130, 'Your guild has no house yet: this perk comes with the guild house.'),
('mod-guild-level', 131, 'You are not in a guild.'),
('mod-guild-level', 132, 'You must be in a group.'),
('mod-guild-level', 133, 'Active perks:'),
('mod-guild-level', 134, '{} (.gperk {}, level {}): {}'),
('mod-guild-level', 135, 'ready'),
('mod-guild-level', 136, 'locked'),
('mod-guild-level', 137, '{} min {} s left'),
('mod-guild-level', 138, 'Summon the group'),
('mod-guild-level', 139, 'Resurrect the group'),
('mod-guild-level', 140, 'Guild vault'),
('mod-guild-level', 141, 'Guild supplier'),
('mod-guild-level', 142, 'Guild house'),
('mod-guild-level', 143, 'Nothing can be summoned here.');

DELETE FROM `module_string_locale` WHERE `module` = 'mod-guild-level' AND `id` BETWEEN 120 AND 143;
INSERT INTO `module_string_locale` (`module`, `id`, `locale`, `string`) VALUES
('mod-guild-level', 120, 'frFR', 'Votre guilde doit être de niveau {} pour ce bonus.'),
('mod-guild-level', 121, 'frFR', 'Ce bonus n''est pas prêt : encore {} min {} s.'),
('mod-guild-level', 122, 'frFR', 'Pas en combat.'),
('mod-guild-level', 123, 'frFR', 'Pas en champ de bataille ni en arène.'),
('mod-guild-level', 124, 'frFR', 'Invocation envoyée à {} membre(s) de votre groupe.'),
('mod-guild-level', 125, 'frFR', 'Résurrection proposée à {} membre(s) mort(s) de votre groupe.'),
('mod-guild-level', 126, 'frFR', 'Aucun membre de votre groupe à invoquer.'),
('mod-guild-level', 127, 'frFR', 'Aucun membre mort de votre groupe ici.'),
('mod-guild-level', 128, 'frFR', 'Un coffre de guilde se tient ici pendant {} min.'),
('mod-guild-level', 129, 'frFR', 'Un fournisseur de la guilde attend ici pendant {} min.'),
('mod-guild-level', 130, 'frFR', 'Votre guilde n''a pas encore de maison : ce bonus viendra avec la maison de guilde.'),
('mod-guild-level', 131, 'frFR', 'Vous n''êtes pas dans une guilde.'),
('mod-guild-level', 132, 'frFR', 'Vous devez être dans un groupe.'),
('mod-guild-level', 133, 'frFR', 'Bonus actifs :'),
('mod-guild-level', 134, 'frFR', '{} (.gperk {}, niveau {}) : {}'),
('mod-guild-level', 135, 'frFR', 'prêt'),
('mod-guild-level', 136, 'frFR', 'verrouillé'),
('mod-guild-level', 137, 'frFR', 'encore {} min {} s'),
('mod-guild-level', 138, 'frFR', 'Invoquer le groupe'),
('mod-guild-level', 139, 'frFR', 'Ressusciter le groupe'),
('mod-guild-level', 140, 'frFR', 'Coffre de guilde'),
('mod-guild-level', 141, 'frFR', 'Fournisseur de la guilde'),
('mod-guild-level', 142, 'frFR', 'Maison de guilde'),
('mod-guild-level', 143, 'frFR', 'Rien ne peut être invoqué ici.'),

('mod-guild-level', 120, 'deDE', 'Eure Gilde muss für diesen Bonus Stufe {} haben.'),
('mod-guild-level', 121, 'deDE', 'Dieser Bonus ist nicht bereit: noch {} Min. {} Sek.'),
('mod-guild-level', 122, 'deDE', 'Nicht im Kampf.'),
('mod-guild-level', 123, 'deDE', 'Nicht auf einem Schlachtfeld oder in einer Arena.'),
('mod-guild-level', 124, 'deDE', 'Beschwörung an {} Gruppenmitglied(er) gesendet.'),
('mod-guild-level', 125, 'deDE', 'Wiederbelebung {} toten Gruppenmitglied(ern) angeboten.'),
('mod-guild-level', 126, 'deDE', 'Kein Gruppenmitglied zum Beschwören.'),
('mod-guild-level', 127, 'deDE', 'Kein totes Gruppenmitglied hier.'),
('mod-guild-level', 128, 'deDE', 'Ein Gildentresor steht hier für {} Min.'),
('mod-guild-level', 129, 'deDE', 'Ein Gildenlieferant wartet hier für {} Min.'),
('mod-guild-level', 130, 'deDE', 'Eure Gilde hat noch kein Haus: Dieser Bonus kommt mit dem Gildenhaus.'),
('mod-guild-level', 131, 'deDE', 'Ihr seid in keiner Gilde.'),
('mod-guild-level', 132, 'deDE', 'Ihr müsst in einer Gruppe sein.'),
('mod-guild-level', 133, 'deDE', 'Aktive Boni:'),
('mod-guild-level', 134, 'deDE', '{} (.gperk {}, Stufe {}): {}'),
('mod-guild-level', 135, 'deDE', 'bereit'),
('mod-guild-level', 136, 'deDE', 'gesperrt'),
('mod-guild-level', 137, 'deDE', 'noch {} Min. {} Sek.'),
('mod-guild-level', 138, 'deDE', 'Gruppe beschwören'),
('mod-guild-level', 139, 'deDE', 'Gruppe wiederbeleben'),
('mod-guild-level', 140, 'deDE', 'Gildentresor'),
('mod-guild-level', 141, 'deDE', 'Gildenlieferant'),
('mod-guild-level', 142, 'deDE', 'Gildenhaus'),
('mod-guild-level', 143, 'deDE', 'Hier kann nichts beschworen werden.'),

('mod-guild-level', 120, 'esES', 'Tu hermandad debe ser de nivel {} para esta ventaja.'),
('mod-guild-level', 121, 'esES', 'Esta ventaja no está lista: faltan {} min {} s.'),
('mod-guild-level', 122, 'esES', 'No en combate.'),
('mod-guild-level', 123, 'esES', 'No en un campo de batalla ni en una arena.'),
('mod-guild-level', 124, 'esES', 'Invocación enviada a {} miembro(s) de tu grupo.'),
('mod-guild-level', 125, 'esES', 'Resurrección ofrecida a {} miembro(s) muerto(s) de tu grupo.'),
('mod-guild-level', 126, 'esES', 'Ningún miembro de tu grupo que invocar.'),
('mod-guild-level', 127, 'esES', 'Ningún miembro muerto de tu grupo aquí.'),
('mod-guild-level', 128, 'esES', 'Un cofre de hermandad está aquí durante {} min.'),
('mod-guild-level', 129, 'esES', 'Un proveedor de la hermandad espera aquí durante {} min.'),
('mod-guild-level', 130, 'esES', 'Tu hermandad aún no tiene casa: esta ventaja llegará con la casa de hermandad.'),
('mod-guild-level', 131, 'esES', 'No estás en una hermandad.'),
('mod-guild-level', 132, 'esES', 'Debes estar en un grupo.'),
('mod-guild-level', 133, 'esES', 'Ventajas activas:'),
('mod-guild-level', 134, 'esES', '{} (.gperk {}, nivel {}): {}'),
('mod-guild-level', 135, 'esES', 'lista'),
('mod-guild-level', 136, 'esES', 'bloqueada'),
('mod-guild-level', 137, 'esES', 'faltan {} min {} s'),
('mod-guild-level', 138, 'esES', 'Invocar al grupo'),
('mod-guild-level', 139, 'esES', 'Resucitar al grupo'),
('mod-guild-level', 140, 'esES', 'Cofre de hermandad'),
('mod-guild-level', 141, 'esES', 'Proveedor de la hermandad'),
('mod-guild-level', 142, 'esES', 'Casa de hermandad'),
('mod-guild-level', 143, 'esES', 'Aquí no se puede invocar nada.'),


('mod-guild-level', 120, 'ruRU', 'Для этого бонуса гильдия должна быть {}-го уровня.'),
('mod-guild-level', 121, 'ruRU', 'Бонус ещё не готов: осталось {} мин {} с.'),
('mod-guild-level', 122, 'ruRU', 'Нельзя в бою.'),
('mod-guild-level', 123, 'ruRU', 'Нельзя на поле боя или на арене.'),
('mod-guild-level', 124, 'ruRU', 'Призыв отправлен участникам группы: {}.'),
('mod-guild-level', 125, 'ruRU', 'Воскрешение предложено погибшим участникам группы: {}.'),
('mod-guild-level', 126, 'ruRU', 'В группе некого призвать.'),
('mod-guild-level', 127, 'ruRU', 'Здесь нет погибших участников группы.'),
('mod-guild-level', 128, 'ruRU', 'Хранилище гильдии стоит здесь {} мин.'),
('mod-guild-level', 129, 'ruRU', 'Поставщик гильдии ждёт здесь {} мин.'),
('mod-guild-level', 130, 'ruRU', 'У гильдии ещё нет дома: этот бонус появится вместе с домом гильдии.'),
('mod-guild-level', 131, 'ruRU', 'Вы не состоите в гильдии.'),
('mod-guild-level', 132, 'ruRU', 'Вы должны быть в группе.'),
('mod-guild-level', 133, 'ruRU', 'Активные бонусы:'),
('mod-guild-level', 134, 'ruRU', '{} (.gperk {}, уровень {}): {}'),
('mod-guild-level', 135, 'ruRU', 'готов'),
('mod-guild-level', 136, 'ruRU', 'недоступен'),
('mod-guild-level', 137, 'ruRU', 'осталось {} мин {} с'),
('mod-guild-level', 138, 'ruRU', 'Призвать группу'),
('mod-guild-level', 139, 'ruRU', 'Воскресить группу'),
('mod-guild-level', 140, 'ruRU', 'Хранилище гильдии'),
('mod-guild-level', 141, 'ruRU', 'Поставщик гильдии'),
('mod-guild-level', 142, 'ruRU', 'Дом гильдии'),
('mod-guild-level', 143, 'ruRU', 'Здесь ничего нельзя призвать.');

-- THE HELP of the player commands (.help gperk).
DELETE FROM `command` WHERE `name` = 'gperk' OR `name` LIKE 'gperk %';
INSERT INTO `command` (`name`, `security`, `help`) VALUES
('gperk', 0, 'Syntax: .gperk $subcommand\n\nThe active perks of your guild, from level 21. Each has its own cooldown.'),
('gperk list', 0, 'Syntax: .gperk list\n\nThe active perks of your guild: locked, ready, or the time left before you can use them again.'),
('gperk raid', 0, 'Syntax: .gperk raid\n\nGuild level 21. Sends a summon, to accept like a summoning stone''s, to every member of your group. Out of combat, not in a battleground or an arena. 1 hour cooldown.'),
('gperk rez', 0, 'Syntax: .gperk rez\n\nGuild level 22. Offers a resurrection to every dead member of your group on this map, out of combat. 10 minutes cooldown.'),
('gperk bank', 0, 'Syntax: .gperk bank\n\nGuild level 23. A guild vault stands in front of you for 5 minutes. 1 hour cooldown.'),
('gperk vendor', 0, 'Syntax: .gperk vendor\n\nGuild level 24. A guild supplier, reagent vendor and repairer, waits in front of you for 5 minutes. 1 hour cooldown.'),
('gperk home', 0, 'Syntax: .gperk home\n\nGuild level 25. To the guild house: it comes with the guild house.');
