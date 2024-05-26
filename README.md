# ![logo](https://raw.githubusercontent.com/azerothcore/azerothcore.github.io/master/images/logo-github.png) AzerothCore

# Item upgrades for AzerothCore

## Overview

Adds the possibility to individually upgrade items. **EVERY** stat can be upgraded, provided you enable them in the .conf file and add them in database. Upgrades are rank-based, meaning you can incrementally upgrade each stat, starting with rank 1 to rank n. Players will need to purchase the previous rank in order to advance to the next. Stats that can be upgraded along with their ranks are in reloadable tables, meaning you can add stats and/or ranks, modify or even delete them without restarting the server. Player's upgrades will be updated accordingly after the data is reloaded.

## Limitations

1. Due to the nature of **WOTLK** client, the upgraded **STATS** will only be visible to the owner. Also, items with random properties (like "of the Bear", "of Intellect" etc) will always send the original stats. **This is only visual, stats will be there nonetheless!** 
2. You **CAN'T** add or replace stats, you can only upgrade current item's stats.
3. Upgrades will be lost (of course) when trading, sending mail, depositing to guild bank, deposit to auction.
4. Heirlooms can't be upgraded.

## How to install

1. Clone this repository somewhere on your device.
2. Copy mod-item-upgrade to your AzerothCore repo modules folder.
3. Re-run cmake to generate the solution.
4. Re-build your project.
5. You should have mod_item_upgrade.conf.dist copied in configs/modules after building, copy this to your server's binaries folder.
6. Start the server, .sql files should automatically be imported in DB, if not, apply them manually.

WARNING: this mod requires at least this version of AzerothCore https://github.com/azerothcore/azerothcore-wotlk/commit/3988e9581d736f8c7891baaf13011e9df4f46fa4

## How to use

Let's say you want players to be able to upgrade **STAMINA** by 5% and 10% respectively. All you need to do is insert **two** lines in **mod_item_upgrade_stats** table:
* **id** - this is an unique identifier for this stat/rank, just choose the next available id (current maximum + 1), try to keep it consecutive
* **stat_type** - since we want stamina, put 7 for this. This column corresponds to **ItemModType** enum in **ItemTemplate.h**. Consult this enum to find each stat, the enum names should be self-explanatory. **EVERY** stat is allowed for upgrade!
* **stat_mod_pct** - percentage increase for the rank, since we want 5% and 10%, put **5** for first line and **10** for second line.
* **stat_rank** - this **MUST ALWAYS** start with 1 and be consecutive. Each rank **MUST ALWAYS** be an upgrade, meaning **stat_mod_pct** for rank **x** must be **SMALLER** that **stat_mod_pct** for rank **x+1**. So put **1** for the **5%** increase line and **2** for the **10%** increase line. If later you want to add the upgrade possibility to **20%** for stamina, just add another line with **stat_mod_pct** = 20 and **stat_rank** = 3.

Let's say you inserted the two lines and their **id** is 10 (for rank 1) and 11 (for rank 2). Now players are ready to upgrade item's stamina by 5% and 10% respectively. But now they **CAN** do it for free, since these two ranks have no requirements! If you want these ranks to have some requirements, let's use the second table **mod_item_upgrade_stats_req**:
* **id** - this is just an unique identifier with AUTO_INCREMENT, just leave it null and MySQL will generate an id for you
* **stat_id** - this corresponds to **mod_item_upgrade_stats.id**, in our case we have 10 (for rank 1) and 11 (for rank 2).
* **req_type** - type of requirement, this corresponds to **UpgradeStatReqType** enum in **item_upgrade.h**. Possible values:
  * 1 - requires **money**
  * 2 - requires **honor points**
  * 3 - requires **arena points**
  * 4 - requires **item(s)**
  * 5 - requires **NOTHING**, has no effect whether added or not in this table, should not be used here, only in **mod_item_upgrade_stats_req_override** (see below)
* **req_val1** - based on **req_type**:
  * when req_type = 1 (money), then this is a numeric value corresponding to the amount of required **copper** (e.g 10000000 means the rank will require 1000 gold to be bought)
  * when req_type = 2 (honor), then this is a numeric value corresponding to how many honor points are required to buy the rank
  * when req_type = 3 (arena), then this is a numeric value corresponding to how many arena points are required to buy the rank
  * when req_type = 4 (item), then this is the **entry** (see **item_template.entry**) of the required item(s) to buy the rank
* **req_val2** - based on **req_type**:
  * **UNUSED** when req_type is not 4
  * when req_type = 4 (item), then this is the amount of **req_val1** item(s) required to buy the rank

So let's say you want players to upgrade stamina by 5% for **FREE**, then you don't have to insert anything in this table for **stat_id** 10. But for the 10% increase, you want the players to have 10x Badge of Justice and 1000 honor. So, we need to insert two lines: one with **stat_id** = 11, **req_type** = 2 (honor) and **req_val1** = 1000 and another line with **stat_id** = 11, **req_type** = 4 (item), **req_val1** = 29434 (entry for Badge of Justice) and **req_val2** = 10 (requires 10x badges).

There is some sample data already inserted in these tables.

### Overriding requirements on per item basis

**mod_item_upgrade_stats_req** will be used to globally define requirements for each rank. This means that **EVERY** item will have the same requirements for a certain rank. We can override this behaviour and set requirements for each item individually. For this, use **mod_item_upgrade_stats_req_override** table which has the exact same structure as **mod_item_upgrade_stats_req**, except there is one more field: **item_entry** which is the entry of the item. So simply follow the above procedure to add requirements and simply fill **item_entry** with the entry of the item that you want.

### Allowing and blacklisting items

You can allow or blacklist certain items by using two tables:
* **mod_item_upgrade_allowed_items** - if you add some entries to this table, then **ONLY** these items will be available for upgrade. This is useful when you want to exclude **ALL** items and only add a few that have the possibily to be upgraded.
* **mod_item_upgrade_blacklisted_items** - every item that is added here will no longer be available for upgrading. This is useful when you want to add upgrade possibility to **ALL** items but simply exclude a few.

These two tables have only one column **entry** which corresponds to the entry of the item (**item_template.entry**). You can have the same entry in both allowed and blacklisted tables, in which case the item will become **blacklisted**. If players already bought some upgrades for a certain item and then you decide to **blacklist** that item, then the upgrade will become **inactive** (it will show as inactive in the Upgraded items menu). The upgrade will then become **active** if you decide to remove the blacklist for the item.

### Allowing and blacklisting **ranks** for items

There is the possibility to allow or blacklist **ranks** for certain items. Just like above, two tables are involved:
* **mod_item_upgrade_allowed_stats_items** - link a certain rank (**stat_id**, this points to **mod_item_upgrade_stats.id**) with as many items as you want. If there are records for a certain **stat_id**, this means that **ONLY** these items will be able to gain/use this rank.
* **mod_item_upgrade_blacklisted_stats_items** - If there are records for a certain **stat_id**, this means that these items will no longer be able to gain/use this rank.

### Random upgrades on loot

There are configurable options to automatically upgrade items when players loot them (Titanforging-like system). Maximum possible rank gained by each stat is configurable via **ItemUpgrade.RandomUpgradeMaxRank** option. The chance for an automatic upgrade to occur is configurable via **ItemUpgrade.RandomUpgradeChance**. The maximum number of stats that can be upgraded is also configurable via **ItemUpgrade.RandomUpgradeMaxStatCount**. Stats to be upgraded are chosen **randomly**. Rewarded quest items and items looted via party (need, greed rolls) are also eligible for automatic upgrades.

## Ingame usage

Use .npc add 200003 to spawn the Master Item Upgrade NPC. The rest is self explanatory.

### Editing related tables and hot-reloading data

Everything is reloadable, meaning you can **add** stats and rank(s), **modify** current ranks, **delete** stats and ranks, add **allowed** and **blacklisted** items. The only table that you shouldn't manually modify is **character_item_upgrade**, as the data here will be validated against the main tables and orphaned records will be automatically deleted. However, modifying this table won't cause any harm, and you can actually manually delete or add character upgrades here if you want.
**WARNING**: before starting to edit database, you should **lock** the Gossip NPC so that players can't use it in the meantime. This is **NOT** required but **strongly** advised, in this way players can't buy some upgrades while you can potentially remove them in the background, etc. Locking the NPC is done via **.item_upgrade lock** command or via the **NPC itself** if the player has administrator role (GM level 3). After you locked the NPC and finished editing the database, you can use **.item_upgrade reload** command to reload everything. This will **correctly** refresh everything related to item upgrades for every connected player (stats, visuals) and will refresh the menus for the Gossip NPC.

### Removing upgrades from an item

There is a configuration option that allows players to restore items to their original stats (remove upgrades). You can also configure a **token** (and it's quantity) to be given to the player when purging an upgrade. You **can't** purge individual stats or ranks, there is no point, you can only remove **ALL** upgrades from an item at once.

## Some photos

![pic1](https://github.com/silviu20092/azerothcore-item-upgrades/blob/master/pics/pic1.jpg?raw=true)
![pic2](https://github.com/silviu20092/azerothcore-item-upgrades/blob/master/pics/pic2.jpg?raw=true)
![pic3](https://github.com/silviu20092/azerothcore-item-upgrades/blob/master/pics/pic3.jpg?raw=true)
![pic4](https://github.com/silviu20092/azerothcore-item-upgrades/blob/master/pics/pic4.jpg?raw=true)
![pic4_1](https://github.com/silviu20092/azerothcore-item-upgrades/blob/master/pics/pic4_1.jpg?raw=true)
![pic5](https://github.com/silviu20092/azerothcore-item-upgrades/blob/master/pics/pic5.jpg?raw=true)
![pic6](https://github.com/silviu20092/azerothcore-item-upgrades/blob/master/pics/pic6.jpg?raw=true)
![pic7](https://github.com/silviu20092/azerothcore-item-upgrades/blob/master/pics/pic7.jpg?raw=true)
![pic8](https://github.com/silviu20092/azerothcore-item-upgrades/blob/master/pics/pic8.jpg?raw=true)
![pic9](https://github.com/silviu20092/azerothcore-item-upgrades/blob/master/pics/pic9.jpg?raw=true)
![pic10](https://github.com/silviu20092/azerothcore-item-upgrades/blob/master/pics/pic10.jpg?raw=true)

## Credits
- silviu20092