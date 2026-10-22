-- mod-guild-level — the Customization objects in one block (92100-94599): the numbers they held in
-- 82200-82899 before (items and displays of the objects 1900 and up) are given back to the tranche 82.
--
-- Only the module's own rows go: its items use spell 92080, its displays are its models
-- (World\GuildLevel\Decor\, each _ standing for the backslash). Idempotent.

DELETE FROM `item_template_locale` WHERE `ID` BETWEEN 82200 AND 82899
  AND `ID` IN (SELECT `entry` FROM `item_template` WHERE `spellid_1` = 92080);
DELETE FROM `item_template` WHERE `entry` BETWEEN 82200 AND 82899 AND `spellid_1` = 92080;
DELETE FROM `gameobjectdisplayinfo_dbc` WHERE `ID` BETWEEN 82200 AND 82899
  AND `ModelName` LIKE 'World_GuildLevel_Decor_%';
