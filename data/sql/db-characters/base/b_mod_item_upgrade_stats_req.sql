DROP TABLE IF EXISTS `mod_item_upgrade_stats_req`;
CREATE TABLE `mod_item_upgrade_stats_req`(
	`id` int unsigned not null AUTO_INCREMENT,
	`stat_id` int unsigned not null,
    `req_type` tinyint unsigned not null,
    `req_val1` float not null,
    `req_val2` float,
    PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

INSERT INTO `mod_item_upgrade_stats_req` (`stat_id`, `req_type`, `req_val1`) VALUES (1, 1, 10000000);
INSERT INTO `mod_item_upgrade_stats_req` (`stat_id`, `req_type`, `req_val1`) VALUES (2, 1, 10000000);
INSERT INTO `mod_item_upgrade_stats_req` (`stat_id`, `req_type`, `req_val1`) VALUES (3, 1, 10000000);
INSERT INTO `mod_item_upgrade_stats_req` (`stat_id`, `req_type`, `req_val1`) VALUES (4, 1, 10000000);
INSERT INTO `mod_item_upgrade_stats_req` (`stat_id`, `req_type`, `req_val1`) VALUES (5, 1, 10000000);
INSERT INTO `mod_item_upgrade_stats_req` (`stat_id`, `req_type`, `req_val1`) VALUES (6, 1, 10000000);
INSERT INTO `mod_item_upgrade_stats_req` (`stat_id`, `req_type`, `req_val1`) VALUES (7, 1, 10000000);
INSERT INTO `mod_item_upgrade_stats_req` (`stat_id`, `req_type`, `req_val1`) VALUES (8, 1, 10000000);
INSERT INTO `mod_item_upgrade_stats_req` (`stat_id`, `req_type`, `req_val1`) VALUES (9, 1, 10000000);
INSERT INTO `mod_item_upgrade_stats_req` (`stat_id`, `req_type`, `req_val1`) VALUES (10, 1, 10000000);
INSERT INTO `mod_item_upgrade_stats_req` (`stat_id`, `req_type`, `req_val1`) VALUES (11, 1, 10000000);
INSERT INTO `mod_item_upgrade_stats_req` (`stat_id`, `req_type`, `req_val1`) VALUES (12, 1, 10000000);
INSERT INTO `mod_item_upgrade_stats_req` (`stat_id`, `req_type`, `req_val1`) VALUES (13, 1, 10000000);
INSERT INTO `mod_item_upgrade_stats_req` (`stat_id`, `req_type`, `req_val1`) VALUES (14, 1, 10000000);
INSERT INTO `mod_item_upgrade_stats_req` (`stat_id`, `req_type`, `req_val1`) VALUES (15, 1, 10000000);
INSERT INTO `mod_item_upgrade_stats_req` (`stat_id`, `req_type`, `req_val1`) VALUES (16, 1, 10000000);
INSERT INTO `mod_item_upgrade_stats_req` (`stat_id`, `req_type`, `req_val1`, `req_val2`) VALUES (9, 4, 29434, 100);
INSERT INTO `mod_item_upgrade_stats_req` (`stat_id`, `req_type`, `req_val1`, `req_val2`) VALUES (10, 4, 29434, 100);
INSERT INTO `mod_item_upgrade_stats_req` (`stat_id`, `req_type`, `req_val1`, `req_val2`) VALUES (11, 4, 29434, 100);
INSERT INTO `mod_item_upgrade_stats_req` (`stat_id`, `req_type`, `req_val1`, `req_val2`) VALUES (12, 4, 29434, 100);
INSERT INTO `mod_item_upgrade_stats_req` (`stat_id`, `req_type`, `req_val1`, `req_val2`) VALUES (13, 4, 29434, 100);
INSERT INTO `mod_item_upgrade_stats_req` (`stat_id`, `req_type`, `req_val1`, `req_val2`) VALUES (14, 4, 29434, 100);
INSERT INTO `mod_item_upgrade_stats_req` (`stat_id`, `req_type`, `req_val1`, `req_val2`) VALUES (15, 4, 29434, 100);
INSERT INTO `mod_item_upgrade_stats_req` (`stat_id`, `req_type`, `req_val1`, `req_val2`) VALUES (16, 4, 29434, 100);