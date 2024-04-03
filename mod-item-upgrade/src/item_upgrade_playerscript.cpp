/*
 * Credits: silviu20092
 */

#include "ScriptMgr.h"
#include "DatabaseEnv.h"
#include "Player.h"
#include "item_upgrade.h"

class item_upgrade_playerscript : public PlayerScript
{
private:
    class SendUpgradePackets : public BasicEvent
    {
    public:
        SendUpgradePackets(Player* player) : player(player)
        {
            player->m_Events.AddEvent(this, player->m_Events.CalculateTime(DELAY_MS));
        }

        bool Execute(uint64 /*e_time*/, uint32 /*p_time*/)
        {
            sItemUpgrade->UpdateVisualCache(player);
            return true;
        }
    private:
        static constexpr uint64 DELAY_MS = 3000;

        Player* player;
    };
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

    void OnLogin(Player* player) override
    {
        new SendUpgradePackets(player);
    }
};

void AddSC_item_upgrade_playerscript()
{
    new item_upgrade_playerscript();
}
