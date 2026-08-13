from typing import NamedTuple


class LocationDefinition(NamedTuple):
    id: int


location_table = {
    # --- GATES OF HELL: weapons/accessories ---
    "Shop: Buy Onyx Roses": LocationDefinition(60001),
    "Shop: Buy Shuraba": LocationDefinition(60002),
    "Shop: Buy Kulshedra": LocationDefinition(60003),
    "Shop: Buy Durga": LocationDefinition(60004),
    "Shop: Buy Odette": LocationDefinition(60005),
    "Shop: Buy Kilgore": LocationDefinition(60006),
    "Shop: Moon of Mahaa-Kalaa": LocationDefinition(60007),
    "Shop: Pulley's Butterfly": LocationDefinition(60008),
    "Shop: Evil Harvest Rosary": LocationDefinition(60012),
    "Shop: Gaze of Despair": LocationDefinition(60013),
    "Shop: Infernal Communicator": LocationDefinition(60014),
    "Shop: Selene's Light": LocationDefinition(60015),
    "Shop: Star of Dineta": LocationDefinition(60016),
    "Shop: Sergey's Lover": LocationDefinition(60017),
    "Shop: Immortal Marionette": LocationDefinition(60018),

    # --- ALFHEIM PORTALS (21 total) ---
    "Chapter I: Alfheim 1":        LocationDefinition(60101),
    "Chapter II: Alfheim 1":       LocationDefinition(60102),
    "Chapter II: Alfheim 2":       LocationDefinition(60103),
    "Chapter II: Alfheim 3":       LocationDefinition(60104),
    "Chapter III: Alfheim 1":      LocationDefinition(60105),
    "Chapter III: Alfheim 2":      LocationDefinition(60106),
    "Chapter III: Alfheim 3":      LocationDefinition(60107),
    "Chapter III: Alfheim 4":      LocationDefinition(60108),
    "Chapter V: Alfheim 1":        LocationDefinition(60109),
    "Chapter V: Alfheim 2":        LocationDefinition(60110),
    "Chapter V: Alfheim 3":        LocationDefinition(60111),
    "Chapter VI: Alfheim 1":       LocationDefinition(60112),
    "Chapter IX: Alfheim 1":       LocationDefinition(60113),
    "Chapter IX: Alfheim 2":       LocationDefinition(60114),
    "Chapter IX: Alfheim 3":       LocationDefinition(60115),
    "Chapter X: Alfheim 1":        LocationDefinition(60116),
    "Chapter X: Alfheim 2":        LocationDefinition(60117),
    "Chapter XII: Alfheim 1":      LocationDefinition(60118),
    "Chapter XII: Alfheim 2":      LocationDefinition(60119),
    "Chapter XV: Alfheim 1":       LocationDefinition(60120),
    "Requiem: Alfheim 1":          LocationDefinition(60121),
}

TECHNIQUE_SHOP = [
    "After Burner Kick", "Air Dodge", "Bat Within",
    "Crow Within", "Breakdance", "Heel Slide",
    "Heel Stomp", "Stiletto", "Tetsuzanko",
    "Umbran Portal Kick", "Umbran Spear", "Witch Twist",
]
for i, tech in enumerate(TECHNIQUE_SHOP):
    location_table[f"Learn {tech}"] = LocationDefinition(60201 + i)

TECHNIQUE_LOCATION_NAMES = {f"Learn {tech}" for tech in TECHNIQUE_SHOP}

CHEST_LOCATIONS = {
    "Chapter I: Chest - Verse 1":          LocationDefinition(60301),
    "Chapter I: Chest - Verse 4 (Gazebo)": LocationDefinition(60302),
    "Chapter II: Chest - Verse 5 #1": LocationDefinition(60303),
    "Chapter II: Chest - Verse 5 #2": LocationDefinition(60304),
    "Chapter II: Chest - Verse 5 #3": LocationDefinition(60305),
    "Chapter II: Chest - Verse 10":    LocationDefinition(60306),
    "Chapter III: Chest - Verse 2":      LocationDefinition(60307),
    "Chapter III: Chest - Verse 5 #1":   LocationDefinition(60308),
    "Chapter III: Chest - Verse 5 #2":   LocationDefinition(60309),
    "Chapter III: Chest - Verse 8":      LocationDefinition(60310),
    "Chapter III: Chest - Verse 13 #1":  LocationDefinition(60311),
    "Chapter III: Chest - Verse 13 #2":  LocationDefinition(60312),
    "Chapter V: Chest - Verse 6 #1":   LocationDefinition(60313),
    "Chapter V: Chest - Verse 6 #2":   LocationDefinition(60314),
    "Chapter V: Chest - Verse 11":    LocationDefinition(60315),
    "Chapter V: Chest - Verse 12":    LocationDefinition(60316),
    "Chapter VI: Chest - Verse 1 #1": LocationDefinition(60317),
    "Chapter VI: Chest - Verse 1 #2": LocationDefinition(60318),
    "Chapter VI: Chest - Verse 2":    LocationDefinition(60319),
    "Chapter VI: Chest - Verse 10":   LocationDefinition(60320),
    "Chapter IX: Chest - Verse 1":      LocationDefinition(60321),
    "Chapter IX: Chest - Verse 2":      LocationDefinition(60322),
    "Chapter IX: Chest - Verse 5 #1":   LocationDefinition(60323),
    "Chapter IX: Chest - Verse 5 #2":   LocationDefinition(60324),
    "Chapter IX: Chest - Verse 5 #3":   LocationDefinition(60325),
    "Chapter IX: Chest - Verse 5 #4":   LocationDefinition(60326),
    "Chapter IX: Chest - Verse 5 #5":   LocationDefinition(60327),
    "Chapter IX: Chest - Verse 6":      LocationDefinition(60328),
    "Chapter IX: Chest - Verse 7":      LocationDefinition(60329),
    "Chapter IX: Chest - Verse 10 #1":  LocationDefinition(60330),
    "Chapter IX: Chest - Verse 10 #2":  LocationDefinition(60331),
    "Chapter X: Chest - Verse 2":    LocationDefinition(60332),
    "Chapter X: Chest - Verse 3":    LocationDefinition(60333),
    "Chapter X: Chest - Verse 8 #1": LocationDefinition(60334),
    "Chapter X: Chest - Verse 8 #2": LocationDefinition(60335),
    "Chapter X: Chest - Verse 10":   LocationDefinition(60336),
    "Chapter XII: Chest - Verse 1 #1": LocationDefinition(60337),
    "Chapter XII: Chest - Verse 1 #2": LocationDefinition(60338),
    "Chapter XII: Chest - Verse 1 #3": LocationDefinition(60339),
    "Chapter XII: Chest - Verse 3":    LocationDefinition(60340),
    "Chapter XV: Chest - Verse 1":      LocationDefinition(60341),
    "Chapter XV: Chest - Verse 11 #1":  LocationDefinition(60342),
    "Chapter XV: Chest - Verse 11 #2":  LocationDefinition(60343),
    "Chapter XV: Chest - Verse 13 #1":  LocationDefinition(60344),
    "Chapter XV: Chest - Verse 13 #2":  LocationDefinition(60345),
}
location_table.update(CHEST_LOCATIONS)
CHEST_LOCATION_NAMES = set(CHEST_LOCATIONS)

CHAPTER_DATA = [
    ("Chapter I",    1,  "Chapter 1 Unlock",  "The Angel's Metropolis",           8),
    ("Chapter II",   2,  "Chapter 2 Unlock",  "Vigrid, City of Déjà Vu",   10),
    ("Chapter III",  3,  "Chapter 3 Unlock",  "The Burning Ground",                 13),
    ("Chapter IV",   4,  "Chapter 4 Unlock",  "The Cardinal Virtue of Fortitude",    1),
    ("Chapter V",    5,  "Chapter 5 Unlock",  "The Lost Holy Grounds",              14),
    ("Chapter VI",   6,  "Chapter 6 Unlock",  "The Gates of Paradise",              10),
    ("Chapter VII",  7,  "Chapter 7 Unlock",  "The Cardinal Virtue of Temperance",   1),
    ("Chapter VIII", 8,  "Chapter 8 Unlock",  "Route 666",                           5),
    ("Chapter IX",   9,  "Chapter 9 Unlock",  "Paradiso - A Remembrance of Time",    11),
    ("Chapter X",    10, "Chapter 10 Unlock", "Paradiso - A Sea of Stars",           11),
    ("Chapter XI",   11, "Chapter 11 Unlock", "The Cardinal Virtue of Justice",      1),
    ("Chapter XII",  12, "Chapter 12 Unlock", "The Broken Sky",                     10),
    ("Chapter XIII", 13, "Chapter 13 Unlock", "The Cardinal Virtue of Prudence",     1),
    ("Chapter XIV",  14, "Chapter 14 Unlock", "Isla Del Sol",                        3),
    ("Chapter XV",   15, "Chapter 15 Unlock", "A Tower to Truth",                  14),
    ("Chapter XVI",  16, "Chapter 16 Unlock", "The Lumen Sage",                     1),
    ("Requiem",      17, "Requiem Unlock",    "The Witch Hunts",                     7),
]

CHAPTERS = [(key, num, unlock) for (key, num, unlock, _title, _vc) in CHAPTER_DATA]

PROLOGUE_VERSES = 2
PROLOGUE_KEY = "Prologue"
REQUIEM_KEY = "Requiem"

BOSS_CHAPTER_KEYS = [
    "Chapter IV", "Chapter VII", "Chapter XI",
    "Chapter XIII", "Chapter XIV", "Chapter XVI",
]

CHAPTER_TITLES = {key: title for (key, _num, _unlock, title, _vc) in CHAPTER_DATA}
CHAPTER_VERSE_COUNTS = {key: vc for (key, _num, _unlock, _title, vc) in CHAPTER_DATA}
CHAPTER_LOCATIONS = {}


def _chapter_prefix(chapter_key: str) -> str:
    return f"{chapter_key}: {CHAPTER_TITLES[chapter_key]}"


_prologue_names = []
for pv in range(1, PROLOGUE_VERSES + 1):
    _loc = f"{PROLOGUE_KEY} - Verse {pv}"
    location_table[_loc] = LocationDefinition(610000 + pv)
    _prologue_names.append(_loc)
location_table[f"{PROLOGUE_KEY} - Complete"] = LocationDefinition(610099)
_prologue_names.append(f"{PROLOGUE_KEY} - Complete")
CHAPTER_LOCATIONS[PROLOGUE_KEY] = _prologue_names

for chap_key, chap_num, _unlock, _title, verse_count in CHAPTER_DATA:
    names = []
    base = 610000 + chap_num * 100
    prefix = _chapter_prefix(chap_key)
    for v in range(1, verse_count + 1):
        loc_name = f"{prefix} - Verse {v}"
        location_table[loc_name] = LocationDefinition(base + v)
        names.append(loc_name)
    complete_name = f"{prefix} - Complete"
    location_table[complete_name] = LocationDefinition(base + 99)
    names.append(complete_name)
    CHAPTER_LOCATIONS[chap_key] = names

# Define rank options structures
RANK_TIERS_ANY_MEDAL = {"Any Medal": 0}
RANK_TIERS_ALL_MEDALS = {
    "Bronze+": 0,
    "Silver+": 2000,
    "Gold+": 4000,
    "Platinum+": 6000,
    "Pure Platinum": 8000
}

RANK_LOCATION_NAMES = set()

# Populate location table with BOTH sets dynamically so IDs exist safely
for rank_name, offset in RANK_TIERS_ANY_MEDAL.items():
    for pv in range(1, PROLOGUE_VERSES + 1):
        location_table[f"{PROLOGUE_KEY} - Verse {pv} Rank ({rank_name})"] = LocationDefinition(620000 + offset + pv)
    for chap_key, chap_num, _unlock, _title, verse_count in CHAPTER_DATA:
        prefix = _chapter_prefix(chap_key)
        base = 620000 + offset + (chap_num * 100)
        for v in range(1, verse_count + 1):
            location_table[f"{prefix} - Verse {v} Rank ({rank_name})"] = LocationDefinition(base + v)

for rank_name, offset in RANK_TIERS_ALL_MEDALS.items():
    for pv in range(1, PROLOGUE_VERSES + 1):
        loc_name = f"{PROLOGUE_KEY} - Verse {pv} Rank ({rank_name})"
        location_table[loc_name] = LocationDefinition(620500 + offset + pv) # Offset block to avoid collision with Any Medal
        RANK_LOCATION_NAMES.add(loc_name)
    for chap_key, chap_num, _unlock, _title, verse_count in CHAPTER_DATA:
        prefix = _chapter_prefix(chap_key)
        base = 620500 + offset + (chap_num * 100)
        for v in range(1, verse_count + 1):
            loc_name = f"{prefix} - Verse {v} Rank ({rank_name})"
            location_table[loc_name] = LocationDefinition(base + v)
            RANK_LOCATION_NAMES.add(loc_name)

ANY_RANK_LOCATION_NAMES = {f"{PROLOGue if False else PROLOGUE_KEY} - Verse {pv} Rank (Any Medal)" for pv in range(1, PROLOGUE_VERSES + 1)} # simplified check builder
# Add any-medal names properly
ANY_RANK_LOCATION_NAMES = set()
for pv in range(1, PROLOGUE_VERSES + 1):
    ANY_RANK_LOCATION_NAMES.add(f"{PROLOGUE_KEY} - Verse {pv} Rank (Any Medal)")
for chap_key, _num, _un, _ti, vc in CHAPTER_DATA:
    pre = _chapter_prefix(chap_key)
    for v in range(1, vc + 1):
        ANY_RANK_LOCATION_NAMES.add(f"{pre} - Verse {v} Rank (Any Medal)")

RANK_LOCATION_NAMES_ALL = RANK_LOCATION_NAMES | ANY_RANK_LOCATION_NAMES

# --- Alfheim pairing ---
ALFHEIM_VERSES = [
    ("Chapter I",   3), ("Chapter II",  5), ("Chapter II",  6), ("Chapter II",  8),
    ("Chapter III", 2), ("Chapter III", 5), ("Chapter III", 10), ("Chapter III", 11),
    ("Chapter V",   3), ("Chapter V",   8), ("Chapter V",   10), ("Chapter VI",  4),
    ("Chapter IX",  2), ("Chapter IX",  7), ("Chapter IX",  11), ("Chapter X",   3),
    ("Chapter X",   9), ("Chapter XII", 4), ("Chapter XII", 7), ("Chapter XV",  9),
    ("Requiem",     5),
]

ALFHEIM_PORTAL_NAMES = {name for name in location_table if "Alfheim" in name}
ALFHEIM_PAIRED_NAMES = set()
for _chap_key, _v in ALFHEIM_VERSES:
    _prefix = _chapter_prefix(_chap_key)
    ALFHEIM_PAIRED_NAMES.add(f"{_prefix} - Verse {_v}")
    for rank_name in {**RANK_TIERS_ANY_MEDAL, **RANK_TIERS_ALL_MEDALS}:
        ALFHEIM_PAIRED_NAMES.add(f"{_prefix} - Verse {_v} Rank ({rank_name})")

ALFHEIM_ALL_NAMES = ALFHEIM_PORTAL_NAMES | ALFHEIM_PAIRED_NAMES

# --- Umbran Tears of Blood locations ---
TEAR_LOCATION_NAMES = set()
tears_data = [
    ("Chapter I: Tear of Blood - Verse 4 (Normal)", 630001),
    ("Chapter I: Tear of Blood - Verse 8 (Normal)", 630002),
    ("Chapter II: Tear of Blood - Verse 3 (Normal)", 630003),
    ("Chapter II: Tear of Blood - Verse 10 (Normal)", 630004),
    ("Chapter III: Tear of Blood - Verse 2 (Normal)", 630005),
    ("Chapter III: Tear of Blood - Verse 13 (Normal)", 630006),
    ("Chapter V: Tear of Blood - Verse 5 (Normal)", 630007),
    ("Chapter V: Tear of Blood - Verse 12 (Normal)", 630008),
    ("Chapter VI: Tear of Blood - Verse 1 (Normal)", 630009),
    ("Chapter VI: Tear of Blood - Verse 7 (Normal)", 630010),
    ("Chapter IX: Tear of Blood - Verse 1 (Normal)", 630011),
    ("Chapter IX: Tear of Blood - Verse 5-6 (Normal)", 630012),
    ("Chapter IX: Tear of Blood - Verse 10-11 (Normal)", 630013),
    ("Chapter X: Tear of Blood - Verse 3 (Normal)", 630014),
    ("Chapter X: Tear of Blood - Verse 8 (Normal)", 630015),
    ("Chapter XII: Tear of Blood - Verse 1 (Normal)", 630016),
    ("Chapter XV: Tear of Blood - Verse 1 (Normal)", 630017),
    ("Chapter XV: Tear of Blood - Verse 11 (Normal)", 630018),
]
for t_name, t_id in tears_data:
    location_table[t_name] = LocationDefinition(t_id)
    TEAR_LOCATION_NAMES.add(t_name)

for loc_name in location_table:
    if (": Alfheim " in loc_name or ": Chest" in loc_name or ": Tear of Blood " in loc_name):
        parent = loc_name.split(":", 1)[0]
        if parent in CHAPTER_LOCATIONS:
            CHAPTER_LOCATIONS[parent].append(loc_name)
        elif loc_name.startswith("Prologue"):
            CHAPTER_LOCATIONS[PROLOGUE_KEY].append(loc_name)

def chapter_complete_location(chapter_key: str) -> str:
    if chapter_key == PROLOGUE_KEY:
        return f"{PROLOGUE_KEY} - Complete"
    return f"{_chapter_prefix(chapter_key)} - Complete"

def chapter_clear_event(chapter_key: str) -> str:
    return f"{chapter_key} Clear"

def chapter_clear_event_location(chapter_key: str) -> str:
    return f"{chapter_key} - Cleared"

ALL_CHAPTER_KEYS = [PROLOGUE_KEY] + [key for key, _n, _u in CHAPTERS]
location_name_groups = {
    "Alfheims": ALFHEIM_ALL_NAMES
}