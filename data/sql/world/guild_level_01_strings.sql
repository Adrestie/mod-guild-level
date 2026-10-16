-- mod-guild-level — the texts shown to the players and the game masters.
--
-- English by default; every other language is served from module_string_locale,
-- falling back on English.
--
--   1 to 99     announcements to every member of the guild online, in the
--               green of the guild channel. {} is the level reached.
--   101 to 199  answers of the GM commands (.guildlevel).
--
-- Regenerable: the module's rows are deleted before being written again, so
-- the file may be replayed at will.

DELETE FROM `module_string` WHERE `module` = 'mod-guild-level';
INSERT INTO `module_string` (`module`, `id`, `string`) VALUES
('mod-guild-level',   1, '[Guild] Your guild has reached level {}!'),
('mod-guild-level',   2, '[Guild] Your guild has reached Paragon level {}!'),
('mod-guild-level',   3, '[Guild] Your guild has reached level {}, the highest! Its experience now goes to Paragon levels.'),
('mod-guild-level',   4, '[Guild] Your guild has reached Paragon level {}, the highest!'),
('mod-guild-level', 101, 'No guild found: name one (or give its id), select one of its members, or be in a guild.'),
('mod-guild-level', 102, 'Guild {} (id {}): level {}/{}, Paragon {}/{}.'),
('mod-guild-level', 103, 'Experience: {} / {} toward the next level.'),
('mod-guild-level', 104, 'Experience: every level is reached.'),
('mod-guild-level', 105, 'Lifetime experience: {}.'),
('mod-guild-level', 106, 'Out of range: {} at most.'),
('mod-guild-level', 107, 'Done.'),
('mod-guild-level', 108, 'The module is turned off (GuildLevel.Enable = 0): no guild earns experience.');

DELETE FROM `module_string_locale` WHERE `module` = 'mod-guild-level';
INSERT INTO `module_string_locale` (`module`, `id`, `locale`, `string`) VALUES
('mod-guild-level',   1, 'frFR', '[Guilde] Votre guilde atteint le niveau {} !'),
('mod-guild-level',   2, 'frFR', '[Guilde] Votre guilde atteint le niveau de Parangon {} !'),
('mod-guild-level',   3, 'frFR', '[Guilde] Votre guilde atteint le niveau {}, le plus haut ! Son expérience va désormais aux niveaux de Parangon.'),
('mod-guild-level',   4, 'frFR', '[Guilde] Votre guilde atteint le niveau de Parangon {}, le plus haut !'),
('mod-guild-level', 101, 'frFR', 'Aucune guilde trouvée : nommez-en une (ou donnez son id), sélectionnez un de ses membres, ou soyez dans une guilde.'),
('mod-guild-level', 102, 'frFR', 'Guilde {} (id {}) : niveau {}/{}, Parangon {}/{}.'),
('mod-guild-level', 103, 'frFR', 'Expérience : {} / {} vers le niveau suivant.'),
('mod-guild-level', 104, 'frFR', 'Expérience : tous les niveaux sont atteints.'),
('mod-guild-level', 105, 'frFR', 'Expérience totale : {}.'),
('mod-guild-level', 106, 'frFR', 'Hors limites : {} au plus.'),
('mod-guild-level', 107, 'frFR', 'Fait.'),
('mod-guild-level', 108, 'frFR', 'Le module est désactivé (GuildLevel.Enable = 0) : aucune guilde ne gagne d''expérience.'),

('mod-guild-level',   1, 'deDE', '[Gilde] Eure Gilde hat Stufe {} erreicht!'),
('mod-guild-level',   2, 'deDE', '[Gilde] Eure Gilde hat Paragonstufe {} erreicht!'),
('mod-guild-level',   3, 'deDE', '[Gilde] Eure Gilde hat Stufe {} erreicht, die höchste! Ihre Erfahrung fließt nun in Paragonstufen.'),
('mod-guild-level',   4, 'deDE', '[Gilde] Eure Gilde hat Paragonstufe {} erreicht, die höchste!'),
('mod-guild-level', 101, 'deDE', 'Keine Gilde gefunden: Nennt eine (oder ihre ID), wählt eines ihrer Mitglieder aus oder seid in einer Gilde.'),
('mod-guild-level', 102, 'deDE', 'Gilde {} (ID {}): Stufe {}/{}, Paragon {}/{}.'),
('mod-guild-level', 103, 'deDE', 'Erfahrung: {} / {} bis zur nächsten Stufe.'),
('mod-guild-level', 104, 'deDE', 'Erfahrung: Alle Stufen sind erreicht.'),
('mod-guild-level', 105, 'deDE', 'Gesamte Erfahrung: {}.'),
('mod-guild-level', 106, 'deDE', 'Außerhalb des Bereichs: höchstens {}.'),
('mod-guild-level', 107, 'deDE', 'Erledigt.'),
('mod-guild-level', 108, 'deDE', 'Das Modul ist deaktiviert (GuildLevel.Enable = 0): Keine Gilde erhält Erfahrung.'),

('mod-guild-level',   1, 'esES', '[Hermandad] ¡Tu hermandad ha alcanzado el nivel {}!'),
('mod-guild-level',   2, 'esES', '[Hermandad] ¡Tu hermandad ha alcanzado el nivel de Paragón {}!'),
('mod-guild-level',   3, 'esES', '[Hermandad] ¡Tu hermandad ha alcanzado el nivel {}, el más alto! Su experiencia va ahora a los niveles de Paragón.'),
('mod-guild-level',   4, 'esES', '[Hermandad] ¡Tu hermandad ha alcanzado el nivel de Paragón {}, el más alto!'),
('mod-guild-level', 101, 'esES', 'No se encontró ninguna hermandad: nombra una (o da su id), selecciona a uno de sus miembros o pertenece a una hermandad.'),
('mod-guild-level', 102, 'esES', 'Hermandad {} (id {}): nivel {}/{}, Paragón {}/{}.'),
('mod-guild-level', 103, 'esES', 'Experiencia: {} / {} hacia el siguiente nivel.'),
('mod-guild-level', 104, 'esES', 'Experiencia: se han alcanzado todos los niveles.'),
('mod-guild-level', 105, 'esES', 'Experiencia total: {}.'),
('mod-guild-level', 106, 'esES', 'Fuera de rango: {} como máximo.'),
('mod-guild-level', 107, 'esES', 'Hecho.'),
('mod-guild-level', 108, 'esES', 'El módulo está desactivado (GuildLevel.Enable = 0): ninguna hermandad gana experiencia.'),

('mod-guild-level',   1, 'esMX', '[Hermandad] ¡Tu hermandad alcanzó el nivel {}!'),
('mod-guild-level',   2, 'esMX', '[Hermandad] ¡Tu hermandad alcanzó el nivel de Paragón {}!'),
('mod-guild-level',   3, 'esMX', '[Hermandad] ¡Tu hermandad alcanzó el nivel {}, el más alto! Su experiencia va ahora a los niveles de Paragón.'),
('mod-guild-level',   4, 'esMX', '[Hermandad] ¡Tu hermandad alcanzó el nivel de Paragón {}, el más alto!'),
('mod-guild-level', 101, 'esMX', 'No se encontró ninguna hermandad: nombra una (o da su id), selecciona a uno de sus miembros o pertenece a una hermandad.'),
('mod-guild-level', 102, 'esMX', 'Hermandad {} (id {}): nivel {}/{}, Paragón {}/{}.'),
('mod-guild-level', 103, 'esMX', 'Experiencia: {} / {} hacia el siguiente nivel.'),
('mod-guild-level', 104, 'esMX', 'Experiencia: se alcanzaron todos los niveles.'),
('mod-guild-level', 105, 'esMX', 'Experiencia total: {}.'),
('mod-guild-level', 106, 'esMX', 'Fuera de rango: {} como máximo.'),
('mod-guild-level', 107, 'esMX', 'Hecho.'),
('mod-guild-level', 108, 'esMX', 'El módulo está desactivado (GuildLevel.Enable = 0): ninguna hermandad gana experiencia.'),

('mod-guild-level',   1, 'ruRU', '[Гильдия] Ваша гильдия достигла {}-го уровня!'),
('mod-guild-level',   2, 'ruRU', '[Гильдия] Ваша гильдия достигла {}-го уровня Парагона!'),
('mod-guild-level',   3, 'ruRU', '[Гильдия] Ваша гильдия достигла {}-го уровня, наивысшего! Теперь её опыт идёт на уровни Парагона.'),
('mod-guild-level',   4, 'ruRU', '[Гильдия] Ваша гильдия достигла {}-го уровня Парагона, наивысшего!'),
('mod-guild-level', 101, 'ruRU', 'Гильдия не найдена: укажите её название (или id), выберите одного из её членов или состойте в гильдии.'),
('mod-guild-level', 102, 'ruRU', 'Гильдия {} (id {}): уровень {}/{}, Парагон {}/{}.'),
('mod-guild-level', 103, 'ruRU', 'Опыт: {} / {} до следующего уровня.'),
('mod-guild-level', 104, 'ruRU', 'Опыт: все уровни достигнуты.'),
('mod-guild-level', 105, 'ruRU', 'Всего опыта: {}.'),
('mod-guild-level', 106, 'ruRU', 'Вне допустимого диапазона: не более {}.'),
('mod-guild-level', 107, 'ruRU', 'Готово.'),
('mod-guild-level', 108, 'ruRU', 'Модуль отключён (GuildLevel.Enable = 0): гильдии не получают опыт.');
