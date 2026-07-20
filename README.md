# Bayonetta Archipelago Randomizer

A custom [Archipelago](https://archipelago.gg) client for **Bayonetta 1 (PC)**.
It integrates with **BayoHook** (a `dinput8.dll` proxy) to send and receive
location checks and items in real time during a multiworld randomizer run.

## Features

Location checks across:

- All 21 Alfheim Portals
- 17 Chapters
- All Weapons
- All Accessories
- All Techniques
- All Consumables
- Halos
- All LPs

Plus:

- Chapter-blocking enforcement — only AP-unlocked chapters appear in the
  Chapter Select menu.
- DeathLink support (both directions).
- Built-in tracker (grouped by chapter, natural sort, real names).
- In-game console and hint system.

## Roadmap

### Done

- AP client core (connect, slot auth, items, checks, chat, hints, tracker UI)
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
- DeathLink (send + receive)
- Save-flag memory layout reverse engineered (per-chapter flag lanes, static addresses)
- In-DLL tooling for flag discovery (guard-page access tracer + bit-flip logger)

### In Progress

- Chest (Umbran Resting Place) locations — memory layout solved, collecting
  bit mappings via flip-logger playthrough (4/~44 mapped)

### To Do

- Witch Heart / Moon Pearl World-pickup Locations
- Umbran Tears of Blood Locations
- Verse Platinum Rank as Locations
- Full Shop-Purchase Location Coverage
- Witch Time as an unlockable item
- Weapon switching as an unlockable item
- More Goal Options
- Release All Checks when beating Goal.
- APWorld package polish (logic, options, location/item pools) for submission

### Not Planned / Stretch

- Randomized interactables (keys, levers, Temporal Witch Power Statues, ect)

## Requirements

- **Bayonetta 1 (Steam version)**
- **BayoHook** and **z.dll** — the trainer/hook DLL (`dinput8.dll`). This client is built as a module inside BayoHook.
- An **Archipelago server** hosting a multiworld session.

## Installation

1. Download the latest `dinput8.dll` and `z.dll` from the
   [Releases](https://github.com/Boi-027/BayonettaArchipelago/releases) page.
2. Place them into your Bayonetta game folder (the one containing `Bayonetta.exe`).
3. Launch the game — BayoHook and the Archipelago client load automatically.
4. Make sure to backup your saves before playing!

## APWorld setup

1. Place the `bayonetta.apworld` file into your Archipelago `custom_worlds` folder.
2. Generate your multiworld as usual — the Bayonetta world will now be available.

## Playing

1. Start Bayonetta.
2. Open BayoHook's UI (**Delete** key).
3. Go to the **Archipelago → Connect** tab, enter your server details, and connect.
4. Start a new game (or load your randomizer save) and play. Checks fire
   automatically as you complete verses and chapters and make shop purchases.

## Troubleshooting

- **DLL doesn't load:** make sure you have the Visual C++ 2010 redistributable installed.
- **Newly acquired chapters don't appear:** back out of the Chapter Select menu and re-enter it.
- **"Unknown ID" in the tracker:** the APWorld needs regenerating with the latest
  `.py` files. Please open an Issue with the Unknown ID it shows.
- **Something else, or an idea for the project?** I'm open to suggestions — open an
  Issue and let me know.

## Credits

- Built on top of [BayoHook](https://github.com/SSSiyan/BayoHook) by SSSiyan and others,
  used and redistributed with permission.
- Archipelago integration and chapter-blocking by **Boi**.
