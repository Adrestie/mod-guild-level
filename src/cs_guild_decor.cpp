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
 * .ghouse — the Customization mode of the guild house (GuildDecor.h). The
 * members' interface (GuildHouse.lua) runs them as the player: begin, place,
 * cancel and remove are what an edit is made of, and say nothing when they
 * succeed. give, for a game master, fills a guild's stock.
 */

#include "Chat.h"
#include "ChatCommand.h"
#include "Guild.h"
#include "GuildDecor.h"
#include "GuildLevel.h"
#include "GuildMgr.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "StringConvert.h"

using namespace Acore::ChatCommands;

namespace
{
    template<typename... Args>
    void Say(ChatHandler* handler, uint32 id, Args&&... args)
    {
        handler->SendSysMessage(handler->PGetParseModuleString(GUILD_LEVEL_MODULE, id, std::forward<Args>(args)...));
    }

    // Told only when refused.
    bool Tell(ChatHandler* handler, GuildDecorResult result)
    {
        if (result == GuildDecorResult::Locked)
            Say(handler, GuildDecorMgr::TextOf(result), uint32(sGuildDecorMgr->Config().paragon));
        else if (result == GuildDecorResult::NotAllowed)
            Say(handler, GuildDecorMgr::TextOf(result), uint32(sGuildDecorMgr->Config().rank));
        else if (uint32 text = GuildDecorMgr::TextOf(result))
            Say(handler, text);
        return true;
    }
}

class guild_decor_commandscript : public CommandScript
{
public:
    guild_decor_commandscript() : CommandScript("guild_decor_commandscript") { }

    ChatCommandTable GetCommands() const override
    {
        static ChatCommandTable houseTable =
        {
            { "begin",  HandleBeginCommand,  SEC_PLAYER,     Console::No },
            { "place",  HandlePlaceCommand,  SEC_PLAYER,     Console::No },
            { "cancel", HandleCancelCommand, SEC_PLAYER,     Console::No },
            { "remove", HandleRemoveCommand, SEC_PLAYER,     Console::No },
            { "give",   HandleGiveCommand,   SEC_GAMEMASTER, Console::Yes },
        };
        static ChatCommandTable commandTable =
        {
            { "ghouse", houseTable },
        };
        return commandTable;
    }

    // .ghouse begin <placed id>: in hand, out of the house for the others
    static bool HandleBeginCommand(ChatHandler* handler, uint32 id)
    {
        return Tell(handler, sGuildDecorMgr->Begin(handler->GetPlayer(), id));
    }

    // .ghouse place <placed id, 0 for a new one> <object> <x> <y> <z> <qx> <qy> <qz> <qw> <scale>
    static bool HandlePlaceCommand(ChatHandler* handler, uint32 id, uint32 decor, float x, float y, float z,
        float qx, float qy, float qz, float qw, float scale)
    {
        uint32 placed = 0;
        return Tell(handler, sGuildDecorMgr->Place(handler->GetPlayer(), id, decor, { x, y, z }, { qx, qy, qz, qw },
            scale, placed));
    }

    // .ghouse cancel <placed id>: back as it was
    static bool HandleCancelCommand(ChatHandler* handler, uint32 id)
    {
        return Tell(handler, sGuildDecorMgr->Cancel(handler->GetPlayer(), id));
    }

    // .ghouse remove <placed id>: back to the stock
    static bool HandleRemoveCommand(ChatHandler* handler, uint32 id)
    {
        return Tell(handler, sGuildDecorMgr->Remove(handler->GetPlayer(), id));
    }

    // .ghouse give <object, 0 for every one> <count, may be negative> [guild]
    static bool HandleGiveCommand(ChatHandler* handler, uint32 decor, int32 count, Optional<Tail> named)
    {
        Guild* guild = nullptr;
        if (named && !named->empty())
        {
            std::string const name(*named);
            guild = sGuildMgr->GetGuildByName(name);
            if (!guild)
                if (Optional<uint32> gid = Acore::StringTo<uint32>(name))
                    guild = sGuildMgr->GetGuildById(*gid);
        }
        else if (handler->GetSession())
        {
            Player* player = handler->getSelectedPlayerOrSelf();
            if (player && player->GetGuildId())
                guild = sGuildMgr->GetGuildById(player->GetGuildId());
        }
        if (!guild)
        {
            Say(handler, GUILD_LEVEL_STR_CMD_NO_GUILD);
            return true;
        }
        if (!sGuildDecorMgr->Give(guild->GetId(), decor, count))
        {
            Say(handler, GUILD_LEVEL_STR_DECOR_UNKNOWN);
            return true;
        }
        Say(handler, GUILD_LEVEL_STR_DECOR_GIVEN, count, decor, guild->GetName());
        return true;
    }
};

void AddSC_guild_decor_commands()
{
    new guild_decor_commandscript();
}
