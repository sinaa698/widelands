/*
 * Copyright (C) 2009-2017 by the Widelands Development Team
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 */

#include "ai/ai_help_structs.h"

//#include "ai/tmp_constants.h"

#include "base/macros.h"
#include "base/time_string.h"
#include "logic/map.h"
#include "logic/map.h"
#include "logic/player.h"

// couple of constants for calculation of road interconnections
constexpr int kRoadNotFound = -1000;
constexpr int kShortcutWithinSameEconomy = 1000;
constexpr int kRoadToDifferentEconomy = 10000;
constexpr bool kReInitialize = true;

namespace Widelands {

// CheckStepRoadAI
CheckStepRoadAI::CheckStepRoadAI(Player* const pl, uint8_t const mc, bool const oe)
   : player(pl), movecaps(mc), open_end(oe) {
}

bool CheckStepRoadAI::allowed(
   Map& map, FCoords start, FCoords end, int32_t, CheckStep::StepId const id) const {
	uint8_t endcaps = player->get_buildcaps(end);

	// we should not cross fields with road or flags (or any other immovable)
	if ((map.get_immovable(start)) && !(id == CheckStep::stepFirst)) {
		return false;
	}

	// Calculate cost and passability
	if (!(endcaps & movecaps))
		return false;

	// Check for blocking immovables
	if (BaseImmovable const* const imm = map.get_immovable(end))
		if (imm->get_size() >= BaseImmovable::SMALL) {
			if (id != CheckStep::stepLast && !open_end)
				return false;

			if (dynamic_cast<Flag const*>(imm))
				return true;

			if (!dynamic_cast<Road const*>(imm) || !(endcaps & BUILDCAPS_FLAG))
				return false;
		}

	return true;
}

bool CheckStepRoadAI::reachable_dest(const Map& map, const FCoords& dest) const {
	NodeCaps const caps = dest.field->nodecaps();

	if (!(caps & movecaps)) {
		if (!((movecaps & MOVECAPS_SWIM) && (caps & MOVECAPS_WALK)))
			return false;

		if (!map.can_reach_by_water(dest))
			return false;
	}

	return true;
}

// We are looking for fields we can walk on
// and owned by hostile player.
FindNodeEnemy::FindNodeEnemy(Player* p, Game& g) : player(p), game(g) {
}

bool FindNodeEnemy::accept(const Map&, const FCoords& fc) const {
	return (fc.field->nodecaps() & MOVECAPS_WALK) && fc.field->get_owned_by() != 0 &&
	       player->is_hostile(*game.get_player(fc.field->get_owned_by()));
}

// We are looking for buildings owned by hostile player
// (sometimes there is a enemy's teritorry without buildings, and
// this confuses the AI)
FindNodeEnemiesBuilding::FindNodeEnemiesBuilding(Player* p, Game& g) : player(p), game(g) {
}

bool FindNodeEnemiesBuilding::accept(const Map&, const FCoords& fc) const {
	return (fc.field->get_immovable()) && fc.field->get_owned_by() != 0 &&
	       player->is_hostile(*game.get_player(fc.field->get_owned_by()));
}

// When looking for unowned terrain to acquire, we are actually
// only interested in fields we can walk on.
// Fields should either be completely unowned or owned by an opposing player
FindEnemyNodeWalkable::FindEnemyNodeWalkable(Player* p, Game& g)
   : player(p), game(g) {
}

bool FindEnemyNodeWalkable::accept(const Map&, const FCoords& fc) const {
	return ((fc.field->nodecaps() & MOVECAPS_WALK) &&
	       (fc.field->get_owned_by() > 0) &&
	        player->is_hostile(*game.get_player(fc.field->get_owned_by())));
}

// Sometimes we need to know how many nodes our allies owns
FindNodeAllyOwned::FindNodeAllyOwned(Player* p, Game& g, PlayerNumber n)
   : player(p), game(g), player_number(n) {
}

bool FindNodeAllyOwned::accept(const Map&, const FCoords& fc) const {
	return (fc.field->nodecaps() & MOVECAPS_WALK) && (fc.field->get_owned_by() != 0) &&
	       (fc.field->get_owned_by() != player_number) &&
	       !player->is_hostile(*game.get_player(fc.field->get_owned_by()));
}

// When looking for unowned terrain to acquire, we must
// pay speciall attention to fields where mines can be built.
// Fields should be completely unowned
FindNodeUnownedMineable::FindNodeUnownedMineable(Player* p, Game& g, int32_t t) : player(p), game(g), ore_type(t) {
}

bool FindNodeUnownedMineable::accept(const Map&, const FCoords& fc) const {
	if (ore_type == INVALID_INDEX) {
		return (fc.field->nodecaps() & BUILDCAPS_MINE)  && (fc.field->get_owned_by() == 0);
	}
	return (fc.field->nodecaps() & BUILDCAPS_MINE)  && (fc.field->get_owned_by() == 0) && fc.field->get_resources() == ore_type;
}

FindNodeUnownedBuildable::FindNodeUnownedBuildable(Player* p, Game& g) : player(p), game(g) {
}

bool FindNodeUnownedBuildable::accept(const Map&, const FCoords& fc) const {
	return (fc.field->nodecaps() & BUILDCAPS_SIZEMASK) && (fc.field->get_owned_by() == 0);
}

// Unowned but walkable fields nearby
FindNodeUnownedWalkable::FindNodeUnownedWalkable(Player* p, Game& g) : player(p), game(g) {
}

bool FindNodeUnownedWalkable::accept(const Map&, const FCoords& fc) const {
	return (fc.field->nodecaps() & MOVECAPS_WALK) && (fc.field->get_owned_by() == 0);
}

// Looking only for mines-capable fields nearby
// of specific type
FindNodeMineable::FindNodeMineable(Game& g, DescriptionIndex r) : game(g), res(r) {
}

bool FindNodeMineable::accept(const Map&, const FCoords& fc) const {

	return (fc.field->nodecaps() & BUILDCAPS_MINE) && (fc.field->get_resources() == res);
}

// Fishers and fishbreeders must be built near water
FindNodeWater::FindNodeWater(const World& world) : world_(world) {
}

bool FindNodeWater::accept(const Map& map, const FCoords& coord) const {
	return (world_.terrain_descr(coord.field->terrain_d()).get_is() &
	        TerrainDescription::Is::kWater) ||
	       (world_.terrain_descr(map.get_neighbour(coord, WALK_W).field->terrain_r()).get_is() &
	        TerrainDescription::Is::kWater) ||
	       (world_.terrain_descr(map.get_neighbour(coord, WALK_NW).field->terrain_r()).get_is() &
	        TerrainDescription::Is::kWater);
}

bool FindNodeOpenWater::accept(const Map& /* map */, const FCoords& coord) const {
	return !(coord.field->nodecaps() & MOVECAPS_WALK) && (coord.field->nodecaps() & MOVECAPS_SWIM);
}

// FindNodeWithFlagOrRoad
bool FindNodeWithFlagOrRoad::accept(const Map&, FCoords fc) const {
	if (upcast(PlayerImmovable const, pimm, fc.field->get_immovable()))
		return (dynamic_cast<Flag const*>(pimm) ||
		        (dynamic_cast<Road const*>(pimm) && (fc.field->nodecaps() & BUILDCAPS_FLAG)));
	return false;
}

NearFlag::NearFlag(const Flag& f, int32_t const c, int32_t const d)
   : flag(&f), cost(c), distance(d) {
}

BuildableField::BuildableField(const Widelands::FCoords& fc)
	: coords(fc),
	  field_info_expiration(20000),
	  preferred(false),
	  enemy_nearby(0),
	  enemy_accessible_(false),
	  unowned_land_nearby(0),
	  near_border(false),
	  unowned_mines_spots_nearby(0),
	  unowned_iron_mines_nearby(false),
	  trees_nearby(0),
	  // explanation of starting values
	  // this is done to save some work for AI (CPU utilization)
	  // base rules are:
	  // count of rocks can only decrease, so  amount of rocks
	  // is recalculated only when previous count is positive
	  // count of water fields are stable, so if the current count is
	  // non-negative, water is not recaldulated
	  rocks_nearby(1),
	  water_nearby(-1),
	  open_water_nearby(-1),
	  distant_water(0),
	  fish_nearby(-1),
	  critters_nearby(-1),
	  ground_water(1),
	  space_consumers_nearby(0),
	  rangers_nearby(0),
	  area_military_capacity(0),
	  military_loneliness(1000),
	  military_in_constr_nearby(0),
	  own_military_presence(0),
	  enemy_military_presence(0),
	  ally_military_presence(0),
	  military_stationed(0),
	  unconnected_nearby(false),
	  military_unstationed(0),
	  own_non_military_nearby(0),
	  is_portspace(Widelands::ExtendedBool::kUnset),
	  port_nearby(false),
	  portspace_nearby(Widelands::ExtendedBool::kUnset),
	  max_buildcap_nearby(0),
	  last_resources_check_time(0),
	  military_score_(0),
	  inland(false),
	  local_soldier_capacity(0),
      is_militarysite(false) {
}

int32_t BuildableField::own_military_sites_nearby_() {
	return military_stationed + military_unstationed;
}

MineableField::MineableField(const Widelands::FCoords& fc)
   : coords(fc),
     field_info_expiration(20000),
     preferred(false),
     mines_nearby(0),
     same_mine_fields_nearby(0) {
}

EconomyObserver::EconomyObserver(Widelands::Economy& e) : economy(e) {
	dismantle_grace_time = std::numeric_limits<int32_t>::max();
}

int32_t BuildingObserver::total_count() const {
	return cnt_built + cnt_under_construction;
}

Widelands::AiModeBuildings BuildingObserver::aimode_limit_status() {
	if (total_count() > cnt_limit_by_aimode) {
		return Widelands::AiModeBuildings::kLimitExceeded;
	} else if (total_count() == cnt_limit_by_aimode) {
		return Widelands::AiModeBuildings::kOnLimit;
	} else {
		return Widelands::AiModeBuildings::kAnotherAllowed;
	}
}
bool BuildingObserver::buildable(Widelands::Player& p) {
	return is_buildable && p.is_building_type_allowed(id);
}

// Computer player does not get notification messages about enemy militarysites
// and warehouses, so following is collected based on observation
// It is conventient to have some information preserved, like nearby minefields,
// when it was attacked, whether it is warehouse and so on
// Also AI test more such targets when considering attack and calculated score is
// is stored in the observer
EnemySiteObserver::EnemySiteObserver()
   : is_warehouse(false),
     attack_soldiers_strength(0),
     defenders_strength(0),
     stationed_soldiers(0),
     last_time_attackable(std::numeric_limits<uint32_t>::max()),
     last_tested(0),
     score(0),
     mines_nearby(Widelands::ExtendedBool::kUnset),
     no_attack_counter(0) {
}

// as all mines have 3 levels, AI does not know total count of mines per mined material
// so this observer will be used for this
MineTypesObserver::MineTypesObserver() : in_construction(0), finished(0) {
}

ExpansionType::ExpansionType() {
	type = ExpansionMode::kResources;
}

void ExpansionType::set_expantion_type(const ExpansionMode etype) {
	type = etype;
}

int16_t ExpansionType::get_proportions(const ScoreBlock block, bool dismantlement) {
	if (dismantlement) {
		return proportion_table[static_cast<uint8_t>(ScoreBlock::kDismantle)][static_cast<uint8_t>(block)];
	}
	return proportion_table[static_cast<uint8_t>(type)][static_cast<uint8_t>(block)];
}



ManagementData::ManagementData() {
		score = 1;
		review_count = 0;
		primary_parent = 255;
		last_mutate_time = 0;
		next_neuron_id = 0;
		performance_change = 0;
}



Neuron::Neuron(int8_t w, uint8_t f, uint16_t i) : 
	weight(w),type(f), id(i) {
	assert(type < neuron_curves.size());
	assert(weight >= -100 && weight <= 100);
	lowest_pos = std::numeric_limits<uint8_t>::max();
	highest_pos = std::numeric_limits<uint8_t>::min();
	recalculate();
}


FNeuron::FNeuron(uint32_t c){
	core = c;
}

// Returning result depending on combinations of 5 bools
bool FNeuron::get_result(const bool bool1, const bool bool2, const bool bool3, const bool bool4, const bool bool5){
	return core.test(bool1 * 16 + bool2 * 8 + bool3 * 4 + bool4 * 2 + bool5);
}

// Returning bool on a position
bool FNeuron::get_position(const uint8_t pos) {
	assert (pos < f_neuron_bit_size);
	return core.test(pos);
}

// Returning numerical value of FNeuron. Used for saving and priting into log
uint32_t FNeuron::get_int(){
	return core.to_ulong();
}

void FNeuron::flip_bit(const uint8_t pos){
	assert (pos < f_neuron_bit_size);
	core.flip(pos);
}

// Normal neuron is defined by weight and curve
void Neuron::set_weight(int8_t w) {
	if (w > 100) {
		weight = 100;
	}else if (w < -100) {
		weight = -100;
	} else {
		weight = w;
	}
}

// Neuron stores calculated values in array
void Neuron::recalculate() {
	assert (neuron_curves.size() > type);
	for (int8_t i = 0; i <= 20; i+=1) {
		results[i] = weight * neuron_curves[type][i] / 100;
	}
}

// Following two functions returns Neuron value on position
int8_t Neuron::get_result(const uint8_t pos){
	assert(pos <= 20);
	return results[pos];
}

int8_t Neuron::get_result_safe(int32_t pos, const bool absolute){
	if (pos > highest_pos) {highest_pos = pos;};
	if (pos < lowest_pos) {lowest_pos = pos;};
	if (pos < 0) {
		pos = 0;
	}
	if (pos > 20) {
		pos = 20;
	}
	assert(pos <= 20);
	assert (results[pos] >= -100 && results[pos] <=100);
	if (absolute) {
		return std::abs(results[pos]);
	}	
	return results[pos];
}

// Setting the type of function
void Neuron::set_type(uint8_t new_type) {
	assert (new_type < neuron_curves.size());
	type = new_type;
}

// Shifting the value in range -100 to 100, if zero_align is true, it is now allowed to shift
// from negative to positive and vice versa, 0 must be used.
int8_t ManagementData::shift_weight_value(const int8_t old_value, const bool aggressive){
	
	int16_t halfVArRange = 35;
	if (aggressive){
		//printf ("aggressive mode \n");
		halfVArRange = 200;
		
		}
 	int16_t new_value = static_cast<int16_t>(old_value) - halfVArRange + std::rand() % (halfVArRange * 2 );
	
	if (!aggressive && ((old_value > 0 && new_value < 0) || (old_value < 0 && new_value > 0))) {
		new_value = 0;
	}
	
	if (new_value < -100) {
		new_value = -100;
	}
	if (new_value > 100) {
		new_value = 100;
	}
	return static_cast<int8_t>(new_value);
}

// This randomly sets new values into neurons and AI magic numbers
void ManagementData::mutate(const uint32_t gametime, const PlayerNumber pn) {

	const int8_t old_probability = get_military_number_at(MutationRatePosition);

	int16_t probability = shift_weight_value(get_military_number_at(MutationRatePosition), false) + 101;
	if (probability > 40) {
			probability = 40;
	}
	if (probability < 25) {
			probability = 25;
	}

	set_military_number_at(MutationRatePosition, probability - 101);

	//Wild card
	if (std::rand() % 8 == 0) {

		probability /= 3;
		printf ("Wild card new probability: %d \n", probability);
	}
	
	//probability += 101; //to get in  range 0-200
	
	assert(probability > 0 && probability <= 201 );
	
	printf (" %2d ... mutating , time since last mutation: %6d, final prob.: 1 / %3d (old: 1 / %3d)\n",
	pn,
	(gametime - last_mutate_time) / 1000 / 60,
	probability,
	old_probability + 101);
	printf (" %d  current muation probability: %3d\n", pn, probability);
	last_mutate_time = gametime;
	const int16_t preferred_ratio = 50;

	if (probability < 201) {
		
		
		// Modifying pool of Military numbers
		std::set<int32_t>preferred_numbers = {79};   //92,93,58,83,136,133};
		for (uint16_t i = 0; i < magic_numbers_size; i += 1){
			if (i == MutationRatePosition) { //mutated above
				continue;
			}
		   	if ((preferred_numbers.count(i) > 0 && std::rand() % probability * 10 / preferred_ratio == 0) || std::rand() % probability == 0) {
				// poor man's gausian distribution probability
				const int16_t old_value = get_military_number_at(i);
				int16_t new_value = shift_weight_value(get_military_number_at(i), (probability < 10));
				if (new_value < -100) {
					new_value = -100;
				}
				if (new_value > 100) {
					new_value = 100;
				}			
				set_military_number_at(i,new_value);
				printf ("      Magic number %2d: value changed: %4d -> %4d\n", i, old_value, new_value);
			}
		}
	
		// Modifying pool of neurons	
		std::set<int32_t> preferred_neurons = {};   //22,25,27,15,17,33,18,8,31};
		for (auto& item : neuron_pool){
			if ((preferred_neurons.count(item.get_id()) > 0 && std::rand() % probability * 10 / preferred_ratio  == 0) || std::rand() % probability == 0) {
				const int16_t old_value = item.get_weight();
				if (std::rand() % 4 == 0){
					assert(neuron_curves.size() > 0);
					item.set_type(std::rand() % neuron_curves.size());
					pd->neuron_functs[item.get_id()] = item.get_type();
				} else {
					int16_t new_value = shift_weight_value(item.get_weight(),(probability < 10));
					if (new_value < -100) {
						new_value = -100;
					}
					if (new_value > 100) {
						new_value = 100;
					}
					item.set_weight(new_value);			
					pd->neuron_weights[item.get_id()] = item.get_weight();
				}
				printf ("      Neuron %2d: weight: %4d -> %4d, new curve: %d\n", item.get_id(), old_value, item.get_weight(), item.get_type());
				item.recalculate();
			}
			////temporarily reducing the values
			//else {
				//const int16_t old_value = item.get_weight();
				//int16_t new_value = old_value;
				//if (old_value < -5) {
					//new_value = old_value + 5;
				//}
				//if (old_value > 5) {
					//new_value = old_value - 5;
				//}			
				//item.set_weight(new_value);	
				//printf ("      Neuron %2d: weight: %4d -> %4d\n", item.get_id(), old_value, item.get_weight());
				//item.recalculate();			
			//} 	 
		}
	
		// Modifying pool of f-neurons
		std::set<int32_t> preffered_neurons = {56,57,58};
		uint16_t pos = 0;	
		for (auto& item : f_neuron_pool){
			bool fneuron_changed = false;
			for (uint8_t i = 0; i < f_neuron_bit_size; i += 1) {
				if (preffered_neurons.count(pos) > 0 && std::rand() % (probability * 3 * 10 / preferred_ratio)  == 0) {//NOCOM - temporarily
					item.flip_bit(i);
					fneuron_changed = true;
				} else 
				if (std::rand() % (probability * 3) == 0) {
					item.flip_bit(i);
					fneuron_changed = true;
				}
			}
	
			if (fneuron_changed) {
				pd->f_neurons[pos] = item.get_int();
				printf ("      F-Neuron %2d: new value: %ul\n", pos, item.get_int());
			}
			pos += 1; 
		}
	}

	test_consistency();
	dump_data();
}

// Used to score performance of AI
void ManagementData::review(const uint32_t gametime, PlayerNumber pn,
 const uint32_t strength, const uint32_t land, const uint32_t old_land, const uint16_t attackers,
   const uint32_t first_ironmine, const int16_t ts_b,  const int16_t ts_a) {
	assert(!pd->magic_numbers.empty());
	const int16_t current_land_divider = 6;
	const int16_t land_delta_divider = 1;
	const int16_t land_delta_bonus = 200;
	const int16_t strength_multiplicator = 4;
	const uint32_t strength_cap = 100;
	const int16_t attackers_multiplicator = 6;
	const int16_t attack_bonus = 100;
	const uint32_t ironmine_start_tresh = 30;
	const uint32_t ironmine_end_tresh = 120;
	const int16_t ts_cap = 2;
	const int16_t ts_score = 50;	
	const uint32_t ironmines_max_bonus = 1000;	
	printf ("first ironmine time: %d\n", first_ironmine / 60 / 1000);
	uint32_t ironmines_bonus = 0;
	uint32_t first_iron_mine_min = first_ironmine / 60 / 1000;
	if (first_iron_mine_min < ironmine_start_tresh) {
		ironmines_bonus = ironmines_max_bonus;
	} else if (first_iron_mine_min > ironmine_end_tresh) {
		ironmines_bonus = 0;
	} else {
		ironmines_bonus = (ironmine_end_tresh - first_iron_mine_min)  * ironmines_max_bonus / (ironmine_end_tresh - ironmine_start_tresh);
		printf ("ironmine details %d %d %d\n", (ironmine_end_tresh - first_iron_mine_min),
				  (ironmine_end_tresh - first_iron_mine_min)  * ironmines_max_bonus,
				  (ironmine_end_tresh - first_iron_mine_min)  * ironmines_max_bonus / (ironmine_end_tresh - ironmine_start_tresh));
	}
	assert(ironmines_bonus <= ironmines_max_bonus);
	
	score = land / current_land_divider
		+ static_cast<int32_t>(land - old_land) / land_delta_divider
		+ ((static_cast<int32_t>(land-old_land) > 0) ? land_delta_bonus : 0)
		+ std::min(strength,strength_cap)  * strength_multiplicator
		+ attackers * attackers_multiplicator
		+ ((attackers > 0) ? attack_bonus : -attack_bonus)
		+ ironmines_bonus 
		+ std::min(ts_b, ts_cap) * ts_score 
		+ std::min(ts_a, ts_cap) * ts_score;
		
	printf (" %2d %s: reviewing AI mngm. data, score: %4d Pr.p: %d (l: %4d / %3d / %4d, st.: %4d, att: %3d, im: %4d, ts: %3d(%1d+%1d))\n",
		pn, gamestring_with_leading_zeros(gametime), score,
		primary_parent, 
		land / current_land_divider,
		(static_cast<int32_t>(land-old_land) > 0) ? land_delta_bonus : 0,
		static_cast<int32_t>(land - old_land) / land_delta_divider,
		std::min(strength,strength_cap)  * strength_multiplicator,
		attackers * attackers_multiplicator,
		ironmines_bonus,
		std::min(ts_b, ts_cap) * ts_score + std::min(ts_a, ts_cap) * ts_score,
		ts_b,
		ts_a);

	
	if (score < -10000 || score > 20000){
		printf ("MASTERERROR %2d %s: reviewing AI mngm. data, score: %4d (land: %4d  strength: %4d, attackers: %3d)\n",
	pn, gamestring_with_leading_zeros(gametime), score,
	land / 86, strength, attackers);
	}
	assert (score > -10000 && score < 100000);

	//If under 25 seconds we re-initialize
	if (gametime < 25 * 1000 && kReInitialize){
		printf (" %d - reinitializing DNA\n", pn);
		initialize(pn, true);
			mutate(gametime, pn);
		dump_data();
	} else {
		printf ("   still using mutate from %d minutes ago:\n",
		(gametime - last_mutate_time) / 1000 / 60); 
		dump_data();
	}
	
	review_count += 1;	
}

// Initializing and reintializing DNA
void ManagementData::initialize( const uint8_t pn, const bool reinitializing) {
	printf (" ... initialize starts %s\n", reinitializing?" * reinitializing *":"");

    //AutoSCore_AIDNA_1
    const std::vector<int16_t> AI_initial_military_numbers_A =
      {  81 ,  -100 ,  -70 ,  100 ,  -22 ,   55 ,  -22 ,    0 ,  -72 ,   28 ,  //AutoContent_01_AIDNA_1
         -8 ,   65 ,  -75 ,   35 ,  -91 ,  -92 ,    0 ,  100 ,   -6 ,  -45 ,  //AutoContent_02_AIDNA_1
         15 ,  100 ,  -100 ,  -100 ,  -91 ,  -37 ,   93 ,  -25 ,  100 ,  -36 ,  //AutoContent_03_AIDNA_1
         79 ,  100 ,  -100 ,   92 ,    9 ,   83 ,  -14 ,  -100 ,   27 ,  -100 ,  //AutoContent_04_AIDNA_1
        -74 ,   56 ,  -76 ,  -95 ,  100 ,  -44 ,  100 ,  100 ,   16 ,    0 ,  //AutoContent_05_AIDNA_1
        -79 ,  -26 ,  -86 ,  -100 ,  -21 ,  100 ,   35 ,   40 ,   20 ,    0 ,  //AutoContent_06_AIDNA_1
         42 ,  100 ,    0 ,  -66 ,  -17 ,   18 ,  -40 ,  -100 ,   71 ,  -100 ,  //AutoContent_07_AIDNA_1
        -100 ,    0 ,  -47 ,   52 ,  100 ,  -16 ,  -100 ,    6 ,   29 ,  -100 ,  //AutoContent_08_AIDNA_1
        -79 ,   23 ,   56 ,  -88 ,    0 ,  -46 ,    0 ,  100 ,   16 ,    3 ,  //AutoContent_09_AIDNA_1
         40 ,   83 ,  -100 ,  100 ,   34 ,  -29 ,  -43 ,   13 ,  -100 ,  -10 ,  //AutoContent_10_AIDNA_1
         68 ,  -12 ,   77 ,   47 ,   12 ,   72 ,  -67 ,   43 ,  -100 ,   88 ,  //AutoContent_11_AIDNA_1
         91 ,   85 ,  -100 ,   30 ,  100 ,  100 ,  -50 ,   89 ,   81 ,  100 ,  //AutoContent_12_AIDNA_1
          0 ,  -100 ,   -7 ,   47 ,   70 ,   24 ,  -18 ,  -100 ,   82 ,   49 ,  //AutoContent_13_AIDNA_1
        -77 ,   59 ,   26 ,  -63 ,   18 ,  -100 ,  -100 ,   95 ,  -92 ,  -100 ,  //AutoContent_14_AIDNA_1
        100 ,  -100 ,  -100 ,  100 ,  -100 ,  100 ,   -1 ,  -84 ,  100 ,   40  //AutoContent_15_AIDNA_1
       }
		;
	
	assert(magic_numbers_size == AI_initial_military_numbers_A.size());
	
	const std::vector<int8_t> input_weights_A =
		//0		1		2		3	4		5		6		7		8	9
      {  100 ,  -100 ,  -66 ,    0 ,  -24 ,   -4 ,  -32 ,  -34 ,  100 ,  -74 ,  //AutoContent_16_AIDNA_1
        100 ,  -35 ,  -100 ,   63 ,   62 ,   -2 ,  -100 ,  -100 ,  -100 ,   62 ,  //AutoContent_17_AIDNA_1
        -46 ,  -26 ,  100 ,  -18 ,  -100 ,   67 ,    0 ,  -100 ,  100 ,  -100 ,  //AutoContent_18_AIDNA_1
        100 ,  100 ,   22 ,   45 ,   82 ,  -100 ,  -44 ,  -100 ,  -47 ,  -100 ,  //AutoContent_19_AIDNA_1
        -92 ,  -90 ,  -39 ,  -93 ,  100 ,   84 ,   88 ,  -42 ,  -42 ,    2 ,  //AutoContent_20_AIDNA_1
        -100 ,  -32 ,   67 ,  -100 ,  -35 ,  -100 ,   83 ,   47 ,    0 ,   49 ,  //AutoContent_21_AIDNA_1
        -80 ,   13 ,  -94 ,  -82 ,  -80 ,  -83 ,  100 ,  -100 ,  -28 ,  -100 ,  //AutoContent_22_AIDNA_1
        100 ,  -83 ,  -100 ,  100 ,  -100 ,  100 ,   53 ,  -100 ,   65 ,  -54  //AutoContent_23_AIDNA_1
	}
			;
	const std::vector<int8_t> input_func_A =
      {    2 ,    2 ,    0 ,    1 ,    1 ,    1 ,    0 ,    0 ,    0 ,    0 ,  //AutoContent_24_AIDNA_1
          2 ,    2 ,    1 ,    1 ,    0 ,    2 ,    2 ,    1 ,    0 ,    1 ,  //AutoContent_25_AIDNA_1
          1 ,    2 ,    2 ,    1 ,    1 ,    0 ,    1 ,    0 ,    1 ,    0 ,  //AutoContent_26_AIDNA_1
          1 ,    1 ,    1 ,    0 ,    0 ,    2 ,    1 ,    0 ,    1 ,    1 ,  //AutoContent_27_AIDNA_1
          1 ,    1 ,    1 ,    1 ,    0 ,    1 ,    1 ,    0 ,    1 ,    1 ,  //AutoContent_28_AIDNA_1
          2 ,    2 ,    1 ,    2 ,    2 ,    1 ,    1 ,    2 ,    2 ,    1 ,  //AutoContent_29_AIDNA_1
          0 ,    1 ,    1 ,    1 ,    1 ,    1 ,    1 ,    1 ,    2 ,    0 ,  //AutoContent_30_AIDNA_1
          1 ,    2 ,    2 ,    2 ,    1 ,    1 ,    0 ,    2 ,    0 ,    1  //AutoContent_31_AIDNA_1
	}
		;
	assert(neuron_pool_size == input_func_A.size());
	assert(neuron_pool_size == input_weights_A.size());

	const std::vector<uint32_t> f_neurons_A =
      {  893882022 ,  1012949768 ,  1282767981 ,  3407235130 ,  3488362998 ,  1378359763 ,  3750471457 ,  3081116877 ,  1458320738 ,  3170464195 ,  //AutoContent_32_AIDNA_1
        3905971216 ,  2431616046 ,  3953923374 ,  1845086815 ,  1836137029 ,  3646071245 ,  2696265141 ,  2349270127 ,  2541958369 ,  112271656 ,  //AutoContent_33_AIDNA_1
        2585687829 ,  1160096990 ,  747039753 ,  2568184014 ,  3664092356 ,  3104318854 ,  635697358 ,  4023463768 ,  3995831573 ,  810417449 ,  //AutoContent_34_AIDNA_1
        2514696973 ,  2929871575 ,  3993670318 ,  4150727141 ,  678008812 ,  3994635043 ,  4075247787 ,  1828808396 ,  861393788 ,  4097686692 ,  //AutoContent_35_AIDNA_1
        912147624 ,  3938965162 ,  1917001130 ,  3589205776 ,  3024535343 ,  2542193932 ,  2188433370 ,  2114073999 ,  1228715640 ,  3622652715 ,  //AutoContent_36_AIDNA_1
        3174765257 ,  1932367510 ,  4058855805 ,  1474295732 ,  3830756535 ,  4251837603 ,  1242644035 ,  1803864740 ,  668753102 ,  46743497  //AutoContent_37_AIDNA_1
	 };
	assert(f_neuron_pool_size == f_neurons_A.size());

		
    //AutoSCore_AIDNA_2
	const std::vector<int16_t> AI_initial_military_numbers_B =
      {  81 ,  -100 ,  -70 ,  100 ,  -89 ,   55 ,  -40 ,    0 ,  -72 ,   28 ,  //AutoContent_01_AIDNA_2
         -8 ,   65 ,  -75 ,   35 ,  -91 ,  -92 ,  -26 ,  100 ,  -22 ,  -45 ,  //AutoContent_02_AIDNA_2
         15 ,  100 ,  -77 ,  -100 ,  -70 ,   31 ,   93 ,    0 ,   74 ,  -56 ,  //AutoContent_03_AIDNA_2
         79 ,  100 ,  -62 ,   92 ,    9 ,   83 ,  -14 ,  -100 ,   36 ,  -100 ,  //AutoContent_04_AIDNA_2
        -74 ,   56 ,  -81 ,  -95 ,  -30 ,  -44 ,  100 ,  100 ,  -86 ,   33 ,  //AutoContent_05_AIDNA_2
        -79 ,  -26 ,  -86 ,  -100 ,   63 ,  -61 ,   35 ,  -58 ,    0 ,    0 ,  //AutoContent_06_AIDNA_2
         42 ,  100 ,    0 ,  -40 ,  -17 ,   97 ,  -40 ,  -100 ,   71 ,  -100 ,  //AutoContent_07_AIDNA_2
        -100 ,    0 ,  -47 ,  -82 ,  100 ,  -16 ,  -100 ,    6 ,   29 ,   -9 ,  //AutoContent_08_AIDNA_2
        -79 ,   23 ,   80 ,  -88 ,    0 ,  100 ,  -17 ,   52 ,  -50 ,  -57 ,  //AutoContent_09_AIDNA_2
         40 ,   83 ,  -68 ,   88 ,   32 ,  -29 ,  -43 ,   13 ,  -100 ,  -67 ,  //AutoContent_10_AIDNA_2
         68 ,  -12 ,   77 ,  -14 ,   12 ,   72 ,  -67 ,   43 ,   63 ,   88 ,  //AutoContent_11_AIDNA_2
         91 ,  -100 ,  -34 ,   30 ,  100 ,  100 ,   33 ,   76 ,   81 ,  100 ,  //AutoContent_12_AIDNA_2
        -31 ,  -32 ,   -7 ,   47 ,   70 ,    0 ,  -18 ,  -100 ,   82 ,   51 ,  //AutoContent_13_AIDNA_2
        -77 ,   59 ,   26 ,  -63 ,  100 ,  -100 ,  -61 ,  100 ,  100 ,  -100 ,  //AutoContent_14_AIDNA_2
        100 ,   -3 ,  -100 ,  100 ,  -100 ,  100 ,   -1 ,  -57 ,   44 ,  -100  //AutoContent_15_AIDNA_2
		}
		;
	assert(magic_numbers_size == AI_initial_military_numbers_B.size());
		
	const std::vector<int8_t> input_weights_B =
      {  100 ,  -100 ,  -66 ,    0 ,  -100 ,   -4 ,  -32 ,  -34 ,  100 ,  -74 ,  //AutoContent_16_AIDNA_2
        100 ,  -35 ,  -100 ,   63 ,   62 ,   17 ,  -100 ,  -100 ,  -100 ,   62 ,  //AutoContent_17_AIDNA_2
        -46 ,   47 ,  100 ,  -18 ,  -100 ,   81 ,    0 ,  -95 ,  100 ,  -95 ,  //AutoContent_18_AIDNA_2
         59 ,  100 ,   22 ,   56 ,   66 ,  -100 ,  -44 ,  -100 ,  -47 ,  -47 ,  //AutoContent_19_AIDNA_2
        -100 ,  -90 ,  -24 ,  -93 ,  100 ,  -100 ,   88 ,  -24 ,   86 ,    2 ,  //AutoContent_20_AIDNA_2
        -100 ,  -32 ,   68 ,  -100 ,  -35 ,  -100 ,   83 ,   47 ,    4 ,   49 ,  //AutoContent_21_AIDNA_2
        -80 ,   13 ,  -94 ,  -82 ,  -80 ,  -100 ,  100 ,  -89 ,  -28 ,  -100 ,  //AutoContent_22_AIDNA_2
        100 ,  -83 ,  -100 ,  100 ,  -100 ,  100 ,   53 ,  -100 ,   65 ,  -54  //AutoContent_23_AIDNA_2
}
	      ;
	
	const std::vector<int8_t> input_func_B = 
      {    2 ,    2 ,    1 ,    1 ,    1 ,    1 ,    0 ,    0 ,    0 ,    0 ,  //AutoContent_24_AIDNA_2
          2 ,    2 ,    1 ,    2 ,    1 ,    2 ,    2 ,    1 ,    0 ,    1 ,  //AutoContent_25_AIDNA_2
          1 ,    2 ,    2 ,    1 ,    1 ,    0 ,    1 ,    0 ,    1 ,    0 ,  //AutoContent_26_AIDNA_2
          1 ,    1 ,    1 ,    0 ,    0 ,    2 ,    1 ,    0 ,    1 ,    1 ,  //AutoContent_27_AIDNA_2
          1 ,    1 ,    1 ,    1 ,    0 ,    1 ,    1 ,    0 ,    1 ,    1 ,  //AutoContent_28_AIDNA_2
          2 ,    2 ,    1 ,    2 ,    2 ,    1 ,    0 ,    2 ,    2 ,    1 ,  //AutoContent_29_AIDNA_2
          0 ,    1 ,    1 ,    1 ,    1 ,    1 ,    1 ,    1 ,    2 ,    0 ,  //AutoContent_30_AIDNA_2
          1 ,    2 ,    2 ,    2 ,    1 ,    1 ,    0 ,    2 ,    0 ,    1  //AutoContent_31_AIDNA_2
}
		;
		assert(neuron_pool_size == input_func_B.size());
		assert(neuron_pool_size == input_weights_B.size());

      
	const std::vector<uint32_t> f_neurons_B =
      {  1430358758 ,  3095552264 ,  1282801261 ,  3409324314 ,  2414637556 ,  308941267 ,  3754665761 ,  3081112781 ,  4074077506 ,  3187241154 ,  //AutoContent_32_AIDNA_2
        1271948823 ,  2427356220 ,  2067531174 ,  2830749311 ,  1844541029 ,  1364386253 ,  3235199397 ,  2887205990 ,  2508666081 ,  45162797 ,  //AutoContent_33_AIDNA_2
        2048546577 ,  1098235354 ,  747031625 ,  2299781327 ,  3638925522 ,  3106669958 ,  2682510542 ,  1836167003 ,  3928993053 ,  538836270 ,  //AutoContent_34_AIDNA_2
        3517135645 ,  2927774933 ,  3997741742 ,  4159115621 ,  1751749196 ,  3894021923 ,  4064763067 ,  1761699018 ,  4081548412 ,  4096375974 ,  //AutoContent_35_AIDNA_2
        912148139 ,  4203139770 ,  1916999978 ,  3321818896 ,  3588668847 ,  3583421725 ,  3329349594 ,  2115122575 ,  1562162300 ,  3573370731 ,  //AutoContent_36_AIDNA_2
        4246344297 ,  1881970390 ,  904604125 ,  3502766012 ,  3864312883 ,  3160796071 ,  856384099 ,  3701294241 ,  1148662363 ,  47037385  //AutoContent_37_AIDNA_2
	 };
	assert(f_neuron_pool_size == f_neurons_B.size());


    //AutoSCore_AIDNA_3
	const std::vector<int16_t> AI_initial_military_numbers_C =
      {  81 ,  -100 ,  -70 ,  100 ,  -89 ,   55 ,  -40 ,    0 ,  -72 ,   28 ,  //AutoContent_01_AIDNA_3
         -8 ,   65 ,  -75 ,   35 ,  -91 ,  -92 ,    0 ,  100 ,   -6 ,  -45 ,  //AutoContent_02_AIDNA_3
         15 ,   94 ,  -77 ,  -100 ,  -91 ,   31 ,   93 ,  -25 ,  100 ,  -36 ,  //AutoContent_03_AIDNA_3
        100 ,  100 ,  -100 ,   92 ,    9 ,   83 ,  -14 ,  -100 ,   27 ,  -100 ,  //AutoContent_04_AIDNA_3
        -74 ,   56 ,  -81 ,  -95 ,  100 ,  -44 ,  100 ,  100 ,   16 ,   17 ,  //AutoContent_05_AIDNA_3
        -100 ,  -26 ,  -86 ,  -100 ,  100 ,  100 ,   35 ,   23 ,  -10 ,  -28 ,  //AutoContent_06_AIDNA_3
         42 ,  100 ,    0 ,  -66 ,  -17 ,  100 ,  -40 ,  -100 ,   71 ,  -100 ,  //AutoContent_07_AIDNA_3
        -76 ,    0 ,  -47 ,  -70 ,  100 ,  -16 ,  -100 ,    6 ,   29 ,  -45 ,  //AutoContent_08_AIDNA_3
        -79 ,   23 ,   56 ,  -88 ,    0 ,  100 ,    0 ,   23 ,  -50 ,  -45 ,  //AutoContent_09_AIDNA_3
         40 ,   83 ,  -100 ,  100 ,   34 ,  -29 ,  -43 ,   13 ,  -100 ,  -10 ,  //AutoContent_10_AIDNA_3
         68 ,  -12 ,   77 ,  -100 ,   12 ,   72 ,  -67 ,   43 ,  -100 ,   88 ,  //AutoContent_11_AIDNA_3
        100 ,   93 ,  -34 ,   30 ,  100 ,  100 ,   33 ,   89 ,   81 ,  100 ,  //AutoContent_12_AIDNA_3
          0 ,  -100 ,   -7 ,   47 ,   70 ,   24 ,  -18 ,  -100 ,   82 ,   49 ,  //AutoContent_13_AIDNA_3
        -77 ,   59 ,   26 ,  -45 ,  100 ,  -100 ,  -100 ,   95 ,  -92 ,  -100 ,  //AutoContent_14_AIDNA_3
        100 ,   -3 ,  -100 ,  100 ,  -100 ,  100 ,   -1 ,  -84 ,  100 ,   40  //AutoContent_15_AIDNA_3
       }

		;
	
		assert(magic_numbers_size == AI_initial_military_numbers_C.size());
	
	const std::vector<int8_t> input_weights_C =
      {   89 ,  -100 ,  -66 ,    0 ,  -100 ,   -4 ,  -32 ,  -34 ,  100 ,  -74 ,  //AutoContent_16_AIDNA_3
         81 ,  -35 ,  -100 ,   63 ,   62 ,   -2 ,  -100 ,  -100 ,  -100 ,   62 ,  //AutoContent_17_AIDNA_3
        -79 ,  -26 ,  100 ,  -18 ,  -100 ,   67 ,    0 ,  -100 ,  100 ,  -95 ,  //AutoContent_18_AIDNA_3
        100 ,  100 ,   22 ,   56 ,   82 ,  -100 ,  -44 ,  -100 ,  -47 ,  -100 ,  //AutoContent_19_AIDNA_3
        -92 ,  -90 ,  -39 ,  -93 ,  100 ,  -100 ,   88 ,  -24 ,  -42 ,    2 ,  //AutoContent_20_AIDNA_3
        -100 ,  -32 ,   67 ,  -100 ,  -35 ,  -100 ,   83 ,   47 ,    0 ,   49 ,  //AutoContent_21_AIDNA_3
        -80 ,   13 ,  -94 ,  -82 ,  -80 ,  -83 ,  100 ,  -100 ,  -28 ,  -100 ,  //AutoContent_22_AIDNA_3
        100 ,  -83 ,  -100 ,  100 ,  -100 ,  100 ,   53 ,  -100 ,   65 ,  -54  //AutoContent_23_AIDNA_3
       }
			;
	const std::vector<int8_t> input_func_C =
      {    2 ,    2 ,    0 ,    1 ,    1 ,    1 ,    0 ,    0 ,    1 ,    0 ,  //AutoContent_24_AIDNA_3
          2 ,    2 ,    0 ,    2 ,    1 ,    2 ,    2 ,    1 ,    0 ,    1 ,  //AutoContent_25_AIDNA_3
          1 ,    2 ,    2 ,    1 ,    1 ,    0 ,    1 ,    0 ,    1 ,    0 ,  //AutoContent_26_AIDNA_3
          1 ,    1 ,    1 ,    0 ,    0 ,    2 ,    1 ,    0 ,    1 ,    1 ,  //AutoContent_27_AIDNA_3
          1 ,    1 ,    1 ,    1 ,    0 ,    1 ,    1 ,    0 ,    1 ,    1 ,  //AutoContent_28_AIDNA_3
          2 ,    2 ,    1 ,    2 ,    2 ,    1 ,    1 ,    2 ,    2 ,    1 ,  //AutoContent_29_AIDNA_3
          0 ,    1 ,    1 ,    1 ,    1 ,    1 ,    1 ,    1 ,    2 ,    0 ,  //AutoContent_30_AIDNA_3
          1 ,    2 ,    2 ,    2 ,    1 ,    1 ,    0 ,    2 ,    0 ,    1  //AutoContent_31_AIDNA_3
       }
			;
	assert(neuron_pool_size == input_func_C.size());
	assert(neuron_pool_size == input_weights_C.size());
	
	const std::vector<uint32_t> f_neurons_C =
      {  893882022 ,  1008788232 ,  1282767981 ,  3406981402 ,  3488359910 ,  1382554067 ,  3754669861 ,  3081116877 ,  1996242274 ,  3170463939 ,  //AutoContent_32_AIDNA_3
        3419431952 ,  2427421756 ,  3945532718 ,  1777977951 ,  1836138053 ,  1498587597 ,  2163605429 ,  2350318702 ,  2541958369 ,  511254568 ,  //AutoContent_33_AIDNA_3
        2585425689 ,  1092992222 ,  747031561 ,  2299633863 ,  3638920388 ,  3171419526 ,  2783181002 ,  3891343184 ,  3996093717 ,  810417453 ,  //AutoContent_34_AIDNA_3
        2497919757 ,  2930919637 ,  3997348526 ,  4159025508 ,  2825492428 ,  3927657251 ,  4140259499 ,  1839293516 ,  3008876924 ,  4097686692 ,  //AutoContent_35_AIDNA_3
        912150176 ,  4207400618 ,  1912839594 ,  3590254352 ,  3578183471 ,  2542202124 ,  2188695514 ,  2114073999 ,  1562162808 ,  3589098283 ,  //AutoContent_36_AIDNA_3
        3174765257 ,  1933416087 ,  4125964797 ,  1473771452 ,  3830756531 ,  4251837603 ,  2807436931 ,  1635798158 ,  249306730 ,  47005673  //AutoContent_37_AIDNA_3
	 };
	assert(f_neuron_pool_size == f_neurons_C.size());

		
    //AutoSCore_AIDNA_4
	const std::vector<int16_t> AI_initial_military_numbers_D =
      {  81 ,  -100 ,  -54 ,  100 ,  -89 ,   55 ,  -40 ,    0 ,  -72 ,   28 ,  //AutoContent_01_AIDNA_4
         -8 ,   65 ,  -75 ,   35 ,   85 ,  -92 ,  -17 ,  100 ,  -22 ,  -45 ,  //AutoContent_02_AIDNA_4
         15 ,  100 ,  -77 ,  -100 ,  -70 ,   31 ,   93 ,    0 ,  100 ,  -56 ,  //AutoContent_03_AIDNA_4
         79 ,  100 ,  -62 ,   92 ,    9 ,  100 ,   -5 ,  -100 ,   36 ,   78 ,  //AutoContent_04_AIDNA_4
        -74 ,   56 ,  -81 ,  -95 ,  -30 ,  -37 ,  100 ,   63 ,  -86 ,  -100 ,  //AutoContent_05_AIDNA_4
        -79 ,  -26 ,  -86 ,  -100 ,   63 ,  -61 ,   35 ,  -58 ,   38 ,    0 ,  //AutoContent_06_AIDNA_4
        100 ,  100 ,    0 ,  -40 ,  -100 ,   97 ,  -40 ,  -100 ,   71 ,  -100 ,  //AutoContent_07_AIDNA_4
        -100 ,    0 ,  -47 ,  -82 ,  100 ,  -16 ,  -100 ,    6 ,   29 ,  -31 ,  //AutoContent_08_AIDNA_4
        -79 ,  -100 ,   80 ,  -88 ,    0 ,  100 ,  -17 ,   52 ,  -50 ,  -57 ,  //AutoContent_09_AIDNA_4
         67 ,   83 ,  -68 ,  -18 ,   32 ,  -29 ,  -43 ,   13 ,   92 ,  -67 ,  //AutoContent_10_AIDNA_4
         68 ,  -12 ,   77 ,  -21 ,   12 ,   72 ,  -67 ,   43 ,   63 ,   32 ,  //AutoContent_11_AIDNA_4
         91 ,  -100 ,  -100 ,   30 ,  100 ,  100 ,  100 ,  -100 ,   44 ,  100 ,  //AutoContent_12_AIDNA_4
          0 ,  100 ,   -7 ,   47 ,   70 ,  -81 ,  -18 ,  -100 ,  100 ,  100 ,  //AutoContent_13_AIDNA_4
        -77 ,   59 ,   26 ,  -63 ,  100 ,  -100 ,   71 ,  100 ,  100 ,  -100 ,  //AutoContent_14_AIDNA_4
        100 ,   -3 ,  -100 ,  100 ,  -100 ,  100 ,   -1 ,  -57 ,   44 ,  -100  //AutoContent_15_AIDNA_4
	}
		;
	assert(magic_numbers_size == AI_initial_military_numbers_D.size());
		
	const std::vector<int8_t> input_weights_D =
      {  100 ,  -100 ,  -66 ,    0 ,  -100 ,   -4 ,  -32 ,  -34 ,   96 ,  -74 ,  //AutoContent_16_AIDNA_4
        100 ,  -100 ,  -100 ,   63 ,   62 ,   17 ,  -100 ,  -100 ,  -100 ,  -31 ,  //AutoContent_17_AIDNA_4
        -46 ,   47 ,  100 ,  -18 ,  -100 ,   81 ,    0 ,  -95 ,  100 ,  -95 ,  //AutoContent_18_AIDNA_4
         59 ,  100 ,   22 ,   56 ,   66 ,  -100 ,    9 ,  -100 ,  -47 ,  -97 ,  //AutoContent_19_AIDNA_4
        -92 ,  -90 ,  -24 ,  -93 ,  100 ,  -100 ,   88 ,  -24 ,   86 ,    2 ,  //AutoContent_20_AIDNA_4
         20 ,  -32 ,   68 ,  -100 ,  -35 ,   32 ,   83 ,   47 ,   29 ,   49 ,  //AutoContent_21_AIDNA_4
        -100 ,   13 ,  -94 ,  -82 ,  -80 ,  -100 ,  100 ,  -89 ,  -28 ,  -100 ,  //AutoContent_22_AIDNA_4
        -84 ,  -83 ,  -100 ,  100 ,  -100 ,  100 ,   53 ,  -100 ,   40 ,  -54  //AutoContent_23_AIDNA_4
	}
	      ;
	
	const std::vector<int8_t> input_func_D = 
      {    2 ,    2 ,    1 ,    1 ,    1 ,    1 ,    0 ,    0 ,    0 ,    0 ,  //AutoContent_24_AIDNA_4
          2 ,    2 ,    1 ,    2 ,    1 ,    2 ,    2 ,    1 ,    0 ,    1 ,  //AutoContent_25_AIDNA_4
          1 ,    2 ,    2 ,    1 ,    1 ,    0 ,    1 ,    0 ,    1 ,    0 ,  //AutoContent_26_AIDNA_4
          1 ,    1 ,    1 ,    0 ,    0 ,    2 ,    1 ,    0 ,    1 ,    1 ,  //AutoContent_27_AIDNA_4
          1 ,    1 ,    1 ,    1 ,    0 ,    1 ,    1 ,    0 ,    1 ,    1 ,  //AutoContent_28_AIDNA_4
          2 ,    2 ,    1 ,    2 ,    2 ,    1 ,    0 ,    0 ,    2 ,    2 ,  //AutoContent_29_AIDNA_4
          0 ,    1 ,    1 ,    1 ,    1 ,    1 ,    1 ,    1 ,    2 ,    0 ,  //AutoContent_30_AIDNA_4
          1 ,    2 ,    2 ,    2 ,    1 ,    1 ,    0 ,    2 ,    0 ,    1  //AutoContent_31_AIDNA_4
	}
		;
	assert(neuron_pool_size == input_func_D.size());
	assert(neuron_pool_size == input_weights_D.size());

	const std::vector<uint32_t> f_neurons_D =
      {  1698810566 ,  948065036 ,  1148550781 ,  3413518622 ,  2414637557 ,  304748759 ,  3687573281 ,  3081637101 ,  2462481730 ,  3170496707 ,  //AutoContent_32_AIDNA_4
        1271948887 ,  2158986300 ,  2067539366 ,  2897334143 ,  1836086789 ,  1364648397 ,  3268684213 ,  2350335078 ,  3582407905 ,  246489384 ,  //AutoContent_33_AIDNA_4
        4206778257 ,  1098235354 ,  679791753 ,  2299773135 ,  3655703634 ,  2569831814 ,  2682511054 ,  1299558227 ,  3895438653 ,  538819886 ,  //AutoContent_34_AIDNA_4
        2476946207 ,  2894220501 ,  3997741231 ,  4159674853 ,  1751749452 ,  3894021921 ,  4064763067 ,  1694721246 ,  4081548404 ,  4096376454 ,  //AutoContent_35_AIDNA_4
        845039243 ,  4161197754 ,  4065467690 ,  3321818896 ,  4125539759 ,  3583421725 ,  1450235866 ,  2115122447 ,  1562162300 ,  3304933738 ,  //AutoContent_36_AIDNA_4
        4246344265 ,  1882035934 ,  904608189 ,  1363666876 ,  3864312883 ,  3160796071 ,  1937466202 ,  3099091013 ,  2455683018 ,  47037386  //AutoContent_37_AIDNA_4
	 };
	assert(f_neuron_pool_size == f_neurons_D.size());

	printf (" %d: initializing AI's DNA\n", pn);

	// filling vector with zeros
	if (!reinitializing) {
		for (uint16_t i =  0; i < magic_numbers_size; i = i+1){
			pd->magic_numbers.push_back(0);
		}
	}
	assert (pd->magic_numbers.size() == magic_numbers_size);
	
	primary_parent = std::rand() % 4;
	const uint8_t parent2 = std::rand() % 4;
	
	printf (" ... DNA initialization (primary parent: %d, secondary parent: %d)\n", primary_parent, parent2);
	
	for (uint16_t i = 0; i < magic_numbers_size; i += 1){
		// Child inherites DNA with probability 5:1 from main parent
		uint8_t dna_donor = (std::rand() % 50 > 0) ? primary_parent : parent2;
		if (i == MutationRatePosition) { //overwritting
			dna_donor = primary_parent;
		}
		
		switch ( dna_donor ) {
			case 0 : 
				set_military_number_at(i,AI_initial_military_numbers_A[i]);
				break;
			case 1 : 
				set_military_number_at(i,AI_initial_military_numbers_B[i]);
				break;
			case 2 : 
				set_military_number_at(i,AI_initial_military_numbers_C[i]);
				break;
			case 3 : 
				set_military_number_at(i,AI_initial_military_numbers_D[i]);
				break;
			default:
				printf ("parent %d?\n", dna_donor);
				NEVER_HERE();
			}
		}

	if (reinitializing) {
		neuron_pool.clear();
		reset_neuron_id();
		pd->neuron_weights.clear();
		pd->neuron_functs.clear();
		f_neuron_pool.clear();
		pd->f_neurons.clear();
	}

	assert(neuron_pool.empty());
	
	for (uint16_t i = 0; i <neuron_pool_size; i += 1){
		const uint8_t dna_donor = (std::rand() % 20 > 0) ? primary_parent : parent2;
		
		switch ( dna_donor ) {
			case 0 : 
				neuron_pool.push_back(Neuron(input_weights_A[i],input_func_A[i],new_neuron_id()));
				break;
			case 1 : 
				neuron_pool.push_back(Neuron(input_weights_B[i],input_func_B[i],new_neuron_id()));
				break;
			case 2 : 
				neuron_pool.push_back(Neuron(input_weights_C[i],input_func_C[i],new_neuron_id()));
				break;
			case 3 : 
				neuron_pool.push_back(Neuron(input_weights_D[i],input_func_D[i],new_neuron_id()));
				break;
			default:
				printf ("parent %d?\n", dna_donor);
				NEVER_HERE();
		}
	}


	for (uint16_t i = 0; i <f_neuron_pool_size; i += 1){
		const uint8_t dna_donor = (std::rand() % 20 > 0) ? primary_parent : parent2;
		switch ( dna_donor ) {
			case 0 : 
				f_neuron_pool.push_back(FNeuron(f_neurons_A[i]));				
				break;
			case 1 : 
				f_neuron_pool.push_back(FNeuron(f_neurons_B[i]));	
				break;
			case 2 : 
				f_neuron_pool.push_back(FNeuron(f_neurons_C[i]));					
				break;
			case 3 : 
				f_neuron_pool.push_back(FNeuron(f_neurons_D[i]));	
				break;
			default:
				printf ("parent %d?\n", dna_donor);
				NEVER_HERE();
		}
	}
	
	assert(pd->neuron_weights.empty());
	assert(pd->neuron_functs.empty());	
	assert(pd->f_neurons.empty());
		
	for (uint32_t i = 0; i < neuron_pool_size; i = i+1){
		pd->neuron_weights.push_back(neuron_pool[i].get_weight());
		pd->neuron_functs.push_back(neuron_pool[i].get_type());	
	}

	for (uint32_t i = 0; i < f_neuron_pool_size; i = i+1){
		pd->f_neurons.push_back(f_neuron_pool[i].get_int());
	}
	
	pd->magic_numbers_size = magic_numbers_size;
	pd->neuron_pool_size = neuron_pool_size;
	pd->f_neuron_pool_size = f_neuron_pool_size;	
	
	test_consistency();
	printf (" %d: DNA initialized\n", pn);
			
}

bool ManagementData::test_consistency() {

	assert (pd->neuron_weights.size() == pd->neuron_pool_size);
	assert (pd->neuron_functs.size() == pd->neuron_pool_size);
	assert (neuron_pool.size() == pd->neuron_pool_size);
	assert (neuron_pool.size() == neuron_pool_size);
	
	assert (pd->magic_numbers_size == magic_numbers_size);			
	assert (pd->magic_numbers.size() == magic_numbers_size);
	
	assert (pd->f_neurons.size() == pd->f_neuron_pool_size);
	assert (f_neuron_pool.size() == pd->f_neuron_pool_size);
	assert (f_neuron_pool.size() == f_neuron_pool_size);	
	return true; //?
}

// Print DNA data to console, used for training
void ManagementData::dump_data() {
	//dumping new numbers
	printf ("     actual military_numbers (%lu):\n      {", pd->magic_numbers.size());
	uint16_t itemcounter = 1;
	uint16_t line_counter = 1;
	for (const auto& item : pd->magic_numbers) {
		printf (" %3d %s",item,(&item != &pd->magic_numbers.back())?", ":"");
		if (itemcounter % 10 == 0) {
			printf (" //AutoContent_%02d\n       ", line_counter);
			line_counter +=1;
		}
		++itemcounter;
	}
	printf ("}\n");
	
	printf ("     actual neuron setup:\n      ");
	printf ("{ ");
	itemcounter = 1;
	for (auto& item : neuron_pool) {
		printf (" %3d %s",item.get_weight(),(&item != &neuron_pool.back())?", ":"");
		if (itemcounter % 10 == 0) {
			printf (" //AutoContent_%02d\n       ", line_counter);
			line_counter +=1;
		}
		++itemcounter;
	}
	printf ("}\n      { ");
	itemcounter = 1;	
	for (auto& item : neuron_pool) {
		printf (" %3d %s",item.get_type(),(&item != &neuron_pool.back())?", ":"");
		if (itemcounter % 10 == 0) {
			printf (" //AutoContent_%02d\n       ", line_counter);
			line_counter +=1;
		}
		++itemcounter;
	}
	printf ("}\n");


	printf ("     actual f-neuron setup:\n      ");
	printf ("{ ");
	itemcounter = 1;
	for (auto& item : f_neuron_pool) {
		printf (" %8u %s",item.get_int(),(&item != &f_neuron_pool.back())?", ":"");
		if (itemcounter % 10 == 0) {
			printf (" //AutoContent_%02d\n       ", line_counter);
			line_counter +=1;
		}
		++itemcounter;
	}
	printf ("}\n");
}

// Two functions to manipulate military number
int16_t ManagementData::get_military_number_at(uint8_t pos) {
	assert (pos < magic_numbers_size);
	return pd->magic_numbers[pos];
}

void ManagementData::set_military_number_at(const uint8_t pos, int16_t value) {
	assert (pos < magic_numbers_size);
	assert (pos < pd->magic_numbers.size());
	if (value < -100) {
			value = -100;
	}
	if (value > 100) {
			value = 100;
	}
	pd->magic_numbers[pos] = value;
}

uint16_t MineTypesObserver::total_count() const {
	return in_construction + finished;
}

// this is used to count militarysites by their size
MilitarySiteSizeObserver::MilitarySiteSizeObserver() : in_construction(0), finished(0) {
}

// this represents a scheduler task
SchedulerTask::SchedulerTask(const uint32_t time,
                             const Widelands::SchedulerTaskId t,
                             const uint8_t p,
                             const char* d)
   : due_time(time), id(t), priority(p), descr(d) {
}

bool SchedulerTask::operator<(SchedulerTask other) const {
	return priority > other.priority;
}

// List of blocked fields with block time, with some accompanying functions
void BlockedFields::add(Widelands::Coords coords, uint32_t till) {
	const uint32_t hash = coords.hash();
	if (blocked_fields_.count(hash) == 0) {
		blocked_fields_.insert(std::pair<uint32_t, uint32_t>(hash, till));
	} else if (blocked_fields_[hash] < till) {
		blocked_fields_[hash] = till;
	}
	// The third possibility is that a field has been already blocked for longer time than 'till'
}

uint32_t BlockedFields::count() {
	return blocked_fields_.size();
}

void BlockedFields::remove_expired(uint32_t gametime) {
	std::vector<uint32_t> fields_to_remove;
	for (auto field : blocked_fields_) {
		if (field.second < gametime) {
			fields_to_remove.push_back(field.first);
		}
	}
	while (!fields_to_remove.empty()) {
		blocked_fields_.erase(fields_to_remove.back());
		fields_to_remove.pop_back();
	}
}

bool BlockedFields::is_blocked(Coords coords) {
	return (blocked_fields_.count(coords.hash()) != 0);
}

FlagsForRoads::Candidate::Candidate(uint32_t coords, int32_t distance, bool economy)
   : coords_hash(coords), air_distance(distance), different_economy(economy) {
	new_road_possible = false;
	accessed_via_roads = false;
	// Values are only very rough, and are dependant on the map size
	new_road_length = 2 * Widelands::kMapDimensions.at(Widelands::kMapDimensions.size() - 1);
	current_roads_distance = 2 * (Widelands::kMapDimensions.size() - 1);  // must be big enough
	reduction_score = -air_distance;  // allows reasonable ordering from the start
}

bool FlagsForRoads::Candidate::operator<(const Candidate& other) const {
	if (reduction_score == other.reduction_score) {
		return coords_hash < other.coords_hash;
	} else {
		return reduction_score > other.reduction_score;
	}
}

bool FlagsForRoads::Candidate::operator==(const Candidate& other) const {
	return coords_hash == other.coords_hash;
}

void FlagsForRoads::Candidate::calculate_score() {
	if (!new_road_possible) {
		reduction_score = kRoadNotFound - air_distance;  // to have at least some ordering preserved
	} else if (different_economy) {
		reduction_score = kRoadToDifferentEconomy - air_distance - 2 * new_road_length;
	} else if (!accessed_via_roads) {
		if (air_distance + 6 > new_road_length) {
			reduction_score = kShortcutWithinSameEconomy - air_distance - 2 * new_road_length;
		} else {
			reduction_score = kRoadNotFound;
		}
	} else {
		reduction_score = current_roads_distance - 2 * new_road_length;
	}
}

void FlagsForRoads::print() {  // this is for debugging and development purposes
	for (auto& candidate_flag : queue) {
		log("   %starget: %3dx%3d, saving: %5d (%3d), air distance: %3d, new road: %6d, score: %5d "
		    "%s\n",
		    (candidate_flag.reduction_score >= min_reduction && candidate_flag.new_road_possible) ?
		       "+" :
		       " ",
		    Coords::unhash(candidate_flag.coords_hash).x,
		    Coords::unhash(candidate_flag.coords_hash).y,
		    candidate_flag.current_roads_distance - candidate_flag.new_road_length, min_reduction,
		    candidate_flag.air_distance, candidate_flag.new_road_length,
		    candidate_flag.reduction_score,
		    (candidate_flag.new_road_possible) ? ", new road possible" : " ");
	}
}

// Queue is ordered but some target flags are only estimations so we take such a candidate_flag
// first
bool FlagsForRoads::get_best_uncalculated(uint32_t* winner) {
	for (auto& candidate_flag : queue) {
		if (!candidate_flag.new_road_possible) {
			*winner = candidate_flag.coords_hash;
			return true;
		}
	}
	return false;
}

// Road from starting flag to this flag can be built
void FlagsForRoads::road_possible(Widelands::Coords coords, uint32_t distance) {
	// std::set does not allow updating
	Candidate new_candidate_flag = Candidate(0, 0, false);
	for (auto candidate_flag : queue) {
		if (candidate_flag.coords_hash == coords.hash()) {
			new_candidate_flag = candidate_flag;
			assert(new_candidate_flag.coords_hash == candidate_flag.coords_hash);
			queue.erase(candidate_flag);
			break;
		}
	}

	new_candidate_flag.new_road_length = distance;
	new_candidate_flag.new_road_possible = true;
	new_candidate_flag.calculate_score();
	queue.insert(new_candidate_flag);
}

// Remove the flag from candidates as interconnecting road is not possible
void FlagsForRoads::road_impossible(Widelands::Coords coords) {
	const uint32_t hash = coords.hash();
	for (auto candidate_flag : queue) {
		if (candidate_flag.coords_hash == hash) {
			queue.erase(candidate_flag);
			return;
		}
	}
}

// Updating walking distance over existing roads
// Queue does not allow modifying its members so we erase and then eventually insert modified member
void FlagsForRoads::set_road_distance(Widelands::Coords coords, int32_t distance) {
	const uint32_t hash = coords.hash();
	Candidate new_candidate_flag = Candidate(0, 0, false);
	bool replacing = false;
	for (auto candidate_flag : queue) {
		if (candidate_flag.coords_hash == hash) {
			assert(!candidate_flag.different_economy);
			if (distance < candidate_flag.current_roads_distance) {
				new_candidate_flag = candidate_flag;
				queue.erase(candidate_flag);
				replacing = true;
				break;
			}
			break;
		}
	}
	if (replacing) {
		new_candidate_flag.current_roads_distance = distance;
		new_candidate_flag.accessed_via_roads = true;
		new_candidate_flag.calculate_score();
		queue.insert(new_candidate_flag);
	}
}

bool FlagsForRoads::get_winner(uint32_t* winner_hash) {
	// If AI can ask for 2nd position, but there is only one viable candidate
	// we return the first one of course
	bool has_winner = false;
	for (auto candidate_flag : queue) {
		if (candidate_flag.reduction_score < min_reduction || !candidate_flag.new_road_possible) {
			continue;
		}
		assert(candidate_flag.air_distance > 0);
		assert(candidate_flag.reduction_score >= min_reduction);
		assert(candidate_flag.new_road_possible);
		*winner_hash = candidate_flag.coords_hash;
		has_winner = true;

		if (std::rand() % 3 > 0) {
			// with probability of 2/3 we accept this flag
			return true;
		}
	}

	if (has_winner) {
		return true;
	}
	return false;
}

// This is an struct that stores strength of players, info on teams and provides some outputs from
// these data
PlayersStrengths::PlayerStat::PlayerStat() : team_number(0), is_enemy(false), players_power(0), old_players_power(0), old60_players_power(0),players_casualities(0) {
}
PlayersStrengths::PlayerStat::PlayerStat(Widelands::TeamNumber tc, bool e, uint32_t pp, uint32_t op, uint32_t o60p, uint32_t cs, uint32_t land, uint32_t oland, uint32_t o60l)
   : team_number(tc), is_enemy(e), players_power(pp),  old_players_power(op), old60_players_power(o60p), players_casualities(cs), players_land(land),
	old_players_land(oland), old60_players_land(o60l)  {
	 last_time_seen = kNever;  
}

// Inserting/updating data
void PlayersStrengths::add(Widelands::PlayerNumber pn, Widelands::PlayerNumber opn, Widelands::TeamNumber mytn,
   Widelands::TeamNumber pltn, uint32_t pp, uint32_t op, uint32_t o60p, uint32_t cs, uint32_t land, uint32_t oland,  uint32_t o60l) {
	if (all_stats.count(opn) == 0) {
		bool enemy = false;
		if ( pn == opn ) {
			;
		} else if (pltn == 0 || mytn == 0) {
			enemy = true;
		} else if (pltn != mytn) {
			enemy = true;			
		}
		this_player_number = pn;
		//printf (" %d PlayersStrengths: player %d / team: %d - is%s enemy\n", pn, opn, pltn, (enemy)?"":" not");
		all_stats.insert(std::pair<Widelands::PlayerNumber, PlayerStat>(opn, PlayerStat(pltn, enemy, pp, op, o60p, cs, land, oland, o60l)));
	} else {
		all_stats[opn].players_power = pp;
		all_stats[opn].old_players_power = op;
		all_stats[opn].old60_players_power = o60p;
		all_stats[opn].players_casualities = cs;
		all_stats[opn].players_land = land;
		all_stats[opn].old_players_land = oland;
		all_stats[opn].old60_players_land = oland;
	}
}

void PlayersStrengths::recalculate_team_power() {
	team_powers.clear();
	for (auto& item : all_stats) {
		if (item.second.team_number > 0) {  // is a member of a team
			if (team_powers.count(item.second.team_number) > 0) {
				team_powers[item.second.team_number] += item.second.players_power;
			} else {
				team_powers[item.second.team_number] = item.second.players_power;
			}
		}
	}
}

bool PlayersStrengths::any_enemy_seen_lately(const uint32_t gametime){
	for (auto& item : all_stats) {
		if (item.second.is_enemy && player_seen_lately(item.first, gametime)) {
			return true;
		}  // is a member of a team
	}
	return false;
}

uint8_t PlayersStrengths::enemies_seen_lately_count(const uint32_t gametime){
	uint8_t count = 0;
	for (auto& item : all_stats) {
		if (item.second.is_enemy && player_seen_lately(item.first, gametime)) {
			count +=1;
		}  // is a member of a team
	}
	return count;
}

uint32_t PlayersStrengths::enemy_last_seen(){
	uint32_t time = 0;
	for (auto& item : all_stats) {
		if (item.second.last_time_seen == kNever){
			continue;
		}
		if (item.second.is_enemy && item.second.last_time_seen > time) {
			time = item.second.last_time_seen;
		}  
	}
	if (time == 0) {return kNever;}
	return time;
}



void PlayersStrengths::set_last_time_seen(const uint32_t seentime, Widelands::PlayerNumber pn){
	assert(all_stats.count(pn) > 0);
	all_stats[pn].last_time_seen = seentime;
}

bool PlayersStrengths::get_is_enemy(Widelands::PlayerNumber pn){
	assert(all_stats.count(pn) > 0);
	return all_stats[pn].is_enemy;

}

bool PlayersStrengths::player_seen_lately( Widelands::PlayerNumber pn, const uint32_t gametime){
	assert(all_stats.count(pn) > 0);
	if (all_stats[pn].last_time_seen == kNever){
		return false;
	}
	if (all_stats[pn].last_time_seen + static_cast<uint32_t>(2 * 60 * 1000) > gametime){
		return true;
	}
	return false;
}

// This is strength of player
uint32_t PlayersStrengths::get_player_power(Widelands::PlayerNumber pn) {
	if (all_stats.count(pn) > 0) {
		return all_stats[pn].players_power;
	};
	return 0;
}

// This is land size owned by player
uint32_t PlayersStrengths::get_player_land(Widelands::PlayerNumber pn) {
	if (all_stats.count(pn) > 0) {
		return all_stats[pn].players_land;
	};
	return 0;
}

uint32_t PlayersStrengths::get_visible_enemies_power(const uint32_t gametime){
	uint32_t pw = 0;
	for (auto& item : all_stats) {
		if (get_is_enemy(item.first) && player_seen_lately(item.first, gametime)) {
			pw += item.second.players_power;
		}
	}
	return pw;
}

uint32_t PlayersStrengths::get_enemies_average_power(){
	uint32_t sum = 0;
	uint8_t count = 0;
	for (auto& item : all_stats) {
		if (get_is_enemy(item.first)) {
			sum += item.second.players_power;
			count += 1;
		}
	}
	if (count > 0) {
		return sum/count;
	}
	return 0;
}

uint32_t PlayersStrengths::get_enemies_average_land(){
	uint32_t sum = 0;
	uint8_t count = 0;
	for (auto& item : all_stats) {
		if (get_is_enemy(item.first)) {
			sum += item.second.players_land;
			count += 1;
		}
	}
	if (count > 0) {
		return sum/count;
	}
	return 0;
}

uint32_t PlayersStrengths::get_enemies_max_power(){
	uint32_t power = 0;
	for (auto& item : all_stats) {
		if (get_is_enemy(item.first)) {
			power=std::max<uint32_t>(power, item.second.players_power);
		}
	}
	return power;
}

uint32_t PlayersStrengths::get_enemies_max_land(){
	uint32_t land = 0;
	for (auto& item : all_stats) {
		if (get_is_enemy(item.first)) {
			land=std::max<uint32_t>(land, item.second.players_land);
		}
	}
	return land;
}

uint32_t PlayersStrengths::get_old_player_power(Widelands::PlayerNumber pn) {
	if (all_stats.count(pn) > 0) {
		return all_stats[pn].old_players_power;
	};
	return 0;
}

uint32_t PlayersStrengths::get_old60_player_power(Widelands::PlayerNumber pn) {
	if (all_stats.count(pn) > 0) {
		return all_stats[pn].old60_players_power;
	};
	return 0;
}

uint32_t PlayersStrengths::get_old_player_land(Widelands::PlayerNumber pn) {
	if (all_stats.count(pn) == 0) {
		printf (" %d: Players statistics are still empty\n", pn);
		return 0;
	}
	return all_stats[pn].old_players_land;
}

uint32_t PlayersStrengths::get_old60_player_land(Widelands::PlayerNumber pn) {
	if (all_stats.count(pn) == 0) {
		printf (" %d: Players statistics are still empty\n", pn);
		return 0;
	}
	return all_stats[pn].old60_players_land;
}


uint32_t PlayersStrengths::get_old_visible_enemies_power(const uint32_t gametime){
	uint32_t pw = 0;
	for (auto& item : all_stats) {
		if (get_is_enemy(item.first) && player_seen_lately(item.first, gametime)) {
			pw += item.second.old_players_power;
		}
	}
	return pw;
}

// This is casualities of player
uint32_t PlayersStrengths::get_player_casualities(Widelands::PlayerNumber pn) {
	if (all_stats.count(pn) > 0) {
		return all_stats[pn].players_casualities;
	};
	return 0;
}

// This is strength of player plus third of strength of other members of his team
uint32_t PlayersStrengths::get_modified_player_power(Widelands::PlayerNumber pn) {
	uint32_t result = 0;
	Widelands::TeamNumber team = 0;
	if (all_stats.count(pn) > 0) {
		result = all_stats[pn].players_power;
		team = all_stats[pn].team_number;
	};
	if (team > 0 && team_powers.count(team) > 0) {
		result = result + (team_powers[team] - result) / 3;
	};
	return result;
}

bool PlayersStrengths::players_in_same_team(Widelands::PlayerNumber pl1,
                                            Widelands::PlayerNumber pl2) {
	assert(all_stats.count(pl1) > 0);
	assert(all_stats.count(pl2) > 0);
	if (pl1 == pl2) {
		return false;
	} else if (all_stats[pl1].team_number > 0 &&
		       all_stats[pl1].team_number == all_stats[pl2].team_number) {
		// team number 0 = no team
		return true;
	} else {
		return false;
	}
}

bool PlayersStrengths::strong_enough(Widelands::PlayerNumber pl) {
	if (all_stats.count(pl) == 0) {
		return false;
	}
	uint32_t my_strength = all_stats[pl].players_power;
	uint32_t strongest_opponent_strength = 0;
	for (auto item : all_stats) {
		if (!players_in_same_team(item.first, pl) && pl != item.first) {
			if (get_modified_player_power(item.first) > strongest_opponent_strength) {
				strongest_opponent_strength = get_modified_player_power(item.first);
			}
		}
	}
	return my_strength > strongest_opponent_strength + 50;
}

}  // namespace WIdelands
