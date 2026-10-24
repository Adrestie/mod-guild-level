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
 * mod-guild-level — the module's texts (`module_string`), safely.
 *
 * The core's ChatHandler::PGetParseModuleString dereferences the text it looks
 * up: a text missing from the database crashes the server. These helpers give
 * an empty text instead (the core logs the missing one), and send nothing to a
 * bot of mod-playerbots, whose client is the server's own.
 */

#ifndef MOD_GUILD_TEXT_H_
#define MOD_GUILD_TEXT_H_

#include "Chat.h"
#include "GuildLevel.h"
#include "Player.h"
#include "StringFormat.h"

#include <string>

// A bot of mod-playerbots (an AI attached, or one of its accounts).
bool GuildIsBot(Player const* player);

// The text in the handler's language, empty when the database lacks it.
template<typename... Args>
std::string GuildText(ChatHandler const* handler, uint32 id, Args&&... args)
{
    if (!handler || (!handler->IsConsole() && !handler->HasSession()))
        return std::string();
    std::string const* text = handler->GetModuleString(GUILD_LEVEL_MODULE, id);
    if (!text)
        return std::string();
    return Acore::StringFormat(text->c_str(), std::forward<Args>(args)...);
}

// The text to the handler (a command's answer); nothing when it is missing.
template<typename... Args>
void GuildSay(ChatHandler* handler, uint32 id, Args&&... args)
{
    std::string const text = GuildText(handler, id, std::forward<Args>(args)...);
    if (!text.empty())
        handler->SendSysMessage(text);
}

// The text to a player in his own language: nothing to a bot, a player without a session, or when it
// is missing.
template<typename... Args>
void GuildTell(Player* player, uint32 id, Args&&... args)
{
    if (!player || !player->GetSession() || GuildIsBot(player))
        return;
    ChatHandler handler(player->GetSession());
    GuildSay(&handler, id, std::forward<Args>(args)...);
}

#endif
