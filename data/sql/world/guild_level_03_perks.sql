-- mod-guild-level — the passive perks of levels 1 to 20.
--
-- Idempotent: every row is deleted before being written again.

-- THE HIDDEN AURA of the perks that have no core hook, spell 92000 (tranche 92
-- of the WoW-mods registry). Server side only: AzerothCore completes Spell.dbc
-- from `spell_dbc` at startup. Passive, hidden, kept through death, never
-- cancelled. The module sets the amounts from the guild's level:
--   effect 1  aura 281  honor gained, %
--   effect 2  aura 130  ground mounted speed, % (stacks with everything)
--   effect 3  aura 209  flying mounted speed, % (stacks with everything)
-- Attributes: PASSIVE | DO_NOT_DISPLAY | DO_NOT_LOG | CANT_CANCEL;
-- AttributesEx3: DEATH_PERSISTENT. Duration index 21: infinite.
DELETE FROM `spell_dbc` WHERE `ID` = 92000;
INSERT INTO `spell_dbc` (`ID`, `Attributes`, `AttributesEx3`, `CastingTimeIndex`, `ProcChance`, `DurationIndex`, `RangeIndex`, `EquippedItemClass`,
  `Effect_1`, `EffectDieSides_1`, `EffectBasePoints_1`, `ImplicitTargetA_1`, `EffectAura_1`,
  `Effect_2`, `EffectDieSides_2`, `EffectBasePoints_2`, `ImplicitTargetA_2`, `EffectAura_2`,
  `Effect_3`, `EffectDieSides_3`, `EffectBasePoints_3`, `ImplicitTargetA_3`, `EffectAura_3`,
  `EffectChainAmplitude_1`, `EffectChainAmplitude_2`, `EffectChainAmplitude_3`, `SpellIconID`, `SchoolMask`,
  `Name_Lang_enUS`, `Name_Lang_Mask`, `Description_Lang_enUS`, `Description_Lang_Mask`, `AuraDescription_Lang_enUS`, `AuraDescription_Lang_Mask`)
VALUES (92000, 2147484096, 1048576, 1, 101, 21, 1, -1,
  6, 1, -1, 1, 281,
  6, 1, -1, 1, 130,
  6, 1, -1, 1, 209,
  1.0, 1.0, 1.0, 1, 1,
  'Guild Perks', 16712190, 'The perks of your guild''s level.', 16712190, 'The perks of your guild''s level.', 16712190);

-- THE HEARTHSTONE: the perk shortens its cooldown right after the cast.
DELETE FROM `spell_script_names` WHERE `ScriptName` = 'spell_guild_level_hearthstone';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES (8690, 'spell_guild_level_hearthstone');

-- THE TEXTS of the perks. {} placeholders, in order:
--   5    count, item name
--   110  guild level
--   111  experience %, honor %, reputation %, mounted speed %
--   112  vendor %, gold to the guild bank %, Hearthstone minutes
--   113  gathering chance %, extra items, resurrection (114 / 115), consumable minutes
--   117  damaged items mended, seconds between two mendings
DELETE FROM `module_string` WHERE `module` = 'mod-guild-level' AND `id` IN (5, 110, 111, 112, 113, 114, 115, 116, 117);
INSERT INTO `module_string` (`module`, `id`, `string`) VALUES
('mod-guild-level',   5, '[Guild] Bountiful gathering: {} more {}.'),
('mod-guild-level', 110, 'Perks of a level {} guild:'),
('mod-guild-level', 111, 'Experience +{}%, honor +{}%, reputation +{}%, mounted speed +{}%.'),
('mod-guild-level', 112, 'Vendor prices and repairs -{}%, looted gold added to the guild bank {}%, Hearthstone cooldown -{} min.'),
('mod-guild-level', 113, 'Gathering: {}% chance of {} more. Full resurrection by a guildmate: {}. Flasks, elixirs and food buffs: +{} min.'),
('mod-guild-level', 114, 'yes'),
('mod-guild-level', 115, 'no'),
('mod-guild-level', 116, 'The perks are turned off (GuildLevel.Perks.Enable = 0).'),
('mod-guild-level', 117, 'Durability: {} damaged item(s) worn mended by 1 point every {} seconds.');

DELETE FROM `module_string_locale` WHERE `module` = 'mod-guild-level' AND `id` IN (5, 110, 111, 112, 113, 114, 115, 116, 117);
INSERT INTO `module_string_locale` (`module`, `id`, `locale`, `string`) VALUES
('mod-guild-level',   5, 'frFR', '[Guilde] Récolte abondante : {} {} de plus.'),
('mod-guild-level', 110, 'frFR', 'Bonus d''une guilde de niveau {} :'),
('mod-guild-level', 111, 'frFR', 'Expérience +{} %, honneur +{} %, réputation +{} %, vitesse en monture +{} %.'),
('mod-guild-level', 112, 'frFR', 'Prix des vendeurs et réparations -{} %, or ramassé ajouté à la banque de guilde {} %, recharge de la pierre de foyer -{} min.'),
('mod-guild-level', 113, 'frFR', 'Récolte : {} % de chance d''en obtenir {} de plus. Résurrection complète par un membre de la guilde : {}. Flacons, élixirs et nourriture : +{} min.'),
('mod-guild-level', 114, 'frFR', 'oui'),
('mod-guild-level', 115, 'frFR', 'non'),
('mod-guild-level', 116, 'frFR', 'Les bonus sont désactivés (GuildLevel.Perks.Enable = 0).'),
('mod-guild-level', 117, 'frFR', 'Durabilité : {} objet(s) porté(s) abîmé(s) réparé(s) de 1 point toutes les {} secondes.'),

('mod-guild-level',   5, 'deDE', '[Gilde] Reiche Ernte: {} weitere {}.'),
('mod-guild-level', 110, 'deDE', 'Boni einer Gilde der Stufe {}:'),
('mod-guild-level', 111, 'deDE', 'Erfahrung +{}%, Ehre +{}%, Ruf +{}%, Reittiergeschwindigkeit +{}%.'),
('mod-guild-level', 112, 'deDE', 'Händlerpreise und Reparaturen -{}%, erbeutetes Gold zusätzlich zu {}% in die Gildenbank, Abklingzeit des Ruhesteins -{} Min.'),
('mod-guild-level', 113, 'deDE', 'Sammeln: {}% Chance auf {} weitere. Volle Wiederbelebung durch ein Gildenmitglied: {}. Fläschchen, Elixiere und Essensbuffs: +{} Min.'),
('mod-guild-level', 114, 'deDE', 'ja'),
('mod-guild-level', 115, 'deDE', 'nein'),
('mod-guild-level', 116, 'deDE', 'Die Boni sind deaktiviert (GuildLevel.Perks.Enable = 0).'),
('mod-guild-level', 117, 'deDE', 'Haltbarkeit: {} beschädigte(r) getragene(r) Gegenstand/Gegenstände alle {} Sekunden um 1 Punkt repariert.'),

('mod-guild-level',   5, 'esES', '[Hermandad] Recolección abundante: {} más de {}.'),
('mod-guild-level', 110, 'esES', 'Ventajas de una hermandad de nivel {}:'),
('mod-guild-level', 111, 'esES', 'Experiencia +{}%, honor +{}%, reputación +{}%, velocidad de montura +{}%.'),
('mod-guild-level', 112, 'esES', 'Precios de vendedores y reparaciones -{}%, oro despojado añadido al banco de hermandad {}%, reutilización de la piedra de hogar -{} min.'),
('mod-guild-level', 113, 'esES', 'Recolección: {}% de probabilidad de {} más. Resurrección completa por un miembro de la hermandad: {}. Frascos, elixires y comida: +{} min.'),
('mod-guild-level', 114, 'esES', 'sí'),
('mod-guild-level', 115, 'esES', 'no'),
('mod-guild-level', 116, 'esES', 'Las ventajas están desactivadas (GuildLevel.Perks.Enable = 0).'),
('mod-guild-level', 117, 'esES', 'Durabilidad: {} objeto(s) equipado(s) dañado(s) reparado(s) 1 punto cada {} segundos.'),

('mod-guild-level',   5, 'esMX', '[Hermandad] Recolección abundante: {} más de {}.'),
('mod-guild-level', 110, 'esMX', 'Ventajas de una hermandad de nivel {}:'),
('mod-guild-level', 111, 'esMX', 'Experiencia +{}%, honor +{}%, reputación +{}%, velocidad de montura +{}%.'),
('mod-guild-level', 112, 'esMX', 'Precios de vendedores y reparaciones -{}%, oro despojado añadido al banco de hermandad {}%, reutilización de la piedra de hogar -{} min.'),
('mod-guild-level', 113, 'esMX', 'Recolección: {}% de probabilidad de {} más. Resurrección completa por un miembro de la hermandad: {}. Frascos, elixires y comida: +{} min.'),
('mod-guild-level', 114, 'esMX', 'sí'),
('mod-guild-level', 115, 'esMX', 'no'),
('mod-guild-level', 116, 'esMX', 'Las ventajas están desactivadas (GuildLevel.Perks.Enable = 0).'),
('mod-guild-level', 117, 'esMX', 'Durabilidad: {} objeto(s) equipado(s) dañado(s) reparado(s) 1 punto cada {} segundos.'),

('mod-guild-level',   5, 'ruRU', '[Гильдия] Богатый сбор: ещё {} — {}.'),
('mod-guild-level', 110, 'ruRU', 'Бонусы гильдии {}-го уровня:'),
('mod-guild-level', 111, 'ruRU', 'Опыт +{}%, честь +{}%, репутация +{}%, скорость верхом +{}%.'),
('mod-guild-level', 112, 'ruRU', 'Цены торговцев и ремонт -{}%, добытое золото дополнительно в банк гильдии {}%, восстановление камня возвращения -{} мин.'),
('mod-guild-level', 113, 'ruRU', 'Сбор: {}% шанс получить ещё {}. Полное воскрешение согильдийцем: {}. Настои, эликсиры и еда: +{} мин.'),
('mod-guild-level', 114, 'ruRU', 'да'),
('mod-guild-level', 115, 'ruRU', 'нет'),
('mod-guild-level', 116, 'ruRU', 'Бонусы отключены (GuildLevel.Perks.Enable = 0).'),
('mod-guild-level', 117, 'ruRU', 'Прочность: {} повреждённых надетых предметов чинится на 1 единицу каждые {} с.');

DELETE FROM `command` WHERE `name` = 'guildlevel perks';
INSERT INTO `command` (`name`, `security`, `help`) VALUES
('guildlevel perks', 2, 'Syntax: .guildlevel perks [$guild]\n\nThe perks the members of the guild get at its current level.');
