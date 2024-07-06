/*
 * Credits: silviu20092
 */

#ifndef _ITEM_UPGRADE_CONFIG_H_
#define _ITEM_UPGRADE_CONFIG_H_

#include <string>
#include "Define.h"

enum ItemUpgradeBoolConfigs
{
    CONFIG_ITEM_UPGRADE_ENABLED = 0,
    CONFIG_ITEM_UPGRADE_SEND_PACKETS,
    CONFIG_ITEM_UPGRADE_ALLOW_PURGE,
    CONFIG_ITEM_UPGRADE_REFUND_ALL_ON_PURGE,
    CONFIG_ITEM_UPGRADE_RANDOM_UPGRADES,
    CONFIG_ITEM_UPGRADE_RANDOM_UPGRADES_BUY,
    CONFIG_ITEM_UPGRADE_RANDOM_UPGRADES_LOOT,
    CONFIG_ITEM_UPGRADE_RANDOM_UPGRADES_WIN,
    CONFIG_ITEM_UPGRADE_RANDOM_UPGRADES_QUEST_REWARD,
    CONFIG_ITEM_UPGRADE_RANDOM_UPGRADES_CRAFTING,
    MAX_ITEM_UPGRADE_BOOL_CONFIGS
};

enum ItemUpgradeStringConfigs
{
    CONFIG_ITEM_UPGRADE_ALLOWED_STATS = 0,
    CONFIG_ITEM_UPGRADE_RANDOM_UPGRADES_LOGIN_MSG,
    MAX_ITEM_UPGRADE_STRING_CONFIGS
};

enum ItemUpgradeFloatConfigs
{
    CONFIG_ITEM_UPGRADE_RANDOM_UPGRADES_CHANCE = 0,
    MAX_ITEM_UPGRADE_FLOAT_CONFIGS
};

enum ItemUpgradeIntConfigs
{
    CONFIG_ITEM_UPGRADE_PURGE_TOKEN = 0,
    CONFIG_ITEM_UPGRADE_PURGE_TOKEN_COUNT,
    CONFIG_ITEM_UPGRADE_RANDOM_UPGRADES_MAX_STATS,
    CONFIG_ITEM_UPGRADE_RANDOM_UPGRADES_MAX_RANK,
    MAX_ITEM_UPGRADE_INT_CONFIGS
};

class ItemUpgradeConfig
{
private:
    bool boolConfigs[MAX_ITEM_UPGRADE_BOOL_CONFIGS];
    std::string stringConfigs[MAX_ITEM_UPGRADE_STRING_CONFIGS];
    float floatConfigs[MAX_ITEM_UPGRADE_FLOAT_CONFIGS];
    int32 intConfigs[MAX_ITEM_UPGRADE_INT_CONFIGS];
public:
    ItemUpgradeConfig();

    void Initialize();

    bool GetBoolConfig(ItemUpgradeBoolConfigs index) const;
    std::string GetStringConfig(ItemUpgradeStringConfigs index) const;
    float GetFloatConfig(ItemUpgradeFloatConfigs index) const;
    int32 GetIntConfig(ItemUpgradeIntConfigs index) const;
};

#endif
