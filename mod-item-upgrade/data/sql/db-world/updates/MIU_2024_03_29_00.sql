SET @Entry = 200003;
SET @Name = "Master";
SET @Subname = "Item Upgrades";

DELETE FROM `creature_template_model` WHERE `CreatureID` = @Entry;
DELETE FROM `creature_template` WHERE `entry` = @Entry;

INSERT INTO `creature_template` (`entry`, `name`, `subname`, `IconName`, `gossip_menu_id`, `minlevel`, `maxlevel`, `exp`, `faction`, `npcflag`, `scale`, `rank`, `dmgschool`, `baseattacktime`, `rangeattacktime`, `unit_class`, `unit_flags`, `type`, `type_flags`, `lootid`, `pickpocketloot`, `skinloot`, `AIName`, `MovementType`, `HoverHeight`, `RacialLeader`, `movementId`, `RegenHealth`, `mechanic_immune_mask`, `flags_extra`, `ScriptName`) VALUES
(@Entry, @Name, @Subname, null, 0, 80, 80, 2, 35, 1, 1, 0, 0, 2000, 0, 1, 2147483648, 7, 138936390, 0, 0, 0, '', 0, 1, 0, 0, 1, 0, 0, 'npc_item_upgrade');
INSERT INTO `creature_template_model` (`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`, `VerifiedBuild`) VALUES (@Entry, 0, 19646, 1, 1, 0);
