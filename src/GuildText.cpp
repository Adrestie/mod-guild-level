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

#include "GuildText.h"

#include "Player.h"
#include "WorldSession.h"

// Judged on the account too: at login a bot has no AI attached yet (as mod-shared-professions does).
#if defined(MOD_PLAYERBOTS)
#include "Playerbots.h"
#include "PlayerbotAIConfig.h"
#include "RandomPlayerbotMgr.h"

bool GuildIsBot(Player const* player)
{
    if (GET_PLAYERBOT_AI(const_cast<Player*>(player)))
        return true;
    uint32 const account = player->GetSession() ? player->GetSession()->GetAccountId() : 0;
    return account && (sPlayerbotAIConfig.IsInRandomAccountList(account) || sRandomPlayerbotMgr.IsAddClassAccount(account));
}
#else
bool GuildIsBot(Player const* /*player*/)
{
    return false;
}
#endif
