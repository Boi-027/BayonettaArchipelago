from BaseClasses import Region, ItemClassification
from .Locations import (
    location_table, CHAPTERS, CHAPTER_LOCATIONS, CHEST_LOCATION_NAMES,
    RANK_LOCATION_NAMES, ANY_RANK_LOCATION_NAMES, TEAR_LOCATION_NAMES, TECHNIQUE_LOCATION_NAMES,
    PROLOGUE_KEY, ALL_CHAPTER_KEYS,
    chapter_clear_event, chapter_clear_event_location,
)


def create_regions(world, location_class, item_class):
    multiworld = world.multiworld
    player = world.player
    include_chests = bool(world.options.include_chests.value)
    
    # verse_rank_target option: 0 = disabled, 1 = any_medal, 2 = all_medals
    rank_target_mode = int(world.options.verse_rank_target.value)
    include_tears = bool(world.options.include_tears.value)

    def make_locations(region, names):
        for loc_name in names:
            if not include_chests and loc_name in CHEST_LOCATION_NAMES:
                continue
            
            # Handle rank filtering based on user setting
            if rank_target_mode == 0:
                if loc_name in RANK_LOCATION_NAMES or loc_name in ANY_RANK_LOCATION_NAMES:
                    continue
            elif rank_target_mode == 1: # Any Medal
                if loc_name in RANK_LOCATION_NAMES: # skip all_medals tier checks
                    continue
            elif rank_target_mode == 2: # All Medals
                if loc_name in ANY_RANK_LOCATION_NAMES: # skip any_medal check
                    continue

            if not include_tears and loc_name in TEAR_LOCATION_NAMES:
                continue
            loc = location_class(player, loc_name, location_table[loc_name].id, region)
            region.locations.append(loc)

    def add_clear_event(region, chapter_key):
        event_loc = location_class(
            player, chapter_clear_event_location(chapter_key), None, region)
        event_item = item_class(
            chapter_clear_event(chapter_key),
            ItemClassification.progression, None, player)
        event_loc.place_locked_item(event_item)
        region.locations.append(event_loc)

    # Menu (origin)
    menu = Region("Menu", player, multiworld)
    multiworld.regions.append(menu)

    hub = Region("Gates of Hell", player, multiworld)
    make_locations(hub, [name for name in location_table
                         if name.startswith("Shop:") or name in TECHNIQUE_LOCATION_NAMES])
    multiworld.regions.append(hub)
    menu.connect(hub, "Enter Gates of Hell")

    prologue = Region(PROLOGUE_KEY, player, multiworld)
    make_locations(prologue, CHAPTER_LOCATIONS[PROLOGUE_KEY])
    add_clear_event(prologue, PROLOGUE_KEY)
    multiworld.regions.append(prologue)
    hub.connect(prologue, f"Enter {PROLOGUE_KEY}")

    for chap_key, _chap_num, _unlock in CHAPTERS:
        region = Region(chap_key, player, multiworld)
        make_locations(region, CHAPTER_LOCATIONS[chap_key])
        add_clear_event(region, chap_key)
        multiworld.regions.append(region)
        hub.connect(region, f"Enter {chap_key}")