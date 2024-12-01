DROP TABLE IF EXISTS `mod_item_upgrade_blacklisted_stats_items`;
CREATE TABLE `mod_item_upgrade_blacklisted_stats_items`(
	`stat_id` int unsigned not null,
	`entry` int unsigned not null,
    PRIMARY KEY (`stat_id`, `entry`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;