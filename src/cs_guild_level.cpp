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
 * mod-guild-level — the GM commands (.guildlevel). The guild is the one named
 * at the end of the line (its name, spaces allowed, or its id); without it,
 * the guild of the selected player, else your own.
 */

#include "Chat.h"
#include "ChatCommand.h"
#include "Guild.h"
#include "GuildActivePerks.h"
#include "GuildLevel.h"
#include "GuildPerks.h"
#include "GuildMgr.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "StringConvert.h"

using namespace Acore::ChatCommands;

namespace
{
    // Through the handler's own lookup: the player's language in game, English
    // on the console.
    template<typename... Args>
    void Say(ChatHandler* handler, uint32 id, Args&&... args)
    {
        handler->SendSysMessage(handler->PGetParseModuleString(GUILD_LEVEL_MODULE, id, std::forward<Args>(args)...));
    }

    Guild* FindGuild(ChatHandler* handler, Optional<Tail> const& named)
    {
        if (named && !named->empty())
        {
            std::string const name(*named);
            if (Guild* guild = sGuildMgr->GetGuildByName(name))
                return guild;
            if (Optional<uint32> id = Acore::StringTo<uint32>(name))
                return sGuildMgr->GetGuildById(*id);
            return nullptr;
        }

        if (!handler->GetSession())
            return nullptr;
        Player* player = handler->getSelectedPlayerOrSelf();
        return (player && player->GetGuildId()) ? sGuildMgr->GetGuildById(player->GetGuildId()) : nullptr;
    }

    void ShowState(ChatHandler* handler, Guild const* guild)
    {
        GuildLevelState const state = sGuildLevelMgr->Get(guild->GetId());
        Say(handler, GUILD_LEVEL_STR_CMD_INFO, guild->GetName(), guild->GetId(),
            uint32(state.level), uint32(GUILD_LEVEL_MAX), uint32(state.paragon), uint32(GUILD_PARAGON_MAX));

        if (uint64 const need = sGuildLevelMgr->Needed(state))
            Say(handler, GUILD_LEVEL_STR_CMD_INFO_XP, state.xp, need);
        else
            Say(handler, GUILD_LEVEL_STR_CMD_INFO_DONE);
        Say(handler, GUILD_LEVEL_STR_CMD_INFO_TOTAL, state.totalXp);

        if (!sGuildLevelMgr->Config().enable)
            Say(handler, GUILD_LEVEL_STR_CMD_DISABLED);
    }
}

class guild_level_commandscript : public CommandScript
{
public:
    guild_level_commandscript() : CommandScript("guild_level_commandscript") { }

    ChatCommandTable GetCommands() const override
    {
        static ChatCommandTable guildLevelTable =
        {
            { "info",       HandleInfoCommand,       SEC_GAMEMASTER, Console::Yes },
            { "setlevel",   HandleSetLevelCommand,   SEC_GAMEMASTER, Console::Yes },
            { "setparagon", HandleSetParagonCommand, SEC_GAMEMASTER, Console::Yes },
            { "setxp",      HandleSetXPCommand,      SEC_GAMEMASTER, Console::Yes },
            { "addxp",      HandleAddXPCommand,      SEC_GAMEMASTER, Console::Yes },
            { "perks",      HandlePerksCommand,      SEC_GAMEMASTER, Console::Yes },
        };

        static ChatCommandTable commandTable =
        {
            { "guildlevel", guildLevelTable },
        };

        return commandTable;
    }

    static bool HandleInfoCommand(ChatHandler* handler, Optional<Tail> named)
    {
        Guild* guild = FindGuild(handler, named);
        if (!guild)
        {
            Say(handler, GUILD_LEVEL_STR_CMD_NO_GUILD);
            return true;
        }
        ShowState(handler, guild);
        return true;
    }

    static bool HandleSetLevelCommand(ChatHandler* handler, uint32 level, Optional<Tail> named)
    {
        Guild* guild = FindGuild(handler, named);
        if (!guild)
        {
            Say(handler, GUILD_LEVEL_STR_CMD_NO_GUILD);
            return true;
        }
        if (level > GUILD_LEVEL_MAX)
        {
            Say(handler, GUILD_LEVEL_STR_CMD_BAD_LEVEL, uint32(GUILD_LEVEL_MAX));
            return true;
        }
        sGuildLevelMgr->SetLevel(guild->GetId(), uint8(level));
        Say(handler, GUILD_LEVEL_STR_CMD_DONE);
        ShowState(handler, guild);
        return true;
    }

    static bool HandleSetParagonCommand(ChatHandler* handler, uint32 paragon, Optional<Tail> named)
    {
        Guild* guild = FindGuild(handler, named);
        if (!guild)
        {
            Say(handler, GUILD_LEVEL_STR_CMD_NO_GUILD);
            return true;
        }
        if (paragon > GUILD_PARAGON_MAX)
        {
            Say(handler, GUILD_LEVEL_STR_CMD_BAD_LEVEL, uint32(GUILD_PARAGON_MAX));
            return true;
        }
        sGuildLevelMgr->SetParagon(guild->GetId(), uint8(paragon));
        Say(handler, GUILD_LEVEL_STR_CMD_DONE);
        ShowState(handler, guild);
        return true;
    }

    static bool HandleSetXPCommand(ChatHandler* handler, uint64 xp, Optional<Tail> named)
    {
        Guild* guild = FindGuild(handler, named);
        if (!guild)
        {
            Say(handler, GUILD_LEVEL_STR_CMD_NO_GUILD);
            return true;
        }
        sGuildLevelMgr->SetXP(guild->GetId(), xp);
        Say(handler, GUILD_LEVEL_STR_CMD_DONE);
        ShowState(handler, guild);
        return true;
    }

    // The perks the guild's members get at its current level.
    static bool HandlePerksCommand(ChatHandler* handler, Optional<Tail> named)
    {
        Guild* guild = FindGuild(handler, named);
        if (!guild)
        {
            Say(handler, GUILD_LEVEL_STR_CMD_NO_GUILD);
            return true;
        }
        uint8 const level = sGuildLevelMgr->Get(guild->GetId()).level;
        sGuildPerkMgr->Show(handler, level);

        // The cooldowns shown are those of the selected player, else your
        // own, when that character belongs to the guild.
        Player* player = handler->GetSession() ? handler->getSelectedPlayerOrSelf() : nullptr;
        if (player && player->GetGuildId() != guild->GetId())
            player = nullptr;
        sGuildActivePerkMgr->List(handler, level, player);
        return true;
    }

    // As if the guild had earned it: the levels it climbs are announced.
    static bool HandleAddXPCommand(ChatHandler* handler, uint64 xp, Optional<Tail> named)
    {
        Guild* guild = FindGuild(handler, named);
        if (!guild)
        {
            Say(handler, GUILD_LEVEL_STR_CMD_NO_GUILD);
            return true;
        }
        sGuildLevelMgr->AddXP(guild->GetId(), xp);
        Say(handler, GUILD_LEVEL_STR_CMD_DONE);
        ShowState(handler, guild);
        return true;
    }
};

void AddSC_guild_level_commands()
{
    new guild_level_commandscript();
}
