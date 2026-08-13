from BaseClasses import ItemClassification
from typing import NamedTuple


class ItemDefinition(NamedTuple):
    id: int
    classification: ItemClassification


# Archipelago requires unique IDs. Bayonetta items live in the 50000 block.
item_table = {
    # --- WEAPONS (Progression) ---
    "Scarborough Fair": ItemDefinition(50001, ItemClassification.progression),
    "Onyx Roses": ItemDefinition(50002, ItemClassification.progression),
    "Shuraba": ItemDefinition(50003, ItemClassification.progression),
    "Kulshedra": ItemDefinition(50004, ItemClassification.progression),
    "Durga": ItemDefinition(50005, ItemClassification.progression),
    "Odette": ItemDefinition(50006, ItemClassification.progression),
    "Kilgore": ItemDefinition(50007, ItemClassification.progression),
    "Sai Fung": ItemDefinition(50008, ItemClassification.progression),
    "Bazillions": ItemDefinition(50009, ItemClassification.progression),
    "Pillow Talk": ItemDefinition(50010, ItemClassification.progression),
    "Rodin": ItemDefinition(50011, ItemClassification.progression),
    # --- GOLDEN LPS ---
    "LP - Trois Marches Militaires": ItemDefinition(50050, ItemClassification.progression),
    "LP - Fantaisie-Impromptu": ItemDefinition(50051, ItemClassification.progression),
    "LP - Sonate in DK. 448": ItemDefinition(50052, ItemClassification.progression),
    "LP - Les Patineurs Waltz op.183": ItemDefinition(50053, ItemClassification.progression),
    "LP - Walkure Ride": ItemDefinition(50054, ItemClassification.progression),
    "LP - Turangalila-Symphonie": ItemDefinition(50055, ItemClassification.progression),
    # --- LP PARTS (Community request: collectible individual vinyl sections) ---
    "LP Part - Onyx Roses": ItemDefinition(50070, ItemClassification.progression),
    "LP Part - Kulshedra": ItemDefinition(50071, ItemClassification.progression),
    "LP Part - Durga": ItemDefinition(50072, ItemClassification.progression),
    "LP Part - Odette": ItemDefinition(50073, ItemClassification.progression),
    "LP Part - Kilgore": ItemDefinition(50074, ItemClassification.progression),
    "LP Part - Shuraba": ItemDefinition(50075, ItemClassification.progression),
    # --- ACCESSORIES ---
    "Climax Brace": ItemDefinition(50101, ItemClassification.useful),
    "Eternal Testimony": ItemDefinition(50102, ItemClassification.useful),
    "Bracelet of Time": ItemDefinition(50103, ItemClassification.progression),
    "Evil Harvest Rosary": ItemDefinition(50104, ItemClassification.useful),
    "Gaze of Despair": ItemDefinition(50105, ItemClassification.useful),
    "Infernal Communicator": ItemDefinition(50106, ItemClassification.useful),
    "Moon of Mahaa-Kalaa": ItemDefinition(50107, ItemClassification.progression),
    "Pulley's Butterfly": ItemDefinition(50108, ItemClassification.useful),
    "Selene's Light": ItemDefinition(50109, ItemClassification.useful),
    "Star of Dineta": ItemDefinition(50110, ItemClassification.useful),
    "Sergey's Lover": ItemDefinition(50111, ItemClassification.useful),
    "Immortal Marionette": ItemDefinition(50112, ItemClassification.useful),
    # --- COLLECTIBLES ---
    "Broken Witch Heart": ItemDefinition(50200, ItemClassification.useful),
    "Witch Heart (Full)": ItemDefinition(50201, ItemClassification.useful),
    "Broken Moon Pearl": ItemDefinition(50202, ItemClassification.useful),
    "Moon Pearl (Full)": ItemDefinition(50203, ItemClassification.useful),
    # --- CONSUMABLES / FILLER ---
    "Green Herb Lollipop": ItemDefinition(50300, ItemClassification.filler),
    "Mega Green Herb Lollipop": ItemDefinition(50301, ItemClassification.filler),
    "Purple Magic Lollipop": ItemDefinition(50302, ItemClassification.filler),
    "Mega Purple Magic Lollipop": ItemDefinition(50303, ItemClassification.filler),
    "Bloody Rose Lollipop": ItemDefinition(50304, ItemClassification.filler),
    "Mega Bloody Rose Lollipop": ItemDefinition(50305, ItemClassification.filler),
    "Yellow Moon Lollipop": ItemDefinition(50306, ItemClassification.filler),
    "Mega Yellow Moon Lollipop": ItemDefinition(50307, ItemClassification.filler),
    "Magic Flute": ItemDefinition(50308, ItemClassification.filler),
    "Red Hot Shot": ItemDefinition(50309, ItemClassification.filler),
    # --- CRAFTING INGREDIENTS ---
    "Unicorn Horn x5": ItemDefinition(50320, ItemClassification.filler),
    "Unicorn Horn x10": ItemDefinition(50321, ItemClassification.filler),
    "Unicorn Horn x15": ItemDefinition(50322, ItemClassification.filler),
    "Baked Gecko x5": ItemDefinition(50323, ItemClassification.filler),
    "Baked Gecko x10": ItemDefinition(50324, ItemClassification.filler),
    "Baked Gecko x15": ItemDefinition(50325, ItemClassification.filler),
    "Mandragora Root x5": ItemDefinition(50326, ItemClassification.filler),
    "Mandragora Root x10": ItemDefinition(50327, ItemClassification.filler),
    "Mandragora Root x15": ItemDefinition(50328, ItemClassification.filler),
    # --- CURRENCY ---
    "Enzo's Pocket Change": ItemDefinition(50412, ItemClassification.filler),
    "1,000 Halos": ItemDefinition(50405, ItemClassification.filler),
    "2,500 Halos": ItemDefinition(50406, ItemClassification.filler),
    "5,000 Halos": ItemDefinition(50400, ItemClassification.filler),
    "7,500 Halos": ItemDefinition(50407, ItemClassification.filler),
    "10,000 Halos": ItemDefinition(50401, ItemClassification.filler),
    "20,000 Halos": ItemDefinition(50408, ItemClassification.filler),
    "25,000 Halos": ItemDefinition(50402, ItemClassification.filler),
    "30,000 Halos": ItemDefinition(50409, ItemClassification.filler),
    "50,000 Halos": ItemDefinition(50403, ItemClassification.filler),
    "75,000 Halos": ItemDefinition(50410, ItemClassification.filler),
    "100,000 Halos": ItemDefinition(50404, ItemClassification.filler),
    "200,000 Halos": ItemDefinition(50411, ItemClassification.filler),
    "300,000 Halos": ItemDefinition(50413, ItemClassification.filler),
    "500,000 Halos": ItemDefinition(50414, ItemClassification.filler),
    "1,000,000 Halos": ItemDefinition(50415, ItemClassification.filler),
    # --- TECHNIQUES ---
    "Technique - After Burner Kick": ItemDefinition(50500, ItemClassification.progression),
    "Technique - Air Dodge": ItemDefinition(50501, ItemClassification.progression),
    "Technique - Beast Within": ItemDefinition(50502, ItemClassification.progression),
    "Technique - Bat Within": ItemDefinition(50503, ItemClassification.progression),
    "Technique - Crow Within": ItemDefinition(50504, ItemClassification.progression),
    "Technique - Breakdance": ItemDefinition(50505, ItemClassification.useful),
    "Technique - Bullet Climax": ItemDefinition(50506, ItemClassification.useful),
    "Technique - Heel Slide": ItemDefinition(50507, ItemClassification.useful),
    "Technique - Heel Stomp": ItemDefinition(50508, ItemClassification.useful),
    "Technique - Stiletto": ItemDefinition(50509, ItemClassification.useful),
    "Technique - Tetsuzanko": ItemDefinition(50510, ItemClassification.useful),
    "Technique - Umbran Portal Kick": ItemDefinition(50512, ItemClassification.progression),
    "Technique - Umbran Spear": ItemDefinition(50513, ItemClassification.progression),
    "Technique - Witch Twist": ItemDefinition(50514, ItemClassification.progression),
    "Witch Time": ItemDefinition(50515, ItemClassification.progression),
    # --- TRAPS ---
    "Pickpocket Trap": ItemDefinition(50600, ItemClassification.trap),
    "Bloodletting Trap": ItemDefinition(50601, ItemClassification.trap),
    "Fragile Witch Trap": ItemDefinition(50602, ItemClassification.trap),
    "Magic Drain Trap": ItemDefinition(50603, ItemClassification.trap),
    "Amnesia Trap": ItemDefinition(50604, ItemClassification.trap),
    "Sticky Fingers Trap": ItemDefinition(50605, ItemClassification.trap),
    "Squish Trap": ItemDefinition(50606, ItemClassification.trap),
    "Angel Ambush Trap": ItemDefinition(50607, ItemClassification.trap),
    "Grace & Glory Trap": ItemDefinition(50608, ItemClassification.trap),
    "Nemesis Trap": ItemDefinition(50609, ItemClassification.trap),
    "Alfheim Curse Trap": ItemDefinition(50610, ItemClassification.trap),
    "Berserk Trap": ItemDefinition(50611, ItemClassification.trap),
    "Gracious & Glorious Trap": ItemDefinition(50612, ItemClassification.trap),
    "Fairness & Fearless Trap": ItemDefinition(50613, ItemClassification.trap),
    "Squash Trap": ItemDefinition(50614, ItemClassification.trap),
    # --- MACGUFFIN ---
    "Memory Fragment": ItemDefinition(50700, ItemClassification.progression_skip_balancing),
    # --- COMMUNITY CUSTOM COMBAT ITEMS ---
    "Punch": ItemDefinition(50800, ItemClassification.progression),
    "Kick": ItemDefinition(50801, ItemClassification.progression),
    "Torture Attacks": ItemDefinition(50803, ItemClassification.progression),
    "Angel Arms": ItemDefinition(50804, ItemClassification.progression),
    # --- CHAPTER UNLOCKS ---
    "Chapter 1 Unlock": ItemDefinition(51001, ItemClassification.progression),
    "Chapter 2 Unlock": ItemDefinition(51002, ItemClassification.progression),
    "Chapter 3 Unlock": ItemDefinition(51003, ItemClassification.progression),
    "Chapter 4 Unlock": ItemDefinition(51004, ItemClassification.progression),
    "Chapter 5 Unlock": ItemDefinition(51005, ItemClassification.progression),
    "Chapter 6 Unlock": ItemDefinition(51006, ItemClassification.progression),
    "Chapter 7 Unlock": ItemDefinition(51007, ItemClassification.progression),
    "Chapter 8 Unlock": ItemDefinition(51008, ItemClassification.progression),
    "Chapter 9 Unlock": ItemDefinition(51009, ItemClassification.progression),
    "Chapter 10 Unlock": ItemDefinition(51010, ItemClassification.progression),
    "Chapter 11 Unlock": ItemDefinition(51011, ItemClassification.progression),
    "Chapter 12 Unlock": ItemDefinition(51012, ItemClassification.progression),
    "Chapter 13 Unlock": ItemDefinition(51013, ItemClassification.progression),
    "Chapter 14 Unlock": ItemDefinition(51014, ItemClassification.progression),
    "Chapter 15 Unlock": ItemDefinition(51015, ItemClassification.progression),
    "Chapter 16 Unlock": ItemDefinition(51016, ItemClassification.progression),
    "Requiem Unlock": ItemDefinition(51017, ItemClassification.progression),
}


def _names_in_range(lo: int, hi: int):
    return {name for name, data in item_table.items() if lo <= data.id <= hi}


WEAPON_NAMES = _names_in_range(50001, 50011)
LP_NAMES = _names_in_range(50050, 50055)
LP_PART_NAMES = _names_in_range(50070, 50075)
ACCESSORY_NAMES = _names_in_range(50101, 50112)
COLLECTIBLE_NAMES = _names_in_range(50200, 50203)
CONSUMABLE_NAMES = _names_in_range(50300, 50309)
CRAFTING_NAMES = _names_in_range(50320, 50328)
HALO_NAMES = _names_in_range(50400, 50415)
TECHNIQUE_NAMES = _names_in_range(50500, 50515)
CHAPTER_UNLOCK_NAMES = sorted(_names_in_range(51001, 51017), key=lambda n: item_table[n].id)
TRAP_NAMES = _names_in_range(50600, 50649)
CUSTOM_COMBAT_NAMES = _names_in_range(50800, 50804)
FILLER_ITEM_NAMES = [
    name for name, data in item_table.items()
    if data.classification == ItemClassification.filler
]
UNPLACED_ITEM_NAMES = {"Witch Heart (Full)", "Moon Pearl (Full)"}
MACGUFFIN_NAME = "Memory Fragment"