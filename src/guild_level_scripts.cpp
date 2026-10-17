/*
 * This file is part of mod-guild-level.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * mod-guild-level — the hooks. Each one hands over to GuildLevelMgr.
 */

#include "Chat.h"
#include "Guild.h"
#include "GuildActivePerks.h"
#include "GuildHouse.h"
#include "GuildLevel.h"
#include "GuildPerks.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "SpellAuras.h"
#include "SpellScript.h"
#include "SpellScriptLoader.h"

class GuildLevelWorldScript : public WorldScript
{
public:
    GuildLevelWorldScript() : WorldScript("GuildLevelWorldScript",
        {
            WORLDHOOK_ON_AFTER_CONFIG_LOAD,
            WORLDHOOK_ON_STARTUP,
            WORLDHOOK_ON_UPDATE,
            WORLDHOOK_ON_SHUTDOWN
        }) { }

    // At startup and on every `.reload config`.
    void OnAfterConfigLoad(bool reload) override
    {
        sGuildLevelMgr->LoadConfig();
        sGuildPerkMgr->LoadConfig();
        sGuildActivePerkMgr->LoadConfig();
        sGuildHouseMgr->LoadConfig();

        // New amounts for the members online at once.
        if (reload)
            for (auto const& [guid, player] : ObjectAccessor::GetPlayers())
                sGuildPerkMgr->Apply(player);
    }

    // The guilds are loaded by then.
    void OnStartup() override
    {
        sGuildLevelMgr->LoadFromDB();
        sGuildHouseMgr->LoadFromDB();
    }

    void OnUpdate(uint32 diff) override
    {
        sGuildLevelMgr->Update(diff);
        sGuildPerkMgr->Update();
    }

    void OnShutdown() override
    {
        sGuildLevelMgr->SaveDirty();
    }
};

class GuildLevelPlayerScript : public PlayerScript
{
public:
    GuildLevelPlayerScript() : PlayerScript("GuildLevelPlayerScript",
        {
            // THIS LIST GATES THE CALLS: an overridden method missing from
            // here compiles and never fires.
            PLAYERHOOK_ON_GIVE_EXP,
            PLAYERHOOK_ON_LOGIN,
            PLAYERHOOK_ON_GIVE_REPUTATION,
            PLAYERHOOK_ON_GET_REPUTATION_PRICE_DISCOUNT,
            PLAYERHOOK_ON_BEFORE_LOOT_MONEY,
            PLAYERHOOK_ON_LOOT_ITEM,
            PLAYERHOOK_ON_PLAYER_RESURRECT,
            PLAYERHOOK_ON_UPDATE,
            PLAYERHOOK_ON_DELETE_FROM_DB
        }) { }

    // Only the creature overload is ours; the faction one stays the base class's.
    using PlayerScript::OnPlayerGetReputationPriceDiscount;

    // Every experience gain, right before the player gets it. A kill also
    // earns guild experience, read from the kill itself: the experience perk
    // does not feed it.
    void OnPlayerGiveXP(Player* player, uint32& amount, Unit* victim, uint8 xpSource) override
    {
        if (xpSource == XPSOURCE_KILL && amount)
            sGuildLevelMgr->OnKillXP(player, victim);
        sGuildPerkMgr->OnGiveXP(player, amount);
    }

    void OnPlayerLogin(Player* player) override
    {
        sGuildPerkMgr->Apply(player);
        sGuildActivePerkMgr->Load(player);
    }

    void OnPlayerDeleteFromDB(CharacterDatabaseTransaction trans, uint32 guid) override
    {
        sGuildActivePerkMgr->DeleteCharacter(trans, guid);
    }

    void OnPlayerGiveReputation(Player* player, int32 /*factionID*/, float& amount, ReputationSource /*repSource*/) override
    {
        sGuildPerkMgr->OnGiveReputation(player, amount);
    }

    void OnPlayerGetReputationPriceDiscount(Player const* player, Creature const* creature, float& discount) override
    {
        sGuildPerkMgr->OnPriceDiscount(player, creature, discount);
    }

    void OnPlayerBeforeLootMoney(Player* player, Loot* loot) override
    {
        sGuildPerkMgr->OnLootMoney(player, loot);
    }

    void OnPlayerLootItem(Player* player, Item* item, uint32 /*count*/, ObjectGuid lootGuid) override
    {
        sGuildPerkMgr->OnLootItem(player, item, lootGuid);
    }

    void OnPlayerResurrect(Player* player, float restorePercent, bool& /*applySickness*/) override
    {
        sGuildPerkMgr->OnResurrect(player, restorePercent);
    }

    // Every player tick, on his map's thread: the durability perk's clock.
    void OnPlayerUpdate(Player* player, uint32 diff) override
    {
        sGuildPerkMgr->OnUpdate(player, diff);
    }
};

class GuildLevelUnitScript : public UnitScript
{
public:
    GuildLevelUnitScript() : UnitScript("GuildLevelUnitScript", true,
        {
            UNITHOOK_ON_AURA_APPLY
        }) { }

    // Flasks, elixirs and food buffs of guild members.
    void OnAuraApply(Unit* unit, Aura* aura) override
    {
        if (Player* player = unit->ToPlayer())
            sGuildPerkMgr->OnAuraApply(player, aura);
    }
};

// The Hearthstone (spell_script_names, data/sql/world): its cooldown is set
// when the cast ends, the perk takes its minutes off right after.
class spell_guild_level_hearthstone : public SpellScript
{
    PrepareSpellScript(spell_guild_level_hearthstone);

    void HandleAfterCast()
    {
        if (Player* player = GetCaster()->ToPlayer())
            sGuildPerkMgr->OnHearthstone(player);
    }

    void Register() override
    {
        AfterCast += SpellCastFn(spell_guild_level_hearthstone::HandleAfterCast);
    }
};

// The spells of the active perks (spell_script_names, data/sql/world): each
// does what its `.gperk` command does; inside the guild house, the house's
// spell takes the player back out.
class spell_guild_level_perk : public SpellScript
{
    PrepareSpellScript(spell_guild_level_perk);

    void HandleDummy(SpellEffIndex /*effIndex*/)
    {
        Player* player = GetCaster()->ToPlayer();
        int const perk = GuildActivePerkMgr::PerkOfSpell(GetSpellInfo()->Id);
        if (!player || perk < 0)
            return;

        ChatHandler handler(player->GetSession());
        if (perk == GUILD_ACTIVE_HOME && GuildHouseMgr::IsHouseMap(player->GetMapId()))
        {
            sGuildHouseMgr->Leave(&handler, player);
            return;
        }
        sGuildActivePerkMgr->Use(&handler, GuildActivePerk(perk));
    }

    void Register() override
    {
        OnEffectHit += SpellEffectFn(spell_guild_level_perk::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

class GuildLevelGuildScript : public GuildScript
{
public:
    GuildLevelGuildScript() : GuildScript("GuildLevelGuildScript",
        {
            GUILDHOOK_ON_DISBAND,
            GUILDHOOK_ON_ADD_MEMBER,
            GUILDHOOK_ON_REMOVE_MEMBER
        }) { }

    // The perks follow the member: given on joining, taken on leaving, on the
    // next world tick, when the guild id of the character is settled.
    void OnAddMember(Guild* /*guild*/, Player* player, uint8& /*plRank*/) override
    {
        if (player)
            sGuildPerkMgr->QueueRefresh(player->GetGUID());
    }

    void OnRemoveMember(Guild* /*guild*/, Player* player, bool /*isDisbanding*/, bool /*isKicked*/) override
    {
        if (player)
        {
            sGuildPerkMgr->QueueRefresh(player->GetGUID());
            // Out of the house he no longer belongs to.
            sGuildHouseMgr->OnMemberLeft(player->GetGUID());
        }
    }

    void OnDisband(Guild* guild) override
    {
        sGuildLevelMgr->DeleteGuild(guild->GetId());
        sGuildHouseMgr->DeleteGuild(guild->GetId());
    }
};

// The guild house: halls 449 and 450, one phase per open house.
class GuildLevelAllMapScript : public AllMapScript
{
public:
    GuildLevelAllMapScript() : AllMapScript("GuildLevelAllMapScript",
        {
            ALLMAPHOOK_ON_PLAYER_ENTER_ALL,
            ALLMAPHOOK_ON_PLAYER_LEAVE_ALL,
            ALLMAPHOOK_ON_MAP_UPDATE
        }) { }

    void OnPlayerEnterAll(Map* map, Player* player) override
    {
        sGuildHouseMgr->OnPlayerEnterMap(map, player);
    }

    void OnPlayerLeaveAll(Map* map, Player* player) override
    {
        sGuildHouseMgr->OnPlayerLeaveMap(map, player);
    }

    void OnMapUpdate(Map* map, uint32 diff) override
    {
        sGuildHouseMgr->OnMapUpdate(map, diff);
    }
};

void AddSC_guild_level_commands();
void AddSC_guild_perk_commands();

void AddSC_guild_level_scripts()
{
    new GuildLevelWorldScript();
    new GuildLevelPlayerScript();
    new GuildLevelGuildScript();
    new GuildLevelUnitScript();
    new GuildLevelAllMapScript();
    RegisterSpellScript(spell_guild_level_hearthstone);
    RegisterSpellScript(spell_guild_level_perk);
    AddSC_guild_level_commands();
    AddSC_guild_perk_commands();
}
