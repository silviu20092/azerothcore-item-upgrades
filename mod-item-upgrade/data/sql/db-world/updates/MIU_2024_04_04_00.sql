DELETE FROM `command` WHERE `name`='item_upgrade lock';
INSERT INTO `command`(`name`, `security`, `help`) VALUES ('item_upgrade lock', 3, 'Syntax: .item_upgrade lock
Locks the Item Upgrade NPC for safe database edit. Players won''t be able to use the NPC anymore, the lock will be released when using .item_upgrade reload command');
 