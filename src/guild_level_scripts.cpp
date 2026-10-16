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

#include "Guild.h"
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

        // New amounts for the members online at once.
        if (reload)
            for (auto const& [guid, player] : ObjectAccessor::GetPlayers())
                sGuildPerkMgr->Apply(player);
    }

    // The guilds are loaded by then.
    void OnStartup() override
    {
        sGuildLevelMgr->LoadFromDB();
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
            PLAYERHOOK_ON_PLAYER_RESURRECT
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
            sGuildPerkMgr->QueueRefresh(player->GetGUID());
    }

    void OnDisband(Guild* guild) override
    {
        sGuildLevelMgr->DeleteGuild(guild->GetId());
    }
};

void AddSC_guild_level_commands();

void AddSC_guild_level_scripts()
{
    new GuildLevelWorldScript();
    new GuildLevelPlayerScript();
    new GuildLevelGuildScript();
    new GuildLevelUnitScript();
    RegisterSpellScript(spell_guild_level_hearthstone);
    AddSC_guild_level_commands();
}
