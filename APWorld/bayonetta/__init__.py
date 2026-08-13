from worlds.AutoWorld import World
from BaseClasses import Item, Location, ItemClassification
from .Items import (
    item_table, FILLER_ITEM_NAMES, UNPLACED_ITEM_NAMES,
    WEAPON_NAMES, LP_NAMES, LP_PART_NAMES, ACCESSORY_NAMES, TECHNIQUE_NAMES,
    CONSUMABLE_NAMES, CRAFTING_NAMES, HALO_NAMES, CHAPTER_UNLOCK_NAMES,
    TRAP_NAMES, MACGUFFIN_NAME,
)
from .Locations import (
    location_table, CHEST_LOCATION_NAMES, TECHNIQUE_LOCATION_NAMES,
    ALFHEIM_ALL_NAMES, ALFHEIM_PORTAL_NAMES,
)
from .Options import BayonettaOptions
from .Regions import create_regions
from .Rules import set_rules
from .Locations import location_table, location_name_groups


class BayonettaItem(Item):
    game = "Bayonetta"


class BayonettaLocation(Location):
    game = "Bayonetta"


CONSUMABLE_FILLER_COUNTS = {
    "Green Herb Lollipop": 15,
    "Mega Green Herb Lollipop": 15,
    "Purple Magic Lollipop": 15,
    "Mega Purple Magic Lollipop": 15,
    "Bloody Rose Lollipop": 15,
    "Mega Bloody Rose Lollipop": 15,
    "Yellow Moon Lollipop": 15,
    "Mega Yellow Moon Lollipop": 15,
    "Magic Flute": 15,
    "Red Hot Shot": 15,
    "Unicorn Horn x5": 8,
    "Unicorn Horn x10": 5,
    "Unicorn Horn x15": 3,
    "Baked Gecko x5": 8,
    "Baked Gecko x10": 5,
    "Baked Gecko x15": 3,
    "Mandragora Root x5": 8,
    "Mandragora Root x10": 5,
    "Mandragora Root x15": 3,
}

EXTRA_FRAGMENTS = (("Broken Witch Heart", 39), ("Broken Moon Pearl", 15))
HALO_FILLER = sorted(HALO_NAMES)


class BayonettaWorld(World):
    game = "Bayonetta"
    location_name_groups = location_name_groups
    options_dataclass = BayonettaOptions
    options: BayonettaOptions
    item_name_to_id = {name: data.id for name, data in item_table.items()}
    location_name_to_id = {name: data.id for name, data in location_table.items()}
    item_name_groups = {
        "Weapons": WEAPON_NAMES,
        "Golden LPs": LP_NAMES,
        "LP Parts": LP_PART_NAMES,
        "Accessories": ACCESSORY_NAMES,
        "Techniques": TECHNIQUE_NAMES,
        "Chapter Unlocks": set(CHAPTER_UNLOCK_NAMES),
        "Consumables": CONSUMABLE_NAMES,
        "Crafting Compounds": CRAFTING_NAMES,
        "Halos": HALO_NAMES,
        "Traps": TRAP_NAMES,
    }
    location_name_groups = {
        "Shop": {name for name in location_table if name.startswith("Shop:")},
        "Techniques": TECHNIQUE_LOCATION_NAMES,
        "Chests": CHEST_LOCATION_NAMES,
        "Alfheims": ALFHEIM_ALL_NAMES,
        "Alfheim Portals": ALFHEIM_PORTAL_NAMES,
        "Chapter Completions": {name for name in location_table if name.endswith("- Complete")},
    }

    def fill_slot_data(self):
        return {
            "death_link": self.options.death_link.value,
            "damage_link": self.options.damage_link.value,
            "trap_link": self.options.trap_link.value,
            "ring_link": self.options.ring_link.value,
            "starting_chapter": self.options.starting_chapter.value,
            "include_weapons": self.options.include_weapons.value,
            "include_techniques": self.options.include_techniques.value,
            "include_accessories": self.options.include_accessories.value,
            "include_consumables": self.options.include_consumables.value,
            "include_crafting": self.options.include_crafting.value,
            "include_halos": self.options.include_halos.value,
            "include_golden_lps": self.options.include_golden_lps.value,
            "include_chests": self.options.include_chests.value,
            "trap_percentage": self.options.trap_percentage.value,
            "prologue_visible": self.options.prologue_visible.value,
            "chapter_blocking": self.options.chapter_blocking.value,
            "goal": self.options.goal.value,
            "goal_chapter_count": self.options.goal_chapter_count.value,
            "memory_fragments_required": min(self.options.memory_fragments_required.value,
                                        getattr(self, "_memory_fragments_placed", 0)
                                        or self.options.memory_fragments_total.value),
            "verse_rank_target": self.options.verse_rank_target.value,
            "include_tears": self.options.include_tears.value,
            "include_angel_arms": self.options.include_angel_arms.value,
            "include_punches": self.options.include_punches.value,
            "include_kicks": self.options.include_kicks.value,
            "include_torture_attacks": self.options.include_torture_attacks.value,
        }

    def starting_chapter_unlock(self) -> str:
        return f"Chapter {self.options.starting_chapter.value} Unlock"

    def create_item(self, name: str) -> Item:
        data = item_table[name]
        return BayonettaItem(name, data.classification, data.id, self.player)

    def create_regions(self):
        create_regions(self, BayonettaLocation, BayonettaItem)

    def create_items(self):
        pool = []
        starting_unlock = self.starting_chapter_unlock()

        excluded = set(UNPLACED_ITEM_NAMES) | TRAP_NAMES | {MACGUFFIN_NAME}
        if not self.options.include_weapons.value:
            excluded |= WEAPON_NAMES
        if not self.options.include_golden_lps.value:
            excluded |= LP_NAMES | LP_PART_NAMES
        if not self.options.include_techniques.value:
            excluded |= TECHNIQUE_NAMES
        if not self.options.include_accessories.value:
            excluded |= ACCESSORY_NAMES
        if not self.options.include_angel_arms.value:
            excluded |= {"Angel Arms"}
        if not self.options.include_punches.value:
            excluded |= {"Punch"}
        if not self.options.include_kicks.value:
            excluded |= {"Kick"}
        if not self.options.include_torture_attacks.value:
            excluded |= {"Torture Attacks"}

        for name, data in item_table.items():
            if data.classification == ItemClassification.filler:
                continue
            if name == starting_unlock or name in excluded:
                continue
            if name == "Requiem Unlock":
                if self.options.goal.value == 1:
                    pool.append(self.create_item(self.get_filler_item_name()))
                else:
                    pool.append(self.create_item(name))
            else:
                pool.append(self.create_item(name))

        self.multiworld.push_precollected(self.create_item(starting_unlock))
        total_locations = len(self.multiworld.get_unfilled_locations(self.player))

        if self.options.goal.value == 4:
            target = self.options.memory_fragments_total.value
            room = max(0, total_locations - len(pool))
            placed = min(target, room)
            for _ in range(placed):
                pool.append(self.create_item(MACGUFFIN_NAME))
            self._memory_fragments_placed = placed
        else:
            self._memory_fragments_placed = 0

        for name, extra in EXTRA_FRAGMENTS:
            for _ in range(extra):
                if len(pool) >= total_locations:
                    break
                pool.append(self.create_item(name))

        weights = self.options.trap_weights.value
        trap_list = [name for name in sorted(TRAP_NAMES) if weights.get(name, 0) > 0]
        trap_weight_list = [weights[name] for name in trap_list]
        trap_count = ((total_locations - len(pool))
                     * self.options.trap_percentage.value // 100)
        if trap_list:
            for _ in range(trap_count):
                pool.append(self.create_item(
                    self.random.choices(trap_list, weights=trap_weight_list)[0]))

        free = total_locations - len(pool)
        halo_share = free // 3 if self.options.include_halos.value else 0
        consumable_budget = max(0, free - halo_share)
        wanted = []
        for name, cnt in CONSUMABLE_FILLER_COUNTS.items():
            if name in CONSUMABLE_NAMES and not self.options.include_consumables.value:
                continue
            if name in CRAFTING_NAMES and not self.options.include_crafting.value:
                continue
            wanted.extend([name] * cnt)
        self.random.shuffle(wanted)  
        for name in wanted[:consumable_budget]:
            pool.append(self.create_item(name))

        if self.options.include_halos.value:
            while len(pool) < total_locations:
                pool.append(self.create_item(self.random.choice(HALO_FILLER)))

        while len(pool) < total_locations:
            pool.append(self.create_item(self.get_filler_item_name()))

        if len(pool) > total_locations:
            raise Exception(
                f"Bayonetta ({self.player_name}): item pool ({len(pool)}) exceeds "
                f"location count ({total_locations}); check the include_* options.")

        self.multiworld.itempool += pool

    def get_filler_item_name(self) -> str:
        allowed = []
        if self.options.include_consumables.value:
            allowed += sorted(CONSUMABLE_NAMES)
        if self.options.include_crafting.value:
            allowed += sorted(CRAFTING_NAMES)
        if self.options.include_halos.value:
            allowed += HALO_FILLER
        if not allowed:
            allowed = HALO_FILLER
        return self.random.choice(allowed)

    def set_rules(self):
        set_rules(self)