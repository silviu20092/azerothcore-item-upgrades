/*
 * Credits: silviu20092
 */

#include "ScriptMgr.h"
#include "ScriptedGossip.h"
#include "Creature.h"
#include "Player.h"
#include "Item.h"
#include "item_upgrade.h"

class npc_item_upgrade : public CreatureScript
{
private:
    bool CloseGossip(Player* player, bool retValue = true)
    {
        CloseGossipMenuFor(player);
        return retValue;
    }

    static Item* GetPagedDataItem(const ItemUpgrade::PagedData& pagedData, Player* player)
    {
        Item* item = player->GetItemByGuid(pagedData.item.guid);
        if (!sItemUpgrade->IsValidItemForUpgrade(item, player))
        {
            ItemUpgrade::SendMessage(player, "Item is no longer available for upgrade.");
            return nullptr;
        }

        return item;
    }
public:
    npc_item_upgrade() : CreatureScript("npc_item_upgrade")
    {
    }

    bool OnGossipHello(Player* player, Creature* creature) override
    {
        if (sItemUpgrade->GetReloading())
        {
            ItemUpgrade::SendMessage(player, "Item Upgrade data is being reloaded by an administrator, please retry.");
            return CloseGossip(player);
        }

        sItemUpgrade->GetPagedData(player).reloaded = false;

        if (!sItemUpgrade->GetBoolConfig(CONFIG_ITEM_UPGRADE_ENABLED))
            AddGossipItemFor(player, GOSSIP_ICON_CHAT, "|cffb50505NOT AVAILABLE|r", GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF);
        else
        {
            AddGossipItemFor(player, GOSSIP_ICON_MONEY_BAG, "Choose an item to upgrade (by stat, one-by-one)", GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 2);
            AddGossipItemFor(player, GOSSIP_ICON_MONEY_BAG, "Choose an item to upgrade (all stats at once)", GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 7);
            if (sItemUpgrade->GetBoolConfig(CONFIG_ITEM_UPGRADE_ALLOW_PURGE))
                AddGossipItemFor(player, GOSSIP_ICON_INTERACT_1, "Purge upgrades", GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 6);
            AddGossipItemFor(player, GOSSIP_ICON_INTERACT_1, "See upgraded items", GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 3);
            AddGossipItemFor(player, GOSSIP_ICON_BATTLE, "Update visual cache", GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 4);

            if (player->GetSession()->GetSecurity() == SEC_ADMINISTRATOR)
                AddGossipItemFor(player, GOSSIP_ICON_INTERACT_1, "Lock for database edit", GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 5, "The NPC will no longer be available to players until you release the lock with .item_upgrade reload command.", 0, false);
        }
        AddGossipItemFor(player, GOSSIP_ICON_CHAT, "Nevermind...", GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 1);
        SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, creature->GetGUID());
        return true;
    }

    bool OnGossipSelect(Player* player, Creature* creature, uint32 sender, uint32 action) override
    {
        ItemUpgrade::PagedData& pagedData = sItemUpgrade->GetPagedData(player);
        if (sItemUpgrade->GetReloading() || pagedData.reloaded)
        {
            ItemUpgrade::SendMessage(player, "Item Upgrade data is being reloaded by an administrator, please retry.");
            return CloseGossip(player, false);
        }

        if (sender == GOSSIP_SENDER_MAIN)
        {
            if (action == GOSSIP_ACTION_INFO_DEF)
            {
                ClearGossipMenuFor(player);
                return OnGossipHello(player, creature);
            }
            else if (action == GOSSIP_ACTION_INFO_DEF + 1)
                return CloseGossip(player);
            else if (action == GOSSIP_ACTION_INFO_DEF + 2)
            {
                sItemUpgrade->BuildUpgradableItemCatalogue(player, ItemUpgrade::PAGED_DATA_TYPE_ITEMS);
                return sItemUpgrade->AddPagedData(player, creature, 0);
            }
            else if (action == GOSSIP_ACTION_INFO_DEF + 3)
            {
                sItemUpgrade->BuildAlreadyUpgradedItemsCatalogue(player, ItemUpgrade::PAGED_DATA_TYPE_UPGRADED_ITEMS);
                return sItemUpgrade->AddPagedData(player, creature, 0);
            }
            else if (action == GOSSIP_ACTION_INFO_DEF + 4)
            {
                sItemUpgrade->UpdateVisualCache(player);
                sItemUpgrade->VisualFeedback(player);
                return CloseGossip(player);
            }
            else if (action == GOSSIP_ACTION_INFO_DEF + 5)
            {
                sItemUpgrade->SetReloading(true);
                return CloseGossip(player);
            }
            else if (action == GOSSIP_ACTION_INFO_DEF + 6)
            {
                sItemUpgrade->BuildAlreadyUpgradedItemsCatalogue(player, ItemUpgrade::PAGED_DATA_TYPE_ITEMS_FOR_PURGE);
                return sItemUpgrade->AddPagedData(player, creature, 0);
            }
            else if (action == GOSSIP_ACTION_INFO_DEF + 7)
            {
                sItemUpgrade->BuildUpgradableItemCatalogue(player, ItemUpgrade::PAGED_DATA_TYPE_ITEMS_BULK);
                return sItemUpgrade->AddPagedData(player, creature, 0);
            }
        }
        else if (sender == GOSSIP_SENDER_MAIN + 1)
        {
            uint32 id = action - GOSSIP_ACTION_INFO_DEF;
            return sItemUpgrade->TakePagedDataAction(player, creature, id);
        }
        else if (sender == GOSSIP_SENDER_MAIN + 2)
        {
            uint32 page = action - GOSSIP_ACTION_INFO_DEF;
            return sItemUpgrade->AddPagedData(player, creature, page);
        }
        else if (sender == GOSSIP_SENDER_MAIN + 9)
        {
            sItemUpgrade->BuildUpgradableItemCatalogue(player, ItemUpgrade::PAGED_DATA_TYPE_ITEMS);
            return sItemUpgrade->AddPagedData(player, creature, 0);
        }
        else if (sender == GOSSIP_SENDER_MAIN + 10)
        {
            Item* item = GetPagedDataItem(pagedData, player);
            if (item == nullptr)
                return CloseGossip(player, false);

            sItemUpgrade->BuildStatsUpgradeCatalogue(player, item);
            return sItemUpgrade->AddPagedData(player, creature, 0);
        }
        else if (sender == GOSSIP_SENDER_MAIN + 11)
        {
            sItemUpgrade->BuildAlreadyUpgradedItemsCatalogue(player, ItemUpgrade::PAGED_DATA_TYPE_UPGRADED_ITEMS);
            return sItemUpgrade->AddPagedData(player, creature, 0);
        }
        else if (sender == GOSSIP_SENDER_MAIN + 12)
        {
            sItemUpgrade->BuildUpgradableItemCatalogue(player, ItemUpgrade::PAGED_DATA_TYPE_ITEMS_BULK);
            return sItemUpgrade->AddPagedData(player, creature, 0);
        }
        else if (sender == GOSSIP_SENDER_MAIN + 13)
        {
            Item* item = GetPagedDataItem(pagedData, player);
            if (item == nullptr)
                return CloseGossip(player, false);

            sItemUpgrade->BuildStatsUpgradeCatalogueBulk(player, item);
            return sItemUpgrade->AddPagedData(player, creature, 0);
        }
        else if (sender == GOSSIP_SENDER_MAIN + 14)
        {
            Item* item = GetPagedDataItem(pagedData, player);
            if (item == nullptr)
                return CloseGossip(player, false);

            sItemUpgrade->BuildStatsUpgradeByPctCatalogueBulk(player, item, pagedData.pct);
            return sItemUpgrade->AddPagedData(player, creature, 0);
        }

        return false;
    }
};

void AddSC_npc_item_upgrade()
{
    new npc_item_upgrade();
}
