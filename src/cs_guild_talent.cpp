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
 * mod-guild-level — `.gtalent`, the guild talents. Every member may look and,
 * from the rank allowed, spend and reset; the talent window runs these same
 * commands as the player, so every rule lives here. The edits of the tree are
 * an administrator's: the window's edit mode runs them too.
 */

#include "Chat.h"
#include "GuildText.h"
#include "ChatCommand.h"
#include "Guild.h"
#include "GuildLevel.h"
#include "GuildMgr.h"
#include "GuildPerks.h"
#include "GuildTalents.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "ScriptMgr.h"

using namespace Acore::ChatCommands;

namespace
{
    template<typename... Args>
    void Say(ChatHandler* handler, uint32 id, Args&&... args)
    {
        GuildSay(handler, id, std::forward<Args>(args)...);
    }

    // A refusal, said.
    void Tell(ChatHandler* handler, GuildTalentResult result, uint32 price = 0)
    {
        switch (result)
        {
            case GuildTalentResult::Disabled:       Say(handler, GUILD_LEVEL_STR_TALENT_DISABLED); break;
            case GuildTalentResult::NoGuild:        Say(handler, GUILD_LEVEL_STR_ACTIVE_NO_GUILD); break;
            case GuildTalentResult::NotAllowed:     Say(handler, GUILD_LEVEL_STR_TALENT_NOT_ALLOWED, uint32(sGuildTalentMgr->Config().rank)); break;
            case GuildTalentResult::NoNode:         Say(handler, GUILD_LEVEL_STR_TALENT_NO_NODE); break;
            case GuildTalentResult::Full:           Say(handler, GUILD_LEVEL_STR_TALENT_FULL); break;
            case GuildTalentResult::NotLinked:      Say(handler, GUILD_LEVEL_STR_TALENT_NOT_LINKED); break;
            case GuildTalentResult::Tier:           Say(handler, GUILD_LEVEL_STR_TALENT_TIER); break;
            case GuildTalentResult::NoPoints:       Say(handler, GUILD_LEVEL_STR_TALENT_NO_POINTS); break;
            case GuildTalentResult::NothingToReset: Say(handler, GUILD_LEVEL_STR_TALENT_NOTHING); break;
            case GuildTalentResult::NoMoney:        Say(handler, GUILD_LEVEL_STR_TALENT_NO_MONEY, price); break;
            default: break;
        }
    }

    bool Edited(ChatHandler* handler, bool done, uint32 node)
    {
        if (done)
            Say(handler, GUILD_LEVEL_STR_TALENT_EDITED, node);
        else
            Say(handler, GUILD_LEVEL_STR_TALENT_BAD_EDIT);
        return true;
    }
}

class guild_talent_commandscript : public CommandScript
{
public:
    guild_talent_commandscript() : CommandScript("guild_talent_commandscript") { }

    ChatCommandTable GetCommands() const override
    {
        static ChatCommandTable nodeTable =
        {
            { "add",    HandleNodeAddCommand,    SEC_ADMINISTRATOR, Console::Yes },
            { "set",    HandleNodeSetCommand,    SEC_ADMINISTRATOR, Console::Yes },
            { "move",   HandleNodeMoveCommand,   SEC_ADMINISTRATOR, Console::Yes },
            { "start",  HandleNodeStartCommand,  SEC_ADMINISTRATOR, Console::Yes },
            { "icon",   HandleNodeIconCommand,   SEC_ADMINISTRATOR, Console::Yes },
            { "delete", HandleNodeDeleteCommand, SEC_ADMINISTRATOR, Console::Yes },
        };

        static ChatCommandTable talentTable =
        {
            { "",       HandleShowCommand,   SEC_PLAYER,        Console::No },
            { "buy",    HandleBuyCommand,    SEC_PLAYER,        Console::No },
            { "reset",  HandleResetCommand,  SEC_PLAYER,        Console::No },
            { "node",   nodeTable },
            { "link",   HandleLinkCommand,   SEC_ADMINISTRATOR, Console::Yes },
            { "reload", HandleReloadCommand, SEC_ADMINISTRATOR, Console::Yes },
            { "clear",  HandleClearCommand,  SEC_GAMEMASTER,    Console::No },
        };

        static ChatCommandTable commandTable =
        {
            { "gtalent", talentTable },
        };

        return commandTable;
    }

    // The guild's points and what it bought.
    static bool HandleShowCommand(ChatHandler* handler)
    {
        Player* player = handler->GetPlayer();
        if (!player->GetGuildId())
        {
            Say(handler, GUILD_LEVEL_STR_ACTIVE_NO_GUILD);
            return true;
        }
        sGuildTalentMgr->Show(handler, player->GetGuildId());
        return true;
    }

    static bool HandleBuyCommand(ChatHandler* handler, uint32 node)
    {
        Player* player = handler->GetPlayer();
        GuildTalentResult const result = sGuildTalentMgr->Buy(player, node);
        if (result != GuildTalentResult::Ok)
        {
            Tell(handler, result);
            return true;
        }
        GuildTalentState const state = sGuildTalentMgr->State(player->GetGuildId());
        auto const tree = sGuildTalentMgr->Tree();
        auto held = state.points.find(node);
        auto def = tree.find(node);
        Say(handler, GUILD_LEVEL_STR_TALENT_BOUGHT, node, held != state.points.end() ? uint32(held->second) : 0,
            def != tree.end() ? uint32(def->second.maxPoints) : 0);
        return true;
    }

    // Without "confirm", the price; with it, the reset.
    static bool HandleResetCommand(ChatHandler* handler, Optional<std::string> confirm)
    {
        Player* player = handler->GetPlayer();
        if (!confirm || *confirm != "confirm")
        {
            if (!player->GetGuildId())
                Say(handler, GUILD_LEVEL_STR_ACTIVE_NO_GUILD);
            else
                Say(handler, GUILD_LEVEL_STR_TALENT_RESET_ASK, sGuildTalentMgr->ResetPrice(player->GetGuildId()));
            return true;
        }
        uint32 price = 0;
        GuildTalentResult const result = sGuildTalentMgr->Reset(player, price);
        if (result == GuildTalentResult::Ok)
            Say(handler, GUILD_LEVEL_STR_TALENT_RESET_DONE, price);
        else
            Tell(handler, result, price);
        return true;
    }

    // [chance %] [cooldown seconds] [tier 1-3] after the maximum.
    static bool HandleNodeAddCommand(ChatHandler* handler, int32 x, int32 y, uint32 effect, float amount, uint32 maxPoints,
        Optional<float> chance, Optional<uint32> cooldown, Optional<uint32> tier)
    {
        uint32 const id = IsGuildTalentEffect(effect) && maxPoints && maxPoints <= 255 ?
            sGuildTalentMgr->AddNode(x, y, GuildTalentEffect(effect), amount, uint8(maxPoints), chance.value_or(0.0f),
                cooldown.value_or(0), uint8(std::min<uint32>(tier.value_or(1), 255))) : 0;
        return Edited(handler, id != 0, id);
    }

    static bool HandleNodeSetCommand(ChatHandler* handler, uint32 id, uint32 effect, float amount, uint32 maxPoints,
        Optional<float> chance, Optional<uint32> cooldown, Optional<uint32> tier)
    {
        bool const done = IsGuildTalentEffect(effect) && maxPoints && maxPoints <= 255 &&
            sGuildTalentMgr->SetNode(id, GuildTalentEffect(effect), amount, uint8(maxPoints), chance.value_or(0.0f),
                cooldown.value_or(0), uint8(std::min<uint32>(tier.value_or(1), 255)));
        return Edited(handler, done, id);
    }

    static bool HandleNodeMoveCommand(ChatHandler* handler, uint32 id, int32 x, int32 y)
    {
        return Edited(handler, sGuildTalentMgr->MoveNode(id, x, y), id);
    }

    static bool HandleNodeStartCommand(ChatHandler* handler, uint32 id, uint32 start)
    {
        return Edited(handler, sGuildTalentMgr->SetStart(id, start != 0), id);
    }

    // No icon: back to the effect's own.
    static bool HandleNodeIconCommand(ChatHandler* handler, uint32 id, Optional<std::string> icon)
    {
        return Edited(handler, sGuildTalentMgr->SetIcon(id, icon.value_or("")), id);
    }

    static bool HandleNodeDeleteCommand(ChatHandler* handler, uint32 id)
    {
        return Edited(handler, sGuildTalentMgr->DeleteNode(id), id);
    }

    static bool HandleLinkCommand(ChatHandler* handler, uint32 a, uint32 b)
    {
        bool linked = false;
        return Edited(handler, sGuildTalentMgr->ToggleLink(a, b, linked), a);
    }

    static bool HandleReloadCommand(ChatHandler* handler)
    {
        sGuildTalentMgr->LoadTree();
        for (auto const& [guid, player] : ObjectAccessor::GetPlayers())
            sGuildPerkMgr->Apply(player);
        sGuildTalentMgr->NotifyEveryone();
        handler->SendSysMessage("Guild talents reloaded.");
        return true;
    }

    // GM: every point of a guild back, for free (its name, else the selected
    // player's guild, else yours).
    static bool HandleClearCommand(ChatHandler* handler, Optional<Tail> named)
    {
        Guild* guild = nullptr;
        if (named && !named->empty())
            guild = sGuildMgr->GetGuildByName(std::string(*named));
        else if (Player* player = handler->getSelectedPlayerOrSelf())
            guild = player->GetGuildId() ? sGuildMgr->GetGuildById(player->GetGuildId()) : nullptr;
        if (!guild)
        {
            Say(handler, GUILD_LEVEL_STR_CMD_NO_GUILD);
            return true;
        }
        sGuildTalentMgr->ClearGuild(guild->GetId());
        Say(handler, GUILD_LEVEL_STR_CMD_DONE);
        return true;
    }
};

void AddSC_guild_talent_commands()
{
    new guild_talent_commandscript();
}
