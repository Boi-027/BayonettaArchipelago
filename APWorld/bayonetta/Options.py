from Options import Choice, Toggle, DefaultOnToggle, Range, OptionDict, PerGameCommonOptions
from dataclasses import dataclass


# ============================================================
#  Goal options
# ============================================================
class Goal(Choice):
    """The goal required to complete the game. Set Goal Chapter Count to choose
    how many chapters the count-based goals need.
    0 - All Chapters: Complete all 18 chapters (Prologue + I-XVI + Requiem)
    1 - Chapter Count: Complete a set number of chapters AND Requiem
    2 - Boss Rush: Complete the boss chapters (IV, VII, XI, XIII, XIV, XVI) + Requiem
    3 - Chapter Count (No Requiem): Complete X chapters, Requiem not required
    4 - MacGuffin Hunt: Collect enough Memory Fragments from the item pool
    """
    display_name = "Goal"
    default = 0
    option_all_chapters = 0
    option_chapter_count = 1
    option_boss_rush = 2
    option_chapter_count_no_requiem = 3
    option_macguffin_hunt = 4


class GoalChapterCount(Range):
    """Number of chapters required when using a chapter-count goal.
    The Prologue and Requiem both count toward this number."""
    display_name = "Goal Chapter Count"
    range_start = 1
    range_end = 17
    default = 8


class MacguffinsRequired(Range):
    """Memory Fragments needed to win the MacGuffin Hunt goal.
    Clamped down to the number actually placed if you asked for more Eyes
    than the item pool could hold. Accepts weighted YAML values (e.g.
    "10: 30, 20: 50, random-range-1-100: 20")."""
    display_name = "Macguffins Required"
    range_start = 1
    range_end = 100
    default = 10


class MacguffinsTotal(Range):
    """Memory Fragments placed in the item pool for the MacGuffin Hunt goal.
    If the pool runs out of space, no more Eyes will be added - remaining
    space keeps its usual filler mix. Extras beyond the required count give
    slack in where you find them. Accepts weighted YAML values (e.g.
    "15: 30, 30: 50, random-range-1-100: 20")."""
    display_name = "Macguffins Total"
    range_start = 1
    range_end = 100
    default = 15


class MacGuffinRequiresRequiem(DefaultOnToggle):
    """Require completing Requiem to finish the MacGuffin Hunt goal."""
    display_name = "MacGuffin Hunt Requires Requiem"


class VerseRankTarget(Choice):
    """Add bonus locations for verses when you achieve target ranks.
    This is CUMULATIVE: if you set this to Gold, every verse will have
    three separate checks placed in the world (Bronze, Silver, and Gold)
    that all fire as you reach those ranks.
    - disabled: no rank locations added (default)
    - any: adds 1 check per verse (Bronze/Stone or higher)
    - silver_plus: adds 2 checks per verse (Bronze, Silver)
    - gold_plus: adds 3 checks per verse (Bronze, Silver, Gold)
    - platinum_plus: adds 4 checks per verse (Bronze, Silver, Gold, Platinum)
    - pure_platinum: adds 5 checks per verse (All ranks up to Pure Platinum)
    """
    display_name = "Verse Rank Target"
    default = 0
    option_disabled = 0
    option_any = 1
    option_silver_plus = 2
    option_gold_plus = 3
    option_platinum_plus = 4
    option_pure_platinum = 5


class StartingChapter(Range):
    """Chapter you start with unlocked. Only this chapter's unlock is precollected."""
    display_name = "Starting Chapter"
    range_start = 1
    range_end = 16
    default = 1


# ============================================================
#  Link options (multiplayer interaction)
# ============================================================
class DeathLink(Toggle):
    """Enable Death Link – when you die, everyone dies. When they die, you die."""
    display_name = "Death Link"


class DamageLink(Toggle):
    """Enable Damage Link – shared pain. Damage you take is partially shared with
    other Damage Link players. Never lethal on its own."""
    display_name = "Damage Link"


class TrapLink(Toggle):
    """Enable Trap Link – traps you receive fire for other Trap Link players, and
    their traps fire on you."""
    display_name = "Trap Link"


class RingLink(Toggle):
    """Enable Ring Link – Halos you collect are shared as rings (1 ring = 100 Halos).
    Shop purchases are not shared."""
    display_name = "Ring Link"


# ============================================================
#  Content toggles (item / location pools)
# ============================================================
class IncludeWeapons(DefaultOnToggle):
    """Include weapon items (Scarborough Fair, Onyx Roses, etc.) in the item pool."""
    display_name = "Include Weapons"


class IncludeTechniques(DefaultOnToggle):
    """Include technique items (After Burner Kick, Bat Within, Witch Time, etc.)
    in the item pool."""
    display_name = "Include Techniques"


class IncludeAccessories(DefaultOnToggle):
    """Include accessory items (Moon of Mahaa-Kalaa, Pulley's Butterfly, etc.) in the item pool."""
    display_name = "Include Accessories"


class IncludeConsumables(DefaultOnToggle):
    """Include lollipops, herbs, magic flutes, etc. in the item pool."""
    display_name = "Include Consumables"


class IncludeCraftingCompounds(DefaultOnToggle):
    """Include Unicorn Horns, Baked Geckos, and Mandragora Roots in the item pool."""
    display_name = "Include Crafting Compounds"


class IncludeHalos(DefaultOnToggle):
    """Include Halo bundles in the item pool."""
    display_name = "Include Halos"


class IncludeGoldenLPs(DefaultOnToggle):
    """Include Golden LP records in the item pool. Each LP unlocks one weapon
    purchase at the Gates of Hell."""
    display_name = "Include Golden LPs"


class IncludeChests(DefaultOnToggle):
    """Include treasure chest locations (45 chests across 9 chapters)."""
    display_name = "Include Chests"


class IncludeTears(DefaultOnToggle):
    """Include Umbran Tears of Blood locations (ONLY WORKS IN NORMAL MODE)."""
    display_name = "Include Tears"
    default = False


class IncludePunches(DefaultOnToggle):
    """Include Punch items in the item pool.
    WARNING: When enabled, starting a new game means Punches will be locked
    until you find the item."""
    display_name = "Include Punches"


class IncludeKicks(DefaultOnToggle):
    """Include Kick items in the item pool.
    WARNING: When enabled, starting a new game means Kicks will be locked
    until you find the item."""
    display_name = "Include Kicks"


class IncludeTortureAttacks(DefaultOnToggle):
    """Include Torture Attacks in the item pool."""
    display_name = "Include Torture Attacks"


class IncludeAngelArms(DefaultOnToggle):
    """Include Angel Arms in the item pool."""
    display_name = "Include Angel Arms"


class TrapPercentage(Range):
    """Percentage of filler items to replace with traps. 0 = no traps, 100 = all fillers become traps."""
    display_name = "Trap Percentage"
    range_start = 0
    range_end = 100
    default = 0


class TrapWeights(OptionDict):
    """Which traps can appear, and how likely each is relative to the others.
    Set a trap to 0 to turn it off entirely.
    Values are relative weights, NOT percentages: a trap at 20 is twice as
    likely as one at 10, and the weights do not need to add up to anything
    in particular. Only matters when Trap Percentage is above 0.
    If every trap is set to 0, no traps are placed regardless of
    Trap Percentage, and those slots keep their usual filler mix."""
    display_name = "Trap Weights"
    default = {
        "Pickpocket Trap": 10,
        "Bloodletting Trap": 10,
        "Fragile Witch Trap": 10,
        "Magic Drain Trap": 10,
        "Amnesia Trap": 10,
        "Sticky Fingers Trap": 10,
        "Squish Trap": 10,
        "Squash Trap": 10,
        "Angel Ambush Trap": 10,
        "Grace & Glory Trap": 10,
        "Nemesis Trap": 10,
        "Alfheim Curse Trap": 10,
        "Berserk Trap": 10,
        "Gracious & Glorious Trap": 10,
        
    }


# ============================================================
#  Miscellaneous options
# ============================================================
class PrologueVisible(DefaultOnToggle):
    """Keep the Prologue visible in the Chapter Select menu (normally hidden until completed)."""
    display_name = "Prologue Visible"


class ChapterBlocking(DefaultOnToggle):
    """Only show AP-unlocked chapters in the Chapter Select menu."""
    display_name = "Chapter Blocking"


# ============================================================
#  Main option dataclass
# ============================================================
@dataclass
class BayonettaOptions(PerGameCommonOptions):
    # Goal
    goal:                 Goal
    goal_chapter_count:   GoalChapterCount
    macguffins_required:  MacguffinsRequired
    macguffins_total:     MacguffinsTotal
    macguffin_requires_requiem: MacGuffinRequiresRequiem
    verse_rank_target:    VerseRankTarget
    starting_chapter:     StartingChapter
    # Links
    death_link:           DeathLink
    damage_link:          DamageLink
    trap_link:            TrapLink
    ring_link:            RingLink
    # Content
    include_weapons:      IncludeWeapons
    include_techniques:   IncludeTechniques
    include_accessories:  IncludeAccessories
    include_consumables:  IncludeConsumables
    include_crafting:     IncludeCraftingCompounds
    include_halos:        IncludeHalos
    include_golden_lps:   IncludeGoldenLPs
    include_chests:       IncludeChests
    include_tears:        IncludeTears
    include_punches:      IncludePunches
    include_kicks:        IncludeKicks
    include_torture_attacks: IncludeTortureAttacks
    include_angel_arms:   IncludeAngelArms
    trap_percentage:      TrapPercentage
    trap_weights:         TrapWeights
    # Misc
    prologue_visible:     PrologueVisible
    chapter_blocking:     ChapterBlocking