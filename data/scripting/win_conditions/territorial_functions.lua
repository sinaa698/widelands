-- RST
-- territorial_functions.lua
-- ---------------------------
--
-- This file contains common code for the "Territorial Lord" and "Territorial Time" win conditions.

set_textdomain("win_conditions")

include "scripting/richtext.lua"
include "scripting/win_conditions/win_condition_texts.lua"

local team_str = _"Team %i"
local wc_has_territory = _"%1$s has %2$3.0f%% of the land (%3$i of %4$i)."
local wc_had_territory = _"%1$s had %2$3.0f%% of the land (%3$i of %4$i)."

-- RST
-- .. function:: get_buildable_fields()
--
--    Collects all fields that are buildable
--
--    :returns: a table with the map's buildable fields
--
function get_buildable_fields()
   local fields = {}
   local map = wl.Game().map
   for x=0, map.width-1 do
      for y=0, map.height-1 do
         local f = map:get_field(x,y)
         if f.buildable then
            table.insert(fields, f)
         end
      end
   end
   print("NOCOM Found " .. #fields .. " buildable fields")
   return fields
end

-- RST
-- .. function:: count_owned_fields_for_all_players(fields, players)
--
--    Counts all owned fields for each player.
--
--    :arg fields: Table of all buildable fields
--    :arg players: Table of all players
--
--    :returns: a table with ``playernumber = count_of_owned_fields``  entries
--
local function count_owned_fields_for_all_players(fields, players)
   local owned_fields = {}
   -- init the landsizes for each player
   for idx,plr in ipairs(players) do
      owned_fields[plr.number] = 0
   end

   for idx,f in ipairs(fields) do
      -- check if field is owned by a player
      local owner = f.owner
      if owner then
         local owner_number = owner.number
         if owned_fields[owner_number] == nil then
            -- In case player was defeated and lost all their warehouses, make sure they don't count
            owned_fields[owner_number] = -1
         elseif owned_fields[owner_number] >= 0 then
            owned_fields[owner_number] = owned_fields[owner_number] + 1
         end
      end
   end
   return owned_fields
end


-- Used by calculate_territory_points keep track of when the winner changes
local winning_players = {}
local winning_teams = {}


-- RST
-- .. data:: territory_points
--
--    This table contains information about the current points and winning status for all
--    players and teams:
--
--    .. code-block:: lua
--
--       territory_points = {
--          -- The currently winning team, if any. -1 means that no team is currently winning.
--          last_winning_team = -1,
--          -- The currently winning player, if any. -1 means that no player is currently winning.
--          last_winning_player = -1,
--          -- Remaining time in secs for victory by > 50% territory. Default value is also used to calculate whether to send a report to players.
--          remaining_time = 10,
--          -- Points by player
--          all_player_points = {},
--          -- Points by rank, used to generate messages to the players
--          points = {}
--       }
--
territory_points = {
   -- TODO(GunChleoc): We want to be able to list multiple winners in case of a draw.
   last_winning_team = -1,
   last_winning_player = -1,
   remaining_time = 10,
   all_player_points = {},
   points = {}
}

-- RST
-- .. function:: calculate_territory_points(fields, players, wc_descname, wc_version)
--
--    First checks if a player was defeated, then fills the ``territory_points`` table
--    with current data.
--
--    :arg fields: Table of all buildable fields
--    :arg players: Table of all players
--    :arg wc_descname: String with the win condition's descname
--    :arg wc_version: Number with the win condition's descname
--
function calculate_territory_points(fields, players, wc_descname, wc_version)
   -- A player might have been defeated since the last calculation
   check_player_defeated(players, lost_game.title, lost_game.body, wc_descname, wc_version)

   local points = {} -- tracking points of teams and players without teams
   local territory_was_kept = false

   territory_points.all_player_points = count_owned_fields_for_all_players(fields, players)
   local ranked_players = rank_players(territory_points.all_player_points, players)

   -- Check if we have a winner. The table was sorted, so we can simply grab the first entry.
   local winning_points = -1
   if ranked_players[1].points > ( #fields / 2 ) then
      winning_points = ranked_players[1].points
   end

   -- Calculate which team or player is the current winner, and whether the winner has changed
   for tidx, teaminfo in ipairs(ranked_players) do
      local is_winner = teaminfo.points == winning_points
      if teaminfo.team ~= 0 then
         points[#points + 1] = { team_str:format(teaminfo.team), teaminfo.points }
         if is_winner then
            print("NOCOM Winner is team " .. teaminfo.team .. " with " .. teaminfo.points .. " points")
            territory_was_kept = winning_teams[teaminfo.team] ~= nil
            winning_teams[teaminfo.team] = true
            territory_points.last_winning_team = teaminfo.team
            territory_points.last_winning_player = -1
         else
            winning_teams[teaminfo.team] = nil
         end
      end

      for pidx, playerinfo in ipairs(teaminfo.players) do
         if is_winner and teaminfo.team == 0 and teaminfo.points == playerinfo.points then
            print("NOCOM Winner is player " .. playerinfo.number .. " with " .. playerinfo.points .. " points")
            territory_was_kept = winning_players[playerinfo.number] ~= nil
            winning_players[playerinfo.number] = true
            territory_points.last_winning_player = playerinfo.number
            territory_points.last_winning_team = -1
         else
            winning_players[playerinfo.number] = nil
         end
         if teaminfo.team == 0 and players[playerinfo.number] ~= nil then
            points[#points + 1] = { players[playerinfo.number].name, playerinfo.points }
         end
      end
   end

   -- Set the remaining time according to whether the winner is still the same
   if territory_was_kept then
      -- Still the same winner
      territory_points.remaining_time = territory_points.remaining_time - 30
      print("NOCOM Territory was kept by " .. territory_points.last_winning_team .. " - " .. territory_points.last_winning_player .. ". Remaining time: " .. territory_points.remaining_time)
   elseif winning_points == -1 then
      -- No winner. This value is used to calculate whether to send a report to players.
      territory_points.remaining_time = 10
   else
      -- Winner changed
      territory_points.remaining_time = 20 * 60 -- 20 minutes
      print("NOCOM NEW aqcuisition by " .. territory_points.last_winning_team .. " - " .. territory_points.last_winning_player .. ". Remaining time: " .. territory_points.remaining_time)
   end
   territory_points.points = points
end

-- RST
-- .. function:: territory_status(fields, has_had)
--
--    Returns a string containing the current land percentages of players/teams
--    for messages to the players
--
--    :arg fields: Table of all buildable fields
--    :arg has_had: Use "has" for an interim message, "had" for a game over message.
--
--    :returns: a richtext-formatted string with information on current points for each player/team
--
function territory_status(fields, has_had)
   local function _percent(part, whole)
      return (part * 100) / whole
   end

   local msg = ""
   for i=1,#territory_points.points do
      if (has_had == "has") then
         msg = msg ..
            li(
               (wc_has_territory):bformat(
                  territory_points.points[i][1],
                  _percent(territory_points.points[i][2], #fields),
                  territory_points.points[i][2],
                  #fields))
      else
         msg = msg ..
            li(
               (wc_had_territory):bformat(
                  territory_points.points[i][1],
                  _percent(territory_points.points[i][2], #fields),
                  territory_points.points[i][2],
                  #fields))
      end

   end
   return p(msg)
end

-- RST
-- .. function:: winning_status_header()
--
--    Returns a string containing a status message header for a winning player
--
--    :returns: a richtext-formatted string with header information for a winning player
--
function winning_status_header()
   set_textdomain("win_conditions")
   local remaining_minutes = math.max(0, math.floor(territory_points.remaining_time / 60))

   local message = p(_"You own more than half of the map’s area.")
   message = message .. p(ngettext("Keep it for %i more minute to win the game.",
             "Keep it for %i more minutes to win the game.",
             remaining_minutes))
         :format(remaining_minutes)
   return message
end

-- RST
-- .. function:: losing_status_header(players)
--
--    Returns a string containing a status message header for a losing player
--
--    :arg players: Table of all players
--
--    :returns: a richtext-formatted string with header information for a losing player
--
function losing_status_header(players)
   set_textdomain("win_conditions")
   local winner_name = "Error"
   if territory_points.last_winning_team >= 0 then
      winner_name = team_str:format(territory_points.last_winning_team)
   elseif territory_points.last_winning_player >= 0 then
      winner_name = players[territory_points.last_winning_player].name
   end
   local remaining_minutes = math.max(0, math.floor(territory_points.remaining_time / 60))

   local message = p(_"%s owns more than half of the map’s area."):format(winner_name)
   message = message .. p(ngettext("You’ve still got %i minute to prevent a victory.",
             "You’ve still got %i minutes to prevent a victory.",
             remaining_minutes))
         :format(remaining_minutes)
   return message
end

-- RST
-- .. function:: territory_game_over(fields, players, wc_descname, wc_version)
--
--    Updates the territory points and sends game over reports
--
--    :arg fields: Table of all buildable fields
--    :arg players: Table of all players
--
function territory_game_over(fields, players, wc_descname, wc_version)
   calculate_territory_points(fields, players, wc_descname, wc_version)

   for idx, pl in ipairs(players) do
      pl.see_all = 1

      local wonmsg = won_game_over.body .. game_status.body
      local lostmsg = lost_game_over.body .. game_status.body
      for i=1,#territory_points.points do
         if territory_points.points[i][1] == team_str:format(pl.team) or territory_points.points[i][1] == pl.name then
            if territory_points.points[i][2] >= territory_points.points[1][2] then
               pl:send_message(won_game_over.title, wonmsg .. territory_status(territory_points.points, fields, "had"))
               wl.game.report_result(pl, 1, make_extra_data(pl, wc_descname, wc_version, {score=territory_points.all_player_points[pl.number]}))
            else
               pl:send_message(lost_game_over.title, lostmsg .. territory_status(territory_points.points, fields, "had"))
               wl.game.report_result(pl, 0, make_extra_data(pl, wc_descname, wc_version, {score=territory_points.all_player_points[pl.number]}))
            end
         end
      end
   end
end
