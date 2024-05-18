/*
 * Credits: silviu20092
 */

#include "ScriptMgr.h"
#include "Config.h"
#include "item_upgrade.h"

class item_upgrade_worldscript : public WorldScript
{
public:
    item_upgrade_worldscript() : WorldScript("item_upgrade_worldscript",
        {
            WORLDHOOK_ON_AFTER_CONFIG_LOAD,
            WORLDHOOK_ON_BEFORE_WORLD_INITIALIZED
        }) {}

    void OnAfterConfigLoad(bool reload) override
    {
        if (reload)
        {
            ItemUpgrade::PagedDataMap& pagedData = sItemUpgrade->GetPagedDataMap();
            for (auto& itr : pagedData)
                itr.second.reloaded = true;

            sItemUpgrade->HandleDataReload(false);
        }

        sItemUpgrade->SetEnabled(sConfigMgr->GetOption<bool>("ItemUpgrade.Enable", true));
        sItemUpgrade->LoadAllowedStats(sConfigMgr->GetOption<std::string>("ItemUpgrade.AllowedStats", ItemUpgrade::DefaultAllowedStats));
        sItemUpgrade->SetSendItemPackets(sConfigMgr->GetOption<bool>("ItemUpgrade.SendUpgradedItemsPackets", false));
        sItemUpgrade->LoadPurgeConfig(sConfigMgr->GetOption<bool>("ItemUpgrade.AllowUpgradesPurge", false), sConfigMgr->GetOption<int32>("ItemUpgrade.UpgradePurgeToken", 0), sConfigMgr->GetOption<uint32>("ItemUpgrade.UpgradePurgeTokenCount", 1));
        sItemUpgrade->SetRandomUpgrades(sConfigMgr->GetOption<bool>("ItemUpgrade.RandomUpgradesOnLoot", false));
        sItemUpgrade->SetRandomUpgradesLoginMsg(sConfigMgr->GetOption<std::string>("ItemUpgrade.RandomUpgradesBroadcastLoginMsg", ""));
        sItemUpgrade->SetRandomUpgradeChance(sConfigMgr->GetOption<float>("ItemUpgrade.RandomUpgradeChance", 2.0f));
        sItemUpgrade->SetRandomUpgradeMaxStats(sConfigMgr->GetOption<int32>("ItemUpgrade.RandomUpgradeMaxStatCount", 2));
        sItemUpgrade->SetRandomUpgradeMaxRank(sConfigMgr->GetOption<int32>("ItemUpgrade.RandomUpgradeMaxRank", 3));

        if (reload)
            sItemUpgrade->HandleDataReload(true);
    }

    void OnBeforeWorldInitialized() override
    {
        sItemUpgrade->LoadFromDB();
    }
};

void AddSC_item_upgrade_worldscript()
{
    new item_upgrade_worldscript();
}
