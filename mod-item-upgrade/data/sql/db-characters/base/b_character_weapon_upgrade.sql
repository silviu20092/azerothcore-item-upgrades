DROP TABLE IF EXISTS `character_weapon_upgrade`;
CREATE TABLE `character_weapon_upgrade`(
	`guid` int unsigned not null,
	`item_guid` int unsigned not null,
    `upgrade_perc` float not null,
    PRIMARY KEY (`guid`, `item_guid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;