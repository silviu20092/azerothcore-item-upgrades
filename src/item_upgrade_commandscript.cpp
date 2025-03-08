/*
 * Credits: silviu20092
 */

#include "ScriptMgr.h"
#include "Chat.h"
#include "CommandScript.h"
#include "item_upgrade.h"

using namespace Acore::ChatCommands;

class item_upgrade_commandscript : public CommandScript
{
private:
    static std::unordered_map<uint32, uint32> cmdListUpgradesTimerMap;
    static constexpr uint32 listUpgradesDiffTimer = 10000;
public:
    item_upgrade_commandscript() : CommandScript("item_upgrade_commandscript") { }

    ChatCommandTable GetCommands() const override
    {
        static ChatCommandTable itemUpgradeSubcommandTable =
        {
            { "reload", HandleReloadModItemUpgrade, SEC_ADMINISTRATOR, Console::Yes },
            { "lock",   HandleLockItemUpgrade,      SEC_ADMINISTRATOR, Console::Yes },
            { "list",   HandleListUpgrades,         SEC_PLAYER,        Console::No  }
        };

        static ChatCommandTable itemUpgradeCommandTable =
        {
            { "item_upgrade", itemUpgradeSubcommandTable }
        };

        return itemUpgradeCommandTable;
    }
private:
    static bool HandleReloadModItemUpgrade(ChatHandler* handler)
    {
        ItemUpgrade::PagedDataMap& pagedData = sItemUpgrade->GetPagedDataMap();
        for (auto& itr : pagedData)
            itr.second.reloaded = true;

        sItemUpgrade->SetReloading(true);
        sItemUpgrade->HandleDataReload(false);

        sItemUpgrade->LoadFromDB(true);

        sItemUpgrade->HandleDataReload(true);
        sItemUpgrade->SetReloading(false);

        handler->SendGlobalGMSysMessage("Item Upgrade module data successfully reloaded.");
        return true;
    }

    static bool HandleLockItemUpgrade(ChatHandler* handler)
    {
        sItemUpgrade->SetReloading(true);
        handler->SendSysMessage("Item Upgrade NPC is now locked, it is now safe to edit database tables. Release the lock by using .item_upgrade reload command");
        return true;
    }

    static bool HandleListUpgrades(ChatHandler* handler, Optional<PlayerIdentifier> target)
    {
        if (!target)
            target = PlayerIdentifier::FromTargetOrSelf(handler);

        if (!target)
            return false;

        Player* player = target->GetConnectedPlayer();

        uint32 currentTime = getMSTime();
        uint32 lastTime = cmdListUpgradesTimerMap[player->GetGUID().GetCounter()];
        uint32 diff = getMSTimeDiff(lastTime, currentTime);
        if (lastTime > 0 && diff < listUpgradesDiffTimer)
        {
            handler->PSendSysMessage("Please try again in {} seconds.", (listUpgradesDiffTimer - diff) / 1000);
            return true;
        }
        cmdListUpgradesTimerMap[player->GetGUID().GetCounter()] = currentTime;

        uint32 upgradedItems = 0;
        uint32 upgradedStats = 0;
        uint32 weaponUpgrades = 0;
        for (uint8 i = EQUIPMENT_SLOT_START; i < EQUIPMENT_SLOT_END; i++)
        {
            if (const Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, i))
            {
                std::vector<const ItemUpgrade::UpgradeStat*> upgrades = sItemUpgrade->FindUpgradesForItem(player, item);
                const ItemUpgrade::UpgradeStat* weaponUpgrade = sItemUpgrade->FindUpgradeForWeapon(player, item);

                if (!upgrades.empty() || weaponUpgrade != nullptr)
                {
                    upgradedItems++;
                    std::string slot = ItemUpgrade::EquipmentSlotToString((EquipmentSlots)i);
                    handler->PSendSysMessage("{} [{}]", ItemUpgrade::ItemLink(player, item), slot);
                    if (!upgrades.empty())
                    {
                        upgradedStats += upgrades.size();
                        std::vector<_ItemStat> statInfo = ItemUpgrade::LoadItemStatInfo(item);
                        handler->PSendSysMessage("Found {} stat upgrades:", upgrades.size());
                        for (const auto* stat : upgrades)
                        {
                            const _ItemStat* foundStat = ItemUpgrade::GetStatByType(statInfo, stat->statType);
                            ASSERT(foundStat != nullptr);
                            std::ostringstream oss;
                            oss << "|cffb50505" << foundStat->ItemStatValue << "|r --> ";
                            oss << "|cff056e3a" << ItemUpgrade::CalculateModPct(foundStat->ItemStatValue, stat) << "|r";
                            std::ostringstream statusOss;
                            if (sItemUpgrade->IsInactiveStatUpgrade(item, stat))
                                statusOss << "|cffb50505INACTIVE|r";
                            else
                                statusOss << "|cff056e3aACTIVE|r";
                            handler->PSendSysMessage("{} increased by {}% [RANK {}] [{}] [{}]", ItemUpgrade::StatTypeToString(stat->statType), stat->statModPct, stat->statRank, oss.str(), statusOss.str());
                        }
                    }
                    if (weaponUpgrade != nullptr)
                    {
                        weaponUpgrades++;
                        std::pair<float, float> dmgInfo = ItemUpgrade::GetItemProtoDamage(item);
                        float upgradedMinDamage = std::floor(ItemUpgrade::CalculateModPctF(dmgInfo.first, weaponUpgrade));
                        float upgradedMaxDamage = std::ceil(ItemUpgrade::CalculateModPctF(dmgInfo.second, weaponUpgrade));

                        std::ostringstream statusOss;
                        if (sItemUpgrade->IsInactiveWeaponUpgrade())
                            statusOss << "|cffb50505INACTIVE|r";
                        else
                            statusOss << "|cff056e3aACTIVE|r";

                        handler->PSendSysMessage("This weapon is upgraded by {}%, [MIN DAMAGE {}], [MAX DAMAGE {}] [{}]",
                            ItemUpgrade::FormatFloat(weaponUpgrade->statModPct),
                            ItemUpgrade::FormatIncrease(dmgInfo.first, upgradedMinDamage),
                            ItemUpgrade::FormatIncrease(dmgInfo.second, upgradedMaxDamage),
                            statusOss.str());
                    }
                    handler->SendSysMessage("--------------- NEXT ITEM OR END ---------------");
                }
            }
        }

        if (upgradedItems == 0 && weaponUpgrades == 0)
            handler->PSendSysMessage("{} does not have any upgrades.", player->GetPlayerName());
        else
            handler->PSendSysMessage("{} has a total of: {} upgraded item(s), {} upgraded stat(s), {} upgraded weapon(s).", player->GetPlayerName(), upgradedItems, upgradedStats, weaponUpgrades);

        return true;
    }
};

std::unordered_map<uint32, uint32> item_upgrade_commandscript::cmdListUpgradesTimerMap;

void AddSC_item_upgrade_commandscript()
{
    new item_upgrade_commandscript();
}
