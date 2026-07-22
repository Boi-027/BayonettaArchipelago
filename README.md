# Bayonetta Archipelago Randomizer

A custom [Archipelago](https://archipelago.gg) client for **Bayonetta 1 (PC)**.
It integrates with **BayoHook** (a `dinput8.dll` proxy) to send and receive
location checks and items in real time during a multiworld randomizer run.

## Features

### Location Checks
- All 21 Alfheim Portals
- 17 Chapters (completion checks)
- All Verses (per-chapter)
- All Weapons (via LP turn-in detection)
- All Accessories (shop purchases)
- All Techniques (shop purchases)
- Consumables and crafting ingredients
- Halos (multiple denominations: 1,000–200,000)

### Client Features
- **On-Screen Notifications** — Color-coded popups for items, checks, deaths,
  hints, and completions. Always visible, no key press needed.
- **Shop Auto-Scouting** — Entering the Gates of Hell automatically reveals what
  items are at each shop location without using hint points.
- **Chapter-Blocking Enforcement** — Only AP-unlocked chapters appear in the
  Chapter Select menu.
- **DeathLink Support** — Send and receive deaths with other players.
- **Built-in Tracker** — Grouped by chapter with natural sort, real location
  names, and live progress updates.
- **In-Game Console** — Full chat, hint commands, and AP interaction.
- **Local Send Logging** — All checks and items log in the same format as the
  Archipelago website.

## Roadmap

### Done
- AP client core (connect, slot auth, items, checks, chat, hints, tracker UI)
- On-screen notification system with color-coded events
- Shop auto-scouting (LocationScouts without hint points)
- Chapter unlocks as items (+ in-game chapter blocking hook)
- Chapter completion locations
- Verse completion locations
- Alfheim locations
- Weapons as items (grant + ownership bits)
- LP locations (turn-in detection sends checks)
- Techniques as items (incl. Panther/Crow/Bat Within, purchase detection)
- Accessories as items (+ purchase locations)
- Consumables, Halos, Witch Heart & Moon Pearl fragments as items
- Max HP/MP scaling with received fragments
- DeathLink (send + receive) with verse completion isolation
- Save-flag memory layout reverse engineered (per-chapter flag lanes, static addresses)
- In-DLL tooling for flag discovery (guard-page access tracer + bit-flip logger)
- Expanded filler item pool (22 halo denominations, 36 crafting/consumable variants)
- Debug toggles for isolating system issues (For dev purposes)
- Chest (Umbran Resting Place) locations
- More Goal Options
- Release All Checks when beating Goal
- Traps
- Full Shop-Purchase Location Coverage

### In Progress
- Verse Platinum Rank as Locations

### To Do
- Witch Heart / Moon Pearl World-pickup Locations
- Umbran Tears of Blood Locations
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
3. YAML options (include/exclude toggles for weapons, techniques, accessories,
   etc.) are now fully functional as of v1.2.

## Playing

1. Start Bayonetta.
2. Open BayoHook's UI (**Delete** key).
3. Go to the **Archipelago → Connect** tab, enter your server details, and connect.
4. Start a new game (or load your randomizer save) and play. Checks fire
   automatically as you complete verses and chapters and make shop purchases.
5. Notifications appear in the top-left corner even when the menu is closed.
6. Visit the Gates of Hell to auto-scout shop items — no hint points needed!

## Troubleshooting

- **DLL doesn't load:** Make sure you have the Visual C++ 2010 redistributable
  installed.
- **Newly acquired chapters don't appear:** Back out of the Chapter Select menu
  and re-enter it.
- **"Unknown ID" in the tracker:** The APWorld needs regenerating with the latest
  `.py` files. Please open an Issue with the Unknown ID it shows.
- **Game process stays open after closing:** If you encounter this, let me know —
  it should be fixed in v1.2. Check in Task Manager to see if its still open.
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
