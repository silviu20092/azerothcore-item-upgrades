/*
 * Credits: silviu20092
 */

#include "ScriptMgr.h"
#include "DatabaseEnv.h"
#include "item_upgrade.h"

class item_upgrade_playerscript : public PlayerScript
{
public:
    item_upgrade_playerscript() : PlayerScript("item_upgrade_playerscript") {}

    void OnAfterMoveItemFromInventory(Player* player, Item* it, uint8 /*bag*/, uint8 /*slot*/, bool /*update*/) override
    {
        sItemUpgrade->HandleItemRemove(player, it);
    }

    void OnDeleteFromDB(CharacterDatabaseTransaction trans, uint32 guid) override
    {
        trans->Append("DELETE FROM character_item_upgrade WHERE guid = {}", guid);
        sItemUpgrade->HandleCharacterRemove(guid);
    }
};

void AddSC_item_upgrade_playerscript()
{
    new item_upgrade_playerscript();
}
