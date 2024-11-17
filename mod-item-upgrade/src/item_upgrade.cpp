/*
 * Credits: silviu20092
 */

#include <numeric>
#include <iomanip>
#include <cmath>
#include "Item.h"
#include "Config.h"
#include "Tokenize.h"
#include "StringConvert.h"
#include "DatabaseEnv.h"
#include "Log.h"
#include "Player.h"
#include "ScriptedGossip.h"
#include "Chat.h"
#include "SpellMgr.h"
#include "item_upgrade.h"

ItemUpgrade::ItemUpgrade()
{
    reloading = false;
}

ItemUpgrade::~ItemUpgrade()
{
    for (auto& pageData : playerPagedData)
        pageData.second.Reset();
}

ItemUpgrade* ItemUpgrade::instance()
{
    static ItemUpgrade instance;
    return &instance;
}

bool ItemUpgrade::IsAllowedStatType(uint32 statType) const
{
    return FindInContainer(allowedStats, statType) != nullptr;
}

void ItemUpgrade::LoadAllowedStats(const std::string& stats)
{
    allowedStats.clear();
    std::vector<std::string_view> tokenized = Acore::Tokenize(stats, ',', false);
    std::transform(tokenized.begin(), tokenized.end(), std::back_inserter(allowedStats),
        [](const std::string_view& str) { return *Acore::StringTo<uint32>(str); });
}

bool ItemUpgrade::GetBoolConfig(ItemUpgradeBoolConfigs index) const
{
    return cfg.GetBoolConfig(index);
}

std::string ItemUpgrade::GetStringConfig(ItemUpgradeStringConfigs index) const
{
    return cfg.GetStringConfig(index);
}

float ItemUpgrade::GetFloatConfig(ItemUpgradeFloatConfigs index) const
{
    return cfg.GetFloatConfig(index);
}

int32 ItemUpgrade::GetIntConfig(ItemUpgradeIntConfigs index) const
{
    return cfg.GetIntConfig(index);
}

void ItemUpgrade::LoadConfig(bool reload)
{
    cfg.Initialize();
    LoadAllowedStats(cfg.GetStringConfig(CONFIG_ITEM_UPGRADE_ALLOWED_STATS));
    LoadWeaponUpgradePercents(cfg.GetStringConfig(CONFIG_ITEM_UPGRADE_WEAPON_DAMAGE_PERCENTS));
    if (reload)
        BuildWeaponUpgradeReqs();
}

void ItemUpgrade::LoadFromDB(bool reload)
{
    LOG_INFO("server.loading", " ");
    LOG_INFO("server.loading", "Loading item upgrade mod custom tables...");

    CleanupDB(reload);

    LoadAllowedItems();
    LoadBlacklistedItems();
    LoadAllowedStatsItems();
    LoadBlacklistedStatsItems();
    LoadStatRequirements();
    LoadStatRequirementsOverrides();

    LoadUpgradeStats();
    if (!CheckDataValidity())
    {
        LOG_ERROR("server.loading", "Found data validity errors while loading item upgrade mod tables. Check the FATAL error messages and fix the issues before attempting to restart the server");
        World::StopNow(ERROR_EXIT_CODE);
        return;
    }

    LoadCharacterUpgradeData();

    LoadCharacterWeaponUpgradeData();

    CreateUpgradesPctMap();
}

void ItemUpgrade::LoadAllowedItems()
{
    allowedItems.clear();

    QueryResult result = CharacterDatabase.Query("SELECT entry FROM mod_item_upgrade_allowed_items");
    if (!result)
        return;

    do
    {
        Field* fields = result->Fetch();

        uint32 entry = fields[0].Get<uint32>();
        const ItemTemplate* itemTemplate = sObjectMgr->GetItemTemplate(entry);
        if (!itemTemplate)
        {
            LOG_ERROR("sql.sql", "Table `mod_item_upgrade_allowed_items` has invalid item entry {}, skip", entry);
            continue;
        }

        allowedItems.insert(entry);
    } while (result->NextRow());
}

void ItemUpgrade::LoadAllowedStatsItems()
{
    allowedStatItems.clear();

    QueryResult result = CharacterDatabase.Query("SELECT stat_id, entry FROM mod_item_upgrade_allowed_stats_items");
    if (!result)
        return;

    do
    {
        Field* fields = result->Fetch();

        uint32 stat_id = fields[0].Get<uint32>();
        uint32 entry = fields[1].Get<uint32>();
        const ItemTemplate* itemTemplate = sObjectMgr->GetItemTemplate(entry);
        if (!itemTemplate)
        {
            LOG_ERROR("sql.sql", "Table `mod_item_upgrade_allowed_stats_items` has invalid item entry {}, skip", entry);
            continue;
        }

        allowedStatItems[stat_id].insert(entry);
    } while (result->NextRow());
}

void ItemUpgrade::LoadBlacklistedItems()
{
    blacklistedItems.clear();

    QueryResult result = CharacterDatabase.Query("SELECT entry FROM mod_item_upgrade_blacklisted_items");
    if (!result)
        return;

    do
    {
        Field* fields = result->Fetch();

        uint32 entry = fields[0].Get<uint32>();
        const ItemTemplate* itemTemplate = sObjectMgr->GetItemTemplate(entry);
        if (!itemTemplate)
        {
            LOG_ERROR("sql.sql", "Table `mod_item_upgrade_blacklisted_items` has invalid item entry {}, skip", entry);
            continue;
        }

        blacklistedItems.insert(entry);
    } while (result->NextRow());
}

void ItemUpgrade::LoadBlacklistedStatsItems()
{
    blacklistedStatItems.clear();

    QueryResult result = CharacterDatabase.Query("SELECT stat_id, entry FROM mod_item_upgrade_blacklisted_stats_items");
    if (!result)
        return;

    do
    {
        Field* fields = result->Fetch();

        uint32 stat_id = fields[0].Get<uint32>();
        uint32 entry = fields[1].Get<uint32>();
        const ItemTemplate* itemTemplate = sObjectMgr->GetItemTemplate(entry);
        if (!itemTemplate)
        {
            LOG_ERROR("sql.sql", "Table `mod_item_upgrade_blacklisted_stats_items` has invalid item entry {}, skip", entry);
            continue;
        }

        blacklistedStatItems[stat_id].insert(entry);
    } while (result->NextRow());
}

void ItemUpgrade::CleanupDB(bool reload)
{
    CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
    trans->Append("DELETE FROM mod_item_upgrade_stats_req WHERE stat_id NOT IN (SELECT id FROM mod_item_upgrade_stats)");
    trans->Append("DELETE FROM mod_item_upgrade_stats_req_override WHERE stat_id NOT IN (SELECT id FROM mod_item_upgrade_stats)");
    trans->Append("DELETE FROM character_item_upgrade WHERE stat_id NOT IN (SELECT id FROM mod_item_upgrade_stats)");
    if (!reload)
    {
        trans->Append("DELETE FROM character_item_upgrade WHERE NOT EXISTS (SELECT 1 FROM item_instance WHERE item_instance.guid = character_item_upgrade.item_guid)");
        trans->Append("DELETE FROM character_weapon_upgrade WHERE NOT EXISTS (SELECT 1 FROM item_instance WHERE item_instance.guid = character_weapon_upgrade.item_guid)");
    }
    trans->Append("DELETE FROM mod_item_upgrade_allowed_stats_items WHERE stat_id NOT IN (SELECT id FROM mod_item_upgrade_stats)");
    trans->Append("DELETE FROM mod_item_upgrade_blacklisted_stats_items WHERE stat_id NOT IN (SELECT id FROM mod_item_upgrade_stats)");
    CharacterDatabase.DirectCommitTransaction(trans);
}

void ItemUpgrade::MergeStatRequirements(std::unordered_map<uint32, StatRequirementContainer>& statRequirementMap, bool validate)
{
    for (auto& statPair : statRequirementMap)
    {
        StatRequirementContainer newStatReq;

        float copperTotal = std::accumulate(statPair.second.begin(), statPair.second.end(), 0.0f,
            [](float a, const UpgradeStatReq& req) { return a + (req.reqType == REQ_TYPE_COPPER ? req.reqVal1 : 0.0f); });
        if (copperTotal > 0.0f)
        {
            int32 val = static_cast<int32>(copperTotal);
            if (validate && (val < 1 || val > MAX_MONEY_AMOUNT))
                LOG_ERROR("sql.sql", "Stat requirement has invalid total copper amount for stat id {}, skip", statPair.first);
            else
                newStatReq.push_back(UpgradeStatReq(statPair.first, REQ_TYPE_COPPER, copperTotal));
        }

        float honorTotal = std::accumulate(statPair.second.begin(), statPair.second.end(), 0.0f,
            [](float a, const UpgradeStatReq& req) { return a + (req.reqType == REQ_TYPE_HONOR ? req.reqVal1 : 0.0f); });
        if (honorTotal > 0.0f)
        {
            int32 val = static_cast<int32>(honorTotal);
            if (validate && (val < 1 || val > sWorld->getIntConfig(CONFIG_MAX_HONOR_POINTS)))
                LOG_ERROR("sql.sql", "Stat requirement has invalid total honor points for stat id {}, skip", statPair.first);
            else
                newStatReq.push_back(UpgradeStatReq(statPair.first, REQ_TYPE_HONOR, honorTotal));
        }

        float arenaTotal = std::accumulate(statPair.second.begin(), statPair.second.end(), 0.0f,
            [](float a, const UpgradeStatReq& req) { return a + (req.reqType == REQ_TYPE_ARENA ? req.reqVal1 : 0.0f); });
        if (arenaTotal > 0.0f)
        {
            int32 val = static_cast<int32>(arenaTotal);
            if (validate && (val < 1 || val > sWorld->getIntConfig(CONFIG_MAX_ARENA_POINTS)))
                LOG_ERROR("sql.sql", "Stat requirement has invalid total arena points for stat id {}, skip", statPair.first);
            else
                newStatReq.push_back(UpgradeStatReq(statPair.first, REQ_TYPE_ARENA, arenaTotal));
        }

        std::unordered_map<uint32, uint32> itemCountMap;
        for (const UpgradeStatReq& req : statPair.second)
        {
            if (req.reqType != REQ_TYPE_ITEM)
                continue;

            itemCountMap[(uint32)req.reqVal1] += (uint32)req.reqVal2;
        }
        if (!itemCountMap.empty())
        {
            for (const auto& itemPair : itemCountMap)
                newStatReq.push_back(UpgradeStatReq(statPair.first, REQ_TYPE_ITEM, itemPair.first, itemPair.second));
        }

        StatRequirementContainer::const_iterator citer = std::find_if(statPair.second.begin(), statPair.second.end(),
            [&](const UpgradeStatReq& req) { return req.reqType == REQ_TYPE_NONE; });
        if (citer != statPair.second.end()) {
            newStatReq.push_back(UpgradeStatReq(statPair.first, REQ_TYPE_NONE));
        }

        statPair.second = newStatReq;
    }
}

void ItemUpgrade::LoadStatRequirements()
{
    baseStatRequirements.clear();

    QueryResult result = CharacterDatabase.Query("SELECT id, stat_id, req_type, req_val1, req_val2 FROM mod_item_upgrade_stats_req");
    if (!result)
        return;

    do
    {
        Field* fields = result->Fetch();

        uint32 statId = fields[1].Get<uint32>();
        uint8 reqType = fields[2].Get<uint8>();
        if (!IsValidReqType(reqType))
        {
            LOG_ERROR("sql.sql", "Table `mod_item_upgrade_stats_req` has invalid `req_type` {}, skip", reqType);
            continue;
        }
        float reqVal1 = fields[3].Get<float>();
        float reqVal2 = fields[4].Get<float>();
        if (!ValidateReq(fields[0].Get<uint32>(), (UpgradeStatReqType)reqType, reqVal1, reqVal2, "mod_item_upgrade_stats_req"))
            continue;

        UpgradeStatReq statReq;
        statReq.statId = statId;
        statReq.reqType = (UpgradeStatReqType)reqType;
        statReq.reqVal1 = reqVal1;
        statReq.reqVal2 = reqVal2;
        baseStatRequirements[statId].push_back(statReq);
    } while (result->NextRow());

    MergeStatRequirements(baseStatRequirements);
}

void ItemUpgrade::LoadStatRequirementsOverrides()
{
    overrideStatRequirements.clear();

    QueryResult result = CharacterDatabase.Query("SELECT id, stat_id, item_entry, req_type, req_val1, req_val2 FROM mod_item_upgrade_stats_req_override");
    if (!result)
        return;

    do
    {
        Field* fields = result->Fetch();

        uint32 statId = fields[1].Get<uint32>();
        uint8 reqType = fields[3].Get<uint8>();
        if (!IsValidReqType(reqType))
        {
            LOG_ERROR("sql.sql", "Table `mod_item_upgrade_stats_req_override` has invalid `req_type` {}, skip", reqType);
            continue;
        }
        uint32 entry = fields[2].Get<uint32>();
        const ItemTemplate* proto = sObjectMgr->GetItemTemplate(entry);
        if (proto == nullptr) {
            LOG_ERROR("sql.sql", "Table `mod_item_upgrade_stats_req_override` has invalid `item_entry` {}, skip", entry);
            continue;
        }
        float reqVal1 = fields[4].Get<float>();
        float reqVal2 = fields[5].Get<float>();
        if (!ValidateReq(fields[0].Get<uint32>(), (UpgradeStatReqType)reqType, reqVal1, reqVal2, "mod_item_upgrade_stats_req_override"))
            continue;

        UpgradeStatReq statReq;
        statReq.statId = statId;
        statReq.reqType = (UpgradeStatReqType)reqType;
        statReq.reqVal1 = reqVal1;
        statReq.reqVal2 = reqVal2;

        overrideStatRequirements[entry][statId].push_back(statReq);
    } while (result->NextRow());

    for (auto& pair : overrideStatRequirements)
        MergeStatRequirements(pair.second);
}

void ItemUpgrade::LoadUpgradeStats()
{
    upgradeStatList.clear();

    QueryResult result = CharacterDatabase.Query("SELECT id, stat_type, stat_mod_pct, stat_rank FROM mod_item_upgrade_stats");
    if (!result)
        return;

    do
    {
        Field* fields = result->Fetch();

        uint32 id = fields[0].Get<uint32>();
        uint32 statType = fields[1].Get<uint32>();
        float statModPct = fields[2].Get<float>();
        uint16 statRank = fields[3].Get<uint16>();

        UpgradeStat upgradeStat;
        upgradeStat.statId = id;
        upgradeStat.statType = statType;
        upgradeStat.statModPct = statModPct;
        upgradeStat.statRank = statRank;
        upgradeStatList.push_back(upgradeStat);
    } while (result->NextRow());
}

void ItemUpgrade::LoadCharacterUpgradeData()
{
    characterUpgradeData.clear();

    uint32 oldMSTime = getMSTime();

    QueryResult result = CharacterDatabase.Query("SELECT guid, item_guid, stat_id FROM character_item_upgrade");
    if (!result)
    {
        LOG_INFO("server.loading", ">> Loaded 0 character item upgrades.");
        LOG_INFO("server.loading", " ");
        return;
    }

    uint32 count = 0;
    do
    {
        Field* fields = result->Fetch();

        uint32 guidLow = fields[0].Get<uint32>();
        ObjectGuid itemGuid = ObjectGuid::Create<HighGuid::Item>(fields[1].Get<uint32>());
        uint32 statId = fields[2].Get<uint32>();

        CharacterUpgrade characterUpgrade;
        characterUpgrade.guid = guidLow;
        characterUpgrade.itemGuid = itemGuid;
        characterUpgrade.upgradeStat = FindUpgradeStat(statId);
        if (characterUpgrade.upgradeStat == nullptr)
        {
            LOG_ERROR("sql.sql", "Table `character_item_upgrade` has invalid `stat_id` {}, this should never happen, skip", statId);
            continue;
        }
        characterUpgradeData[guidLow].push_back(characterUpgrade);
        count++;
    } while (result->NextRow());

    LOG_INFO("server.loading", ">> Loaded {} character item upgrades in {} ms", count, GetMSTimeDiffToNow(oldMSTime));
    LOG_INFO("server.loading", " ");
}

void ItemUpgrade::LoadCharacterWeaponUpgradeData()
{
    characterWeaponUpgradeData.clear();

    uint32 oldMSTime = getMSTime();

    QueryResult result = CharacterDatabase.Query("SELECT guid, item_guid, upgrade_perc FROM character_weapon_upgrade");
    if (!result)
    {
        LOG_INFO("server.loading", ">> Loaded 0 character weapon item upgrades.");
        LOG_INFO("server.loading", " ");
        return;
    }

    uint32 count = 0;
    do
    {
        Field* fields = result->Fetch();

        uint32 guidLow = fields[0].Get<uint32>();
        ObjectGuid itemGuid = ObjectGuid::Create<HighGuid::Item>(fields[1].Get<uint32>());
        float perc = fields[2].Get<float>();

        CharacterUpgrade characterUpgrade;
        characterUpgrade.guid = guidLow;
        characterUpgrade.itemGuid = itemGuid;
        characterUpgrade.upgradeStat = FindWeaponUpgradeStat(perc);
        if (characterUpgrade.upgradeStat == nullptr)
        {
            characterUpgrade.upgradeStat = FindNearestWeaponUpgradeStat(perc);
            if (characterUpgrade.upgradeStat == nullptr) {
                LOG_ERROR("sql.sql", "Table `character_weapon_upgrade` has invalid `upgrade_perc` {}, there is no other near percent that can be chosen, skip", perc);
                continue;
            }
            else
                LOG_INFO("sql.sql", "Table `character_weapon_upgrade` has invalid `upgrade_perc` {} but a near percentage was chosen: {}", perc, characterUpgrade.upgradeStat->statModPct);
        }
        characterUpgrade.upgradeStatModPct = perc;
        characterWeaponUpgradeData[guidLow].push_back(characterUpgrade);
        count++;
    } while (result->NextRow());

    LOG_INFO("server.loading", ">> Loaded {} character weapon item upgrades in {} ms", count, GetMSTimeDiffToNow(oldMSTime));
    LOG_INFO("server.loading", " ");
}

bool ItemUpgrade::IsValidReqType(uint8 reqType) const
{
    return reqType >= REQ_TYPE_COPPER && reqType < MAX_REQ_TYPE;
}

bool ItemUpgrade::ValidateReq(uint32 id, UpgradeStatReqType reqType, float val1, float val2, const std::string& table) const
{
    int32 val1Int = static_cast<int32>(val1);
    switch (reqType)
    {
        case ItemUpgrade::REQ_TYPE_COPPER:
            if (val1Int >= 1 && val1Int <= MAX_MONEY_AMOUNT)
                return true;
            LOG_ERROR("sql.sql", "Table `{}` has invalid `req_val1` {} (copper amount) for `id` {}, skip", table, val1, id);
            return false;
        case ItemUpgrade::REQ_TYPE_HONOR:
            if (val1Int >= 1 && val1Int <= sWorld->getIntConfig(CONFIG_MAX_HONOR_POINTS))
                return true;
            LOG_ERROR("sql.sql", "Table `{}` has invalid `req_val1` {} (honor points) for `id` {}, skip", table, val1, id);
            return false;
        case ItemUpgrade::REQ_TYPE_ARENA:
            if (val1Int >= 1 && val1Int <= sWorld->getIntConfig(CONFIG_MAX_ARENA_POINTS))
                return true;
            LOG_ERROR("sql.sql", "Table `{}` has invalid `req_val1` {} (arena points) for `id` {}, skip", table, val1, id);
            return false;
        case ItemUpgrade::REQ_TYPE_ITEM:
        {
            const ItemTemplate* itemTemplate = sObjectMgr->GetItemTemplate(val1Int);
            if (!itemTemplate)
            {
                LOG_ERROR("sql.sql", "Table `{}` has invalid `req_val1` {} (item entry not found) for `id` {}, skip", table, val1, id);
                return false;
            }
            int32 val2Int = static_cast<int32>(val2);
            if (val2Int >= 1)
                return true;
            LOG_ERROR("sql.sql", "Table `{}` has invalid `req_val2` {} (item count invalid) for `id` {}, skip", table, val2, id);
            return false;
        }
        case ItemUpgrade::REQ_TYPE_NONE:
            return true;
    }
    return false;
}

/*static*/ std::string ItemUpgrade::ItemIcon(const ItemTemplate* proto, uint32 width, uint32 height, int x, int y)
{
    std::ostringstream ss;
    ss << "|TInterface";
    const ItemDisplayInfoEntry* dispInfo = nullptr;
    if (proto)
    {
        dispInfo = sItemDisplayInfoStore.LookupEntry(proto->DisplayInfoID);
        if (dispInfo)
            ss << "/ICONS/" << dispInfo->inventoryIcon;
    }
    if (!dispInfo)
        ss << "/InventoryItems/WoWUnknownItem01";
    ss << ":" << width << ":" << height << ":" << x << ":" << y << "|t";
    return ss.str();
}

/*static*/ std::string ItemUpgrade::ItemIcon(const ItemTemplate* proto)
{
    return ItemIcon(proto, 30, 30, 0, 0);
}

/*static*/ std::string ItemUpgrade::ItemNameWithLocale(const Player* player, const ItemTemplate* itemTemplate, int32 randomPropertyId)
{
    LocaleConstant loc_idx = player->GetSession()->GetSessionDbLocaleIndex();
    std::string name = itemTemplate->Name1;
    if (ItemLocale const* il = sObjectMgr->GetItemLocale(itemTemplate->ItemId))
        ObjectMgr::GetLocaleString(il->Name, loc_idx, name);

    std::array<char const*, 16> const* suffix = nullptr;
    if (randomPropertyId < 0)
    {
        if (const ItemRandomSuffixEntry* itemRandEntry = sItemRandomSuffixStore.LookupEntry(-randomPropertyId))
            suffix = &itemRandEntry->Name;
    }
    else
    {
        if (const ItemRandomPropertiesEntry* itemRandEntry = sItemRandomPropertiesStore.LookupEntry(randomPropertyId))
            suffix = &itemRandEntry->Name;
    }
    if (suffix)
    {
        std::string_view test((*suffix)[(name != itemTemplate->Name1) ? loc_idx : DEFAULT_LOCALE]);
        if (!test.empty())
        {
            name += ' ';
            name += test;
        }
    }

    return name;
}

/*static*/ std::string ItemUpgrade::ItemLink(const Player* player, const ItemTemplate* itemTemplate, int32 randomPropertyId)
{
    std::stringstream oss;
    oss << "|c";
    oss << std::hex << ItemQualityColors[itemTemplate->Quality] << std::dec;
    oss << "|Hitem:";
    oss << itemTemplate->ItemId;
    oss << ":0:0:0:0:0:0:0:0:0|h[";
    oss << ItemNameWithLocale(player, itemTemplate, randomPropertyId);
    oss << "]|h|r";

    return oss.str();
}

/*static*/ std::string ItemUpgrade::ItemLink(const Player* player, const Item* item)
{
    const ItemTemplate* itemTemplate = item->GetTemplate();
    std::stringstream oss;
    oss << "|c";
    oss << std::hex << ItemQualityColors[itemTemplate->Quality] << std::dec;
    oss << "|Hitem:";
    oss << itemTemplate->ItemId;
    oss << ":" << item->GetEnchantmentId(PERM_ENCHANTMENT_SLOT);
    oss << ":" << item->GetEnchantmentId(SOCK_ENCHANTMENT_SLOT);
    oss << ":" << item->GetEnchantmentId(SOCK_ENCHANTMENT_SLOT_2);
    oss << ":" << item->GetEnchantmentId(SOCK_ENCHANTMENT_SLOT_3);
    oss << ":" << item->GetEnchantmentId(BONUS_ENCHANTMENT_SLOT);
    oss << ":" << item->GetItemRandomPropertyId();
    oss << ":" << item->GetItemSuffixFactor();
    oss << ":" << (uint32)item->GetOwner()->GetLevel();
    oss << "|h[" << ItemNameWithLocale(player, itemTemplate, item->GetItemRandomPropertyId());
    oss << "]|h|r";

    return oss.str();
}

/*static*/ void ItemUpgrade::SendMessage(const Player* player, const std::string& message)
{
    ChatHandler(player->GetSession()).SendSysMessage(message);
}

void ItemUpgrade::PagedData::Reset()
{
    totalPages = 0;
    for (Identifier* identifier : data)
        delete identifier;
    data.clear();
}

void ItemUpgrade::PagedData::CalculateTotals()
{
    totalPages = data.size() / PAGE_SIZE;
    if (data.size() % PAGE_SIZE != 0)
        totalPages++;
}

void ItemUpgrade::PagedData::SortAndCalculateTotals()
{
    if (data.size() > 0)
    {
        std::sort(data.begin(), data.end(), CompareIdentifier);
        CalculateTotals();
    }
}

bool ItemUpgrade::PagedData::IsEmpty() const
{
    return data.empty();
}

const ItemUpgrade::Identifier* ItemUpgrade::PagedData::FindIdentifierById(uint32 id) const
{
    std::vector<Identifier*>::const_iterator citer = std::find_if(data.begin(), data.end(), [&](const Identifier* idnt) { return idnt->id == id; });
    if (citer != data.end())
        return *citer;
    return nullptr;
}

void ItemUpgrade::BuildUpgradableItemCatalogue(const Player* player, PagedDataType type)
{
    PagedData& pagedData = GetPagedData(player);
    pagedData.Reset();
    pagedData.item.guid = ObjectGuid::Empty;
    pagedData.upgradeStat = nullptr;
    pagedData.type = type;

    std::vector<Item*> playerItems = GetPlayerItems(player, false);
    std::vector<Item*>::iterator iter = playerItems.begin();
    for (iter; iter != playerItems.end(); ++iter)
    {
        bool valid = type == PAGED_DATA_TYPE_WEAPON_UPGRADE_ITEMS ? IsValidWeaponForUpgrade(*iter, player) : IsValidItemForUpgrade(*iter, player);
        if (valid)
            AddItemToPagedData(*iter, player, pagedData);
    }

    pagedData.SortAndCalculateTotals();
}

bool ItemUpgrade::IsValidItemForUpgrade(const Item* item, const Player* player) const
{
    if (!item)
        return false;

    if (item->GetOwnerGUID() != player->GetGUID())
        return false;

    if (LoadItemStatInfo(item).empty())
        return false;

    const ItemTemplate* proto = item->GetTemplate();
    if (proto->Quality == ITEM_QUALITY_HEIRLOOM)
        return false;

    if (item->IsBroken())
        return false;

    return true;
}

bool ItemUpgrade::IsValidWeaponForUpgrade(const Item* item, const Player* player) const
{
    if (!item)
        return false;

    if (item->GetOwnerGUID() != player->GetGUID())
        return false;

    const ItemTemplate* proto = item->GetTemplate();
    if (proto->Quality == ITEM_QUALITY_HEIRLOOM)
        return false;

    if (item->IsBroken())
        return false;

    std::pair<float, float> dmg = GetItemProtoDamage(proto);
    if (dmg.first > 0 && dmg.second > 0)
        return true;

    return false;
}

void ItemUpgrade::AddItemToPagedData(const Item* item, const Player* player, PagedData& pagedData)
{
    const ItemTemplate* proto = item->GetTemplate();

    ItemIdentifier* itemIdentifier = new ItemIdentifier();
    itemIdentifier->id = pagedData.data.size();
    itemIdentifier->guid = item->GetGUID();
    itemIdentifier->name = ItemNameWithLocale(player, proto, item->GetItemRandomPropertyId());
    itemIdentifier->uiName = ItemLinkForUI(item, player);

    pagedData.data.push_back(itemIdentifier);
}

ItemUpgrade::PagedData& ItemUpgrade::GetPagedData(const Player* player)
{
    return playerPagedData[player->GetGUID().GetCounter()];
}

ItemUpgrade::PagedDataMap& ItemUpgrade::GetPagedDataMap()
{
    return playerPagedData;
}

bool ItemUpgrade::_AddPagedData(Player* player, const PagedData& pagedData, uint32 page) const
{
    const std::vector<Identifier*>& data = pagedData.data;
    if (data.size() == 0 || (page + 1) > pagedData.totalPages)
        return false;

    uint32 lowIndex = page * PagedData::PAGE_SIZE;
    if (data.size() <= lowIndex)
        return false;

    uint32 highIndex = lowIndex + PagedData::PAGE_SIZE - 1;
    if (highIndex >= data.size())
        highIndex = data.size() - 1;

    std::unordered_map<uint32, const UpgradeStat*> upgrades;
    Item* item = nullptr;
    if (pagedData.type == PAGED_DATA_TYPE_STATS || pagedData.type == PAGED_DATA_TYPE_REQS || pagedData.type == PAGED_DATA_TYPE_UPGRADED_ITEMS_STATS
        || pagedData.type == PAGED_DATA_TYPE_STATS_BULK || pagedData.type == PAGED_DATA_TYPE_STAT_UPGRADE_BULK || pagedData.type == PAGED_DATA_TYPE_REQS_BULK
        || pagedData.type == PAGED_DATA_TYPE_WEAPON_UPGRADE_PERCS || pagedData.type == PAGED_DATA_TYPE_WEAPON_UPGRADE_PERC_INFO || pagedData.type == PAGED_DATA_TYPE_WEAPON_UPGRADE_ITEMS_CHECK_INFO)
    {
        item = player->GetItemByGuid(pagedData.item.guid);
        bool validItem = pagedData.type == PAGED_DATA_TYPE_WEAPON_UPGRADE_PERCS
            || pagedData.type == PAGED_DATA_TYPE_WEAPON_UPGRADE_PERC_INFO
            || pagedData.type == PAGED_DATA_TYPE_WEAPON_UPGRADE_ITEMS_CHECK_INFO ? IsValidWeaponForUpgrade(item, player) : IsValidItemForUpgrade(item, player);
        if (!validItem)
            return false;

        AddGossipItemFor(player, GOSSIP_ICON_VENDOR, ItemLinkForUI(item, player), GOSSIP_SENDER_MAIN + 2, GOSSIP_ACTION_INFO_DEF + page);

        if (pagedData.type == PAGED_DATA_TYPE_STATS)
        {
            std::vector<_ItemStat> statTypes = LoadItemStatInfo(item);
            std::ostringstream ossStatTypes;
            ossStatTypes << "HAS STATS: ";
            for (uint32 i = 0; i < statTypes.size(); i++)
            {
                if (IsAllowedStatType(statTypes[i].ItemStatType))
                    ossStatTypes << StatTypeToString(statTypes[i].ItemStatType);
                else
                    ossStatTypes << "|cffb50505" << StatTypeToString(statTypes[i].ItemStatType) << "|r";
                if (i < statTypes.size() - 1)
                    ossStatTypes << ", ";
            }
            AddGossipItemFor(player, GOSSIP_ICON_CHAT, ossStatTypes.str(), GOSSIP_SENDER_MAIN + 2, GOSSIP_ACTION_INFO_DEF + page);
        }
        else if (pagedData.type == PAGED_DATA_TYPE_REQS)
        {
            const UpgradeStat* upgradeStat = pagedData.upgradeStat;
            std::vector<_ItemStat> statInfoList = LoadItemStatInfo(item);
            const _ItemStat* statInfo = GetStatByType(statInfoList, upgradeStat->statType);
            if (!statInfo)
                return false;

            std::ostringstream oss;
            oss << "UPGRADE " << StatTypeToString(upgradeStat->statType) << " [RANK " << upgradeStat->statRank << "]";
            oss << " " << "[" << upgradeStat->statModPct << "% increase - ";
            oss << "|cffb50505" << statInfo->ItemStatValue << "|r --> ";
            oss << "|cff056e3a" << CalculateModPct(statInfo->ItemStatValue, upgradeStat) << "|r]";

            const UpgradeStat* currentUpgrade = FindUpgradeForItem(player, item, upgradeStat->statType);
            if (currentUpgrade != nullptr)
                oss << " [CURRENT: " << CalculateModPct(statInfo->ItemStatValue, currentUpgrade) << "|r]";

            std::pair<uint32, uint32> itemLevel = CalculateItemLevel(player, item, upgradeStat);
            std::ostringstream ilvloss;
            ilvloss << "[ITEM LEVEL ";
            ilvloss << "|cffb50505" << itemLevel.first << "|r --> ";
            ilvloss << "|cff056e3a" << itemLevel.second << "|r]";
            itemLevel = CalculateItemLevel(player, item);
            ilvloss << " [CURRENT: " << itemLevel.second << "]";
            
            AddGossipItemFor(player, GOSSIP_ICON_CHAT, oss.str(), GOSSIP_SENDER_MAIN + 2, GOSSIP_ACTION_INFO_DEF + page);
            AddGossipItemFor(player, GOSSIP_ICON_CHAT, ilvloss.str(), GOSSIP_SENDER_MAIN + 2, GOSSIP_ACTION_INFO_DEF + page);
        }
        else if (pagedData.type == PAGED_DATA_TYPE_UPGRADED_ITEMS_STATS)
        {
            std::pair<uint32, uint32> itemLevel = CalculateItemLevel(player, item);
            uint32 diff = itemLevel.second - itemLevel.first;

            std::ostringstream oss;
            oss << "Item level increased by " << diff;
            oss << " [|cffb50505" << itemLevel.first << "|r --> ";
            oss << " |cff056e3a" << itemLevel.second << "|r]";

            AddGossipItemFor(player, GOSSIP_ICON_CHAT, oss.str(), GOSSIP_SENDER_MAIN + 2, GOSSIP_ACTION_INFO_DEF + page);

            const UpgradeStat* weaponUpgrade = FindUpgradeForWeapon(player, item);
            if (weaponUpgrade != nullptr)
            {
                std::ostringstream wuoss;
                wuoss << "|cff056e3aWEAPON DAMAGE UPGRADED BY " << FormatFloat(weaponUpgrade->statModPct) << "%|r";
                AddGossipItemFor(player, GOSSIP_ICON_CHAT, wuoss.str(), GOSSIP_SENDER_MAIN + 2, GOSSIP_ACTION_INFO_DEF + page);
            }

            if (!item->IsEquipped())
                AddGossipItemFor(player, GOSSIP_ICON_BATTLE, "[EQUIP ITEM]", GOSSIP_SENDER_MAIN + 1, GOSSIP_ACTION_INFO_DEF + 1);
        }
        else if (pagedData.type == PAGED_DATA_TYPE_STAT_UPGRADE_BULK)
        {
            upgrades = FindAllUpgradeableRanks(player, item, pagedData.pct);
            std::ostringstream oss;
            if (upgrades.empty())
                oss << "[ITEM LEVEL |cffb50505won't|r increase, no upgrades to apply]";
            else
            {
                std::pair<uint32, uint32> ilvl = CalculateItemLevel(player, item, upgrades);
                std::pair<uint32, uint32> currentIlvl = CalculateItemLevel(player, item);
                oss << "[ITEM LEVEL |cffb50505" << ilvl.first << "|r --> " << "|cff056e3a" << ilvl.second << "|r]";
                oss << " [CURRENT: " << currentIlvl.second << "]";
            }

            AddGossipItemFor(player, GOSSIP_ICON_CHAT, oss.str(), GOSSIP_SENDER_MAIN + 2, GOSSIP_ACTION_INFO_DEF + page);
        }
    }
    else if (pagedData.type == PAGED_DATA_TYPE_UPGRADED_ITEMS)
    {
        AddGossipItemFor(player, GOSSIP_ICON_CHAT, "Total items upgraded: " + Acore::ToString(pagedData.data.size()), GOSSIP_SENDER_MAIN + 2, GOSSIP_ACTION_INFO_DEF + page);

        uint32 totalUpgrades = 0;
        for (const Identifier* idnt : pagedData.data)
        {
            ItemIdentifier* itemIdnt = (ItemIdentifier*)idnt;
            Item* item = player->GetItemByGuid(itemIdnt->guid);
            if (item)
                totalUpgrades += FindUpgradesForItem(player, item).size();
        }

        AddGossipItemFor(player, GOSSIP_ICON_CHAT, "Total upgrades: " + Acore::ToString(totalUpgrades), GOSSIP_SENDER_MAIN + 2, GOSSIP_ACTION_INFO_DEF + page);
    }
    else if (pagedData.type == PAGED_DATA_TYPE_ITEMS_FOR_PURGE)
    {
        AddGossipItemFor(player, GOSSIP_ICON_CHAT, "|cffb50505PURGE UPGRADES|r", GOSSIP_SENDER_MAIN + 2, GOSSIP_ACTION_INFO_DEF + page);
        const ItemTemplate* proto = sObjectMgr->GetItemTemplate((uint32)GetIntConfig(CONFIG_ITEM_UPGRADE_PURGE_TOKEN));
        if (proto != nullptr)
        {
            AddGossipItemFor(player, GOSSIP_ICON_CHAT, "Will receive after purge:", GOSSIP_SENDER_MAIN + 2, GOSSIP_ACTION_INFO_DEF + page);

            std::ostringstream oss;
            oss << ItemIcon(proto);
            oss << ItemLink(player, proto, 0);
            oss << " " << (uint32)GetIntConfig(CONFIG_ITEM_UPGRADE_PURGE_TOKEN_COUNT) << "x";
            AddGossipItemFor(player, GOSSIP_ICON_VENDOR, oss.str(), GOSSIP_SENDER_MAIN + 2, GOSSIP_ACTION_INFO_DEF + page);
        }
        if (GetBoolConfig(CONFIG_ITEM_UPGRADE_REFUND_ALL_ON_PURGE))
            AddGossipItemFor(player, GOSSIP_ICON_CHAT, "|cff056e3aWILL REFUND EVERYTHING ON PURGE|r", GOSSIP_SENDER_MAIN + 2, GOSSIP_ACTION_INFO_DEF + page);
        AddGossipItemFor(player, GOSSIP_ICON_CHAT, "Choose an upgraded item to purge:", GOSSIP_SENDER_MAIN + 2, GOSSIP_ACTION_INFO_DEF + page);
    }

    for (uint32 i = lowIndex; i <= highIndex; i++)
    {
        const Identifier* identifier = data[i];
        if (pagedData.type != PAGED_DATA_TYPE_ITEMS_FOR_PURGE)
            AddGossipItemFor(player, identifier->optionIcon, identifier->uiName, GOSSIP_SENDER_MAIN + 1, GOSSIP_ACTION_INFO_DEF + identifier->id);
        else
            AddGossipItemFor(player, identifier->optionIcon, identifier->uiName, GOSSIP_SENDER_MAIN + 1, GOSSIP_ACTION_INFO_DEF + identifier->id, "Are you sure you want to remove all upgrades? This cannot be undone!", 0, false);
    }

    if (pagedData.type == PAGED_DATA_TYPE_REQS)
        AddGossipItemFor(player, GOSSIP_ICON_TRAINER, (MeetsRequirement(player, pagedData.upgradeStat, item) ? "|cff056e3a[PURCHASE]|r" : "|cffb50505[PURCHASE]|r"), GOSSIP_SENDER_MAIN + 1, GOSSIP_ACTION_INFO_DEF + 1, "Are you sure you want to upgrade?", 0, false);

    if (pagedData.type == PAGED_DATA_TYPE_WEAPON_UPGRADE_PERC_INFO)
        AddGossipItemFor(player, GOSSIP_ICON_TRAINER, (MeetsWeaponUpgradeRequirement(player) ? "|cff056e3a[UPGRADE]|r" : "|cffb50505[UPGRADE]|r"), GOSSIP_SENDER_MAIN + 1, GOSSIP_ACTION_INFO_DEF + 1, "Are you sure you want to upgrade this weapon?", 0, false);

    if (!upgrades.empty())
    {
        AddGossipItemFor(player, GOSSIP_ICON_CHAT, "[ALL REQUIREMENTS]", GOSSIP_SENDER_MAIN + 1, GOSSIP_ACTION_INFO_DEF + 1);
        StatRequirementContainer reqs = BuildBulkRequirements(upgrades, item);
        AddGossipItemFor(player, GOSSIP_ICON_TRAINER, (MeetsRequirement(player, &reqs) ? "|cff056e3a[PURCHASE ALL]|r" : "|cffb50505[PURCHASE ALL]|r"), GOSSIP_SENDER_MAIN + 1, GOSSIP_ACTION_INFO_DEF + 2, "Are you sure you want to upgrade?", 0, false);
    }

    if (pagedData.type == PAGED_DATA_TYPE_WEAPON_UPGRADE_ITEMS_CHECK_INFO)
        AddGossipItemFor(player, GOSSIP_ICON_INTERACT_1, "|cffb50505REMOVE UPGRADE|r", GOSSIP_SENDER_MAIN + 1, GOSSIP_ACTION_INFO_DEF + 1, "Are you sure you want to remove this weapon upgrade? This cannot be undone!", 0, false);

    if (page + 1 < pagedData.totalPages)
        AddGossipItemFor(player, GOSSIP_ICON_CHAT, "[Next] ->", GOSSIP_SENDER_MAIN + 2, GOSSIP_ACTION_INFO_DEF + page + 1);

    uint32 pageZeroSender = GOSSIP_SENDER_MAIN;
    if (pagedData.type == PAGED_DATA_TYPE_STATS)
        pageZeroSender += 9;
    else if (pagedData.type == PAGED_DATA_TYPE_REQS)
        pageZeroSender += 10;
    else if (pagedData.type == PAGED_DATA_TYPE_UPGRADED_ITEMS_STATS)
        pageZeroSender += 11;
    else if (pagedData.type == PAGED_DATA_TYPE_STATS_BULK)
        pageZeroSender += 12;
    else if (pagedData.type == PAGED_DATA_TYPE_STAT_UPGRADE_BULK)
        pageZeroSender += 13;
    else if (pagedData.type == PAGED_DATA_TYPE_REQS_BULK)
        pageZeroSender += 14;
    else if (pagedData.type == PAGED_DATA_TYPE_WEAPON_UPGRADE_ITEMS)
        pageZeroSender += 15;
    else if (pagedData.type == PAGED_DATA_TYPE_WEAPON_UPGRADE_PERCS)
        pageZeroSender += 16;
    else if (pagedData.type == PAGED_DATA_TYPE_WEAPON_UPGRADE_PERC_INFO)
        pageZeroSender += 17;
    else if (pagedData.type == PAGED_DATA_TYPE_WEAPON_UPGRADE_ITEMS_CHECK)
        pageZeroSender += 18;
    else if (pagedData.type == PAGED_DATA_TYPE_WEAPON_UPGRADE_ITEMS_CHECK_INFO)
        pageZeroSender += 19;

    AddGossipItemFor(player, GOSSIP_ICON_CHAT, "<- [Back]", page == 0 ? pageZeroSender : GOSSIP_SENDER_MAIN + 2, page == 0 ? GOSSIP_ACTION_INFO_DEF : GOSSIP_ACTION_INFO_DEF + page - 1);

    return true;
}

bool ItemUpgrade::AddPagedData(Player* player, Creature* creature, uint32 page)
{
    ClearGossipMenuFor(player);
    PagedData& pagedData = GetPagedData(player);
    while (!_AddPagedData(player, pagedData, page))
    {
        if (page == 0)
        {
            NoPagedData(player, pagedData);
            break;
        }
        else
            page--;
    }
    
    pagedData.currentPage = page;

    SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, creature->GetGUID());
    return true;
}

void ItemUpgrade::NoPagedData(Player* player, const PagedData& pagedData) const
{
    if (pagedData.type == PAGED_DATA_TYPE_ITEMS || pagedData.type == PAGED_DATA_TYPE_UPGRADED_ITEMS || pagedData.type == PAGED_DATA_TYPE_ITEMS_BULK || pagedData.type == PAGED_DATA_TYPE_WEAPON_UPGRADE_ITEMS
        || pagedData.type == PAGED_DATA_TYPE_WEAPON_UPGRADE_ITEMS_CHECK || pagedData.type == PAGED_DATA_TYPE_WEAPON_UPGRADE_ITEMS_CHECK_INFO)
        AddGossipItemFor(player, GOSSIP_ICON_CHAT, "|cffb50505NOTHING ON THIS PAGE|r", GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF);
    else if (pagedData.type == PAGED_DATA_TYPE_STATS || pagedData.type == PAGED_DATA_TYPE_STATS_BULK || pagedData.type == PAGED_DATA_TYPE_STAT_UPGRADE_BULK
        || pagedData.type == PAGED_DATA_TYPE_WEAPON_UPGRADE_PERCS || pagedData.type == PAGED_DATA_TYPE_WEAPON_UPGRADE_PERC_INFO)
    {
        Item* item = player->GetItemByGuid(pagedData.item.guid);
        bool validItem = pagedData.type == PAGED_DATA_TYPE_WEAPON_UPGRADE_PERCS || pagedData.type == PAGED_DATA_TYPE_WEAPON_UPGRADE_PERC_INFO ? IsValidWeaponForUpgrade(item, player) : IsValidItemForUpgrade(item, player);
        if (validItem)
            AddGossipItemFor(player, GOSSIP_ICON_VENDOR, ItemLinkForUI(item, player), GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF);
        AddGossipItemFor(player, GOSSIP_ICON_CHAT, "|cffb50505ITEM CAN'T BE UPGRADED|r", GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF);
    }
    else if (pagedData.type == PAGED_DATA_TYPE_ITEMS_FOR_PURGE)
        AddGossipItemFor(player, GOSSIP_ICON_CHAT, "|cffb50505NOTHING TO PURGE|r", GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF);
    AddGossipItemFor(player, GOSSIP_ICON_CHAT, "<- [First Page]", GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF);
}

bool ItemUpgrade::TakePagedDataAction(Player* player, Creature* creature, uint32 action)
{
    PagedData& pagedData = GetPagedData(player);
    if (pagedData.type == PAGED_DATA_TYPE_ITEMS)
    {
        Item* item = FindItemIdentifierFromPage(pagedData, action, player);
        if (item == nullptr)
            SendMessage(player, "Item is no longer available for upgrade.");
        else
        {
            BuildStatsUpgradeCatalogue(player, item);
            return AddPagedData(player, creature, 0);
        }
    }
    else if (pagedData.type == PAGED_DATA_TYPE_STATS)
    {
        const Identifier* identifier = pagedData.FindIdentifierById(action);
        if (identifier != nullptr)
        {
            Item* item = player->GetItemByGuid(pagedData.item.guid);
            if (!IsValidItemForUpgrade(item, player))
                SendMessage(player, "Item is no longer available for upgrade.");
            else
            {
                const UpgradeStat* upgradeStat = FindUpgradeStat(identifier->id);
                if (upgradeStat == nullptr)
                    SendMessage(player, "Upgrade no longer available.");
                else
                {
                    const UpgradeStat* playerUpgrade = FindUpgradeForItem(player, item, upgradeStat->statType);
                    if (playerUpgrade != nullptr)
                    {
                        if (!FindUpgradeStat(upgradeStat->statType, playerUpgrade->statRank + 1))
                        {
                            SendMessage(player, "Already at MAX rank for this stat and item.");
                            BuildStatsUpgradeCatalogue(player, item);
                            return AddPagedData(player, creature, pagedData.currentPage);
                        }
                    }

                    if (!CanApplyUpgradeForItem(item, upgradeStat))
                    {
                        SendMessage(player, "This rank is not available for " + ItemLink(player, item));
                        BuildStatsUpgradeCatalogue(player, item);
                        return AddPagedData(player, creature, pagedData.currentPage);
                    }

                    BuildStatsRequirementsCatalogue(player, upgradeStat, item);
                    return AddPagedData(player, creature, 0);
                }
            }
        }
    }
    else if (pagedData.type == PAGED_DATA_TYPE_REQS)
    {
        if (action == 0)
        {
            Item* item = player->GetItemByGuid(pagedData.item.guid);
            if (!IsValidItemForUpgrade(item, player))
                SendMessage(player, "Item is no longer available for upgrade.");
            else
            {
                BuildStatsRequirementsCatalogue(player, pagedData.upgradeStat, item);
                return AddPagedData(player, creature, pagedData.currentPage);
            }
        }
        else
        {
            if (!PurchaseUpgrade(player))
                SendMessage(player, "Upgrade could not be processed. This should not happen, unless the item is no longer available.");
            else
            {
                CloseGossipMenuFor(player);
                return true;
            }
        }
    }
    else if (pagedData.type == PAGED_DATA_TYPE_UPGRADED_ITEMS)
    {
        Item* item = FindItemIdentifierFromPage(pagedData, action, player);
        if (item == nullptr)
            SendMessage(player, "Item is no longer available.");
        else
        {
            BuildItemUpgradeStatsCatalogue(player, item);
            return AddPagedData(player, creature, 0);
        }
    }
    else if (pagedData.type == PAGED_DATA_TYPE_UPGRADED_ITEMS_STATS)
    {
        Item* item = player->GetItemByGuid(pagedData.item.guid);
        if (!IsValidItemForUpgrade(item, player))
            SendMessage(player, "Item is no longer available.");
        else
        {
            if (action == 1)
                EquipItem(player, item);

            BuildItemUpgradeStatsCatalogue(player, item);
            return AddPagedData(player, creature, pagedData.currentPage);
        }
    }
    else if (pagedData.type == PAGED_DATA_TYPE_ITEMS_FOR_PURGE)
    {
        Item* item = FindItemIdentifierFromPage(pagedData, action, player);
        if (item == nullptr)
            SendMessage(player, "Item is no longer available.");
        else
        {
            if (PurgeUpgrade(player, item))
                VisualFeedback(player);

            BuildAlreadyUpgradedItemsCatalogue(player, ItemUpgrade::PAGED_DATA_TYPE_ITEMS_FOR_PURGE);
            return AddPagedData(player, creature, pagedData.currentPage);
        }
    }
    else if (pagedData.type == PAGED_DATA_TYPE_ITEMS_BULK)
    {
        Item* item = FindItemIdentifierFromPage(pagedData, action, player);
        if (item == nullptr)
            SendMessage(player, "Item is no longer available for upgrade.");
        else
        {
            BuildStatsUpgradeCatalogueBulk(player, item);
            return AddPagedData(player, creature, 0);
        }
    }
    else if (pagedData.type == PAGED_DATA_TYPE_STATS_BULK)
    {
        const Identifier* identifier = pagedData.FindIdentifierById(action);
        if (identifier != nullptr && identifier->GetType() == FLOAT_IDENTIFIER)
        {
            Item* item = player->GetItemByGuid(pagedData.item.guid);
            if (!IsValidItemForUpgrade(item, player))
                SendMessage(player, "Item is no longer available.");
            else
            {
                const FloatIdentifier* bulkIdentifier = (FloatIdentifier*)identifier;
                BuildStatsUpgradeByPctCatalogueBulk(player, item, bulkIdentifier->modPct);
                return AddPagedData(player, creature, 0);
            }
        }
    }
    else if (pagedData.type == PAGED_DATA_TYPE_STAT_UPGRADE_BULK)
    {
        Item* item = player->GetItemByGuid(pagedData.item.guid);
        if (!IsValidItemForUpgrade(item, player))
            SendMessage(player, "Item is no longer available.");
        else
        {
            if (action == 0)
            {
                BuildStatsUpgradeByPctCatalogueBulk(player, item, pagedData.pct);
                return AddPagedData(player, creature, 0);
            }
            else if (action == 1)
            {
                BuildStatsRequirementsCatalogueBulk(player, item, pagedData.pct);
                return AddPagedData(player, creature, 0);
            }
            else if (action == 2)
            {
                if (!PurchaseUpgradeBulk(player))
                    SendMessage(player, "Upgrade could not be processed. This should not happen, unless the item is no longer available.");
                else
                {
                    CloseGossipMenuFor(player);
                    return true;
                }
            }
        }
    }
    else if (pagedData.type == PAGED_DATA_TYPE_REQS_BULK)
    {
        Item* item = player->GetItemByGuid(pagedData.item.guid);
        if (!IsValidItemForUpgrade(item, player))
            SendMessage(player, "Item is no longer available.");
        else
        {
            BuildStatsRequirementsCatalogueBulk(player, item, pagedData.pct);
            return AddPagedData(player, creature, 0);
        }
    }
    else if (pagedData.type == PAGED_DATA_TYPE_WEAPON_UPGRADE_ITEMS)
    {
        Item* item = FindItemIdentifierFromPage(pagedData, action, player);
        if (item == nullptr)
            SendMessage(player, "Weapon is no longer available for upgrade.");
        else
        {
            BuildWeaponPercentUpgradesCatalogue(player, item);
            return AddPagedData(player, creature, 0);
        }
    }
    else if (pagedData.type == PAGED_DATA_TYPE_WEAPON_UPGRADE_PERCS)
    {
        const Identifier* identifier = pagedData.FindIdentifierById(action);
        if (identifier != nullptr && identifier->GetType() == FLOAT_IDENTIFIER)
        {
            Item* item = player->GetItemByGuid(pagedData.item.guid);
            if (!IsValidWeaponForUpgrade(item, player))
                SendMessage(player, "Weapon is no longer available.");
            else
            {
                auto rebuildPage = [&]()
                {
                    BuildWeaponPercentUpgradesCatalogue(player, item);
                    return AddPagedData(player, creature, pagedData.currentPage);
                };

                const FloatIdentifier* floatIdentifier = (FloatIdentifier*)identifier;
                const UpgradeStat* weaponUpgrade = FindUpgradeForWeapon(player, item);
                if (weaponUpgrade != nullptr)
                {
                    if (weaponUpgrade->statModPct >= floatIdentifier->modPct)
                    {
                        SendMessage(player, "You already bought this weapon upgrade!");
                        return rebuildPage();
                    }
                    else
                    {
                        const UpgradeStat* nextWeaponUpgrade = FindNextWeaponUpgradeStat(weaponUpgrade->statModPct);
                        if (nextWeaponUpgrade != nullptr)
                        {
                            if (floatIdentifier->modPct > nextWeaponUpgrade->statModPct)
                            {
                                SendMessage(player, "You must buy the previous upgrade first!");
                                return rebuildPage();
                            }
                            else
                            {
                                BuildWeaponUpgradesPercentInfoCatalogue(player, item, floatIdentifier->modPct);
                                return AddPagedData(player, creature, 0);
                            }
                        }
                    }
                }
                else
                {
                    if (floatIdentifier->modPct > weaponUpgradeStats[0].statModPct)
                    {
                        SendMessage(player, "You must buy the previous upgrade first!");
                        return rebuildPage();
                    }
                    else
                    {
                        BuildWeaponUpgradesPercentInfoCatalogue(player, item, floatIdentifier->modPct);
                        return AddPagedData(player, creature, 0);
                    }
                }
            }
        }
    }
    else if (pagedData.type == PAGED_DATA_TYPE_WEAPON_UPGRADE_PERC_INFO)
    {
        Item* item = player->GetItemByGuid(pagedData.item.guid);
        if (!IsValidWeaponForUpgrade(item, player))
            SendMessage(player, "Weapon is no longer available.");
        else
        {
            if (action == 0)
            {
                BuildWeaponUpgradesPercentInfoCatalogue(player, item, pagedData.upgradeStat->statModPct);
                return AddPagedData(player, creature, 0);
            }
            else if (action == 1)
            {
                if (!PurchaseWeaponUpgrade(player))
                    SendMessage(player, "Upgrade could not be processed. This should not happen, unless the weapon is no longer available.");
                else
                {
                    CloseGossipMenuFor(player);
                    return true;
                }
            }
        }
    }
    else if (pagedData.type == PAGED_DATA_TYPE_WEAPON_UPGRADE_ITEMS_CHECK)
    {
        Item* item = FindItemIdentifierFromPage(pagedData, action, player);
        if (item == nullptr)
            SendMessage(player, "Weapon is no longer available for upgrade.");
        else
        {
            BuildWeaponUpgradeInfoCatalogue(player, item);
            return AddPagedData(player, creature, 0);
        }
    }
    else if (pagedData.type == PAGED_DATA_TYPE_WEAPON_UPGRADE_ITEMS_CHECK_INFO)
    {
        Item* item = player->GetItemByGuid(pagedData.item.guid);
        if (!IsValidWeaponForUpgrade(item, player))
            SendMessage(player, "Weapon is no longer available.");
        else
        {
            if (action == 0)
            {
                BuildWeaponUpgradeInfoCatalogue(player, item);
                return AddPagedData(player, creature, pagedData.currentPage);
            }
            else if (action == 1)
            {
                if (PurgeWeaponUpgrade(player, item))
                    VisualFeedback(player);

                CloseGossipMenuFor(player);
                return true;
            }
            else if (action == 2)
            {
                EquipItem(player, item);

                BuildWeaponUpgradeInfoCatalogue(player, item);
                return AddPagedData(player, creature, pagedData.currentPage);
            }
        }
    }

    CloseGossipMenuFor(player);
    return false;
}

Item* ItemUpgrade::FindItemIdentifierFromPage(const PagedData& pagedData, uint32 id, Player* player) const
{
    const Identifier* identifier = pagedData.FindIdentifierById(id);
    if (identifier != nullptr && identifier->GetType() == ITEM_IDENTIFIER)
    {
        const ItemIdentifier* itemIdentifier = (ItemIdentifier*)identifier;
        Item* item = player->GetItemByGuid(itemIdentifier->guid);
        bool valid = pagedData.type == PAGED_DATA_TYPE_WEAPON_UPGRADE_ITEMS || pagedData.type == PAGED_DATA_TYPE_WEAPON_UPGRADE_ITEMS_CHECK ? IsValidWeaponForUpgrade(item, player) : IsValidItemForUpgrade(item, player);
        if (valid)
            return item;
    }

    return nullptr;
}

bool ItemUpgrade::HandlePurchaseRank(Player* player, Item* item, const UpgradeStat* upgrade)
{
    const UpgradeStat* foundUpgrade = FindUpgradeForItem(player, item, upgrade->statType);
    std::vector<CharacterUpgrade>& upgrades = characterUpgradeData[player->GetGUID().GetCounter()];
    if (foundUpgrade != nullptr)
    {
        std::vector<CharacterUpgrade>::const_iterator citer = std::remove_if(upgrades.begin(), upgrades.end(),
            [&](const CharacterUpgrade& upgrade) { return upgrade.itemGuid == item->GetGUID() && upgrade.upgradeStat->statId == foundUpgrade->statId; });
        if (citer == upgrades.end())
            return false;
        upgrades.erase(citer, upgrades.end());

        CharacterDatabase.Execute("UPDATE character_item_upgrade SET stat_id = {} WHERE guid = {} AND item_guid = {} AND stat_id = {}",
            upgrade->statId, player->GetGUID().GetCounter(), item->GetGUID().GetCounter(), foundUpgrade->statId);
    }
    else
        AddItemUpgradeToDB(player, item, upgrade);

    CharacterUpgrade newUpgrade;
    newUpgrade.guid = player->GetGUID().GetCounter();
    newUpgrade.itemGuid = item->GetGUID();
    newUpgrade.upgradeStat = upgrade;
    upgrades.push_back(newUpgrade);

    return true;
}

bool ItemUpgrade::HandlePurchaseWeaponUpgrade(Player* player, Item* item, const UpgradeStat* upgrade)
{
    std::vector<CharacterUpgrade>& upgrades = characterWeaponUpgradeData[player->GetGUID().GetCounter()];
    std::vector<CharacterUpgrade>::const_iterator citer = std::remove_if(upgrades.begin(), upgrades.end(),
        [&](const CharacterUpgrade& upgrade) { return upgrade.itemGuid == item->GetGUID(); });
    upgrades.erase(citer, upgrades.end());

    CharacterDatabase.Execute("REPLACE INTO character_weapon_upgrade (guid, item_guid, upgrade_perc) VALUES ({}, {}, {})",
        player->GetGUID().GetCounter(), item->GetGUID().GetCounter(), upgrade->statModPct);

    CharacterUpgrade newUpgrade;
    newUpgrade.guid = player->GetGUID().GetCounter();
    newUpgrade.itemGuid = item->GetGUID();
    newUpgrade.upgradeStat = upgrade;
    newUpgrade.upgradeStatModPct = upgrade->statModPct;
    upgrades.push_back(newUpgrade);

    return true;
}

bool ItemUpgrade::PurchaseUpgrade(Player* player)
{
    PagedData& pagedData = GetPagedData(player);
    if (!pagedData.upgradeStat)
        return false;

    Item* item = player->GetItemByGuid(pagedData.item.guid);
    if (!item)
        return false;

    if (!MeetsRequirement(player, pagedData.upgradeStat, item))
    {
        SendMessage(player, "You do not meet the requirements to buy this upgrade.");
        return true;
    }

    if (item->IsEquipped())
        player->_ApplyItemMods(item, item->GetSlot(), false);

    HandlePurchaseRank(player, item, pagedData.upgradeStat);

    if (item->IsEquipped())
        player->_ApplyItemMods(item, item->GetSlot(), true);

    TakeRequirements(player, pagedData.upgradeStat, item);

    VisualFeedback(player);
    SendMessage(player, "Item successfully upgraded!");

    SendItemPacket(player, item);

    return true;
}

bool ItemUpgrade::PurchaseWeaponUpgrade(Player* player)
{
    PagedData& pagedData = GetPagedData(player);
    if (!pagedData.upgradeStat)
        return false;

    Item* item = player->GetItemByGuid(pagedData.item.guid);
    if (!IsValidWeaponForUpgrade(item, player))
        return false;

    if (!MeetsWeaponUpgradeRequirement(player))
    {
        SendMessage(player, "You do not meet the requirements to buy this upgrade.");
        return true;
    }

    if (item->IsEquipped())
        player->_ApplyItemMods(item, item->GetSlot(), false);

    HandlePurchaseWeaponUpgrade(player, item, pagedData.upgradeStat);

    if (item->IsEquipped())
        player->_ApplyItemMods(item, item->GetSlot(), true);

    TakeWeaponUpgradeRequirements(player);

    VisualFeedback(player);
    SendMessage(player, "Weapon successfully upgraded!");

    SendItemPacket(player, item);

    return true;
}

bool ItemUpgrade::PurchaseUpgradeBulk(Player* player)
{
    PagedData& pagedData = GetPagedData(player);
    Item* item = player->GetItemByGuid(pagedData.item.guid);
    if (!item)
        return false;

    std::unordered_map<uint32, const UpgradeStat*> upgrades = FindAllUpgradeableRanks(player, item, pagedData.pct);
    if (upgrades.empty())
        return false;

    StatRequirementContainer reqs = BuildBulkRequirements(upgrades, item);
    if (!MeetsRequirement(player, &reqs))
    {
        SendMessage(player, "You do not meet the requirements to buy these upgrades.");
        return true;
    }

    if (item->IsEquipped())
        player->_ApplyItemMods(item, item->GetSlot(), false);

    for (const auto& upair : upgrades)
        HandlePurchaseRank(player, item, upair.second);

    if (item->IsEquipped())
        player->_ApplyItemMods(item, item->GetSlot(), true);

    TakeRequirements(player, &reqs);

    VisualFeedback(player);
    SendMessage(player, "Item successfully upgraded!");

    SendItemPacket(player, item);

    return true;
}

int32 ItemUpgrade::HandleStatModifier(const Player* player, uint8 slot, uint32 statType, int32 amount) const
{
    if (amount == 0)
        return 0;

    Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
    if (!item)
        return amount;

    return HandleStatModifier(player, item, statType, amount, MAX_ENCHANTMENT_SLOT);
}

int32 ItemUpgrade::HandleStatModifier(const Player* player, Item* item, uint32 statType, int32 amount, EnchantmentSlot slot) const
{
    if (!GetBoolConfig(CONFIG_ITEM_UPGRADE_ENABLED) || !IsAllowedItem(item) || IsBlacklistedItem(item) || !IsAllowedStatType(statType))
        return amount;

    if (slot < MAX_INSPECTED_ENCHANTMENT_SLOT)
        return amount;

    const UpgradeStat* foundUpgrade = FindUpgradeForItem(player, item, statType);
    if (foundUpgrade != nullptr && CanApplyUpgradeForItem(item, foundUpgrade))
        return CalculateModPct(amount, foundUpgrade);

    return amount;
}

std::pair<float, float> ItemUpgrade::HandleWeaponModifier(const Player* player, uint8 slot, float minDamage, float maxDamage) const
{
    return HandleWeaponModifier(player, player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot), minDamage, maxDamage);
}

std::pair<float, float> ItemUpgrade::HandleWeaponModifier(const Player* player, const Item* item, float minDamage, float maxDamage) const
{
    if (!GetBoolConfig(CONFIG_ITEM_UPGRADE_ENABLED))
        return std::make_pair(minDamage, maxDamage);

    if (!GetBoolConfig(CONFIG_ITEM_UPGRADE_WEAPON_DAMAGE))
        return std::make_pair(minDamage, maxDamage);

    if (!item)
        return std::make_pair(minDamage, maxDamage);

    if (minDamage == 0.0f || maxDamage == 0.0f)
        return std::make_pair(minDamage, maxDamage);

    const UpgradeStat* weaponUpgrade = FindUpgradeForWeapon(player, item);
    if (weaponUpgrade == nullptr)
        return std::make_pair(minDamage, maxDamage);

    float upgradedMinDamage = std::floor(CalculateModPctF(minDamage, weaponUpgrade));
    float upgradedMaxDamage = std::ceil(CalculateModPctF(maxDamage, weaponUpgrade));
    return std::make_pair(upgradedMinDamage, upgradedMaxDamage);
}

void ItemUpgrade::HandleItemRemove(Player* player, Item* item)
{
    bool hasItemUpgrades = !FindUpgradesForItem(player, item).empty();
    bool hasWeaponUpgrade = FindUpgradeForWeapon(player, item) != nullptr;
    if (hasItemUpgrades || hasWeaponUpgrade)
    {
        player->_ApplyItemMods(item, item->GetSlot(), false);
        if (hasItemUpgrades)
            RemoveItemUpgrade(player, item);
        if (hasWeaponUpgrade)
            RemoveWeaponUpgrade(player, item);
        player->_ApplyItemMods(item, item->GetSlot(), true);
    }
}

void ItemUpgrade::RemoveItemUpgradeFromContainer(CharacterUpgradeContainer& upgradesContainer, Player* player, Item* item)
{
    std::vector<CharacterUpgrade>& upgrades = upgradesContainer[player->GetGUID().GetCounter()];
    std::vector<CharacterUpgrade>::const_iterator citer = std::remove_if(upgrades.begin(), upgrades.end(),
        [&](const CharacterUpgrade& upgrade) { return upgrade.itemGuid == item->GetGUID(); });
    upgrades.erase(citer, upgrades.end());
}

void ItemUpgrade::RemoveItemUpgrade(Player* player, Item* item)
{
    RemoveItemUpgradeFromContainer(characterUpgradeData, player, item);
    CharacterDatabase.Execute("DELETE FROM character_item_upgrade WHERE guid = {} AND item_guid = {}", player->GetGUID().GetCounter(), item->GetGUID().GetCounter());
}

void ItemUpgrade::RemoveWeaponUpgrade(Player* player, Item* item)
{
    RemoveItemUpgradeFromContainer(characterWeaponUpgradeData, player, item);
    CharacterDatabase.Execute("DELETE FROM character_weapon_upgrade WHERE guid = {} AND item_guid = {}", player->GetGUID().GetCounter(), item->GetGUID().GetCounter());
}

void ItemUpgrade::HandleCharacterRemove(uint32 guid)
{
    characterUpgradeData[guid].clear();
    characterWeaponUpgradeData[guid].clear();
}

void ItemUpgrade::BuildRequirementsPage(const Player* player, PagedData& pagedData, const StatRequirementContainer* reqs) const
{
    if (EmptyRequirements(reqs))
    {
        Identifier* identifier = new Identifier();
        identifier->id = 0;
        identifier->name = "0";
        identifier->uiName = "NO REQUIREMENTS, CAN BE FREELY BOUGHT";
        pagedData.data.push_back(identifier);
    }
    else
    {
        for (const auto& req : *reqs)
        {
            if (req.reqType == REQ_TYPE_NONE)
                continue;

            std::ostringstream oss;
            switch (req.reqType)
            {
                case REQ_TYPE_COPPER:
                    oss << "MONEY: " << CopperToMoneyStr((uint32)req.reqVal1, true);
                    break;
                case REQ_TYPE_HONOR:
                    oss << "HONOR: " << (uint32)req.reqVal1 << " points";
                    break;
                case REQ_TYPE_ARENA:
                    oss << "ARENA: " << (uint32)req.reqVal1 << " points";
                    break;
                case REQ_TYPE_ITEM:
                {
                    const ItemTemplate* proto = sObjectMgr->GetItemTemplate((uint32)req.reqVal1);
                    oss << ItemIcon(proto);
                    oss << ItemLink(player, proto, 0);
                    if (req.reqVal2 > 1.0f)
                        oss << " - " << (uint32)req.reqVal2 << "x";
                    break;
                }
            }

            std::string missing;
            if (!MeetsRequirement(player, req))
            {
                switch (req.reqType)
                {
                    case REQ_TYPE_COPPER:
                        missing = "missing " + CopperToMoneyStr((uint32)req.reqVal1 - player->GetMoney(), true);
                        break;
                    case REQ_TYPE_HONOR:
                        missing = "missing " + Acore::ToString<uint32>((uint32)req.reqVal1 - player->GetHonorPoints()) + " points";
                        break;
                    case REQ_TYPE_ARENA:
                        missing = "missing " + Acore::ToString<uint32>((uint32)req.reqVal1 - player->GetArenaPoints()) + " points";
                        break;
                    case REQ_TYPE_ITEM:
                        missing = "missing " + Acore::ToString<uint32>((uint32)req.reqVal2 - player->GetItemCount((uint32)req.reqVal1, true)) + " items";
                        break;
                }
            }

            oss << " - ";
            if (missing.empty())
                oss << "|cff056e3aDONE|r";
            else
                oss << "|cffb50505IN PROGRESS|r" << " - " << missing;

            Identifier* identifier = new Identifier();
            identifier->id = 0;
            identifier->name = Acore::ToString<uint32>((uint32)req.reqType);
            identifier->uiName = oss.str();
            pagedData.data.push_back(identifier);
        }
    }
}

void ItemUpgrade::BuildStatsRequirementsCatalogue(const Player* player, const UpgradeStat* upgradeStat, const Item* item)
{
    PagedData& pagedData = GetPagedData(player);
    pagedData.Reset();
    pagedData.upgradeStat = upgradeStat;
    pagedData.type = PAGED_DATA_TYPE_REQS;

    BuildRequirementsPage(player, pagedData, GetStatRequirements(upgradeStat, item));

    pagedData.SortAndCalculateTotals();
}

void ItemUpgrade::BuildAlreadyUpgradedItemsCatalogue(const Player* player, PagedDataType type)
{
    PagedData& pagedData = GetPagedData(player);
    pagedData.Reset();
    pagedData.upgradeStat = nullptr;
    pagedData.item.guid = ObjectGuid::Empty;
    pagedData.type = type;

    for (uint8 i = INVENTORY_SLOT_ITEM_START; i < INVENTORY_SLOT_ITEM_END; i++)
        if (Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, i))
            AddUpgradedItemToPagedData(item, player, pagedData, "backpack");

    for (uint8 i = INVENTORY_SLOT_BAG_START; i < INVENTORY_SLOT_BAG_END; i++)
        if (Bag* bag = player->GetBagByPos(i))
            for (uint32 j = 0; j < bag->GetBagSize(); j++)
                if (Item* item = player->GetItemByPos(i, j))
                    AddUpgradedItemToPagedData(item, player, pagedData, "bags");

    for (uint8 i = EQUIPMENT_SLOT_START; i < EQUIPMENT_SLOT_END; i++)
        if (Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, i))
            AddUpgradedItemToPagedData(item, player, pagedData, "equipped");

    for (uint8 i = BANK_SLOT_ITEM_START; i < BANK_SLOT_ITEM_END; i++)
        if (Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, i))
            AddUpgradedItemToPagedData(item, player, pagedData, "bank");

    for (uint8 i = BANK_SLOT_BAG_START; i < BANK_SLOT_BAG_END; i++)
        if (Bag* bag = player->GetBagByPos(i))
            for (uint32 j = 0; j < bag->GetBagSize(); j++)
                if (Item* item = player->GetItemByPos(i, j))
                    AddUpgradedItemToPagedData(item, player, pagedData, "bank bags");

    pagedData.SortAndCalculateTotals();
}

void ItemUpgrade::AddUpgradedItemToPagedData(const Item* item, const Player* player, PagedData& pagedData, const std::string& from)
{
    bool shouldAdd = false;
    if (pagedData.type == PAGED_DATA_TYPE_WEAPON_UPGRADE_ITEMS_CHECK)
        shouldAdd = FindUpgradeForWeapon(player, item) != nullptr;
    else
        shouldAdd = !FindUpgradesForItem(player, item).empty();

    if (shouldAdd)
    {
        const ItemTemplate* proto = item->GetTemplate();

        ItemIdentifier* itemIdentifier = new ItemIdentifier();
        itemIdentifier->id = pagedData.data.size();
        itemIdentifier->guid = item->GetGUID();
        itemIdentifier->name = ItemNameWithLocale(player, proto, item->GetItemRandomPropertyId());
        itemIdentifier->uiName = ItemLinkForUI(item, player) + " [" + from + "]";

        if (pagedData.type != PAGED_DATA_TYPE_WEAPON_UPGRADE_ITEMS_CHECK)
        {
            if (!IsAllowedItem(item) || IsBlacklistedItem(item))
                itemIdentifier->uiName += " [|cffb50505INACTIVE|r]";
        }

        pagedData.data.push_back(itemIdentifier);
    }
}

void ItemUpgrade::BuildItemUpgradeStatsCatalogue(const Player* player, const Item* item)
{
    PagedData& pagedData = GetPagedData(player);
    pagedData.Reset();
    pagedData.upgradeStat = nullptr;
    pagedData.item.guid = item->GetGUID();
    pagedData.type = PAGED_DATA_TYPE_UPGRADED_ITEMS_STATS;

    std::vector<const UpgradeStat*> itemUpgrades = FindUpgradesForItem(player, item);
    if (!itemUpgrades.empty())
    {
        std::vector<_ItemStat> statInfo = LoadItemStatInfo(item);
        for (const UpgradeStat* upgradeStat : itemUpgrades)
        {
            const _ItemStat* foundStat = GetStatByType(statInfo, upgradeStat->statType);
            if (!foundStat)
                continue;

            std::string statTypeStr = StatTypeToString(upgradeStat->statType);

            std::ostringstream oss;
            oss << "UPGRADED " << statTypeStr << " [RANK " << upgradeStat->statRank << "]";
            oss << " " << "[" << upgradeStat->statModPct << "% increase - ";
            oss << "|cffb50505" << foundStat->ItemStatValue << "|r --> ";
            oss << "|cff056e3a" << CalculateModPct(foundStat->ItemStatValue, upgradeStat) << "|r]";

            if (!IsAllowedItem(item)
                || IsBlacklistedItem(item)
                || !IsAllowedStatType(upgradeStat->statType)
                || !CanApplyUpgradeForItem(item, upgradeStat))
                oss << " [|cffb50505INACTIVE|r]";

            Identifier* identifier = new Identifier();
            identifier->id = 0;
            identifier->name = statTypeStr;
            identifier->uiName = oss.str();
            pagedData.data.push_back(identifier);
        }
    }

    pagedData.SortAndCalculateTotals();
}

void ItemUpgrade::BuildWeaponPercentUpgradesCatalogue(const Player* player, const Item* item)
{
    PagedData& pagedData = GetPagedData(player);
    pagedData.Reset();
    pagedData.upgradeStat = nullptr;
    pagedData.item.guid = item->GetGUID();
    pagedData.type = PAGED_DATA_TYPE_WEAPON_UPGRADE_PERCS;

    const UpgradeStat* weaponUpgrade = FindUpgradeForWeapon(player, item);
    bool nextRankSet = false;

    for (size_t i = 0; i < weaponUpgradeStats.size(); i++)
    {
        FloatIdentifier* identifier = new FloatIdentifier();
        identifier->id = pagedData.data.size();
        identifier->name = "";
        identifier->modPct = weaponUpgradeStats[i].statModPct;

        bool toPurchase = false;
        bool purchased = false;
        if (weaponUpgrade == nullptr)
        {
            if (i == 0)
                toPurchase = true;
        }
        else
        {
            if (weaponUpgrade->statModPct >= identifier->modPct)
                purchased = true;
            else
            {
                if (!nextRankSet)
                {
                    nextRankSet = true;
                    toPurchase = true;
                }
            }

        }
        std::ostringstream oss;
        if (toPurchase)
            oss << "|cff056e3a";
        else if (purchased)
            oss << "|cff5c5b57";
        else
            oss << "|cffb50505";
        oss << "Increase by " << identifier->modPct << "%|r";
        if (toPurchase)
            oss << " [PURCHASE]";
        else if (purchased)
            oss << " [DONE]";
        identifier->uiName = oss.str();

        pagedData.data.push_back(identifier);
    }

    pagedData.SortAndCalculateTotals();
}

void ItemUpgrade::BuildWeaponUpgradesPercentInfoCatalogue(const Player* player, const Item* item, float pct)
{
    PagedData& pagedData = GetPagedData(player);
    pagedData.Reset();
    pagedData.upgradeStat = FindWeaponUpgradeStat(pct);
    pagedData.item.guid = item->GetGUID();
    pagedData.type = PAGED_DATA_TYPE_WEAPON_UPGRADE_PERC_INFO;

    Identifier* pctIdnt = new Identifier();
    pctIdnt->id = 0;
    pctIdnt->uiName = "Upgrading damage by " + FormatFloat(pct) + "%";
    pagedData.data.push_back(pctIdnt);

    Identifier* identifier = new Identifier();
    identifier->id = 0;
    identifier->uiName = "Requirements:";
    pagedData.data.push_back(identifier);

    BuildRequirementsPage(player, pagedData, &weaponUpgradeReqs);

    std::pair<float, float> dmgInfo = GetItemProtoDamage(item);
    std::pair<float, float> upgradedDmgInfo = HandleWeaponModifier(player, item, dmgInfo.first, dmgInfo.second);
    float currentMinDamage = upgradedDmgInfo.first;
    float currentMaxDamage = upgradedDmgInfo.second;
    float nextMinDamage = std::floor(CalculateModPctF(dmgInfo.first, pagedData.upgradeStat));
    float nextMaxDamage = std::ceil(CalculateModPctF(dmgInfo.second, pagedData.upgradeStat));

    Identifier* minDmgIdnt = new Identifier();
    minDmgIdnt->id = 0;
    minDmgIdnt->uiName = "MIN DAMAGE " + FormatIncrease(currentMinDamage, nextMinDamage);
    pagedData.data.push_back(minDmgIdnt);

    Identifier* maxDmgIdnt = new Identifier();
    maxDmgIdnt->id = 0;
    maxDmgIdnt->uiName = "MAX DAMAGE " + FormatIncrease(currentMaxDamage, nextMaxDamage);
    pagedData.data.push_back(maxDmgIdnt);

    for (uint32 i = 0; i < pagedData.data.size(); i++)
        pagedData.data[i]->name = Acore::ToString(i);

    pagedData.SortAndCalculateTotals();
}

void ItemUpgrade::BuildWeaponUpgradeInfoCatalogue(const Player* player, const Item* item)
{
    PagedData& pagedData = GetPagedData(player);
    pagedData.Reset();
    pagedData.upgradeStat = nullptr;
    pagedData.item.guid = item->GetGUID();
    pagedData.type = PAGED_DATA_TYPE_WEAPON_UPGRADE_ITEMS_CHECK_INFO;

    const UpgradeStat* weaponUpgrade = FindUpgradeForWeapon(player, item);
    if (weaponUpgrade == nullptr)
        return;

    Identifier* idnt = new Identifier();
    idnt->id = 0;
    idnt->name = "0";
    idnt->uiName = "Damage upgraded by " + FormatFloat(weaponUpgrade->statModPct) + "%";
    pagedData.data.push_back(idnt);

    std::pair<float, float> dmgInfo = GetItemProtoDamage(item);
    std::pair<float, float> upgradedDmgInfo = HandleWeaponModifier(player, item, dmgInfo.first, dmgInfo.second);

    Identifier* minDmgIdnt = new Identifier();
    minDmgIdnt->id = 0;
    minDmgIdnt->name = "1";
    minDmgIdnt->uiName = "MIN DAMAGE " + FormatIncrease(dmgInfo.first, upgradedDmgInfo.first);
    pagedData.data.push_back(minDmgIdnt);

    Identifier* maxDmgIdnt = new Identifier();
    maxDmgIdnt->id = 0;
    maxDmgIdnt->name = "2";
    maxDmgIdnt->uiName = "MAX DAMAGE " + FormatIncrease(dmgInfo.second, upgradedDmgInfo.second);
    pagedData.data.push_back(maxDmgIdnt);

    if (!item->IsEquipped())
    {
        Identifier* equipIdnt = new Identifier();
        equipIdnt->id = 2;
        equipIdnt->name = "3";
        equipIdnt->uiName = "[EQUIP ITEM]";
        equipIdnt->optionIcon = GOSSIP_ICON_BATTLE;
        pagedData.data.push_back(equipIdnt);
    }

    pagedData.SortAndCalculateTotals();
}

bool ItemUpgrade::MeetsRequirement(const Player* player, const UpgradeStatReq& req) const
{
    switch (req.reqType)
    {
        case REQ_TYPE_COPPER:
            return player->HasEnoughMoney((int32)req.reqVal1);
        case REQ_TYPE_HONOR:
            return player->GetHonorPoints() >= (uint32)req.reqVal1;
        case REQ_TYPE_ARENA:
            return player->GetArenaPoints() >= (uint32)req.reqVal1;
        case REQ_TYPE_ITEM:
            return player->HasItemCount((uint32)req.reqVal1, (uint32)req.reqVal2, true);
        case REQ_TYPE_NONE:
            return true;
    }

    return false;
}

bool ItemUpgrade::MeetsRequirement(const Player* player, const UpgradeStat* upgradeStat, const Item* item) const
{
    return MeetsRequirement(player, GetStatRequirements(upgradeStat, item));
}

bool ItemUpgrade::MeetsRequirement(const Player* player, const StatRequirementContainer* reqs) const
{
    if (EmptyRequirements(reqs))
        return true;

    for (const auto& req : *reqs)
        if (!MeetsRequirement(player, req))
            return false;

    return true;
}

void ItemUpgrade::TakeRequirements(Player* player, const UpgradeStat* upgradeStat, const Item* item)
{
    TakeRequirements(player, GetStatRequirements(upgradeStat, item));
}

void ItemUpgrade::TakeRequirements(Player* player, const StatRequirementContainer* reqs)
{
    if (EmptyRequirements(reqs))
        return;

    for (const auto& req : *reqs)
    {
        switch (req.reqType)
        {
            case REQ_TYPE_COPPER:
                player->ModifyMoney(-(int32)req.reqVal1);
                break;
            case REQ_TYPE_HONOR:
                player->ModifyHonorPoints(-(int32)req.reqVal1);
                break;
            case REQ_TYPE_ARENA:
                player->ModifyArenaPoints(-(int32)req.reqVal1);
                break;
            case REQ_TYPE_ITEM:
                player->DestroyItemCount((uint32)req.reqVal1, (uint32)req.reqVal2, true);
                break;
        }
    }
}

void ItemUpgrade::TakeWeaponUpgradeRequirements(Player* player)
{
    TakeRequirements(player, &weaponUpgradeReqs);
}

void ItemUpgrade::BuildStatsUpgradeCatalogue(const Player* player, const Item* item)
{
    PagedData& pagedData = GetPagedData(player);
    pagedData.Reset();
    pagedData.item.guid = item->GetGUID();
    pagedData.upgradeStat = nullptr;
    pagedData.type = PAGED_DATA_TYPE_STATS;

    if (IsAllowedItem(item) && !IsBlacklistedItem(item))
    {
        std::vector<_ItemStat> statInfoList = LoadItemStatInfo(item);
        std::unordered_map<uint32, bool> processed;
        for (const UpgradeStat& stat : upgradeStatList)
        {
            if (processed.find(stat.statType) != processed.end())
                continue;

            if (!IsAllowedStatType(stat.statType))
                continue;

            const _ItemStat* statInfo = GetStatByType(statInfoList, stat.statType);
            if (!statInfo)
                continue;

            processed[stat.statType] = true;

            const UpgradeStat* foundUpgrade = FindUpgradeForItem(player, item, stat.statType);
            const UpgradeStat* currentUpgrade = nullptr;
            bool atMaxRank = false;
            Identifier* identifier = new Identifier();
            std::ostringstream oss;
            oss << "UPGRADE " << StatTypeToString(statInfo->ItemStatType) << " ";
            if (foundUpgrade != nullptr)
            {
                currentUpgrade = foundUpgrade;

                const UpgradeStat* nextUpgrade = FindUpgradeStat(stat.statType, foundUpgrade->statRank + 1);
                if (nextUpgrade == nullptr)
                {
                    oss << "[RANK " << foundUpgrade->statRank << " |cffb50505MAX|r]";
                    identifier->id = foundUpgrade->statId;
                    atMaxRank = true;
                }
                else
                {
                    oss << "[RANK " << foundUpgrade->statRank << " -> " << "|cff056e3a" << foundUpgrade->statRank + 1 << "|r" << "]";
                    identifier->id = nextUpgrade->statId;
                    foundUpgrade = nextUpgrade;
                }
            }
            else
            {
                foundUpgrade = FindUpgradeStat(stat.statType, 1);
                if (foundUpgrade == nullptr)
                    continue;

                oss << "[ACQUIRE RANK 1]";
                identifier->id = foundUpgrade->statId;
            }

            oss << " " << "[" << foundUpgrade->statModPct << "% increase - ";
            oss << "|cffb50505" << statInfo->ItemStatValue << "|r --> ";
            oss << "|cff056e3a" << CalculateModPct(statInfo->ItemStatValue, foundUpgrade) << "|r]";
            if (currentUpgrade != nullptr)
            {
                oss << " [CURRENT: " << CalculateModPct(statInfo->ItemStatValue, currentUpgrade);
                if (!CanApplyUpgradeForItem(item, currentUpgrade))
                    oss << ", |cffb50505INACTIVE|r" << "]";
                else
                    oss << "]";
            }

            if (!atMaxRank && !CanApplyUpgradeForItem(item, foundUpgrade))
                oss << " [|cffb50505UPGRADE FORBIDDEN|r]";

            identifier->uiName = oss.str();
            identifier->name = StatTypeToString(stat.statType);
            pagedData.data.push_back(identifier);
        }
    }

    pagedData.SortAndCalculateTotals();
}

void ItemUpgrade::CreateUpgradesPctMap()
{
    upgradesPctMap.clear();
    for (const UpgradeStat& ustat : upgradeStatList)
        upgradesPctMap[ustat.statModPct].push_back(&ustat);
}

void ItemUpgrade::BuildStatsUpgradeCatalogueBulk(const Player* player, const Item* item)
{
    PagedData& pagedData = GetPagedData(player);
    pagedData.Reset();
    pagedData.item.guid = item->GetGUID();
    pagedData.upgradeStat = nullptr;
    pagedData.type = PAGED_DATA_TYPE_STATS_BULK;

    if (IsAllowedItem(item) && !IsBlacklistedItem(item))
    {
        for (const auto& upair : upgradesPctMap)
        {
            FloatIdentifier* identifier = new FloatIdentifier();
            identifier->id = pagedData.data.size();
            identifier->name = "";
            identifier->modPct = upair.first;
            identifier->uiName = "Upgrade ALL stats by " + FormatFloat(upair.first) + "%";
            pagedData.data.push_back(identifier);
        }
    }

    pagedData.SortAndCalculateTotals();
}

void ItemUpgrade::BuildStatsUpgradeByPctCatalogueBulk(const Player* player, const Item* item, float pct)
{
    PagedData& pagedData = GetPagedData(player);
    pagedData.Reset();
    pagedData.item.guid = item->GetGUID();
    pagedData.upgradeStat = nullptr;
    pagedData.type = PAGED_DATA_TYPE_STAT_UPGRADE_BULK;
    pagedData.pct = pct;

    if (upgradesPctMap.find(pct) != upgradesPctMap.end())
    {
        const std::vector<const UpgradeStat*>& upgrades = upgradesPctMap.at(pct);
        std::vector<_ItemStat> statInfoList = LoadItemStatInfo(item);
        for (const UpgradeStat* stat : upgrades)
        {
            const _ItemStat* foundStat = GetStatByType(statInfoList, stat->statType);
            if (foundStat == nullptr)
                continue;

            std::ostringstream oss;
            std::string statTypeStr = StatTypeToString(stat->statType);
            if (!IsAllowedStatType(stat->statType))
                oss << "|cffb50505Won't|r upgrade " << statTypeStr << ": stat not allowed for upgrade";
            else if (!CanApplyUpgradeForItem(item, stat))
                oss << "|cffb50505Won't|r upgrade " << statTypeStr << ": rank not allowed for this item";
            else
            {
                const UpgradeStat* currentUpgrade = FindUpgradeForItem(player, item, stat->statType);
                bool willUpgrade = false;
                if (currentUpgrade != nullptr)
                {
                    const UpgradeStat* nextUpgrade = FindUpgradeStat(stat->statType, currentUpgrade->statRank + 1);
                    if (nextUpgrade == nullptr)
                        oss << "|cffb50505Won't|r upgrade " << statTypeStr << ": already at max rank";
                    else
                    {
                        if (currentUpgrade->statRank == stat->statRank - 1)
                            willUpgrade = true;
                        else if (currentUpgrade->statRank >= stat->statRank)
                            oss << "|cffb50505Won't|r upgrade " << statTypeStr << ": rank already acquired";
                        else
                            oss << "|cffb50505Won't|r upgrade " << statTypeStr << ": need to acquire previous rank(s)";
                    }
                }
                else
                {
                    if (stat->statRank == 1)
                        willUpgrade = true;
                    else
                        oss << "|cffb50505Won't|r upgrade " << statTypeStr << ": need to acquire previous rank(s)";
                }

                if (willUpgrade)
                {
                    oss << "|cff056e3aWill|r upgrade " << statTypeStr << " to rank " << stat->statRank;
                    oss << " [" << FormatFloat(stat->statModPct) << "% increase, ";
                    oss << "|cffb50505" << foundStat->ItemStatValue << "|r --> ";
                    oss << "|cff056e3a" << CalculateModPct(foundStat->ItemStatValue, stat) << "|r]";

                    if (currentUpgrade != nullptr)
                        oss << " [CURRENT: " << CalculateModPct(foundStat->ItemStatValue, currentUpgrade) << "]";
                }
            }

            Identifier* identifier = new Identifier();
            identifier->id = 0;
            identifier->name = statTypeStr;
            identifier->uiName = oss.str();
            pagedData.data.push_back(identifier);
        }
    }

    pagedData.SortAndCalculateTotals();
}

void ItemUpgrade::BuildStatsRequirementsCatalogueBulk(const Player* player, const Item* item, float pct)
{
    PagedData& pagedData = GetPagedData(player);
    pagedData.Reset();
    pagedData.item.guid = item->GetGUID();
    pagedData.upgradeStat = nullptr;
    pagedData.type = PAGED_DATA_TYPE_REQS_BULK;

    StatRequirementContainer reqs = BuildBulkRequirements(FindAllUpgradeableRanks(player, item, pct), item);
    BuildRequirementsPage(player, pagedData, &reqs);

    pagedData.SortAndCalculateTotals();
}

ItemUpgrade::StatRequirementContainer ItemUpgrade::BuildBulkRequirements(const std::unordered_map<uint32, const UpgradeStat*>& upgrades, const Item* item) const
{
    StatRequirementContainer reqs;
    if (upgrades.empty())
        return reqs;

    uint64 copper = 0;
    uint32 arena = 0;
    uint32 honor = 0;
    std::unordered_map<uint32, uint32> itemMap;
    for (const auto& upair : upgrades)
    {
        const StatRequirementContainer* ureq = GetStatRequirements(upair.second, item);
        if (EmptyRequirements(ureq))
            continue;

        for (const UpgradeStatReq& statReq : *ureq)
        {
            switch (statReq.reqType)
            {
                case REQ_TYPE_COPPER:
                    copper += (uint32)statReq.reqVal1;
                    break;
                case REQ_TYPE_HONOR:
                    honor += (uint32)statReq.reqVal1;
                    break;
                case REQ_TYPE_ARENA:
                    arena += (uint32)statReq.reqVal1;
                    break;
                case REQ_TYPE_ITEM:
                    itemMap[(uint32)statReq.reqVal1] += (uint32)statReq.reqVal2;
                    break;
            }
        }
    }

    if (copper != 0)
    {
        if (copper > MAX_MONEY_AMOUNT)
            copper = MAX_MONEY_AMOUNT;

        reqs.push_back(UpgradeStatReq(0, REQ_TYPE_COPPER, (float)copper));
    }

    if (honor != 0)
        reqs.push_back(UpgradeStatReq(0, REQ_TYPE_HONOR, (float)honor));

    if (arena != 0)
        reqs.push_back(UpgradeStatReq(0, REQ_TYPE_ARENA, (float)arena));

    for (const auto& ipair : itemMap)
        reqs.push_back(UpgradeStatReq(0, REQ_TYPE_ITEM, (float)ipair.first, (float)ipair.second));

    return reqs;
}

std::unordered_map<uint32, const ItemUpgrade::UpgradeStat*> ItemUpgrade::FindAllUpgradeableRanks(const Player* player, const Item* item, float pct) const
{
    std::unordered_map<uint32, const UpgradeStat*> possibleUpgrades;
    if (upgradesPctMap.find(pct) != upgradesPctMap.end())
    {
        const std::vector<const UpgradeStat*>& upgrades = upgradesPctMap.at(pct);
        std::vector<_ItemStat> statInfoList = LoadItemStatInfo(item);
        for (const UpgradeStat* stat : upgrades)
        {
            const _ItemStat* foundStat = GetStatByType(statInfoList, stat->statType);
            if (foundStat == nullptr)
                continue;

            if (!IsAllowedStatType(stat->statType))
                continue;

            if (!CanApplyUpgradeForItem(item, stat))
                continue;

            const UpgradeStat* currentUpgrade = FindUpgradeForItem(player, item, stat->statType);
            if (currentUpgrade != nullptr)
            {
                const UpgradeStat* nextUpgrade = FindUpgradeStat(stat->statType, currentUpgrade->statRank + 1);
                if (nextUpgrade != nullptr && currentUpgrade->statRank == stat->statRank - 1)
                    possibleUpgrades[stat->statType] = stat;
            }
            else
            {
                if (stat->statRank == 1)
                    possibleUpgrades[stat->statType] = stat;
            }
        }
    }
    return possibleUpgrades;
}

/*static*/ int32 ItemUpgrade::CalculateModPct(int32 value, const UpgradeStat* upgradeStat)
{
    int32 newAmount = (int32)(value * (1 + upgradeStat->statModPct / 100.0f));
    return std::max(newAmount, value + upgradeStat->statRank);
}

/*static*/ float ItemUpgrade::CalculateModPctF(float value, const UpgradeStat* upgradeStat)
{
    float newAmount = value * (1.0f + upgradeStat->statModPct / 100.0f);
    return std::max(newAmount, value + upgradeStat->statRank);
}

/*static*/ bool ItemUpgrade::CompareIdentifier(const Identifier* a, const Identifier* b)
{
    if (a->GetType() == FLOAT_IDENTIFIER && b->GetType() == FLOAT_IDENTIFIER)
        return ((FloatIdentifier*)a)->modPct < ((FloatIdentifier*)b)->modPct;

    return a->name < b->name;
}

const _ItemStat* ItemUpgrade::GetStatByType(const std::vector<_ItemStat>& statInfo, uint32 statType) const
{
    std::vector<_ItemStat>::const_iterator citer = std::find_if(statInfo.begin(), statInfo.end(), [&](const _ItemStat& stat) { return stat.ItemStatType == statType; });
    if (citer != statInfo.end())
        return &*citer;
    return nullptr;
}

std::vector<_ItemStat> ItemUpgrade::LoadItemStatInfo(const Item* item) const
{
    std::vector<_ItemStat> statInfo;
    ItemTemplate const* proto = item->GetTemplate();

    for (uint8 i = 0; i < MAX_ITEM_PROTO_STATS; ++i)
    {
        if (i >= proto->StatsCount)
            continue;

        uint32 statType = proto->ItemStat[i].ItemStatType;
        if (proto->ItemStat[i].ItemStatValue > 0)
        {
            _ItemStat stat;
            stat.ItemStatType = statType;
            stat.ItemStatValue = proto->ItemStat[i].ItemStatValue;
            statInfo.push_back(stat);
        }
    }

    for (uint32 slot = PROP_ENCHANTMENT_SLOT_0; slot < MAX_ENCHANTMENT_SLOT; ++slot)
    {
        uint32 enchant_id = item->GetEnchantmentId(EnchantmentSlot(slot));
        if (!enchant_id)
            continue;

        SpellItemEnchantmentEntry const* pEnchant = sSpellItemEnchantmentStore.LookupEntry(enchant_id);
        if (!pEnchant)
            continue;

        for (int s = 0; s < MAX_SPELL_ITEM_ENCHANTMENT_EFFECTS; ++s)
        {
            uint32 enchant_display_type = pEnchant->type[s];
            uint32 enchant_amount = pEnchant->amount[s];
            uint32 enchant_spell_id = pEnchant->spellid[s];

            if (enchant_display_type == ITEM_ENCHANTMENT_TYPE_STAT)
            {
                if (!enchant_amount)
                {
                    ItemRandomSuffixEntry const* item_rand_suffix = sItemRandomSuffixStore.LookupEntry(std::abs(item->GetItemRandomPropertyId()));
                    if (item_rand_suffix)
                    {
                        for (int k = 0; k < MAX_ITEM_ENCHANTMENT_EFFECTS; ++k)
                        {
                            if (item_rand_suffix->Enchantment[k] == enchant_id)
                            {
                                enchant_amount = uint32((item_rand_suffix->AllocationPct[k] * item->GetItemSuffixFactor()) / 10000);
                                break;
                            }
                        }
                    }
                }
                _ItemStat stat;
                stat.ItemStatType = enchant_spell_id;
                stat.ItemStatValue = enchant_amount;
                statInfo.push_back(stat);
            }
        }
    }

    return statInfo;
}

std::string ItemUpgrade::StatTypeToString(uint32 statType) const
{
    static std::unordered_map<uint32, std::string> statTypeToStrMap = {
        {ITEM_MOD_MANA, "Mana"}, {ITEM_MOD_HEALTH, "Health"}, {ITEM_MOD_AGILITY, "Agility"},
        {ITEM_MOD_STRENGTH, "Strength"}, {ITEM_MOD_INTELLECT, "Intellect"}, {ITEM_MOD_SPIRIT, "Spirit"},
        {ITEM_MOD_STAMINA, "Stamina"}, {ITEM_MOD_DEFENSE_SKILL_RATING, "Defense Rating"}, {ITEM_MOD_DODGE_RATING, "Dodge Rating"},
        {ITEM_MOD_PARRY_RATING, "Parry Rating"}, {ITEM_MOD_BLOCK_RATING, "Block Rating"}, {ITEM_MOD_HIT_MELEE_RATING, "Melee Hit Rating"},
        {ITEM_MOD_HIT_RANGED_RATING, "Ranged Hit Rating"}, {ITEM_MOD_HIT_SPELL_RATING, "Spell Hit Rating"}, {ITEM_MOD_CRIT_MELEE_RATING, "Melee Crit Rating"},
        {ITEM_MOD_CRIT_RANGED_RATING, "Ranged Crit Rating"}, {ITEM_MOD_CRIT_SPELL_RATING, "Spell Crit Rating"}, {ITEM_MOD_HIT_TAKEN_MELEE_RATING, "Melee Hit Taken Rating"},
        {ITEM_MOD_HIT_TAKEN_RANGED_RATING, "Ranged Hit Taken Rating"}, {ITEM_MOD_HIT_TAKEN_SPELL_RATING, "Spell Hit Taken Rating"}, {ITEM_MOD_CRIT_TAKEN_MELEE_RATING, "Melee Crit Taken Rating"},
        {ITEM_MOD_CRIT_TAKEN_RANGED_RATING, "Ranged Crit Taken Rating"}, {ITEM_MOD_CRIT_TAKEN_SPELL_RATING, "Spell Crit Taken Rating"}, {ITEM_MOD_HASTE_MELEE_RATING, "Melee Haste Rating"},
        {ITEM_MOD_HASTE_RANGED_RATING, "Ranged Haste Rating"}, {ITEM_MOD_HASTE_SPELL_RATING, "Spell Haste Rating"}, {ITEM_MOD_HIT_RATING, "Hit Rating"},
        {ITEM_MOD_CRIT_RATING, "Crit Rating"}, {ITEM_MOD_HIT_TAKEN_RATING, "Hit Taken Rating"}, {ITEM_MOD_CRIT_TAKEN_RATING, "Crit Taken Rating"},
        {ITEM_MOD_RESILIENCE_RATING, "Resilience Rating"}, {ITEM_MOD_HASTE_RATING, "Haste Rating"}, {ITEM_MOD_EXPERTISE_RATING, "Expertise"},
        {ITEM_MOD_ATTACK_POWER, "Attack Power"}, {ITEM_MOD_RANGED_ATTACK_POWER, "Ranged Attack Power"}, {ITEM_MOD_MANA_REGENERATION, "Mana Regen"},
        {ITEM_MOD_ARMOR_PENETRATION_RATING, "Armor Penetration"}, {ITEM_MOD_SPELL_POWER, "Spell Power"}, {ITEM_MOD_HEALTH_REGEN, "HP Regen"},
        {ITEM_MOD_SPELL_PENETRATION, "Spell Penetration"}, {ITEM_MOD_BLOCK_VALUE, "Block Value"}
    };

    if (statTypeToStrMap.find(statType) != statTypeToStrMap.end())
        return statTypeToStrMap.at(statType);

    return "unknown";
}

bool ItemUpgrade::IsValidStatType(uint32 statType) const
{
    return StatTypeToString(statType) != "unknown";
}

std::string ItemUpgrade::ItemLinkForUI(const Item* item, const Player* player) const
{
    const ItemTemplate* proto = item->GetTemplate();
    std::ostringstream oss;
    oss << ItemIcon(proto);
    oss << ItemLink(player, proto, item->GetItemRandomPropertyId());
    return oss.str();
}

const ItemUpgrade::UpgradeStat* ItemUpgrade::FindUpgradeStat(uint32 statId) const
{
    return _FindUpgradeStat(upgradeStatList, [&](const UpgradeStat& stat) { return stat.statId == statId; });
}

const ItemUpgrade::UpgradeStat* ItemUpgrade::FindUpgradeStat(uint32 statType, uint16 rank) const
{
    return _FindUpgradeStat(upgradeStatList, [&](const UpgradeStat& stat) { return stat.statType == statType && stat.statRank == rank; });
}

const ItemUpgrade::UpgradeStat* ItemUpgrade::FindWeaponUpgradeStat(float pct) const
{
    return _FindUpgradeStat(weaponUpgradeStats, [&](const UpgradeStat& stat) { return stat.statModPct == pct; });
}

const ItemUpgrade::UpgradeStat* ItemUpgrade::FindNearestWeaponUpgradeStat(float pct) const
{
    if (weaponUpgradeStats.empty())
        return nullptr;

    for (int i = weaponUpgradeStats.size() - 1; i >= 0; i--)
        if (weaponUpgradeStats[i].statModPct < pct)
            return &weaponUpgradeStats[i];

    for (int i = 0; i < weaponUpgradeStats.size(); i++)
        if (weaponUpgradeStats[i].statModPct > pct)
            return &weaponUpgradeStats[i];

    return nullptr;
}

const ItemUpgrade::UpgradeStat* ItemUpgrade::FindNextWeaponUpgradeStat(float pct) const
{
    if (weaponUpgradeStats.empty())
        return nullptr;

    for (int i = 0; i < weaponUpgradeStats.size(); i++)
        if (weaponUpgradeStats[i].statModPct > pct)
            return &weaponUpgradeStats[i];

    return nullptr;
}

std::vector<const ItemUpgrade::UpgradeStat*> ItemUpgrade::_FindUpgradesForItem(const CharacterUpgradeContainer& characterUpgradeDataContainer, const Player* player, const Item* item) const
{
    std::vector<const UpgradeStat*> statsForItem;
    if (characterUpgradeDataContainer.find(player->GetGUID().GetCounter()) == characterUpgradeDataContainer.end())
        return statsForItem;

    const std::vector<CharacterUpgrade>& upgrades = characterUpgradeDataContainer.at(player->GetGUID().GetCounter());
    for (auto const& upgrade : upgrades)
        if (upgrade.itemGuid == item->GetGUID())
            statsForItem.push_back(upgrade.upgradeStat);

    return statsForItem;
}

std::vector<const ItemUpgrade::UpgradeStat*> ItemUpgrade::FindUpgradesForItem(const Player* player, const Item* item) const
{
    return _FindUpgradesForItem(characterUpgradeData, player, item);
}

const ItemUpgrade::UpgradeStat* ItemUpgrade::FindUpgradeForItem(const Player* player, const Item* item, uint32 statType) const
{
    std::vector<const UpgradeStat*> statsForItem = FindUpgradesForItem(player, item);
    if (statsForItem.empty())
        return nullptr;

    std::vector<const UpgradeStat*>::const_iterator citer = std::find_if(statsForItem.begin(), statsForItem.end(), [&](const UpgradeStat* upgradeStat) { return upgradeStat->statType == statType; });
    if (citer != statsForItem.end())
        return *citer;

    return nullptr;
}

const ItemUpgrade::UpgradeStat* ItemUpgrade::FindUpgradeForWeapon(const Player* player, const Item* item) const
{
    std::vector<const UpgradeStat*> weaponUpgrades = _FindUpgradesForItem(characterWeaponUpgradeData, player, item);
    if (weaponUpgrades.empty())
        return nullptr;

    return weaponUpgrades[0];
}

/*static*/ std::string ItemUpgrade::CopperToMoneyStr(uint32 money, bool colored)
{
    uint32 gold = money / GOLD;
    uint32 silver = (money % GOLD) / SILVER;
    uint32 copper = (money % GOLD) % SILVER;

    std::ostringstream oss;
    if (gold > 0)
    {
        if (colored)
            oss << gold << "|cffb3aa34g|r";
        else
            oss << gold << "g";
    }
    if (silver > 0)
    {
        if (colored)
            oss << silver << "|cff7E7C7Fs|r";
        else
            oss << silver << "s";
    }
    if (copper > 0)
    {
        if (colored)
            oss << copper << "|cff974B29c|r";
        else
            oss << copper << "c";
    }

    return oss.str();
}

/*static*/ std::string ItemUpgrade::FormatFloat(float val, uint32 decimals)
{
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(decimals) << val;
    return oss.str();
}

/*static*/ std::string ItemUpgrade::FormatIncrease(float prev, float next)
{
    std::ostringstream oss;
    oss << "[";
    oss << "|cffb50505" << FormatFloat(prev) << "|r ";
    oss << "--> ";
    oss << "|cff056e3a" << FormatFloat(next) << "|r";
    oss << "]";
    return oss.str();
}

void ItemUpgrade::SetReloading(bool value)
{
    reloading = value;
}

bool ItemUpgrade::GetReloading() const
{
    return reloading;
}

void ItemUpgrade::HandleDataReload(bool apply)
{
    const SessionMap& sessions = sWorld->GetAllSessions();
    SessionMap::const_iterator itr;
    for (itr = sessions.begin(); itr != sessions.end(); ++itr)
        if (itr->second && itr->second->GetPlayer() && itr->second->GetPlayer()->IsInWorld())
            HandleDataReload(itr->second->GetPlayer(), apply);
}

void ItemUpgrade::HandleDataReload(Player* player, bool apply)
{
    std::vector<Item*> playerItems = GetPlayerItems(player, true);
    std::vector<Item*>::iterator iter = playerItems.begin();
    for (iter; iter != playerItems.end(); ++iter)
    {
        Item* item = *iter;

        if (!item->IsEquipped())
            continue;

        player->_ApplyItemMods(item, item->GetSlot(), apply);
    }

    if (apply)
        UpdateVisualCache(player);
}

std::vector<Item*> ItemUpgrade::GetPlayerItems(const Player* player, bool inBankAlso) const
{
    std::vector<Item*> items;
    for (uint8 i = INVENTORY_SLOT_ITEM_START; i < INVENTORY_SLOT_ITEM_END; i++)
        if (Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, i))
            items.push_back(item);

    for (uint8 i = INVENTORY_SLOT_BAG_START; i < INVENTORY_SLOT_BAG_END; i++)
        if (Bag* bag = player->GetBagByPos(i))
            for (uint32 j = 0; j < bag->GetBagSize(); j++)
                if (Item* item = player->GetItemByPos(i, j))
                    items.push_back(item);

    for (uint8 i = EQUIPMENT_SLOT_START; i < EQUIPMENT_SLOT_END; i++)
        if (Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, i))
            items.push_back(item);

    if (inBankAlso)
    {
        for (uint8 i = BANK_SLOT_ITEM_START; i < BANK_SLOT_ITEM_END; i++)
            if (Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, i))
                items.push_back(item);

        for (uint8 i = BANK_SLOT_BAG_START; i < BANK_SLOT_BAG_END; i++)
            if (Bag* bag = player->GetBagByPos(i))
                for (uint32 j = 0; j < bag->GetBagSize(); j++)
                    if (Item* item = player->GetItemByPos(i, j))
                        items.push_back(item);
    }

    return items;
}

bool ItemUpgrade::IsAllowedItem(const Item* item) const
{
    if (allowedItems.empty())
        return true;

    return allowedItems.find(item->GetEntry()) != allowedItems.end();
}

bool ItemUpgrade::IsBlacklistedItem(const Item* item) const
{
    if (blacklistedItems.empty())
        return false;

    return blacklistedItems.find(item->GetEntry()) != blacklistedItems.end();
}

void ItemUpgrade::SendItemPacket(Player* player, Item* item) const
{
    ItemTemplate const* pProto = sObjectMgr->GetItemTemplate(item->GetEntry());
    std::string Name = pProto->Name1;
    std::string Description = pProto->Description;

    int loc_idx = player->GetSession()->GetSessionDbLocaleIndex();
    if (loc_idx >= 0)
    {
        if (ItemLocale const* il = sObjectMgr->GetItemLocale(pProto->ItemId))
        {
            ObjectMgr::GetLocaleString(il->Name, loc_idx, Name);
            ObjectMgr::GetLocaleString(il->Description, loc_idx, Description);
        }
    }
    // guess size
    WorldPacket queryData(SMSG_ITEM_QUERY_SINGLE_RESPONSE, 600);
    queryData << pProto->ItemId;
    queryData << pProto->Class;
    queryData << pProto->SubClass;
    queryData << pProto->SoundOverrideSubclass;
    queryData << Name;
    queryData << uint8(0x00);                                //pProto->Name2; // blizz not send name there, just uint8(0x00); <-- \0 = empty string = empty name...
    queryData << uint8(0x00);                                //pProto->Name3; // blizz not send name there, just uint8(0x00);
    queryData << uint8(0x00);                                //pProto->Name4; // blizz not send name there, just uint8(0x00);
    queryData << pProto->DisplayInfoID;
    queryData << pProto->Quality;
    queryData << pProto->Flags;
    queryData << pProto->Flags2;
    queryData << pProto->BuyPrice;
    queryData << pProto->SellPrice;
    queryData << pProto->InventoryType;
    queryData << pProto->AllowableClass;
    queryData << pProto->AllowableRace;
    if (GetBoolConfig(CONFIG_ITEM_UPGRADE_SEND_PACKETS) && pProto->StatsCount > 0)
        queryData << CalculateItemLevel(player, item).second;
    else
        queryData << pProto->ItemLevel;
    queryData << pProto->RequiredLevel;
    queryData << pProto->RequiredSkill;
    queryData << pProto->RequiredSkillRank;
    queryData << pProto->RequiredSpell;
    queryData << pProto->RequiredHonorRank;
    queryData << pProto->RequiredCityRank;
    queryData << pProto->RequiredReputationFaction;
    queryData << pProto->RequiredReputationRank;
    queryData << int32(pProto->MaxCount);
    queryData << int32(pProto->Stackable);
    queryData << pProto->ContainerSlots;
    queryData << pProto->StatsCount;                         // item stats count
    for (uint32 i = 0; i < pProto->StatsCount; ++i)
    {
        queryData << pProto->ItemStat[i].ItemStatType;
        if (GetBoolConfig(CONFIG_ITEM_UPGRADE_SEND_PACKETS))
            queryData << HandleStatModifier(player, item, pProto->ItemStat[i].ItemStatType, pProto->ItemStat[i].ItemStatValue, MAX_ENCHANTMENT_SLOT);
        else
            queryData << pProto->ItemStat[i].ItemStatValue;
    }
    queryData << pProto->ScalingStatDistribution;            // scaling stats distribution
    queryData << pProto->ScalingStatValue;                   // some kind of flags used to determine stat values column
    for (int i = 0; i < MAX_ITEM_PROTO_DAMAGES; ++i)
    {
        if (GetBoolConfig(CONFIG_ITEM_UPGRADE_SEND_PACKETS))
        {
            std::pair<float, float> upgradedDmgInfo = HandleWeaponModifier(player, item, pProto->Damage[i].DamageMin, pProto->Damage[i].DamageMax);
            queryData << upgradedDmgInfo.first;
            queryData << upgradedDmgInfo.second;
        }
        else
        {
            queryData << pProto->Damage[i].DamageMin;
            queryData << pProto->Damage[i].DamageMax;
        }
        
        queryData << pProto->Damage[i].DamageType;
    }

    // resistances (7)
    queryData << pProto->Armor;
    queryData << pProto->HolyRes;
    queryData << pProto->FireRes;
    queryData << pProto->NatureRes;
    queryData << pProto->FrostRes;
    queryData << pProto->ShadowRes;
    queryData << pProto->ArcaneRes;

    queryData << pProto->Delay;
    queryData << pProto->AmmoType;
    queryData << pProto->RangedModRange;

    for (int s = 0; s < MAX_ITEM_PROTO_SPELLS; ++s)
    {
        // send DBC data for cooldowns in same way as it used in Spell::SendSpellCooldown
        // use `item_template` or if not set then only use spell cooldowns
        SpellInfo const* spell = sSpellMgr->GetSpellInfo(pProto->Spells[s].SpellId);
        if (spell)
        {
            bool db_data = pProto->Spells[s].SpellCooldown >= 0 || pProto->Spells[s].SpellCategoryCooldown >= 0;

            queryData << pProto->Spells[s].SpellId;
            queryData << pProto->Spells[s].SpellTrigger;
            queryData << int32(pProto->Spells[s].SpellCharges);

            if (db_data)
            {
                queryData << uint32(pProto->Spells[s].SpellCooldown);
                queryData << uint32(pProto->Spells[s].SpellCategory);
                queryData << uint32(pProto->Spells[s].SpellCategoryCooldown);
            }
            else
            {
                queryData << uint32(spell->RecoveryTime);
                queryData << uint32(spell->GetCategory());
                queryData << uint32(spell->CategoryRecoveryTime);
            }
        }
        else
        {
            queryData << uint32(0);
            queryData << uint32(0);
            queryData << uint32(0);
            queryData << uint32(-1);
            queryData << uint32(0);
            queryData << uint32(-1);
        }
    }
    queryData << pProto->Bonding;
    queryData << Description;
    queryData << pProto->PageText;
    queryData << pProto->LanguageID;
    queryData << pProto->PageMaterial;
    queryData << pProto->StartQuest;
    queryData << pProto->LockID;
    queryData << int32(pProto->Material);
    queryData << pProto->Sheath;
    queryData << pProto->RandomProperty;
    queryData << pProto->RandomSuffix;
    queryData << pProto->Block;
    queryData << pProto->ItemSet;
    queryData << pProto->MaxDurability;
    queryData << pProto->Area;
    queryData << pProto->Map;                                // Added in 1.12.x & 2.0.1 client branch
    queryData << pProto->BagFamily;
    queryData << pProto->TotemCategory;
    for (int s = 0; s < MAX_ITEM_PROTO_SOCKETS; ++s)
    {
        queryData << pProto->Socket[s].Color;
        queryData << pProto->Socket[s].Content;
    }
    queryData << pProto->socketBonus;
    queryData << pProto->GemProperties;
    queryData << pProto->RequiredDisenchantSkill;
    queryData << pProto->ArmorDamageModifier;
    queryData << pProto->Duration;                           // added in 2.4.2.8209, duration (seconds)
    queryData << pProto->ItemLimitCategory;                  // WotLK, ItemLimitCategory
    queryData << pProto->HolidayId;                          // Holiday.dbc?
    player->GetSession()->SendPacket(&queryData);
}

void ItemUpgrade::UpdateVisualCache(Player* player)
{
    std::map<uint32, std::vector<ItemUpgradeInfo>> entryUpgradeMap;
    std::vector<Item*> items = GetPlayerItems(player, true);
    std::vector<Item*>::const_iterator citer = items.begin();
    for (citer; citer != items.end(); ++citer)
    {
        const Item* item = *citer;
        ItemUpgradeInfo upgradeInfo;
        upgradeInfo.itemGuid = item->GetGUID();
        upgradeInfo.upgrades = FindUpgradesForItem(player, item);
        upgradeInfo.weaponUpgrade = FindUpgradeForWeapon(player, item);
        entryUpgradeMap[item->GetEntry()].push_back(upgradeInfo);
    }

    auto chooseVisualItem = [&](const uint32 entry) -> const ItemUpgradeInfo* {
        auto emptyUpgrade = [](const ItemUpgradeInfo* itemUpgradeInfo)
        {
            return itemUpgradeInfo->upgrades.empty() && itemUpgradeInfo->weaponUpgrade == nullptr;
        };

        const std::vector<ItemUpgradeInfo>& upgradeInfo = entryUpgradeMap.at(entry);
        const ItemUpgradeInfo* highestStatUpgrade = &upgradeInfo[0];
        const ItemUpgradeInfo* highestWeaponUpgrade = &upgradeInfo[0];
        for (size_t i = 1; i < upgradeInfo.size(); i++)
        {
            const ItemUpgradeInfo &itemUpgradeInfo = upgradeInfo[i];
            if (itemUpgradeInfo.upgrades.size() > highestStatUpgrade->upgrades.size())
                highestStatUpgrade = &itemUpgradeInfo;
            if (itemUpgradeInfo.weaponUpgrade != nullptr && (highestWeaponUpgrade->weaponUpgrade == nullptr || itemUpgradeInfo.weaponUpgrade->statModPct > highestWeaponUpgrade->weaponUpgrade->statModPct)) {
                highestWeaponUpgrade = &itemUpgradeInfo;
            }
        }
        if (emptyUpgrade(highestStatUpgrade) && emptyUpgrade(highestWeaponUpgrade))
            return nullptr;

        if (GetItemVisualsPriority() == PRIORITIZE_STATS)
        {
            if (!highestStatUpgrade->upgrades.empty())
                return highestStatUpgrade;
            else
                if (highestWeaponUpgrade->weaponUpgrade != nullptr)
                    return highestWeaponUpgrade;
        }
        else
        {
            if (highestWeaponUpgrade->weaponUpgrade != nullptr)
                return highestWeaponUpgrade;
            else
            {
                if (!highestStatUpgrade->upgrades.empty())
                    return highestStatUpgrade;
            }
        }

        return nullptr;
    };

    for (const auto& p : entryUpgradeMap)
    {
        const ItemUpgradeInfo* itemUpgradeInfo = chooseVisualItem(p.first);
        if (itemUpgradeInfo != nullptr)
        {
            Item* item = player->GetItemByGuid(itemUpgradeInfo->itemGuid);
            if (item != nullptr)
                SendItemPacket(player, item);
        }
        else
        {
            for (const ItemUpgradeInfo& upgrInfo : p.second)
            {
                Item* item = player->GetItemByGuid(upgrInfo.itemGuid);
                if (item != nullptr)
                    SendItemPacket(player, item);
            }
        }
    }
}

void ItemUpgrade::VisualFeedback(Player* player)
{
    player->CastSpell(player, VISUAL_FEEDBACK_SPELL_ID, true);
}

std::pair<uint32, uint32> ItemUpgrade::CalculateItemLevel(const Player* player, Item* item, const UpgradeStat* upgrade) const
{
    std::unordered_map<uint32, const UpgradeStat*> upgrades;
    if (upgrade != nullptr)
        upgrades[upgrade->statType] = upgrade;
    return CalculateItemLevel(player, item, upgrades);
}

std::pair<uint32, uint32> ItemUpgrade::CalculateItemLevel(const Player* player, Item* item, std::unordered_map<uint32, const UpgradeStat*> upgrades) const
{
    const ItemTemplate* proto = item->GetTemplate();
    std::vector<_ItemStat> originalStats = LoadItemStatInfo(item);
    if (originalStats.empty())
        return std::make_pair(proto->ItemLevel, proto->ItemLevel);

    uint32 originalSum = std::accumulate(originalStats.begin(), originalStats.end(), 0, [&](uint32 a, const _ItemStat& stat) { return a + stat.ItemStatValue; });
    uint32 upgradedSum = 0;

    for (const _ItemStat& stat : originalStats)
    {
        if (upgrades.find(stat.ItemStatType) != upgrades.end())
            upgradedSum += (uint32)CalculateModPct(stat.ItemStatValue, upgrades.at(stat.ItemStatType));
        else
            upgradedSum += HandleStatModifier(player, item, stat.ItemStatType, stat.ItemStatValue, MAX_ENCHANTMENT_SLOT);
    }

    if (upgradedSum <= originalSum)
        return std::make_pair(proto->ItemLevel, proto->ItemLevel);

    return std::make_pair(proto->ItemLevel, (upgradedSum * proto->ItemLevel) / originalSum);
}

bool ItemUpgrade::TryAddItem(Player* player, uint32 entry, uint32 count, bool add)
{
    const ItemTemplate* proto = sObjectMgr->GetItemTemplate(entry);
    if (proto != nullptr)
    {
        ItemPosCountVec dest;
        InventoryResult msg = player->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, entry, count);
        if (msg != EQUIP_ERR_OK)
        {
            std::ostringstream oss;
            oss << "Trying to add " << count << "x " << ItemLink(player, proto, 0);
            oss << " failed, check your inventory space and retry.";
            SendMessage(player, oss.str());
            return false;
        }

        if (add)
        {
            Item* tokenItem = player->StoreNewItem(dest, entry, true);
            player->SendNewItem(tokenItem, count, true, false);
        }
    }
    return true;
}

bool ItemUpgrade::PurgeUpgrade(Player* player, Item* item)
{
    std::vector<const ItemUpgrade::UpgradeStat*> upgrades = FindUpgradesForItem(player, item);
    if (!upgrades.empty())
    {
        if (!TryAddItem(player, (uint32)GetIntConfig(CONFIG_ITEM_UPGRADE_PURGE_TOKEN), (uint32)GetIntConfig(CONFIG_ITEM_UPGRADE_PURGE_TOKEN_COUNT), true))
            return false;

        if (!RefundEverything(player, item, upgrades))
            return false;

        if (item->IsEquipped())
            player->_ApplyItemMods(item, item->GetSlot(), false);

        RemoveItemUpgrade(player, item);

        if (item->IsEquipped())
            player->_ApplyItemMods(item, item->GetSlot(), true);

        SendItemPacket(player, item);

        return true;
    }
    else
        return false;
}

bool ItemUpgrade::PurgeWeaponUpgrade(Player* player, Item* item)
{
    const UpgradeStat* weaponUpgrade = FindUpgradeForWeapon(player, item);
    if (weaponUpgrade != nullptr)
    {
        StatRequirementContainer allReqs;
        for (const UpgradeStat& upgrade : weaponUpgradeStats)
        {
            if (weaponUpgrade->statModPct >= upgrade.statModPct)
            {
                for (const UpgradeStatReq& req : weaponUpgradeReqs)
                    allReqs.push_back(req);
            }
        }
        std::unordered_map<uint32, StatRequirementContainer> statRequirementMap;
        statRequirementMap[0] = allReqs;
        MergeStatRequirements(statRequirementMap, false);
        if (!TryRefundRequirements(player, statRequirementMap.at(0)))
            return false;

        if (item->IsEquipped())
            player->_ApplyItemMods(item, item->GetSlot(), false);

        RemoveWeaponUpgrade(player, item);

        if (item->IsEquipped())
            player->_ApplyItemMods(item, item->GetSlot(), true);

        SendItemPacket(player, item);

        return true;
    }
    else
        return false;
}

bool ItemUpgrade::TryRefundRequirements(Player* player, const StatRequirementContainer& reqs)
{
    for (const UpgradeStatReq& r : reqs)
    {
        switch (r.reqType)
        {
            case REQ_TYPE_COPPER:
                if (player->GetMoney() + (uint32)r.reqVal1 > MAX_MONEY_AMOUNT)
                {
                    SendMessage(player, "Can't refund copper, would be at gold limit.");
                    return false;
                }
                break;
            case REQ_TYPE_ITEM:
                if (!TryAddItem(player, (uint32)r.reqVal1, (uint32)r.reqVal2, false))
                    return false;
                break;
        }
    }

    for (const UpgradeStatReq& r : reqs)
    {
        switch (r.reqType)
        {
            case REQ_TYPE_COPPER:
                player->ModifyMoney((int32)r.reqVal1);
                break;
            case REQ_TYPE_HONOR:
                player->ModifyHonorPoints((int32)r.reqVal1);
                break;
            case REQ_TYPE_ARENA:
                player->ModifyArenaPoints((int32)r.reqVal1);
                break;
            case REQ_TYPE_ITEM:
                TryAddItem(player, (uint32)r.reqVal1, (uint32)r.reqVal2, true);
                break;
        }
    }

    return true;
}

bool ItemUpgrade::RefundEverything(Player* player, Item* item, const std::vector<const UpgradeStat*>& upgrades)
{
    if (!GetBoolConfig(CONFIG_ITEM_UPGRADE_REFUND_ALL_ON_PURGE))
        return true;

    uint32 index = 0;
    std::unordered_map<uint32, const UpgradeStat*> bulkUpgrades;
    for (const UpgradeStat* stat : upgrades)
    {
        uint16 rank = stat->statRank;
        while (rank >= 1)
        {
            bulkUpgrades[index++] = FindUpgradeStat(stat->statType, rank);
            rank--;
        }
    }

    StatRequirementContainer reqs = BuildBulkRequirements(bulkUpgrades, item);
    return TryRefundRequirements(player, reqs);
}

bool ItemUpgrade::ChooseRandomUpgrade(Player* player, Item* item)
{
    if (!GetBoolConfig(CONFIG_ITEM_UPGRADE_ENABLED))
        return false;

    if (!GetBoolConfig(CONFIG_ITEM_UPGRADE_RANDOM_UPGRADES))
        return false;

    if (!IsAllowedItem(item) || IsBlacklistedItem(item))
        return false;

    if (!FindUpgradesForItem(player, item).empty())
        return false;

    if (!roll_chance_f(GetFloatConfig(CONFIG_ITEM_UPGRADE_RANDOM_UPGRADES_CHANCE)))
        return false;

    uint32 statCountToUpgrade = urand(1, (uint32)GetIntConfig(CONFIG_ITEM_UPGRADE_RANDOM_UPGRADES_MAX_STATS));
    std::vector<_ItemStat> statTypes = LoadItemStatInfo(item);
    std::vector<const UpgradeStat*> upgrades;
    for (const _ItemStat& stat : statTypes)
    {
        if (!IsAllowedStatType(stat.ItemStatType))
            continue;

        const UpgradeStat* foundUpgradeStat = FindNearestUpgradeStat(stat.ItemStatType, (uint16)urand(1, (uint32)GetIntConfig(CONFIG_ITEM_UPGRADE_RANDOM_UPGRADES_MAX_RANK)), item);
        if (foundUpgradeStat != nullptr)
            upgrades.push_back(foundUpgradeStat);
    }

    if (upgrades.empty())
        return false;

    Acore::Containers::RandomShuffle(upgrades);
    uint32 currentStatCount = 0;
    for (const UpgradeStat* stat : upgrades)
    {
        if (currentStatCount == statCountToUpgrade)
            break;

        AddUpgradeForNewItem(player, item, stat, GetStatByType(statTypes, stat->statType));

        currentStatCount++;
    }

    return true;
}

bool ItemUpgrade::AddUpgradeForNewItem(Player* player, Item* item, const UpgradeStat* upgrade, const _ItemStat* stat)
{
    if (stat == nullptr)
        return false;

    const UpgradeStat* foundUpgrade = FindUpgradeForItem(player, item, upgrade->statType);
    std::vector<CharacterUpgrade>& upgrades = characterUpgradeData[player->GetGUID().GetCounter()];
    if (foundUpgrade != nullptr)
        return false;
    else
        AddItemUpgradeToDB(player, item, upgrade);

    CharacterUpgrade newUpgrade;
    newUpgrade.guid = player->GetGUID().GetCounter();
    newUpgrade.itemGuid = item->GetGUID();
    newUpgrade.upgradeStat = upgrade;
    upgrades.push_back(newUpgrade);

    std::ostringstream oss;
    oss << "|cffeb891a[ITEM UPGRADES SYSTEM]:|r";
    oss << " " << ItemLink(player, item);
    oss << " had " << StatTypeToString(upgrade->statType) << " upgraded to RANK " << upgrade->statRank << ".";
    oss << " Increase by " << upgrade->statModPct << "% [";
    oss << stat->ItemStatValue << " --> " << CalculateModPct(stat->ItemStatValue, upgrade) << "]";
    std::pair<uint32, uint32> itemLevel = CalculateItemLevel(player, item);
    oss << " [New ILVL: " << itemLevel.second << "]";
    SendMessage(player, oss.str());

    SendItemPacket(player, item);

    return true;
}

void ItemUpgrade::AddItemUpgradeToDB(const Player* player, const Item* item, const UpgradeStat* upgrade) const
{
    CharacterDatabase.Execute("INSERT INTO character_item_upgrade (guid, item_guid, stat_id) VALUES ({}, {}, {})",
        player->GetGUID().GetCounter(), item->GetGUID().GetCounter(), upgrade->statId);
}

const ItemUpgrade::UpgradeStat* ItemUpgrade::FindNearestUpgradeStat(uint32 statType, uint16 rank, const Item* item) const
{
    while (rank > 0)
    {
        const UpgradeStat* foundStat = FindUpgradeStat(statType, rank);
        if (foundStat != nullptr && CanApplyUpgradeForItem(item, foundStat))
            return foundStat;

        rank--;
    }

    return nullptr;
}

bool ItemUpgrade::IsAllowedStatForItem(const Item* item, const UpgradeStat* upgrade) const
{
    if (allowedStatItems.find(upgrade->statId) == allowedStatItems.end())
        return true;

    const std::set<uint32>& allowedStatsForItems = allowedStatItems.at(upgrade->statId);
    return allowedStatsForItems.find(item->GetEntry()) != allowedStatsForItems.end();
}

bool ItemUpgrade::IsBlacklistedStatForItem(const Item* item, const UpgradeStat* upgrade) const
{
    if (blacklistedStatItems.find(upgrade->statId) == blacklistedStatItems.end())
        return false;

    const std::set<uint32>& blacklistedStatsForItems = blacklistedStatItems.at(upgrade->statId);
    return blacklistedStatsForItems.find(item->GetEntry()) != blacklistedStatsForItems.end();
}

bool ItemUpgrade::CanApplyUpgradeForItem(const Item* item, const UpgradeStat* upgrade) const
{
    return IsAllowedStatForItem(item, upgrade) && !IsBlacklistedStatForItem(item, upgrade);
}

bool ItemUpgrade::CheckDataValidity() const
{
    if (upgradeStatList.empty())
        return true;

    bool ok = true;
    for (const UpgradeStat& upgrade : upgradeStatList)
    {
        if (!IsValidStatType(upgrade.statType))
        {
            LOG_ERROR("sql.sql", "FATAL: Table `mod_item_upgrade_stats` has invalid `stat_type` {}", upgrade.statType);
            ok = false;
        }
        if (upgrade.statModPct <= 0)
        {
            LOG_ERROR("sql.sql", "FATAL: Table `mod_item_upgrade_stats` has invalid `stat_mod_pct` {}", upgrade.statModPct);
            ok = false;
        }
    }

    if (!ok)
        return false;

    std::unordered_map<uint32, std::vector<uint16>> ranksMap;
    for (const UpgradeStat& upgrade : upgradeStatList)
        ranksMap[upgrade.statType].push_back(upgrade.statRank);

    for (auto& rpair : ranksMap)
    {
        std::vector<uint16>& ranks = rpair.second;
        std::sort(ranks.begin(), ranks.end());
        if (ranks[0] != 1)
        {
            ok = false;
            LOG_ERROR("sql.sql", "FATAL: Table `mod_item_upgrade_stats` has invalid starting rank (`stat_rank`) {} for stat type (`stat_type`) {}", ranks[0], rpair.first);
        }

        bool consecutive = true;
        for (uint32 i = 1; i < ranks.size(); i++)
        {
            if (ranks[i] != ranks[i - 1] + 1)
            {
                consecutive = false;
                break;
            }
        }
        if (!consecutive)
        {
            ok = false;
            LOG_ERROR("sql.sql", "FATAL: Table `mod_item_upgrade_stats` does not have consecutive ranks (`stat_rank`) for stat type (`stat_type`) {}", rpair.first);
        }
    }

    return ok;
}

const ItemUpgrade::StatRequirementContainer* ItemUpgrade::GetStatRequirements(const UpgradeStat* upgrade, const Item* item) const
{
    if (overrideStatRequirements.find(item->GetEntry()) != overrideStatRequirements.end())
    {
        const std::unordered_map<uint32, StatRequirementContainer>& itemReqs = overrideStatRequirements.at(item->GetEntry());
        if (itemReqs.find(upgrade->statId) != itemReqs.end())
            return &itemReqs.at(upgrade->statId);
    }

    if (baseStatRequirements.find(upgrade->statId) != baseStatRequirements.end())
        return &baseStatRequirements.at(upgrade->statId);

    return nullptr;
}

bool ItemUpgrade::EmptyRequirements(const StatRequirementContainer* reqs) const
{
    if (reqs == nullptr || reqs->size() == 0)
        return true;

    if (reqs->size() == 1 && reqs->at(0).reqType == REQ_TYPE_NONE)
        return true;

    return false;
}

void ItemUpgrade::EquipItem(Player* player, Item* item)
{
    if (!item || item->IsEquipped())
        return;

    uint16 pos;
    InventoryResult res = player->CanEquipItem(NULL_SLOT, pos, item, true);
    if (res != EQUIP_ERR_OK)
    {
        player->SendEquipError(res, item, nullptr);
        return;
    }

    player->SwapItem(item->GetPos(), pos);
}

void ItemUpgrade::LoadWeaponUpgradePercents(const std::string& percents)
{
    weaponUpgradeStats.clear();

    std::vector<float> weaponUpgradePercents;
    std::vector<std::string_view> tokenized = Acore::Tokenize(percents, ',', false);
    std::transform(tokenized.begin(), tokenized.end(), std::back_inserter(weaponUpgradePercents),
        [](const std::string_view& str) { return *Acore::StringTo<float>(str); });
    std::sort(weaponUpgradePercents.begin(), weaponUpgradePercents.end());
    weaponUpgradePercents.erase(std::unique(weaponUpgradePercents.begin(), weaponUpgradePercents.end()), weaponUpgradePercents.end());

    for (size_t i = 0; i < weaponUpgradePercents.size(); i++)
    {
        UpgradeStat weaponUpgradeStat;
        weaponUpgradeStat.statId = i + 1;
        weaponUpgradeStat.statRank = i + 1;
        weaponUpgradeStat.statModPct = weaponUpgradePercents[i];
        weaponUpgradeStat.statType = 0;
        weaponUpgradeStats.push_back(weaponUpgradeStat);
    }

    for (auto itr = characterWeaponUpgradeData.begin(); itr != characterWeaponUpgradeData.end(); ++itr)
    {
        std::vector<CharacterUpgrade>& weaponUpgrades = itr->second;
        for (CharacterUpgrade& upgrade : weaponUpgrades)
        {
            upgrade.upgradeStat = FindWeaponUpgradeStat(upgrade.upgradeStatModPct);
            if (upgrade.upgradeStat == nullptr)
                upgrade.upgradeStat = FindNearestWeaponUpgradeStat(upgrade.upgradeStatModPct);
        }
    }
}

std::pair<float, float> ItemUpgrade::GetItemProtoDamage(const ItemTemplate* proto) const
{
    return std::make_pair(proto->Damage[0].DamageMin, proto->Damage[0].DamageMax);
}

std::pair<float, float> ItemUpgrade::GetItemProtoDamage(const Item* item) const
{
    return GetItemProtoDamage(item->GetTemplate());
}

bool ItemUpgrade::MeetsWeaponUpgradeRequirement(const Player* player) const
{
    return MeetsRequirement(player, &weaponUpgradeReqs);
}

void ItemUpgrade::BuildWeaponUpgradeReqs()
{
    weaponUpgradeReqs.clear();

    const ItemTemplate* tokenProto = sObjectMgr->GetItemTemplate(GetIntConfig(CONFIG_ITEM_UPGRADE_WEAPON_DAMAGE_TOKEN));
    if (tokenProto != nullptr)
    {
        UpgradeStatReq tokenReq;
        tokenReq.reqType = REQ_TYPE_ITEM;
        tokenReq.reqVal1 = (float)GetIntConfig(CONFIG_ITEM_UPGRADE_WEAPON_DAMAGE_TOKEN);
        tokenReq.reqVal2 = (float)GetIntConfig(CONFIG_ITEM_UPGRADE_WEAPON_DAMAGE_TOKEN_COUNT);
        weaponUpgradeReqs.push_back(tokenReq);
    }

    if (GetIntConfig(CONFIG_ITEM_UPGRADE_WEAPON_DAMAGE_MONEY) > 0)
    {
        UpgradeStatReq moneyReq;
        moneyReq.reqType = REQ_TYPE_COPPER;
        moneyReq.reqVal1 = (float)GetIntConfig(CONFIG_ITEM_UPGRADE_WEAPON_DAMAGE_MONEY);
        weaponUpgradeReqs.push_back(moneyReq);
    }
}

ItemUpgrade::ItemVisualsPriority ItemUpgrade::GetItemVisualsPriority() const
{
    int32 priority = cfg.GetIntConfig(CONFIG_ITEM_UPGRADE_SEND_PACKETS_PRIORITY);
    switch (priority)
    {
        case 0:
            return PRIORITIZE_STATS;
        case 1:
            return PRIORITIZE_WEAPON_DAMAGE;
        default:
            return PRIORITIZE_STATS;
    }
}
