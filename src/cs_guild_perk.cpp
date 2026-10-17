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
 * mod-guild-level — `.gperk`, the player commands of the active perks of
 * levels 21 to 25. Every player may type them; GuildActivePerkMgr::Use checks
 * the guild, its level and the cooldown. `.gperk reset` is a GM's.
 */

#include "Chat.h"
#include "ChatCommand.h"
#include "GuildActivePerks.h"
#include "GuildHouse.h"
#include "GuildLevel.h"
#include "Player.h"
#include "ScriptMgr.h"

using namespace Acore::ChatCommands;

class guild_perk_commandscript : public CommandScript
{
public:
    guild_perk_commandscript() : CommandScript("guild_perk_commandscript") { }

    ChatCommandTable GetCommands() const override
    {
        static ChatCommandTable perkTable =
        {
            { "list",   HandleListCommand,   SEC_PLAYER, Console::No },
            { "raid",   HandleRaidCommand,   SEC_PLAYER, Console::No },
            { "rez",    HandleRezCommand,    SEC_PLAYER, Console::No },
            { "bank",   HandleBankCommand,   SEC_PLAYER, Console::No },
            { "vendor", HandleVendorCommand, SEC_PLAYER, Console::No },
            { "home",   HandleHomeCommand,   SEC_PLAYER, Console::No },
            { "leave",  HandleLeaveCommand,  SEC_PLAYER, Console::No },
            { "reset",  HandleResetCommand,  SEC_GAMEMASTER, Console::Yes },
        };

        static ChatCommandTable commandTable =
        {
            { "gperk", perkTable },
        };

        return commandTable;
    }

    // The active perks of your guild, and your cooldowns.
    static bool HandleListCommand(ChatHandler* handler)
    {
        Player* player = handler->GetPlayer();
        if (!player->GetGuildId())
        {
            handler->SendSysMessage(handler->PGetParseModuleString(GUILD_LEVEL_MODULE, GUILD_LEVEL_STR_ACTIVE_NO_GUILD));
            return true;
        }
        sGuildActivePerkMgr->List(handler, sGuildLevelMgr->Get(player->GetGuildId()).level, player);
        return true;
    }

    static bool HandleRaidCommand(ChatHandler* handler)
    {
        sGuildActivePerkMgr->Use(handler, GUILD_ACTIVE_RAID);
        return true;
    }

    static bool HandleRezCommand(ChatHandler* handler)
    {
        sGuildActivePerkMgr->Use(handler, GUILD_ACTIVE_REZ);
        return true;
    }

    static bool HandleBankCommand(ChatHandler* handler)
    {
        sGuildActivePerkMgr->Use(handler, GUILD_ACTIVE_BANK);
        return true;
    }

    static bool HandleVendorCommand(ChatHandler* handler)
    {
        sGuildActivePerkMgr->Use(handler, GUILD_ACTIVE_VENDOR);
        return true;
    }

    // GM: every cooldown of a character (named, else the selected player,
    // else yourself) back to ready, online or not.
    static bool HandleResetCommand(ChatHandler* handler, Optional<PlayerIdentifier> target)
    {
        if (!target)
            target = PlayerIdentifier::FromTargetOrSelf(handler);
        if (!target)
            return false;

        sGuildActivePerkMgr->ResetCooldowns(target->GetGUID());
        handler->SendSysMessage(handler->PGetParseModuleString(GUILD_LEVEL_MODULE, GUILD_LEVEL_STR_ACTIVE_RESET, target->GetName()));
        return true;
    }

    // Out of the guild house, back where the player came from. No cooldown.
    static bool HandleLeaveCommand(ChatHandler* handler)
    {
        sGuildHouseMgr->Leave(handler, handler->GetPlayer());
        return true;
    }

    static bool HandleHomeCommand(ChatHandler* handler)
    {
        sGuildActivePerkMgr->Use(handler, GUILD_ACTIVE_HOME);
        return true;
    }
};

void AddSC_guild_perk_commands()
{
    new guild_perk_commandscript();
}
