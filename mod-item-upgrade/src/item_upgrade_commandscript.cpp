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
public:
    item_upgrade_commandscript() : CommandScript("item_upgrade_commandscript") { }

    ChatCommandTable GetCommands() const override
    {
        static ChatCommandTable reloadCommandTable =
        {
            { "reload", HandleReloadModItemUpgrade, SEC_ADMINISTRATOR, Console::Yes }
        };

        static ChatCommandTable itemUpgradeCommandTable =
        {
            { "item_upgrade", reloadCommandTable }
        };

        return itemUpgradeCommandTable;
    }
private:
    static bool HandleReloadModItemUpgrade(ChatHandler* handler)
    {
        ItemUpgrade::PagedDataMap& pagedData = sItemUpgrade->GetPagedDataMap();
        for (auto& itr : pagedData)
            itr.second.reloaded = true;

        sItemUpgrade->LoadFromDB();

        handler->SendGlobalGMSysMessage("Item Upgrade module data successfully reloaded.");
        return true;
    }
};

void AddSC_item_upgrade_commandscript()
{
    new item_upgrade_commandscript();
}
