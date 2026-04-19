import typing
import os
import json
from .Items import item_data_table, health_item_data_table, trap_item_data_table, action_item_data_table, cannon_item_data_table, painting_unlock_item_data_table, item_table, SM64Item, Toad_Soul_item_data_table
from .Locations import location_table, SM64Location
from .Options import sm64_options_groups, SM64Options
from .Rules import set_rules
from .Regions import create_regions, sm64_level_to_entrances, SM64Levels
from BaseClasses import Item, Tutorial, ItemClassification, Region
from ..AutoWorld import World, WebWorld


class SM64Web(WebWorld):
	tutorials = [Tutorial(
		"Multiworld Setup Guide",
		"A guide to setting up SM64EX for MultiWorld.",
		"English",
		"setup_en.md",
		"setup/en",
		["N00byKing"]
	)]
	option_groups = sm64_options_groups


class SM64World(World):
	game: str = "Cursed Mario 64"
	topology_present = False
	web = SM64Web()
	item_name_to_id = item_table
	location_name_to_id = location_table
	required_client_version = (0, 3, 5)
	area_connections: typing.Dict[int, int]
	options_dataclass = SM64Options
	number_of_stars: int
	move_rando_bitvec: int
	filler_count: int
	star_costs: typing.Dict[str, int]
	star_costs_spoiler_key_maxlen = len(max([
		'First Floor Big Star Door',
		'Basement Big Star Door',
		'Second Floor Big Star Door',
		'MIPS 1',
		'MIPS 2',
		'Endless Stairs',
	], key=len))

	def generate_early(self):
		max_stars = 240
		if not self.options.enable_coin_stars:
			max_stars -= 15
		self.move_rando_bitvec = 0
		if self.options.enable_move_rando:
			double_jump_bitvec_offset = action_item_data_table['Double Jump'].code
			for action in self.options.move_rando_actions.value:
				max_stars -= 1
				self.move_rando_bitvec |= (1 << (action_item_data_table[action].code - double_jump_bitvec_offset))
		if self.options.exclamation_boxes:
			max_stars += 29
		if self.options.enable_locked_paintings:
			max_stars -= len(painting_unlock_item_data_table)
		self.number_of_stars = min(self.options.amount_of_stars, max_stars)
		self.filler_count = max_stars - self.number_of_stars
		self.star_costs = {
			'FirstBowserDoorCost': round(self.options.first_bowser_star_door_cost * self.number_of_stars / 100),
			'BasementDoorCost': round(self.options.basement_star_door_cost * self.number_of_stars / 100),
			'SecondFloorDoorCost': round(self.options.second_floor_star_door_cost * self.number_of_stars / 100),
			'MIPS1Cost': round(self.options.mips1_cost * self.number_of_stars / 100),
			'MIPS2Cost': round(self.options.mips2_cost * self.number_of_stars / 100),
			'StarsToFinish': round(self.options.stars_to_finish * self.number_of_stars / 100)
		}
		if self.number_of_stars == 120 and self.options.mips1_cost == 12:
			self.star_costs['MIPS1Cost'] = 15
		self.topology_present = self.options.area_rando

	def create_regions(self):
		create_regions(self.multiworld, self.options, self.player)

	def set_rules(self):
		self.area_connections = {}
		set_rules(self.multiworld, self.options, self.player, self.area_connections, self.star_costs, self.move_rando_bitvec)
		if self.topology_present:
			for entrance, destination in self.area_connections.items():
				self.multiworld.spoiler.set_entrance(sm64_level_to_entrances[entrance] + " Entrance", sm64_level_to_entrances[destination], 'entrance', self.player)

	def create_item(self, name: str) -> Item:
		data = item_data_table[name]
		item = SM64Item(name, data.classification, data.code, self.player)
		return item

	def create_items(self):
			itempool = []
			trap_fill_pct = self.options.Trap_Per.value
			health_types = list(health_item_data_table)
			filler_names = health_types + ["1Up Mushroom"]
			base_health_count = len(health_types) * self.filler_count
			base_1up_count = self.filler_count
			base_koopa_shell_count = 20
			total_filler_slots = base_health_count + base_1up_count + base_koopa_shell_count
			trap_count = (total_filler_slots * trap_fill_pct) // 100
			filler_count_remaining = total_filler_slots - trap_count
			trap_weights = {
				"Bonk Trap": self.options.Bonk_Per.value,
				"Fire Trap": self.options.Fire_Per.value,
				"Shock Trap": self.options.Shock_Per.value,
				"Chuck Trap": self.options.Chuck_Per.value,
				"Spin Trap": self.options.Spin_per.value,
				"Literature Trap": self.options.Lit_Per.value,
				"Rainbow Road Trap": self.options.Rr_Per.value,
			}
			total_weight = sum(trap_weights.values())
			if trap_count > 0:
				if total_weight > 0:
					trap_names = list(trap_weights.keys())
					trap_counts = {}
					placed = 0
					for trap_name in trap_names:
						weight = trap_weights[trap_name]
						count = (trap_count * weight) // total_weight
						trap_counts[trap_name] = count
						placed += count
					remainder = trap_count - placed
					if remainder > 0:
						sorted_traps = sorted(
							trap_names,
							key=lambda name: trap_weights[name],
							reverse=True
						)
						for i in range(remainder):
							trap_counts[sorted_traps[i % len(sorted_traps)]] += 1
					for trap_name in trap_names:
						itempool += [self.create_item(trap_name) for _ in range(trap_counts[trap_name])]
			itempool += [
				self.create_item(toad_soul_name)
				for toad_soul_name in Toad_Soul_item_data_table
				for _ in range(1)
			]
			filler_pool_counts = {name: self.filler_count for name in filler_names}
			filler_pool_counts["Koopa Shell"] = 20
			filler_pool_total = sum(filler_pool_counts.values())
			placed = 0
			filler_pool_names = list(filler_pool_counts.keys())
			for i, filler_name in enumerate(filler_pool_names):
				base_count = filler_pool_counts[filler_name]
				if i == len(filler_pool_names) - 1:
					count = filler_count_remaining - placed
				else:
					count = (filler_count_remaining * base_count) // filler_pool_total
					placed += count
				itempool += [self.create_item(filler_name) for _ in range(count)]
			star_range = self.number_of_stars
			if self.options.enable_coin_stars == "vanilla":
				star_range -= 15
			itempool += [self.create_item("Power Star") for _ in range(star_range)]
			if not self.options.progressive_keys:
				itempool += [
					self.create_item("Basement Key"),
					self.create_item("Second Floor Key"),
				]
			else:
				itempool += [self.create_item("Progressive Key") for _ in range(4)]
			itempool += [
				self.create_item(cap_name)
				for cap_name in ["Wing Cap", "Metal Cap", "Tanish Cap"]
			]
			if self.options.buddy_checks:
				itempool += [
					self.create_item(cannon_name)
					for cannon_name in cannon_item_data_table.keys()
				]
			if self.options.enable_locked_paintings:
				itempool += [
					self.create_item(painting_name)
					for painting_name in painting_unlock_item_data_table.keys()
				]
			double_jump_bitvec_offset = action_item_data_table["Double Jump"].code
			itempool += [
				self.create_item(action)
				for action, itemdata in action_item_data_table.items()
				if self.move_rando_bitvec & (1 << (itemdata.code - double_jump_bitvec_offset))
			]
			self.multiworld.itempool += itempool
			
	def generate_basic(self):
		pass

	def get_filler_item_name(self) -> str:
		return "1Up Mushroom"

	def fill_slot_data(self):
		return {
			"AreaRando": self.area_connections,
			"MoveRandoVec": self.move_rando_bitvec,
			"PaintingRando": self.options.enable_locked_paintings.value,
			"DeathLink": self.options.death_link.value,
			"CompletionType": self.options.completion_type.value,
			**self.star_costs
		}

	def generate_output(self, output_directory: str):
		if self.multiworld.players != 1:
			return
		data = {
			"slot_data": self.fill_slot_data(),
			"location_to_item": {self.location_name_to_id[i.name]: item_table[i.item.name] for i in self.multiworld.get_locations()},
			"data_package": {
				"data": {
					"games": {
						self.game: {
							"item_name_to_id": self.item_name_to_id,
							"location_name_to_id": self.location_name_to_id
						}
					}
				}
			}
		}
		filename = f"{self.multiworld.get_out_file_name_base(self.player)}.apsm64ex"
		with open(os.path.join(output_directory, filename), 'w') as f:
			json.dump(data, f)