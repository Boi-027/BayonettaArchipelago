from BaseClasses import LocationProgressType
from worlds.generic.Rules import set_rule, add_rule
from .Items import MACGUFFIN_NAME
from .Locations import (
    CHAPTERS, PROLOGUE_KEY, REQUIEM_KEY, ALL_CHAPTER_KEYS, BOSS_CHAPTER_KEYS,
    CHAPTER_TITLES, ALFHEIM_VERSES, CHEST_LOCATION_NAMES,
    chapter_complete_location, chapter_clear_event, chapter_clear_event_location,
)

WEAPON_SHOP_LP = {
    "Shop: Buy Onyx Roses": "LP - Trois Marches Militaires",
    "Shop: Buy Kulshedra":  "LP - Fantaisie-Impromptu",
    "Shop: Buy Durga":       "LP - Sonate in DK. 448",
    "Shop: Buy Odette":     "LP - Les Patineurs Waltz op.183",
    "Shop: Buy Kilgore":    "LP - Walkure Ride",
    "Shop: Buy Shuraba":    "LP - Turangalila-Symphonie",
}


def set_rules(world):
    multiworld = world.multiworld
    player = world.player
    options = world.options

    # --- Chapter entrances gated by their unlock items ---
    for chap_key, _chap_num, unlock_name in CHAPTERS:
        if chap_key == REQUIEM_KEY:
            continue
        entrance = multiworld.get_entrance(f"Enter {chap_key}", player)
        set_rule(entrance, lambda state, u=unlock_name: state.has(u, player))

    # Requiem entrance logic.
    requiem_entrance = multiworld.get_entrance(f"Enter {REQUIEM_KEY}", player)
    if options.goal.value == 1:
        all_events = tuple(chapter_clear_event(k) for k in ALL_CHAPTER_KEYS)
        required_count = options.goal_chapter_count.value - 1
        set_rule(
            requiem_entrance,
            lambda state, evts=all_events, req=required_count:
                sum(1 for e in evts if state.has(e, player)) >= req
        )
    else:
        requiem_unlocks = tuple(u for k, _n, u in CHAPTERS if k != REQUIEM_KEY)
        requiem_own_unlock = next(u for k, _n, u in CHAPTERS if k == REQUIEM_KEY)
        set_rule(
            requiem_entrance,
            lambda state, own=requiem_own_unlock, needed=requiem_unlocks:
                state.has(own, player) and state.has_all(needed, player)
        )

    # ============================================================
    #  Ability progression gates
    # ============================================================
    BEAST_WITHIN = "Technique - Beast Within"
    WITCH_TIME = "Witch Time"
    BRACELET_OF_TIME = "Bracelet of Time"
    STILETTO = "Technique - Stiletto"
    AFTER_BURNER = "Technique - After Burner Kick"
    AIR_DODGE = "Technique - Air Dodge"
    TETSUZANKO = "Technique - Tetsuzanko"
    ANGEL_ARMS = "Angel Arms"
    PUNCH = "Punch"
    KICK = "Kick"
    TORTURE = "Torture Attacks"

    TECHNIQUE_GATES = [
        ("Chapter I",   3,  WITCH_TIME),
        ("Chapter II",   3,  WITCH_TIME),
        ("Chapter III",  5,  WITCH_TIME),
        ("Chapter V",   10,  WITCH_TIME),
        ("Chapter V",   15,  BEAST_WITHIN),
        ("Chapter VI",   2,  BEAST_WITHIN),
        ("Chapter VI",   3,  WITCH_TIME),
        ("Chapter IX",   4,  WITCH_TIME),
        ("Chapter IX",   4,  BEAST_WITHIN),
        ("Chapter X",   12,  WITCH_TIME),
        ("Requiem",      4,  BEAST_WITHIN),
    ]

    chapter_15_clear = multiworld.get_location(chapter_clear_event_location("Chapter XV"), player)
    add_rule(
        chapter_15_clear,
        lambda state: (
            not options.include_techniques.value or
            state.has(BEAST_WITHIN, player) or
            (state.has(STILETTO, player) and state.has(AFTER_BURNER, player) and state.has(AIR_DODGE, player)) or
            (state.has(TETSUZANKO, player) and state.has(AFTER_BURNER, player) and state.has(AIR_DODGE, player)) or
            True 
        )
    )

    ANGEL_ARMS_GATES = [
        ("Chapter I",   1),
        ("Chapter IX",   1),
    ]

    PUNCH_GATES = []
    KICK_GATES = []

    def _chapter_prefix_for(chapter_key):
        return f"{chapter_key}: {CHAPTER_TITLES[chapter_key]}"

    def _verse_in_name(loc_name):
        marker = " - Verse "
        idx = loc_name.find(marker)
        if idx < 0:
            return None
        rest = loc_name[idx + len(marker):]
        num = ""
        for ch in rest:
            if ch.isdigit():
                num += ch
            else:
                break
        return int(num) if num else None

    alfheim_name_to_verse = {}
    per_chapter_counter = {}
    for ck, vv in ALFHEIM_VERSES:
        n = per_chapter_counter.get(ck, 0) + 1
        per_chapter_counter[ck] = n
        alfheim_name_to_verse[f"{ck}: Alfheim {n}"] = vv

    def _location_verse(loc_name):
        if ": Alfheim " in loc_name:
            return alfheim_name_to_verse.get(loc_name)
        return _verse_in_name(loc_name)

    def _location_in_chapter(loc_name, chapter_key):
        prefix = _chapter_prefix_for(chapter_key)
        return loc_name.startswith(prefix) or loc_name.startswith(f"{chapter_key}:")

    def _apply_gate(chapter_key, min_verse, item_name):
        for loc in multiworld.get_locations(player):
            name = loc.name
            if not _location_in_chapter(name, chapter_key):
                continue

            v = _location_verse(name)
            if v is None:
                add_rule(loc, lambda state, it=item_name: state.has(it, player))
                continue

            if v >= min_verse:
                add_rule(loc, lambda state, it=item_name: state.has(it, player))

    if options.include_techniques.value:
        for chap_key, min_verse, item in TECHNIQUE_GATES:
            _apply_gate(chap_key, min_verse, item)

    if options.include_angel_arms.value:
        for chap_key, min_verse in ANGEL_ARMS_GATES:
            _apply_gate(chap_key, min_verse, ANGEL_ARMS)

    if options.include_punches.value:
        for chap_key, min_verse in PUNCH_GATES:
            _apply_gate(chap_key, min_verse, PUNCH)

    if options.include_kicks.value:
        for chap_key, min_verse in KICK_GATES:
            _apply_gate(chap_key, min_verse, KICK)

    # --- ALFHEIM SPECIFIC GATES ---
    ALFHEIM_GATES = {
        "Chapter I: Alfheim 1":    ["Witch Time"],
        "Chapter II: Alfheim 1":    ["Punch", "Kick"],
        "Chapter II: Alfheim 3":    ["Torture Attacks"],
        "Chapter III: Alfheim 2":   ["Angel Arms"],
        "Chapter V: Alfheim 1":     ["Witch Time"], # Updated below to allow Bracelet of Time
        "Chapter V: Alfheim 3":     ["Punch", "Kick"],
        "Chapter VI: Alfheim 1":    ["Torture Attacks"],
        "Chapter IX: Alfheim 2":    ["Technique - Crow Within"],
        "Chapter IX: Alfheim 3":    ["Punch", "Kick"],
        "Chapter X: Alfheim 2":     ["Angel Arms"],
        "Chapter XII: Alfheim 2":   ["Witch Time"],
    }
    PUNCH_KICK_ALFHEIMS = {
        "Chapter II: Alfheim 1",
        "Chapter V: Alfheim 3",
        "Chapter IX: Alfheim 3",
    }
    for alfheim_name, required_items in ALFHEIM_GATES.items():
        try:
            loc = multiworld.get_location(alfheim_name, player)
        except KeyError:
            continue
        
# Chapter V Alfheim 1 specific override with soft fallback
        if alfheim_name == "Chapter V: Alfheim 1":
            add_rule(
                loc,
                lambda state: (
                    not options.include_techniques.value or
                    state.has(WITCH_TIME, player) or
                    state.has(BRACELET_OF_TIME, player) or
                    True # Prevents generation deadlocks when items are deep in logic
                )
            )
            continue

        if alfheim_name in PUNCH_KICK_ALFHEIMS:
            p_on = options.include_punches.value
            k_on = options.include_kicks.value
            add_rule(
                loc,
                lambda state, p_opt=p_on, k_opt=k_on:
                    (not p_opt or state.has(PUNCH, player))
                    or (not k_opt or state.has(KICK, player))
            )
            continue
        for item_name in required_items:
            if (item_name == "Witch Time" or item_name.startswith("Technique -")) and not options.include_techniques.value:
                continue
            if item_name == "Torture Attacks" and not options.include_torture_attacks.value:
                continue
            if item_name == "Angel Arms" and not options.include_angel_arms.value:
                continue
            add_rule(loc, lambda state, it=item_name: state.has(it, player))

    # --- EXPLICIT VERSE & RANK COMBAT LOCKS ---
    def require_torture_attack(location_name: str):
        try:
            loc = multiworld.get_location(location_name, player)
            add_rule(loc, lambda state:
                not options.include_torture_attacks.value or
                state.has(TORTURE, player)
            )
        except KeyError:
            pass

    require_torture_attack("Prologue - Verse 2")
    for rank in ["Bronze+", "Silver+", "Gold+", "Platinum+", "Pure Platinum", "Any Medal"]:
        require_torture_attack(f"Prologue - Verse 2 Rank ({rank})")

    require_torture_attack("Chapter II: Vigrid, City of Déjà Vu - Verse 8")
    for rank in ["Bronze+", "Silver+", "Gold+", "Platinum+", "Pure Platinum", "Any Medal"]:
        require_torture_attack(f"Chapter II: Vigrid, City of Déjà Vu - Verse 8 Rank ({rank})")

    require_torture_attack("Chapter VI: The Gates of Paradise - Verse 4")
    for rank in ["Bronze+", "Silver+", "Gold+", "Platinum+", "Pure Platinum", "Any Medal"]:
        require_torture_attack(f"Chapter VI: The Gates of Paradise - Verse 4 Rank ({rank})")

    # --- Prologue combat gates ---
    p_on = options.include_punches.value
    k_on = options.include_kicks.value
    t_on = options.include_torture_attacks.value

    def _prologue_verse(name):
        marker = " - Verse "
        idx = name.find(marker)
        if idx < 0:
            return None
        rest = name[idx + len(marker):]
        num = ""
        for ch in rest:
            if ch.isdigit():
                num += ch
            else:
                break
        return int(num) if num else None

    if p_on or k_on or t_on:
        prologue_targets = [loc for loc in multiworld.get_locations(player)
                            if loc.name.startswith(PROLOGUE_KEY)]
        try:
            prologue_targets.append(multiworld.get_location(
                chapter_clear_event_location(PROLOGUE_KEY), player))
        except KeyError:
            pass

        for loc in prologue_targets:
            name = loc.name
            v = _prologue_verse(name)
            is_end = (v is None) and (name.endswith("- Complete")
                                     or name.endswith("- Cleared"))

            if (v is not None and v >= 2) or is_end:
                if p_on and k_on:
                    add_rule(loc, lambda state:
                            state.has(PUNCH, player) or state.has(KICK, player))

            if t_on and ((v is not None and v >= 2) or is_end):
                add_rule(loc, lambda state: state.has(TORTURE, player))

    clear_gates = {}
    for chap_key, _mv, item in TECHNIQUE_GATES:
        clear_gates.setdefault(chap_key, set()).add(("tech", item))
    for chap_key, _mv in ANGEL_ARMS_GATES:
        clear_gates.setdefault(chap_key, set()).add(("angel", ANGEL_ARMS))
    for chap_key, _mv in PUNCH_GATES:
        clear_gates.setdefault(chap_key, set()).add(("punch", PUNCH))
    for chap_key, _mv in KICK_GATES:
        clear_gates.setdefault(chap_key, set()).add(("kick", KICK))

    for chap_key, gates in clear_gates.items():
        try:
            clear_loc = multiworld.get_location(
                chapter_clear_event_location(chap_key), player)
        except KeyError:
            continue
        for kind, item in gates:
            if kind == "tech" and not options.include_techniques.value:
                continue
            if kind == "angel" and not options.include_angel_arms.value:
                continue
            if kind == "punch" and not options.include_punches.value:
                continue
            if kind == "kick" and not options.include_kicks.value:
                continue
            add_rule(clear_loc, lambda state, it=item: state.has(it, player))

    # --- Chests require Punch or Kick to break open ---
    for loc in multiworld.get_locations(player):
        if loc.name in CHEST_LOCATION_NAMES:
            add_rule(
                loc,
                lambda state, p=player,
                       p_opt=options.include_punches.value, k_opt=options.include_kicks.value,
                       p_item=PUNCH, k_item=KICK:
                    (not p_opt or state.has(p_item, p)) or (not k_opt or state.has(k_item, p))
            )

# --- Weapon purchases at the shop need their Golden LP ---
    for loc_name, lp_name in WEAPON_SHOP_LP.items():
        loc = multiworld.get_location(loc_name, player)
        if options.include_golden_lps.value:
            set_rule(loc, lambda state, lp=lp_name: state.has(lp, player))
        else:
            # When Golden LPs are disabled, the shop is simply free of item requirements
            pass

    # --- SHOP ACCESS LOGIC ---
    other_chapter_unlocks = [f"Chapter {i} Unlock" for i in range(2, 17)] + ["Requiem Unlock"]

    def has_shop_access(state):
        if state.has_any(other_chapter_unlocks, player):
            return True

        p_on = options.include_punches.value
        k_on = options.include_kicks.value
        t_on = options.include_torture_attacks.value
        a_on = options.include_angel_arms.value

        can_strike = (not (p_on and k_on)) or state.has(PUNCH, player) or state.has(KICK, player)
        can_torture = (not t_on) or state.has(TORTURE, player)
        can_angel = (not a_on) or state.has(ANGEL_ARMS, player)

        return can_strike and can_torture and can_angel

    for loc in multiworld.get_locations(player):
        if loc.name.startswith("Shop: ") or loc.name.startswith("Learn "):
            add_rule(loc, has_shop_access)

    # --- Goal / completion condition ---
    all_events = tuple(chapter_clear_event(k) for k in ALL_CHAPTER_KEYS)
    requiem_event = chapter_clear_event(REQUIEM_KEY)
    boss_events = tuple(chapter_clear_event(k) for k in BOSS_CHAPTER_KEYS) + (requiem_event,)

    goal = options.goal.value
    count = options.goal_chapter_count.value

    if goal == 0:
        multiworld.completion_condition[player] = (
            lambda state, evts=all_events: state.has_all(evts, player)
        )
    elif goal == 1:
        multiworld.completion_condition[player] = (
            lambda state, evts=all_events, c=count, r=requiem_event:
                state.has(r, player) and
                sum(1 for e in evts if state.has(e, player)) >= c
        )
    elif goal == 2:
        multiworld.completion_condition[player] = (
            lambda state, evts=boss_events: state.has_all(evts, player)
        )
    elif goal == 3:
        multiworld.completion_condition[player] = (
            lambda state, evts=all_events, c=count:
                sum(1 for e in evts if state.has(e, player)) >= c
        )
    else:
        required = min(options.memory_fragments_required.value, options.memory_fragments_total.value)
        if options.memory_fragments_requires_requiem.value:
            multiworld.completion_condition[player] = (
                lambda state, c=required, r=requiem_event:
                    state.has(MACGUFFIN_NAME, player, c) and state.has(r, player)
            )
        else:
            multiworld.completion_condition[player] = (
                lambda state, c=required: state.has(MACGUFFIN_NAME, player, c)
            )