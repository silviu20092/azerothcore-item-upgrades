DELETE FROM `command` WHERE `name`='item_upgrade reload';
INSERT INTO `command`(`name`, `security`, `help`) VALUES ('item_upgrade reload', 3, 'Syntax: .item_upgrade reload
Reloads all necessary data for the Item Upgrades module.');
 