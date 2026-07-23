# Bayonetta Archipelago Randomizer

A custom [Archipelago](https://archipelago.gg) client for **Bayonetta 1 (PC)**.
It integrates with **BayoHook** (a `dinput8.dll` proxy) to send and receive
location checks and items in real time during a multiworld randomizer run.

## Features

### Location Checks
- All 21 Alfheim Portals
- 18 Chapters (Prologue + I–XVI + Requiem completion checks)
- All Verses (per-chapter, over 200 locations)
- 45 Treasure Chests (Umbran Resting Places)
- 18 Umbran Tears of Blood (Normal difficulty, opt-in)
- Optional Verse Rank bonus locations (medal-based, configurable grade)
- All Weapons (via LP turn-in detection at the Gates of Hell)
- All Accessories (shop purchases)
- All Techniques (shop purchases)
- Consumables, crafting ingredients, and Halos (12 denominations from 137 to 200,000)

### Goal Options (set in YAML)
- **All Chapters** — Complete all 18 chapters (Prologue + I–XVI + Requiem)
- **Chapter Count** — Complete a set number of chapters (Requiem can be one of them)
- **Boss Rush** — Complete only the boss chapters (IV, VII, XI, XIII, XIV, XVI) + Requiem
- **Chapter Count (No Requiem)** — Complete X chapters, Requiem not required
- **MacGuffin Hunt** — Collect enough Eyes of the World Fragments from the item pool

### Traps (13)
Junk checks can bite back. The full trap pool:

- **Pickpocket** — Steals up to 10,000 Halos.
- **Bloodletting** — Halves your current HP (never kills).
- **Fragile Witch** — Sets you to 1 HP. Don't get hit.
- **Magic Drain** — Empties your magic gauge.
- **Amnesia** — Forgets a random technique you own for 30 seconds, then gives it back.
- **Sticky Fingers** — Swipes up to 3 of one random consumable.
- **Squish** — Bayonetta gets flattened for 20 seconds (makes you slippery and heavy).
- **Angel Ambush** — Three random rank-and-file angels spawn on you.
- **Grace & Glory** — The claw duo joins your fight.
- **Nemesis** — Enemy Jeanne hunts you down.
- **Alfheim Curse** — You can only survive three more hits before being reduced to 1 HP.
- **Berserk** — Every active enemy instantly becomes enraged.
- **Gracious & Glorious** — The upgraded claw duo joins the fight. Faster, meaner, and definitely not Grace & Glory.

A `trap_percentage` YAML option controls how much of the filler pool becomes traps.

### Multiplayer Link Protocols
- **Death Link** — Shared death. When they die, you die. When you die, they die.
- **Damage Link** — Shared pain. Damage taken is partially sent to other players. Never lethal alone.
- **Trap Link** — Shared traps. Your traps fire on others, theirs fire on you.
- **Ring Link** — Shared Halos. Halos you collect become rings for others (1 ring = 100 Halos). Shop purchases excluded.

### Client Features
- **On-Screen Notifications** — Color-coded popups for items, checks, deaths,
  hints, and completions. Always visible, no key press needed.
- **Shop Auto-Scouting** — Entering the Gates of Hell automatically reveals what
  items are at each shop location without using hint points.
- **Chapter-Blocking Enforcement** — Only AP-unlocked chapters appear in the
  Chapter Select menu.
- **Built-in Tracker** — Grouped by chapter with natural sort, real location
  names, live progress updates, and a "GO Mode" indicator when you're one step
  from victory.
- **Website-Style Colored Console** — Items colored by classification, players
  tinted, per-category filtering.
- **In-Game Chat & Hint Commands** — Full Archipelago chat and hint system
  from the console tab.
- **Session Persistence** — Disconnect and reconnect without losing progress.
  Completed checks, granted items, and unlocked chapters survive disconnects.

## Roadmap

### Done
- AP client core (connect, slot auth, items, checks, chat, hints, tracker UI)
- On-screen notification system with color-coded events
- Shop auto-scouting (LocationScouts without hint points)
- Chapter unlocks as items (+ in-game chapter blocking hook)
- Chapter completion locations
- Verse completion locations
- Alfheim locations
- Treasure Chest locations (all 45 mapped and polling)
- Umbran Tears of Blood locations (18 Normal mapped and polling)
- Verse Rank (medal) bonus locations with configurable grade target
- Weapons as items (grant + ownership bits)
- LP locations (turn-in detection sends checks)
- Techniques as items (incl. Panther/Crow/Bat Within, purchase detection)
- Accessories as items (+ purchase locations)
- Consumables, Halos, Witch Heart & Moon Pearl fragments as items
- Max HP/MP scaling with received fragments
- DeathLink, Damage Link, Trap Link, Ring Link
- 13 trap items (health, magic, amnesia, consumable theft, enemy spawns, curses)
- MacGuffin Hunt goal (Eyes of the World Fragments)
- Multiple goal options (All Chapters, Chapter Count, Boss Rush, No Requiem, MacGuffin)
- GO Mode indicator in tracker
- Session persistence across disconnects
- Website-style colored console with per-category filtering
- Weapon shop logic (Golden LP gating)
- Save-flag memory layout reverse engineered (per-chapter flag lanes, static addresses)
- In-DLL tooling for flag discovery (guard-page access tracer + bit-flip logger, ChestMapper, Rank Finder)
- Expanded filler item pool (22 halo denominations, 36 crafting/consumable variants)
- Debug toggles for isolating system issues (for dev purposes)

### In Progress
- N/A

### To Do
- Witch Heart / Moon Pearl World-pickup Locations
- Witch Time as an unlockable item
- Weapon switching as an unlockable item
- APWorld package polish (logic, options, location/item pools) for submission

### Not Planned / Stretch
- Randomized interactables (keys, levers, Temporal Witch Power Statues, etc.)

## Requirements

- **Bayonetta 1 (Steam version)**
- **BayoHook** — the trainer/hook DLL (`dinput8.dll`). This client is built as
  a module inside BayoHook.
- **z.dll** — required dependency for BayoHook.
- An **Archipelago server** hosting a multiworld session.

## Installation

1. Download the latest `dinput8.dll` and `z.dll` from the
   [Releases](https://github.com/Boi-027/BayonettaArchipelago/releases) page.
2. Place them into your Bayonetta game folder (the one containing `Bayonetta.exe`).
3. Launch the game — BayoHook and the Archipelago client load automatically.
4. **Make sure to backup your saves before playing!**

## APWorld Setup

1. Place the `bayonetta.apworld` file into your Archipelago `custom_worlds` folder.
2. Generate your multiworld as usual — the Bayonetta world will now be available.
3. All YAML options (include/exclude toggles, goal settings, link protocols,
   trap percentage, etc.) are fully functional.

## Playing

1. Start Bayonetta.
2. Open BayoHook's UI (**Delete** key).
3. Go to the **Archipelago → Connect** tab, enter your server details, and connect.
4. Start a new game (or load your randomizer save) and play. Checks fire
   automatically as you complete verses, chapters, open chests, collect tears,
   and make shop purchases.
5. Notifications appear in the top-left corner even when the menu is closed.
6. Visit the Gates of Hell to auto-scout shop items — no hint points needed!

## Troubleshooting

- **DLL doesn't load:** Make sure you have the Visual C++ 2010 redistributable
  installed.
- **Newly acquired chapters don't appear:** Back out of the Chapter Select menu
  and re-enter it.
- **"Unknown ID" in the tracker:** The APWorld needs regenerating with the latest
  `.py` files. Please open an Issue with the Unknown ID it shows.
- **Scroll bar stuck in menus:** This was fixed in v1.5. If you still see it,
  please open an Issue.
- **Something else, or an idea for the project?** I'm open to suggestions — open
  an Issue and let me know.

## Credits

- Built on top of [BayoHook](https://github.com/SSSiyan/BayoHook) by SSSiyan and
  others, used and redistributed with permission.
- Archipelago integration and chapter-blocking by **Boi**.
- Huge thanks to **dowlle** for detailed bug reports and apworld testing.

## AI Usage Disclosure

- The integration of Archipelago within BayoHook was built through hands-on
  reverse-engineering and manual C++ development.
- All core code — memory hooks, chapter-blocking, and debugging tools — was
  written by me through direct reverse-engineering and testing.
- I have used LLMs (Claude and Deepseek) as a way to expand my ideas and for
  confirming small issues I've had:
    - Checking my math when working out memory addresses.
    - Making sure I understood how APWorld options are supposed to work.
    - Catching small mistakes that I really shouldn't have made.
- No AI-generated code was used in this project without being reviewed, tested
  in-game, and modified by me first.
