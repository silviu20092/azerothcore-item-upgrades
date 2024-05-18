/*
 * Credits: silviu20092
 */

#ifndef _ITEM_UPGRADE_H_
#define _ITEM_UPGRADE_H_

#include <vector>
#include "Define.h"

class ItemUpgrade
{
private:
    ItemUpgrade();
    ~ItemUpgrade();
public:
    enum PagedDataType
    {
        PAGED_DATA_TYPE_ITEMS,
        PAGED_DATA_TYPE_STATS,
        PAGED_DATA_TYPE_REQS,
        PAGED_DATA_TYPE_UPGRADED_ITEMS,
        PAGED_DATA_TYPE_UPGRADED_ITEMS_STATS,
        PAGED_DATA_TYPE_ITEMS_FOR_PURGE,
        PAGED_DATA_TYPE_ITEMS_BULK,
        PAGED_DATA_TYPE_STATS_BULK,
        PAGED_DATA_TYPE_STAT_UPGRADE_BULK,
        PAGED_DATA_TYPE_REQS_BULK,
        MAX_PAGED_DATA_TYPE
    };

    enum IdentifierType
    {
        BASE_IDENTIFIER,
        ITEM_IDENTIFIER,
        UPGRADE_BULK_IDENTIFIER
    };

    struct Identifier
    {
        uint32 id;
        std::string name;
        std::string uiName;

        virtual IdentifierType GetType() const
        {
            return BASE_IDENTIFIER;
        }
    };

    struct ItemIdentifier : public Identifier
    {
        ObjectGuid guid;

        IdentifierType GetType() const override
        {
            return ITEM_IDENTIFIER;
        }
    };

    struct UpgradeBulkIdentifier : public Identifier
    {
        float modPct;

        IdentifierType GetType() const override
        {
            return UPGRADE_BULK_IDENTIFIER;
        }
    };

    struct UpgradeStat;
    struct PagedData
    {
        static constexpr int PAGE_SIZE = 12;

        uint32 totalPages;
        uint32 currentPage;
        bool reloaded;
        PagedDataType type;
        const UpgradeStat* upgradeStat;
        ItemIdentifier item;
        std::vector<Identifier *> data;
        float pct;

        PagedData() : totalPages(0), currentPage(0), reloaded(false), type(MAX_PAGED_DATA_TYPE), upgradeStat(nullptr), pct(0.0f) {}

        void Reset();
        void CalculateTotals();
        void SortAndCalculateTotals();
        bool IsEmpty() const;
        const Identifier* FindIdentifierById(uint32 id) const;
    };
    typedef std::unordered_map<uint32, PagedData> PagedDataMap;

    enum UpgradeStatReqType
    {
        REQ_TYPE_COPPER = 1,
        REQ_TYPE_HONOR,
        REQ_TYPE_ARENA,
        REQ_TYPE_ITEM,
        MAX_REQ_TYPE
    };

    struct UpgradeStatReq
    {
        /* Associated stat ID from UpgradeStat */
        uint32 statId;

        /*
            Possible values:
                1 = this rank requires money (copper, gold) to be bought
                2 = this rank requires honor points to be bought
                3 = this rank requires arena points to be bought
                4 = this rank requires certain item(s) to be bought
         */
        UpgradeStatReqType reqType;

        /*
         *   If reqType = 1 THEN required copper to purchase rank
         *   If reqType = 2 THEN required honor points to purchase rank
         *   If reqType = 3 THEN required arena points to purchase rank
         *   If reqType = 4 THEN item ENTRY (from item_template.entry) required to purchase rank
         */
        float reqVal1;

        /*
         *  If reqType = 4 THEN item count required to purchase rank
         *  NOT USED otherwise
         */ 
        float reqVal2;

        UpgradeStatReq()
        {
            statId = 0;
            reqType = MAX_REQ_TYPE;
            reqVal1 = 0.0f;
            reqVal2 = 0.0f;
        }

        UpgradeStatReq(uint32 statId, UpgradeStatReqType reqType, float reqVal1, float reqVal2)
            : statId(statId), reqType(reqType), reqVal1(reqVal1), reqVal2(reqVal2) {}

        UpgradeStatReq(uint32 statId, UpgradeStatReqType reqType, float reqVal1)
            : statId(statId), reqType(reqType), reqVal1(reqVal1), reqVal2(0.0f) {}
    };
    typedef std::vector<UpgradeStatReq> StatRequirementContainer;

    struct UpgradeStat
    {
        uint32 statId;
        uint32 statType;
        float statModPct;
        uint16 statRank;
        StatRequirementContainer statReq;
    };
    typedef std::vector<UpgradeStat> UpgradeStatContainer;

    struct CharacterUpgrade
    {
        uint32 guid;
        ObjectGuid itemGuid;
        const UpgradeStat* upgradeStat;
    };
    typedef std::unordered_map<uint32, std::vector<CharacterUpgrade>> CharacterUpgradeContainer;

    typedef std::set<uint32> ItemEntryContainer;
    typedef std::unordered_map<uint32, std::set<uint32>> StatWithItemContainer;
public:
    static constexpr const char* DefaultAllowedStats = "0,3,4,5,6,7,32,36,45";

    static ItemUpgrade* instance();

    template <class Container, typename T>
    static T* FindInContainer(const Container& c, const T& val)
    {
        typename Container::const_iterator citr = std::find_if(c.begin(), c.end(), [&](const T& value) { return value == val; });
        return citr != c.end() ? (T*)(&*citr) : nullptr;
    }

    void SetEnabled(bool value);
    bool GetEnabled() const;

    bool IsAllowedStatType(uint32 statType) const;
    void LoadAllowedStats(const std::string& stats);

    void LoadFromDB(bool reload = false);

    void BuildUpgradableItemCatalogue(const Player* player, PagedDataType type);
    void BuildStatsUpgradeCatalogue(const Player* player, const Item* item);
    void BuildStatsUpgradeCatalogueBulk(const Player* player, const Item* item);
    void BuildStatsUpgradeByPctCatalogueBulk(const Player* player, const Item* item, float pct);
    void BuildStatsRequirementsCatalogueBulk(const Player* player, const Item* item, float pct);
    void BuildStatsRequirementsCatalogue(const Player* player, const UpgradeStat* upgradeStat);
    void BuildAlreadyUpgradedItemsCatalogue(const Player* player, PagedDataType type);
    void BuildItemUpgradeStatsCatalogue(const Player* player, const Item* item);

    PagedData& GetPagedData(const Player* player);
    PagedDataMap& GetPagedDataMap();
    bool AddPagedData(Player* player, Creature* creature, uint32 page);
    bool TakePagedDataAction(Player* player, Creature* creature, uint32 action);

    bool IsValidItemForUpgrade(const Item* item, const Player* player) const;

    int32 HandleStatModifier(const Player* player, uint8 slot, uint32 statType, int32 amount) const;
    int32 HandleStatModifier(const Player* player, Item* item, uint32 statType, int32 amount, EnchantmentSlot slot) const;
    void HandleItemRemove(Player* player, Item* item);
    void HandleCharacterRemove(uint32 guid);

    void SetReloading(bool value);
    bool GetReloading() const;

    void SetSendItemPackets(bool value);
    bool GetSendItemPackets() const;

    void HandleDataReload(bool apply);

    void UpdateVisualCache(Player* player);
    void VisualFeedback(Player* player);

    void LoadPurgeConfig(bool allow, int32 token, int32 count);
    bool GetAllowPurgeUpgrades() const;
    uint32 GetPurgeToken() const;
    uint32 GetPurgeTokenCount() const;

    bool PurgeUpgrade(Player* player, Item* item);

    void SetRandomUpgrades(bool value);
    bool GetRandomUpgrades() const;
    void SetRandomUpgradesLoginMsg(const std::string& value);
    std::string GetRandomUpgradesLoginMsg() const;
    void SetRandomUpgradeChance(float value);
    float GetRandomUpgradeChance() const;
    void SetRandomUpgradeMaxStats(int32 value);
    uint32 GetRandomUpgradeMaxStats() const;
    void SetRandomUpgradeMaxRank(int32 value);
    uint32 GetRandomUpgradeMaxRank() const;
    bool ChooseRandomUpgrade(Player* player, Item* item);
public:
    static std::string ItemIcon(const ItemTemplate* proto, uint32 width, uint32 height, int x, int y);
    static std::string ItemIcon(const ItemTemplate* proto);
    static std::string ItemNameWithLocale(const Player* player, const ItemTemplate* itemTemplate, int32 randomPropertyId);
    static std::string ItemLink(const Player* player, const ItemTemplate* itemTemplate, int32 randomPropertyId);
    static std::string ItemLink(const Player* player, const Item* item);
    static void SendMessage(const Player* player, const std::string& message);
private:
    static constexpr int VISUAL_FEEDBACK_SPELL_ID = 46331;

    bool enabled;
    bool reloading;
    bool sendItemPackets;
    std::vector<uint32> allowedStats;
    UpgradeStatContainer upgradeStatList;
    PagedDataMap playerPagedData;
    CharacterUpgradeContainer characterUpgradeData;
    ItemEntryContainer allowedItems;
    ItemEntryContainer blacklistedItems;
    StatWithItemContainer allowedStatItems;
    StatWithItemContainer blacklistedStatItems;

    std::map<float, std::vector<const ItemUpgrade::UpgradeStat*>> upgradesPctMap;

    bool allowPurgeUpgrades;
    uint32 purgeToken;
    uint32 purgeTokenCount;

    bool randomUpgrades;
    std::string randomUpgradesLoginMsg;
    float randomUpgradeChance;
    uint32 randomUpgradeMaxStats;
    uint32 randomUpgradeMaxRank;

    static bool CompareIdentifier(const Identifier* a, const Identifier* b);
    static int32 CalculateModPct(int32 value, const UpgradeStat* upgradeStat);
    static std::string CopperToMoneyStr(uint32 money, bool colored);
    static std::string FormatFloat(float val, uint32 decimals = 2);

    void CleanupDB(bool reload);
    void LoadStatRequirements(std::unordered_map<uint32, StatRequirementContainer> &statRequirementMap);
    void LoadUpgradeStats(const std::unordered_map<uint32, StatRequirementContainer>& statRequirementMap);
    void LoadCharacterUpgradeData();
    void LoadAllowedItems();
    void LoadAllowedStatsItems();
    void LoadBlacklistedItems();
    void LoadBlacklistedStatsItems();
    bool IsValidReqType(uint8 reqType) const;
    bool ValidateReq(uint32 id, UpgradeStatReqType reqType, float val1, float val2) const;
    void AddItemToPagedData(const Item* item, const Player* player, PagedData& pagedData);
    bool _AddPagedData(Player* player, const PagedData& pagedData, uint32 page) const;
    void NoPagedData(Player* player, const PagedData& pagedData) const;
    std::vector<_ItemStat> LoadItemStatInfo(const Item* item) const;
    const _ItemStat* GetStatByType(const std::vector<_ItemStat>& statInfo, uint32 statType) const;
    std::string StatTypeToString(uint32 statType) const;
    std::string ItemLinkForUI(const Item* item, const Player* player) const;
    void MergeStatRequirements(std::unordered_map<uint32, StatRequirementContainer>& statRequirementMap);
    const UpgradeStat* FindUpgradeStat(uint32 statId) const;
    const UpgradeStat* FindUpgradeStat(uint32 statType, uint16 rank) const;
    std::vector<const UpgradeStat*> FindUpgradesForItem(const Player* player, const Item* item) const;
    const UpgradeStat* FindUpgradeForItem(const Player* player, const Item* item, uint32 statType) const;
    bool MeetsRequirement(const Player* player, const UpgradeStatReq& req) const;
    bool MeetsRequirement(const Player* player, const UpgradeStat* upgradeStat) const;
    bool MeetsRequirement(const Player* player, const StatRequirementContainer& reqs) const;
    void TakeRequirements(Player* player, const UpgradeStat* upgradeStat);
    void TakeRequirements(Player* player, const StatRequirementContainer& reqs);
    bool PurchaseUpgrade(Player* player);
    void AddUpgradedItemToPagedData(const Item* item, const Player* player, PagedData& pagedData, const std::string &from);
    void HandleDataReload(Player* player, bool apply);
    std::vector<Item*> GetPlayerItems(const Player* player, bool inBankAlso) const;
    bool IsAllowedItem(const Item* item) const;
    bool IsBlacklistedItem(const Item* item) const;
    void SendItemPacket(Player* player, Item* item) const;
    std::pair<uint32, uint32> CalculateItemLevel(const Player* player, Item* item, const UpgradeStat* upgrade = nullptr) const;
    std::pair<uint32, uint32> CalculateItemLevel(const Player* player, Item* item, std::unordered_map<uint32, const UpgradeStat*>) const;
    void RemoveItemUpgrade(Player* player, Item* item);
    bool AddUpgradeForNewItem(Player* player, Item* item, const UpgradeStat* upgrade, const _ItemStat* stat);
    void AddItemUpgradeToDB(const Player* player, const Item* item, const UpgradeStat* upgrade) const;
    const UpgradeStat* FindNearestUpgradeStat(uint32 statType, uint16 rank, const Item* item) const;
    bool IsAllowedStatForItem(const Item* item, const UpgradeStat* upgrade) const;
    bool IsBlacklistedStatForItem(const Item* item, const UpgradeStat* upgrade) const;
    bool CanApplyUpgradeForItem(const Item* item, const UpgradeStat* upgrade) const;
    Item* FindItemIdentifierFromPage(const PagedData& pagedData, uint32 id, Player* player) const;
    void CreateUpgradesPctMap();
    std::unordered_map<uint32, const UpgradeStat*> FindAllUpgradeableRanks(const Player* player, const Item* item, float pct) const;
    StatRequirementContainer BuildBulkRequirements(const std::unordered_map<uint32, const UpgradeStat*>& upgrades) const;
    void BuildRequirementsPage(const Player* player, PagedData& pagedData, const StatRequirementContainer& reqs) const;
    bool PurchaseUpgradeBulk(Player* player);
    bool HandlePurchaseRank(Player* player, Item* item, const UpgradeStat* upgrade);
    bool CheckDataValidity() const;
    bool IsValidStatType(uint32 statType) const;
};

#define sItemUpgrade ItemUpgrade::instance()

#endif
