#include "Chat.h"
#include "Config.h"
#include "Item.h"
#include "Player.h"
#include "ReputationMgr.h"
#include "ScriptMgr.h"
#include "TaskScheduler.h"
#include <Log.h>
#include "DatabaseEnv.h"
#include "WorldSession.h"
#include <World.h>
#include <DBCStores.cpp>
#include <Mail.h>
#include <ObjectMgr.h>

using namespace Trinity::ChatCommands;

TaskScheduler scheduler;

class createPTR : public WorldScript {

public:
    createPTR() : WorldScript("createPTR") { }

    void OnStartup()
    {
        uint32 oldMSTime = getMSTime();
        uint32 count = 0;

        TC_LOG_INFO("server.loading", "Loading index entries for the PTR template script...");
        QueryResult result = WorldDatabase.PQuery("SELECT ID FROM ptrtemplate_index;");

        if (!result)
        {
            TC_LOG_WARN("server.loading", ">> Loaded 0 templates!");
            return;
        }
        do
        {
            ++count;
        } while (result->NextRow());
        TC_LOG_INFO("server.loading", ">> Loaded %u templates in %u ms.", count, GetMSTimeDiffToNow(oldMSTime));
        return;
    }
};

class createTemplate : public PlayerScript {

public:
    createTemplate() : PlayerScript("createTemplate") { }

    void HandleApply(Player* player, uint32 index)
    {
        TC_LOG_DEBUG("scripts", "Applying template %u for character %s.", index, player->GetGUID().ToString());

        scheduler.Schedule(Milliseconds(APPLY_DELAY), [player, index](TaskContext context)
            {
                switch (context.GetRepeatCounter())
                {
                case 0:
                    if (sConfigMgr->GetBoolDefault("TemplateDK", true))
                    {
                        AddTemplateDeathKnight(player);
                        player->SaveToDB(false);
                        TC_LOG_DEBUG("scripts", "Finished applying death knight case for template character %s.", player->GetGUID().ToString());
                    }
                    break;
                case 1:
                    if (sConfigMgr->GetBoolDefault("TemplateLevel", true))
                    {
                        AddTemplateLevel(player, index);
                        player->SaveToDB(false);
                        TC_LOG_DEBUG("scripts", "Finished applying level for template character %s.", player->GetGUID().ToString());
                    }
                    break;
                case 2:
                    if (sConfigMgr->GetBoolDefault("TemplateTaximask", true))
                    {
                        AddTemplateTaxi(player, index);
                        player->SaveToDB(false);
                        TC_LOG_DEBUG("scripts", "Finished applying taximask for template character %s.", player->GetGUID().ToString());
                    }
                    break;
                case 3:
                    if (sConfigMgr->GetBoolDefault("TemplateHomebind", true))
                    {
                        AddTemplateHomebind(player, index);
                        player->SaveToDB(false);
                        TC_LOG_DEBUG("scripts", "Finished applying homebind for template character %s.", player->GetGUID().ToString());
                    }
                    break;
                case 4:
                    if (sConfigMgr->GetBoolDefault("TemplateAchievements", true))
                    {
                        AddTemplateAchievements(player, index);
                        player->SaveToDB(false);
                        TC_LOG_DEBUG("scripts", "Finished applying achievements for template character %s.", player->GetGUID().ToString());
                    }
                    break;
                case 5:
                    if (sConfigMgr->GetBoolDefault("TemplateQuests", true))
                    {
                        AddTemplateQuests(player, index);
                        player->SaveToDB(false);
                        TC_LOG_DEBUG("scripts", "Finished applying quests for template character %s.", player->GetGUID().ToString());
                    }
                    break;
                case 6:
                    if (sConfigMgr->GetBoolDefault("TemplateReputation", true))
                    {
                        AddTemplateReputation(player, index);
                        player->SaveToDB(false);
                        TC_LOG_DEBUG("scripts", "Finished applying reputations for template character %s.", player->GetGUID().ToString());
                    }
                    break;
                case 7:
                    if (sConfigMgr->GetBoolDefault("TemplateSkills", true))
                    {
                        AddTemplateSkills(player, index);
                        player->SaveToDB(false);
                        TC_LOG_DEBUG("scripts", "Finished applying skills for template character %s.", player->GetGUID().ToString());
                    }
                    break;
                case 8:
                    if (sConfigMgr->GetBoolDefault("TemplateEquipGear", true))
                    {
                        AddTemplateWornGear(player, index);
                        player->SaveToDB(false);
                        TC_LOG_DEBUG("scripts", "Finished applying equipment for template character %s.", player->GetGUID().ToString());
                    }
                    break;
                case 9:
                    if (sConfigMgr->GetBoolDefault("TemplateBagGear", true))
                    {
                        AddTemplateBagGear(player, index);
                        player->SaveToDB(false);
                        TC_LOG_DEBUG("scripts", "Finished applying inventory items for template character %s.", player->GetGUID().ToString());
                    }
                    break;
                case 10:
                    if (sConfigMgr->GetBoolDefault("TemplateSpells", true))
                    {
                        AddTemplateSpells(player, index);
                        player->SaveToDB(false);
                        TC_LOG_DEBUG("scripts", "Finished applying spells for template character %s.", player->GetGUID().ToString());
                    }
                    break;
                case 11:
                    if (sConfigMgr->GetBoolDefault("TemplateHotbar", true))
                    {
                        AddTemplateHotbar(player, index);
                        player->SaveToDB(false);
                        TC_LOG_DEBUG("scripts", "Finished applying hotbar spells for template character %s.", player->GetGUID().ToString());
                    }
                    break;
                case 12:
                    if (sConfigMgr->GetBoolDefault("TemplateTeleport", true))
                    {
                        AddTemplatePosition(player, index);
                        player->SaveToDB(false);
                        TC_LOG_DEBUG("scripts", "Finished teleporting template character %s.", player->GetGUID().ToString());
                    }
                    break;
                case 13:
                    if (sConfigMgr->GetBoolDefault("TemplateResources", true))
                    {
                        AddTemplateResources(player);
                        player->SaveToDB(false);
                        TC_LOG_DEBUG("scripts", "Finished applying full resources for template character %s.", player->GetGUID().ToString());
                    }
                    return;
                }
        context.Repeat(Milliseconds(APPLY_RATE));
            });
    }

    static uint8 CheckTemplateQualifier(Player* player, uint32 index)
    {
        uint32 raceMask = player->GetRaceMask();
        uint32 classMask = player->GetClassMask();

        QueryResult queryCheck = WorldDatabase.PQuery( // These are used to figure out if a template has a race/class in mind.
            "SELECT COUNT(*) FROM(" //                   For example, the AQ40 blizzlike template doesn't apply to belfs, draenei, or DKs.
            "SELECT ID FROM ptrtemplate_reputations WHERE(ID = %u AND RaceMask & %u AND ClassMask & %u)"
            " UNION ALL "
            "SELECT ID FROM ptrtemplate_action WHERE(ID = %u AND RaceMask & %u AND ClassMask & %u)"
            " UNION ALL "
            "SELECT ID FROM ptrtemplate_inventory WHERE(ID = %u AND RaceMask & %u AND ClassMask & %u)"
            " UNION ALL "
            "SELECT ID FROM ptrtemplate_skills WHERE(ID = %u AND RaceMask & %u AND ClassMask & %u)"
            " UNION ALL "
            "SELECT ID FROM ptrtemplate_spells WHERE(ID = %u AND RaceMask & %u AND ClassMask & %u)"
            " UNION ALL "
            "SELECT ID FROM ptrtemplate_achievements WHERE(ID = %u AND RaceMask & %u AND ClassMask & %u)"
            " UNION ALL "
            "SELECT ID FROM ptrtemplate_quests WHERE(ID = %u AND RaceMask & %u AND ClassMask & %u)"
            ") AS combined",
            index, raceMask, classMask,
            index, raceMask, classMask,
            index, raceMask, classMask,
            index, raceMask, classMask,
            index, raceMask, classMask,
            index, raceMask, classMask,
            index, raceMask, classMask);

        if (!((*queryCheck)[0].GetUInt64()))
        {
            TC_LOG_DEBUG("scripts", "Template ID %u entered, but no template info available for player %s!", index, player->GetGUID().ToString());
            return MISSING_TEMPLATE_INFO;
        }
        if ((!(player->GetLevel() == (player->GetClass() != CLASS_DEATH_KNIGHT
            ? sWorld->getIntConfig(CONFIG_START_PLAYER_LEVEL)
            : sWorld->getIntConfig(CONFIG_START_DEATH_KNIGHT_PLAYER_LEVEL)))) && !(sConfigMgr->GetBoolDefault("LevelEnable", true)))
        {
            TC_LOG_DEBUG("scripts", "Player %s is not initial level, cannot apply template %u. Current level: %u", player->GetGUID().ToString(), index, player->GetLevel());
            return NOT_INITIAL_LEVEL;
        }
        if (!sConfigMgr->GetBoolDefault("TemplateEnable", true))
        {
            TC_LOG_DEBUG("scripts", "Player %s tried to apply template %u, but templates are disabled.", player->GetGUID().ToString(), index);
            return TEMPLATES_DISABLED;
        }
        if (!(player->GetSession()->GetSecurity() >= sConfigMgr->GetIntDefault("EnableApplySecurity", true)))
        {
            TC_LOG_DEBUG("scripts", "Player %s tried to apply template %u, but does not meet security level.", player->GetGUID().ToString(), index);
            return INSUFFICIENT_SECURITY_LEVEL;
        }
        else
        {
            TC_LOG_DEBUG("scripts", "Player %s has passed qualification for template %u.", player->GetGUID().ToString(), index);
            return CHECK_PASSED;
        }
    }

    enum checkCodes
    {
        MISSING_TEMPLATE_INFO = 1,
        NOT_INITIAL_LEVEL = 2,
        TEMPLATES_DISABLED = 3,
        INSUFFICIENT_SECURITY_LEVEL = 4,
        CHECK_PASSED = 0
    };

private:

    enum TemplateEnums
    {
        APPLY_DELAY = 25,
        APPLY_RATE = 50,
        HORDE_SIMILAR = -1,
        ACTION_BUTTON_BEGIN = 0,
        CONTAINER_BACKPACK = 0,
        CONTAINER_END = 5,
        ITEM_GOLD = 8
    };

    static void AddTemplateAchievements(Player* player, uint32 index)
    { //                                                                0
        QueryResult achievementInfo = WorldDatabase.PQuery("SELECT AchievementID FROM ptrtemplate_achievements WHERE (ID=%u AND RaceMask & %u AND ClassMask & %u)", index, player->GetRaceMask(), player->GetClassMask());
        if (achievementInfo)
        {
            AchievementEntry const* achievementID;
            do
            {
                achievementID = sAchievementStore.LookupEntry((*achievementInfo)[0].GetUInt32());
                player->CompletedAchievement(achievementID);
                TC_LOG_DEBUG("scripts", "Added achievement %u to template character %s.", (*achievementInfo)[0].GetUInt32(), player->GetGUID().ToString());
            } while (achievementInfo->NextRow());
        }
    }

    static void AddTemplateBagGear(Player* player, uint32 index) // Handles bag items and currency.
    { //                                                    0      1       2        3         4         5         6         7         8         9         10
        QueryResult bagInfo = WorldDatabase.PQuery("SELECT BagID, SlotID, ItemID, Quantity, Enchant0, Enchant1, Enchant2, Enchant3, Enchant4, Enchant5, Enchant6 FROM ptrtemplate_inventory WHERE (ID=%u AND RaceMask & %u AND ClassMask & %u)", index, player->GetRaceMask(), player->GetClassMask());
        if (bagInfo)
        {
            do
            {   //                                                           0
                QueryResult containerInfo = CharacterDatabase.PQuery("SELECT slot FROM character_inventory WHERE (bag = 0 AND guid=%u)", (player->GetGUID().GetCounter()));
                Field* bagFields = bagInfo->Fetch();
                Field* containerFields = containerInfo->Fetch();
                uint32 bagEntry = bagFields[0].GetUInt32();
                uint8 slotEntry = bagFields[1].GetUInt8();
                uint32 itemEntry = bagFields[2].GetUInt32();
                uint32 quantityEntry = bagFields[3].GetUInt32();
                if (itemEntry == ITEM_GOLD)
                {
                    player->SetMoney(quantityEntry);
                    continue;
                }
                if ((slotEntry < INVENTORY_SLOT_BAG_END || slotEntry >= PLAYER_SLOT_END) && bagEntry == CONTAINER_BACKPACK) // If item is either an equipped armorpiece, weapon, or container.
                {
                    continue;
                }
                ItemPosCountVec dest;
                if (bagEntry > CONTAINER_BACKPACK && bagEntry < CONTAINER_END) // If bag is an equipped container.
                { // TODO: Make this whole section better.
                    do // Also TODO: Add support for adding to bank bag contents. Damn paladins.
                    {
                        if (!containerFields) // Apparently this can happen sometimes.
                        {
                            continue;
                        }
                        uint8 slotDBInfo = containerFields[0].GetUInt8();
                        if (bagEntry != (slotDBInfo - 18)) // Check if equipped bag matches specified bag in DB.
                        {
                            continue;
                        }
                        if (slotDBInfo < INVENTORY_SLOT_BAG_START || slotDBInfo >= INVENTORY_SLOT_ITEM_START)
                        {
                            continue; // Ignore any non-container slots (i.e. backpack gear, equipped gear)
                        }
                        uint8 validCheck = player->CanStoreNewItem(slotDBInfo, slotEntry, dest, itemEntry, quantityEntry);
                        if (validCheck == EQUIP_ERR_OK)
                        {
                            player->StoreNewItem(dest, itemEntry, true);
                            Item* item = player->GetUseableItemByPos(slotDBInfo, slotEntry);
                            player->SendNewItem(item, 1, false, true); // Broadcast item detail packet.
                            if (item && item->GetEntry() != itemEntry)
                            {
                                continue;
                            }
                            TemplateHelperItemEnchants(bagInfo, player, item, 4);
                        }
                    } while (containerInfo->NextRow());
                }
                else if (bagEntry == CONTAINER_BACKPACK)
                {
                    if (!containerFields) // Apparently this can happen sometimes.
                    {
                        continue;
                    }
                    if (slotEntry < INVENTORY_SLOT_BAG_END || slotEntry >= PLAYER_SLOT_END)
                    {
                        continue; // Ignore any equipped items or invalid slot items.
                    }
                    player->DestroyItem(INVENTORY_SLOT_BAG_0, slotEntry, true);
                    uint8 validCheck = player->CanStoreNewItem(INVENTORY_SLOT_BAG_0, slotEntry, dest, itemEntry, quantityEntry);
                    if (validCheck == EQUIP_ERR_OK)
                    {
                        player->StoreNewItem(dest, itemEntry, true);
                        Item* item = player->GetUseableItemByPos(INVENTORY_SLOT_BAG_0, slotEntry); // TODO: Make this better and cooler.
                        player->SendNewItem(item, 1, false, true); // Broadcast item detail packet.
                        if (item && item->GetEntry() != itemEntry)
                        {
                            continue;
                        }
                        TemplateHelperItemEnchants(bagInfo, player, item, 4);
                    }
                }
                else
                {
                    uint8 validCheck = player->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, itemEntry, quantityEntry);
                    if (validCheck == EQUIP_ERR_OK)
                    {
                        player->StoreNewItem(dest, itemEntry, true); // Add to next available slot in backpack/equipped bags.
                        // TODO: Create the item and make it usable for item enchant helper. Also packet broadcast.
                    }
                    else if (validCheck == EQUIP_ERR_INVENTORY_FULL) // No available slots, post office's problem.
                    {
                        CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
                        Item* itemBuffer = Item::CreateItem(itemEntry, quantityEntry, player);

                        TemplateHelperItemEnchants(bagInfo, player, itemBuffer, 4);

                        std::string subject = player->GetSession()->GetTrinityString(LANG_NOT_EQUIPPED_ITEM);

                        MailDraft draft(subject, "There were problems with equipping item(s).");
                        draft.AddItem(itemBuffer); // TODO: Make a damn queue and allow multiple items to a single message. Druids have so many items their inbox gets full immediately if sending one by one.
                        draft.SendMailTo(trans, player, MailSender(player, MAIL_STATIONERY_GM), MAIL_CHECK_MASK_COPIED);
                        CharacterDatabase.CommitTransaction(trans);
                    }
                }
            } while (bagInfo->NextRow());
        }
    }

    static void AddTemplateDeathKnight(Player* player) // Pretty much all of this is copied from acidmanifesto's lovely work on the skip-dk-starting-area script.
    {
        if (player->GetClass() == CLASS_DEATH_KNIGHT)
        {
            int STARTER_QUESTS[33] = { 12593, 12619, 12842, 12848, 12636, 12641, 12657, 12678, 12679, 12680, 12687, 12698, 12701, 12706, 12716, 12719, 12720, 12722, 12724, 12725, 12727, 12733, -1, 12751, 12754, 12755, 12756, 12757, 12779, 12801, 13165, 13166 };
            // Blizz just dumped all of the special surprise quests on every DK template. Don't know yet if I want to do the same.
            int specialSurpriseQuestId = -1;
            switch (player->GetRace())
            {
            case RACE_TAUREN:
                specialSurpriseQuestId = 12739;
                break;
            case RACE_HUMAN:
                specialSurpriseQuestId = 12742;
                break;
            case RACE_NIGHTELF:
                specialSurpriseQuestId = 12743;
                break;
            case RACE_DWARF:
                specialSurpriseQuestId = 12744;
                break;
            case RACE_GNOME:
                specialSurpriseQuestId = 12745;
                break;
            case RACE_DRAENEI:
                specialSurpriseQuestId = 12746;
                break;
            case RACE_BLOODELF:
                specialSurpriseQuestId = 12747;
                break;
            case RACE_ORC:
                specialSurpriseQuestId = 12748;
                break;
            case RACE_TROLL:
                specialSurpriseQuestId = 12749;
                break;
            case RACE_UNDEAD_PLAYER:
                specialSurpriseQuestId = 12750;
                break;
            }

            STARTER_QUESTS[22] = specialSurpriseQuestId;
            STARTER_QUESTS[32] = player->GetTeamId() == TEAM_ALLIANCE
                ? 13188
                : 13189;

            for (int questId : STARTER_QUESTS)
            {
                if (player->GetQuestStatus(questId) == QUEST_STATUS_NONE)
                {
                    player->AddQuest(sObjectMgr->GetQuestTemplate(questId), nullptr);
                    player->RewardQuest(sObjectMgr->GetQuestTemplate(questId), 0, player, false);
                }
            }

            for (uint8 j = INVENTORY_SLOT_BAG_START; j < INVENTORY_SLOT_ITEM_END; j++) // Removes any items the DK is carrying at the end of the process.
            { //                                                                          Includes starting gear as well as quest rewards.
                player->DestroyItem(INVENTORY_SLOT_BAG_0, j, true); //                    This is done because I hate fun.
            } //                                                                          ^(;,;)^
        }
    }

    static void AddTemplateHomebind(Player* player, uint32 index)
    { //                                                         0              1            2           3           4           5           6          7          8        9       10       11
        QueryResult homeEntry = WorldDatabase.PQuery("SELECT HMapAlliance, HZoneAlliance, HXAlliance, HYAlliance, HZAlliance, HOAlliance, HMapHorde, HZoneHorde, HXHorde, HYHorde, HZHorde, HOHorde FROM ptrtemplate_index WHERE ID=%u", index);
        if (homeEntry)
        {
            uint16 hMapAllianceEntry = (*homeEntry)[0].GetUInt16();
            uint16 hZoneAllianceEntry = (*homeEntry)[1].GetUInt16();
            float HXAllianceEntry = (*homeEntry)[2].GetFloat();
            float HYAllianceEntry = (*homeEntry)[3].GetFloat();
            float HZAllianceEntry = (*homeEntry)[4].GetFloat();
            float HOAllianceEntry = (*homeEntry)[5].GetFloat();
            int16 hMapHordeEntry = (*homeEntry)[6].GetInt16();
            uint16 hZoneHordeEntry = (*homeEntry)[7].GetUInt16();
            float HXHordeEntry = (*homeEntry)[8].GetFloat();
            float HYHordeEntry = (*homeEntry)[9].GetFloat();
            float HZHordeEntry = (*homeEntry)[10].GetFloat();
            float HOHordeEntry = (*homeEntry)[11].GetFloat();
            WorldLocation homebinding;
            if (hMapHordeEntry == HORDE_SIMILAR || player->GetTeamId() == TEAM_ALLIANCE)
            {
                homebinding = WorldLocation(hMapAllianceEntry, HXAllianceEntry, HYAllianceEntry, HZAllianceEntry, HOAllianceEntry);
                player->SetHomebind(homebinding, hZoneAllianceEntry);
                TC_LOG_DEBUG("scripts", "Template character %s has had their homebind replaced with alliance.", player->GetGUID().ToString());
            }
            else
            {
                homebinding = WorldLocation(hMapHordeEntry, HXHordeEntry, HYHordeEntry, HZHordeEntry, HOHordeEntry);
                player->SetHomebind(homebinding, hZoneHordeEntry);
                TC_LOG_DEBUG("scripts", "Template character %s has had their homebind replaced with horde.", player->GetGUID().ToString());
            }
        }
    }

    static void AddTemplateHotbar(Player* player, uint32 index) // Someone smarter than me needs to fix this.
    { //                                                    0       1      2
        QueryResult barInfo = WorldDatabase.PQuery("SELECT Button, Action, Type FROM ptrtemplate_action WHERE (ID=%u AND RaceMask & %u AND ClassMask & %u)", index, player->GetRaceMask(), player->GetClassMask());
        for (uint8 j = ACTION_BUTTON_BEGIN; j <= MAX_ACTION_BUTTONS; j++) // This is supposed to go through every available action slot and remove what's there.
        { //                                                                 This doesn't work for spells added by AddTemplateSpells.
            player->removeActionButton(j); //                                I don't know why and I've tried everything I can think of, but nothing's worked.
        } //                                                                 And yes, I do want the hotbar cleared for characters that don't fit the requirements of the template.
        if (barInfo)
        {
            do
            {
                uint8 buttonEntry = (*barInfo)[0].GetUInt8();
                uint32 actionEntry = (*barInfo)[1].GetUInt32();
                uint8 typeEntry = (*barInfo)[2].GetUInt8();
                player->addActionButton(buttonEntry, actionEntry, typeEntry);
                TC_LOG_DEBUG("scripts", "Added hotbar spell %u on button %u with type %u for template character %s.", actionEntry, buttonEntry, typeEntry, player->GetGUID().ToString());
            } while (barInfo->NextRow());
        }
        player->SendActionButtons(2);
    }

    static void AddTemplateLevel(Player* player, uint32 index)
    { //                                                  0
        QueryResult check = WorldDatabase.PQuery("SELECT Level FROM ptrtemplate_index WHERE ID=%u", index);
        if (check)
        {
            uint8 levelEntry = (*check)[0].GetUInt8();
            player->GiveLevel(levelEntry);
            TC_LOG_DEBUG("scripts", "Template character %s has been made level %u.", player->GetGUID().ToString(), levelEntry);
        }
    }

    static void AddTemplatePosition(Player* player, uint32 index)
    { //                                                        0           1          2          3          4         5        6       7       8       9
        QueryResult posEntry = WorldDatabase.PQuery("SELECT MapAlliance, XAlliance, YAlliance, ZAlliance, OAlliance, MapHorde, XHorde, YHorde, ZHorde, OHorde FROM ptrtemplate_index WHERE ID=%u", index);
        if (posEntry)
        {
            uint16 mapAllianceEntry = (*posEntry)[0].GetUInt16();
            float XAllianceEntry = (*posEntry)[1].GetFloat();
            float YAllianceEntry = (*posEntry)[2].GetFloat();
            float ZAllianceEntry = (*posEntry)[3].GetFloat();
            float OAllianceEntry = (*posEntry)[4].GetFloat();
            int16 mapHordeEntry = (*posEntry)[5].GetInt16();
            float XHordeEntry = (*posEntry)[6].GetFloat();
            float YHordeEntry = (*posEntry)[7].GetFloat();
            float ZHordeEntry = (*posEntry)[8].GetFloat();
            float OHordeEntry = (*posEntry)[9].GetFloat();
            if (mapHordeEntry == HORDE_SIMILAR || player->GetTeamId() == TEAM_ALLIANCE)
            {
                player->TeleportTo(mapAllianceEntry, XAllianceEntry, YAllianceEntry, ZAllianceEntry, OAllianceEntry);
                TC_LOG_DEBUG("scripts", "Template character %s has been teleported to alliance position.", player->GetGUID().ToString());
            }
            else
            {
                player->TeleportTo(mapHordeEntry, XHordeEntry, YHordeEntry, ZHordeEntry, OHordeEntry);
                TC_LOG_DEBUG("scripts", "Template character %s has been teleported to horde position.", player->GetGUID().ToString());
            }
        }
    }

    static void AddTemplateQuests(Player* player, uint32 index)
    { //                                                       0
        QueryResult questInfo = WorldDatabase.PQuery("SELECT QuestID FROM ptrtemplate_quests WHERE (ID=%u AND RaceMask & %u AND ClassMask & %u)", index, player->GetRaceMask(), player->GetClassMask());
        if (questInfo)
        {
            do
            {
                player->SetQuestStatus((*questInfo)[0].GetUInt32(), QUEST_STATUS_COMPLETE);
                TC_LOG_DEBUG("scripts", "Added quest %u to template character %s.", ((*questInfo)[0].GetUInt32()), player->GetGUID().ToString());
            } while (questInfo->NextRow());
        }
    }

    static void AddTemplateReputation(Player* player, uint32 index)
    { //                                                      0         1
        QueryResult repInfo = WorldDatabase.PQuery("SELECT FactionID, Standing FROM ptrtemplate_reputations WHERE (ID=%u AND RaceMask & %u AND ClassMask & %u)", index, player->GetRaceMask(), player->GetClassMask());
        if (repInfo)
        {
            do
            {
                Field* fields = repInfo->Fetch();
                uint16 factionEntry = fields[0].GetUInt16();
                int32 standingEntry = fields[1].GetInt32();
                FactionEntry const* factionId = sFactionStore.LookupEntry(factionEntry);
                player->GetReputationMgr().SetOneFactionReputation(factionId, float(standingEntry), false);
                player->GetReputationMgr().SendState(player->GetReputationMgr().GetState(factionId));
                TC_LOG_DEBUG("scripts", "Added standing %u for faction %u for template character %s.", standingEntry, factionEntry, player->GetGUID().ToString());
            } while (repInfo->NextRow());
        }
    }

    static void AddTemplateResources(Player* player)
    {
        player->SetFullHealth();
        if (player->GetPowerType() == POWER_MANA)
        {
            player->SetPower(POWER_MANA, player->GetMaxPower(POWER_MANA));
        }
        else if (player->GetPowerType() == POWER_ENERGY)
        {
            player->SetPower(POWER_ENERGY, player->GetMaxPower(POWER_ENERGY));
        }
        TC_LOG_DEBUG("scripts", "Template character %s has been given full health/power.", player->GetGUID().ToString());
    }

    static void AddTemplateSkills(Player* player, uint32 index)
    { //                                                       0       1     2
        QueryResult skillInfo = WorldDatabase.PQuery("SELECT SkillID, Value, Max FROM ptrtemplate_skills WHERE (ID=%u AND RaceMask & %u AND ClassMask & %u)", index, player->GetRaceMask(), player->GetClassMask());
        if (skillInfo)
        {
            do
            {
                uint16 skillEntry = (*skillInfo)[0].GetUInt16();
                uint16 valueEntry = (*skillInfo)[1].GetUInt16();
                uint16 maxEntry = (*skillInfo)[2].GetUInt16();
                player->SetSkill(skillEntry, 0, valueEntry, maxEntry); // Don't know what step overload is used for, being zeroed here.
                TC_LOG_DEBUG("scripts", "Added skill %u to template character %s with curvalue %u and maxvalue %u.", skillEntry, player->GetGUID().ToString(), valueEntry, maxEntry);
            } while (skillInfo->NextRow());
            player->SaveToDB();
        }
    }

    static void AddTemplateSpells(Player* player, uint32 index)
    { //                                                       0
        QueryResult spellInfo = WorldDatabase.PQuery("SELECT SpellID FROM ptrtemplate_spells WHERE (ID=%u AND RaceMask & %u AND ClassMask & %u)", index, player->GetRaceMask(), player->GetClassMask());
        if (spellInfo)
        {
            do
            {
                uint64 spellEntry = (*spellInfo)[0].GetUInt64();
                player->LearnSpell(spellEntry, true);
                TC_LOG_DEBUG("scripts", "Added spell %u to template character %s.", spellEntry, player->GetGUID().ToString());
            } while (spellInfo->NextRow());
        }
    }

    static void AddTemplateTaxi(Player* player, uint32 index)
    { //                                                          0           1
        QueryResult taxiEntry = WorldDatabase.PQuery("SELECT TaxiAlliance, TaxiHorde FROM ptrtemplate_index WHERE ID=%u", index);
        if (taxiEntry)
        {
            if (player->GetTeamId() == TEAM_ALLIANCE || (*taxiEntry)[1].GetString() == "-1")
            {
                player->m_taxi.LoadTaxiMask((*taxiEntry)[0].GetString()); // Replaces existing m_taxi with template taximask (if exists)
                TC_LOG_DEBUG("scripts", "Template character %s has been given alliance taxi mask.", player->GetGUID().ToString());
            }
            else
            {
                player->m_taxi.LoadTaxiMask((*taxiEntry)[1].GetString()); // Comes with the downside of having to create a taximask beforehand, but who cares it works
                TC_LOG_DEBUG("scripts", "Template character %s has been given horde taxi mask.", player->GetGUID().ToString());
            }
        }
    }

    static void AddTemplateWornGear(Player* player, uint32 index) // Handles paper doll items and equipped bags.
    { //                                                     0      1       2        3         4         5         6         7         8         9
        QueryResult gearInfo = WorldDatabase.PQuery("SELECT BagID, SlotID, ItemID, Enchant0, Enchant1, Enchant2, Enchant3, Enchant4, Enchant5, Enchant6 FROM ptrtemplate_inventory WHERE (ID=%u AND RaceMask & %u AND ClassMask & %u)", index, player->GetRaceMask(), player->GetClassMask());
        if (gearInfo)
        {
            for (uint8 j = EQUIPMENT_SLOT_START; j < EQUIPMENT_SLOT_END; j++)
            {
                player->DestroyItem(INVENTORY_SLOT_BAG_0, j, true);
            }
            do
            {
                uint32 bagEntry = (*gearInfo)[0].GetUInt32();
                uint8 slotEntry = (*gearInfo)[1].GetUInt8();
                uint32 itemEntry = (*gearInfo)[2].GetUInt32();
                if ((slotEntry >= INVENTORY_SLOT_BAG_END && slotEntry < BANK_SLOT_BAG_START) || (slotEntry >= BANK_SLOT_BAG_END && slotEntry < PLAYER_SLOT_END) || bagEntry != CONTAINER_BACKPACK) // If item is not either an equipped armorpiece, weapon, or container.
                {
                    continue;
                }
                if (slotEntry >= PLAYER_SLOT_END)
                {
                    player->SetAmmo(itemEntry);
                }
                else
                    player->EquipNewItem(slotEntry, itemEntry, true);
                if (slotEntry >= BANK_SLOT_BAG_START && slotEntry < BANK_SLOT_BAG_END)
                {
                    uint8 slotBuffer = slotEntry - (BANK_SLOT_BAG_START - 1);

                    if (player->GetBankBagSlotCount() < slotBuffer)
                    {
                        player->SetBankBagSlotCount(slotBuffer);
                    }
                }
                Item* item = player->GetUseableItemByPos(INVENTORY_SLOT_BAG_0, slotEntry);
                player->SendNewItem(item, 1, false, true); // Broadcast item detail packet.
                if (item && item->GetEntry() != itemEntry)
                {
                    continue;
                }
                TemplateHelperItemEnchants(gearInfo, player, item, 3);
            } while (gearInfo->NextRow());
        }
    }

    static void TemplateHelperItemEnchants(QueryResult query, Player* player, Item* item, uint8 offset)
    {
        uint32 enchant0Entry = (*query)[offset].GetUInt32();
        uint32 enchant1Entry = (*query)[offset + 1].GetUInt32();
        uint32 enchant2Entry = (*query)[offset + 2].GetUInt32();
        uint32 enchant3Entry = (*query)[offset + 3].GetUInt32();
        uint32 enchant4Entry = (*query)[offset + 4].GetUInt32();
        uint32 enchant5Entry = (*query)[offset + 5].GetUInt32();
        uint32 enchant6Entry = (*query)[offset + 6].GetUInt32();

        if (enchant0Entry)
        {
            player->ApplyEnchantment(item, false);
            item->SetEnchantment(PERM_ENCHANTMENT_SLOT, enchant0Entry, 0, 0);
            player->ApplyEnchantment(item, true);
        }
        if (enchant1Entry)
        {
            player->ApplyEnchantment(item, false);
            item->SetEnchantment(TEMP_ENCHANTMENT_SLOT, enchant1Entry, 0, 0);
            player->ApplyEnchantment(item, true);
        }
        if (enchant2Entry)
        {
            player->ApplyEnchantment(item, false);
            item->SetEnchantment(SOCK_ENCHANTMENT_SLOT, enchant2Entry, 0, 0);
            player->ApplyEnchantment(item, true);
        }
        if (enchant3Entry)
        {
            player->ApplyEnchantment(item, false);
            item->SetEnchantment(SOCK_ENCHANTMENT_SLOT_2, enchant3Entry, 0, 0);
            player->ApplyEnchantment(item, true);
        }
        if (enchant4Entry)
        {
            player->ApplyEnchantment(item, false);
            item->SetEnchantment(SOCK_ENCHANTMENT_SLOT_3, enchant4Entry, 0, 0);
            player->ApplyEnchantment(item, true);
        }
        if (enchant5Entry)
        {
            player->ApplyEnchantment(item, false);
            item->SetEnchantment(BONUS_ENCHANTMENT_SLOT, enchant5Entry, 0, 0);
            player->ApplyEnchantment(item, true);
        }
        if (enchant6Entry)
        {
            player->ApplyEnchantment(item, false);
            item->SetEnchantment(PRISMATIC_ENCHANTMENT_SLOT, enchant6Entry, 0, 0);
            player->ApplyEnchantment(item, true);
        }
    }
};

class schedulediff : public WorldScript {

public:
    schedulediff() : WorldScript("schedulediff") { }

    void OnUpdate(uint32 diff)
    {
        scheduler.Update(diff);
    }
};

class announce : public PlayerScript {

public:
    announce() : PlayerScript("announce") { }

    void OnLogin(Player* player, bool /*firstLogin*/)
    {
        if (sConfigMgr->GetBoolDefault("AnnounceEnable", true))
        {
            ChatHandler(player->GetSession()).SendSysMessage("This server is running the PTR Template script.");
        }
    }
};

class ptr_template_commandscript : public CommandScript
{
public:
    ptr_template_commandscript() : CommandScript("ptr_template_commandscript") { }

    ChatCommandTable GetCommands() const override
    {
        static ChatCommandTable templateTable =
        {
            { "apply",   applyTemplate,   rbac::RBAC_ROLE_PLAYER,        Console::No },
            { "disable", disableTemplate, rbac::RBAC_ROLE_ADMINISTRATOR, Console::Yes },
            { "enable",  enableTemplate,  rbac::RBAC_ROLE_ADMINISTRATOR, Console::Yes },
            { "list",    listTemplate,    rbac::RBAC_ROLE_PLAYER,        Console::Yes },
        };

        static ChatCommandTable commandTable =
        {
            { "template", templateTable },
        };
        return commandTable;
    }

    static bool enableTemplate(ChatHandler* handler, uint32 index)
    { //                                                    0
        QueryResult result = WorldDatabase.PQuery("SELECT Comment FROM ptrtemplate_index WHERE ID=%u", index);
        WorldDatabase.PExecute("UPDATE ptrtemplate_index SET Enable=1 WHERE ID=%u", index);
        if (result)
        {
            std::string comment = (*result)[0].GetString();
            handler->PSendSysMessage("Enabled template %u (%s).", index, comment);
            return true;
        }
        else
        {
            handler->PSendSysMessage("This template has not been added.");
            return false;
        }
    }

    static bool disableTemplate(ChatHandler* handler, uint32 index)
    { //                                                    0
        QueryResult result = WorldDatabase.PQuery("SELECT Comment FROM ptrtemplate_index WHERE ID=%u", index);
        WorldDatabase.PExecute("UPDATE ptrtemplate_index SET Enable=0 WHERE ID=%u", index);
        if (result)
        {
            std::string comment = (*result)[0].GetString();
            handler->PSendSysMessage("Disabled template %u (%s).", index, comment);
            return true;
        }
        else
        {
            handler->PSendSysMessage("This template has not been added.");
            return false;
        }
    }

    static bool applyTemplate(ChatHandler* handler, Optional<PlayerIdentifier> player, uint32 index) // TODO: Allow the command to use a target instead of always targetting self.
    { //                                                  0
        QueryResult check = WorldDatabase.PQuery("SELECT Enable FROM ptrtemplate_index WHERE ID=%u", index); // TODO: Check keywords column for template...keywords.
        static createTemplate templatevar;
        bool secure = false;
        if (check)
        {
            uint8 enable = (*check)[0].GetUInt8();
            if (!player)
            {
                player = PlayerIdentifier::FromTargetOrSelf(handler);
            }
            Player* target = player->GetConnectedPlayer();

            if (!enable && (target->GetSession()->GetSecurity() >= sConfigMgr->GetIntDefault("DisableApplySecurity", true)))
            {
                secure = true;
            }
            if (enable || secure)
            {
                switch (templatevar.CheckTemplateQualifier(target, index))
                {
                case templatevar.MISSING_TEMPLATE_INFO:
                    handler->PSendSysMessage("The selected template does not apply to you.");
                    return true;
                case templatevar.NOT_INITIAL_LEVEL:
                    handler->PSendSysMessage("You must be a new character to apply this template.");
                    return true;
                case templatevar.TEMPLATES_DISABLED:
                    handler->PSendSysMessage("Templates currently cannot be applied.");
                    return true;
                case templatevar.INSUFFICIENT_SECURITY_LEVEL:
                    if (secure)
                    {
                        break;
                    }
                    else
                    {
                        handler->PSendSysMessage("You do not meet the security to apply templates.");
                        return true;
                    }
                default:
                    break;
                }
                uint32 oldMSTime = getMSTime();
                templatevar.HandleApply(target, index);
                TC_LOG_DEBUG("scripts", "Handled template apply for character %s in %u ms.", player->GetGUID().ToString(), (GetMSTimeDiffToNow(oldMSTime) - 100));
                handler->PSendSysMessage("Please logout for the template to fully apply."); // This is a dumb message that I feel obligated to add because the hotbar changes when you log back in,
            } //                                                                               because I will never ever ever figure out how to do the hotbar correctly.
            else
            {
                handler->PSendSysMessage("This template is disabled.");
            }
            return true;
        }
        else
        {
            handler->PSendSysMessage("This template has not been added.");
            return true;
        }
    }

    static bool listTemplate(ChatHandler* handler)
    { //                                                 0     1        2
        QueryResult index = WorldDatabase.PQuery("SELECT ID, Enable, Comment FROM ptrtemplate_index ORDER BY ID");
        if (index)
        {
            handler->PSendSysMessage("Available template sets:");

            int8 playerSecurity = handler->IsConsole()
                ? SEC_CONSOLE
                : handler->GetSession()->GetSecurity();

            do
            {
                uint8 indexEntry = (*index)[0].GetUInt8();
                uint8 enableEntry = (*index)[1].GetUInt8();
                std::string commentEntry = (*index)[2].GetString();
                std::string enableText = enableEntry
                    ? "Enabled"
                    : "Disabled";

                if ((playerSecurity >= sConfigMgr->GetIntDefault("EnableListSecurity", true) && enableEntry) || (playerSecurity >= sConfigMgr->GetIntDefault("DisableListSecurity", true) && !enableEntry))
                {
                    if (playerSecurity >= sConfigMgr->GetIntDefault("StatusSecurityText", true))
                    {
                        handler->PSendSysMessage("%u (%s): %s", indexEntry, commentEntry, enableText);
                    }
                    else
                    {
                        handler->PSendSysMessage("%u (%s)", indexEntry, commentEntry);
                    }
                }
            } while (index->NextRow());
        }
        else
        {
            handler->PSendSysMessage("There are no added templates.");
        }
        return true;
    }
};

void Add_ptr_template()
{
    new createPTR();
    new createTemplate();
    new announce();
    new schedulediff();
}

void AddSC_ptr_template_commandscript()
{
    new ptr_template_commandscript();
}
