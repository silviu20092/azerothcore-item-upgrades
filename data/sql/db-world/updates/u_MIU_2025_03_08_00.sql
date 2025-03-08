DELETE FROM `command` WHERE `name`='item_upgrade list';
INSERT INTO `command`(`name`, `security`, `help`) VALUES ('item_upgrade list', 0, 'Syntax: .item_upgrade list
Lists player''s upgraded items, only searches through equipped items.');
 