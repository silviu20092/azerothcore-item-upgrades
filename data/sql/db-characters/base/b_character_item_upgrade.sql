DROP TABLE IF EXISTS `character_item_upgrade`;
CREATE TABLE `character_item_upgrade`(
	`guid` int unsigned not null,
	`item_guid` int unsigned not null,
    `stat_id` int unsigned not null,
    PRIMARY KEY (`guid`, `item_guid`, `stat_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;