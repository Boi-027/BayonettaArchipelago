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

## Requirements

- **Bayonetta 1 (Steam version)**
- **BayoHook** — the trainer/hook DLL (`dinput8.dll`). This client is built as
  a module inside BayoHook.
- An **Archipelago server** hosting a multiworld session.

## Installation (pre-compiled DLL)

1. Download the latest `dinput8.dll` from the
   [Releases](https://github.com/Boi-027/BayonettaArchipelago/releases) page.
2. Place it in your Bayonetta game folder (the one containing `Bayonetta.exe`).
3. Launch the game — BayoHook and the Archipelago client load automatically.

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

- Built on top of **BayoHook** by SSSiyan, berthrage, and others.
- Archipelago integration and chapter-blocking by **Boi**.
