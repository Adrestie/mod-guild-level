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
#include "Player.h"
#include "ScriptMgr.h"

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
    void OnAfterConfigLoad(bool /*reload*/) override
    {
        sGuildLevelMgr->LoadConfig();
    }

    // The guilds are loaded by then.
    void OnStartup() override
    {
        sGuildLevelMgr->LoadFromDB();
    }

    void OnUpdate(uint32 diff) override
    {
        sGuildLevelMgr->Update(diff);
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
            PLAYERHOOK_ON_GIVE_EXP
        }) { }

    // Once per member the kill rewards with experience, right before he gets
    // it; on a map thread. `amount` is left alone.
    void OnPlayerGiveXP(Player* player, uint32& amount, Unit* victim, uint8 xpSource) override
    {
        if (xpSource == XPSOURCE_KILL && amount)
            sGuildLevelMgr->OnKillXP(player, victim);
    }
};

class GuildLevelGuildScript : public GuildScript
{
public:
    GuildLevelGuildScript() : GuildScript("GuildLevelGuildScript",
        {
            GUILDHOOK_ON_DISBAND
        }) { }

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
    AddSC_guild_level_commands();
}
