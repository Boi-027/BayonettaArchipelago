from BaseClasses import Region, ItemClassification
from .Locations import (
    location_table, CHAPTERS, CHAPTER_LOCATIONS, CHEST_LOCATION_NAMES,
    RANK_LOCATION_NAMES, TEAR_LOCATION_NAMES, TECHNIQUE_LOCATION_NAMES,
    PROLOGUE_KEY, ALL_CHAPTER_KEYS,
    chapter_clear_event, chapter_clear_event_location,
)


def create_regions(world, location_class, item_class):
    multiworld = world.multiworld
    player = world.player
    include_chests = bool(world.options.include_chests.value)
    include_ranks = int(world.options.verse_rank_target.value) != 0
    include_tears = bool(world.options.include_tears.value)

    def make_locations(region, names):
        for loc_name in names:
            if not include_chests and loc_name in CHEST_LOCATION_NAMES:
                continue
            if not include_ranks and loc_name in RANK_LOCATION_NAMES:
                continue
            if not include_tears and loc_name in TEAR_LOCATION_NAMES:
                continue
            loc = location_class(player, loc_name, location_table[loc_name].id, region)
            region.locations.append(loc)

    def add_clear_event(region, chapter_key):
        # One event per chapter: an unfillable location holding a locked
        # "<Chapter> Clear" event item. The goal completion conditions count
        # these instead of poking at real (randomized) locations, which keeps
        # logic sound and avoids can_reach inside access rules.
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

    # A shared "Hub" region that holds the non-chapter locations (the shop and
    # the technique learns). Alfheim portals are NOT hub locations: each lives
    # inside the chapter that contains it (see CHAPTER_LOCATIONS), gated by
    # that chapter's unlock.
    hub = Region("Gates of Hell", player, multiworld)
    make_locations(hub, [name for name in location_table
                         if name.startswith("Shop:") or name in TECHNIQUE_LOCATION_NAMES])
    multiworld.regions.append(hub)
    menu.connect(hub, "Enter Gates of Hell")

    # The Prologue plays at the start of a fresh save and has no unlock item of
    # its own, so its locations hang off the hub with no extra gate.
    prologue = Region(PROLOGUE_KEY, player, multiworld)
    make_locations(prologue, CHAPTER_LOCATIONS[PROLOGUE_KEY])
    add_clear_event(prologue, PROLOGUE_KEY)
    multiworld.regions.append(prologue)
    hub.connect(prologue, f"Enter {PROLOGUE_KEY}")

    # One region per chapter. Its locations live inside it; the entrance from
    # the hub is what gets gated by the chapter's unlock item (see Rules.py).
    for chap_key, _chap_num, _unlock in CHAPTERS:
        region = Region(chap_key, player, multiworld)
        make_locations(region, CHAPTER_LOCATIONS[chap_key])
        add_clear_event(region, chap_key)
        multiworld.regions.append(region)
        hub.connect(region, f"Enter {chap_key}")