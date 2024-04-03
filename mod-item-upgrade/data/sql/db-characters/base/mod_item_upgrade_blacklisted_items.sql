DROP TABLE IF EXISTS `mod_item_upgrade_blacklisted_items`;
CREATE TABLE `mod_item_upgrade_blacklisted_items`(
	`entry` int unsigned not null,
    PRIMARY KEY (`entry`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;