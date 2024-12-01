DROP TABLE IF EXISTS `mod_item_upgrade_stats_req_override`;
CREATE TABLE `mod_item_upgrade_stats_req_override` (
  `id` int unsigned NOT NULL AUTO_INCREMENT,
  `stat_id` int unsigned NOT NULL,
  `item_entry` int unsigned NOT NULL,
  `req_type` tinyint unsigned NOT NULL,
  `req_val1` float DEFAULT NULL,
  `req_val2` float DEFAULT NULL,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;