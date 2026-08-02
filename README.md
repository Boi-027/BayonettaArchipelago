# Bayonetta Archipelago Randomizer

A custom Archipelago (https://archipelago.gg) client for **Bayonetta 1 (PC)**.
It integrates with **BayoHook** (a `dinput8.dll` proxy) to send and receive
location checks and items in real time during a multiworld randomizer run, as well as real-time **P2P Co-op multiplayer** player syncing.

---

## Features

### Real-Time Steam Co-op (Experimental)
- **P2P Player Syncing:** Connect directly with another player over Steam networking to see each other in real-time within the game world.
- **Host & Join Rooms:** Easily create or join a co-op room via 64-bit Room IDs directly inside the BayoHook UI.
- **Live Puppet Telemetry:** Syncs 3D coordinates (X, Y, Z), character rotation, move IDs, move parts, and animation frames frame-by-frame.

### Location Checks
- All 21 Alfheim Portals
- 18 Chapters (Prologue + I–XVI + Requiem completion checks)
- All Verses (per-chapter, over 200 locations)
- 45 Treasure Chests (Umbran Resting Places)
- 18 Umbran Tears of Blood (Normal difficulty, opt-in)
- Optional Verse Rank bonus locations (medal-based, configurable grade)
- All Weapons (via LP turn-in / instant grant detection at the Gates of Hell)
- All Accessories (shop purchase checks with dynamic shop transition handling)
- All Techniques (shop purchase checks with edge-triggering detection)
- Consumables, crafting ingredients, and Halos (12 denominations from 137 to 200,000)

### Goal Options (set in YAML)
- **All Chapters** — Complete all 18 chapters (Prologue + I–XVI + Requiem)
- **Chapter Count** — Complete a set number of chapters (Requiem can be one of them)
- **Boss Rush** — Complete only the boss chapters (IV, VII, XI, XIII, XIV, XVI) + Requiem
- **Chapter Count (No Requiem)** — Complete X chapters, Requiem not required
- **MacGuffin Hunt** — Collect enough Eyes of the World Fragments from the item pool

### Traps (15)
Junk checks can bite back. The full trap pool:

- **Pickpocket** — Steals up to 10,000 Halos.
- **Bloodletting** — Halves your current HP (never kills).
- **Fragile Witch** — Sets you to 1 HP. Don't get hit.
- **Magic Drain** — Empties your magic gauge.
- **Amnesia** — Forgets a random technique you own for 30 seconds, then gives it back.
- **Sticky Fingers** — Swipes up to 3 of one random consumable.
- **Squish** — Bayonetta gets squished for 20 seconds (makes you slippery and heavy).
- **Squash** — Bayonetta gets flattened for 3 seconds (as if Golem squashed you).
- **Angel Ambush** — Three random rank-and-file angels spawn on you.
- **Grace & Glory** — The claw duo joins your fight.
- **Nemesis** — Enemy Jeanne hunts you down.
- **Alfheim Curse** — You can only survive three more hits before being reduced to 1 HP.
- **Berserk** — Every active enemy instantly becomes enraged.
- **Gracious & Glorious** — The upgraded claw duo joins the fight. Faster, meaner, and definitely not Grace & Glory.
- **Fairness & Fearless** — Spawns everyone's favorite Fire and Lightning Duo!

A `trap_percentage` YAML option controls how much of the filler pool becomes traps.

### Multiplayer Link Protocols
- **Death Link** — Shared death. When they die, you die. When you die, they die.
- **Damage Link** — Shared pain. Damage taken is partially sent to other players. Never lethal alone.
- **Trap Link** — Shared traps. Your traps fire on others, theirs fire on you.
- **Ring Link** — Shared Halos. Halos you collect become rings for others (1 ring = 100 Halos). Shop purchases excluded.

### Client Features
- **On-Screen Notifications** — Color-coded popups for items, checks, deaths, hints, and completions. Always visible, no key press needed.
- **Shop Auto-Scouting** — Entering the Gates of Hell automatically reveals what items are at each shop location without using hint points.
- **Edge-Triggered Shop Checks** — Real-time purchase detection for Techniques and Alt Weapons so location checks send instantly upon purchase.
- **Dynamic LP-to-Weapon Mapping** — Automatically links LP item IDs to corresponding weapon shop check locations to ensure zero missed location checks on item receipt.
- **Source-Level Combat Filtering** — Locked combat categories (Punches, Kicks, Torture Attacks, Angel Arms, Double Jump) are safely blocked at the engine level without crashing mid-combo. Captures advanced composite combo strings (e.g., PPKP) while leaving standard opener punches available.
- **Chapter-Blocking Enforcement** — Only AP-unlocked chapters appear in the Chapter Select menu.
- **Crash Diagnostics & Organized Logs** — Automatically generates minidump (`.dmp`) files on crashes and organizes all session logs into a dedicated `Archipelago Logs/` folder next to `Bayonetta.exe`.
- **Built-in Tracker** — Grouped by chapter with natural sort, real location names, live progress updates, and a "GO Mode" indicator when you're one step from victory.
- **Website-Style Colored Console** — Items colored by classification, players tinted, per-category filtering, and detailed check skipping reasons.
- **In-Game Chat & Hint Commands** — Full Archipelago chat and hint system from the console tab.
- **Session Persistence** — Disconnect and reconnect without losing progress. Completed checks, granted items, and unlocked chapters survive disconnects.

---

## Roadmap

### Done
- AP client core (connect, slot auth, items, checks, chat, hints, tracker UI)
- Steamworks P2P Co-op integration (Lobby creation/joining, telemetry packet sync, player puppet rendering)
- On-screen notification system with color-coded events
- Shop auto-scouting (LocationScouts without hint points)
- Dynamic LP-to-Weapon location mapping for guaranteed check sending
- Edge-triggered shop check detection for Techniques and Alt Weapons
- Chapter unlocks as items (+ in-game chapter blocking hook)
- Chapter completion locations
- Verse completion locations
- Alfheim locations
- Treasure Chest locations (all 45 mapped and polling)
- Umbran Tears of Blood locations (18 Normal mapped and polling)
- Verse Rank (medal) bonus locations with configurable grade target
- Weapons as items (grant + ownership bits)
- LP locations (instant grant + check sending)
- Techniques as items (incl. Panther/Crow/Bat Within, purchase detection)
- Accessories as items (+ purchase locations)
- Consumables, Halos, Witch Heart & Moon Pearl fragments as items
- Max HP/MP scaling with received fragments
- DeathLink, Damage Link, Trap Link, Ring Link
- 15 trap items (health, magic, amnesia, consumable theft, enemy spawns, curses, squash)
- MacGuffin Hunt goal (Eyes of the World Fragments)
- Multiple goal options (All Chapters, Chapter Count, Boss Rush, No Requiem, MacGuffin)
- GO Mode indicator in tracker
- Session persistence across disconnects
- Website-style colored console with per-category filtering & explicit check skip messaging
- Weapon shop logic (Golden LP gating)
- Save-flag memory layout reverse engineered (per-chapter flag lanes, static addresses)
- In-DLL tooling for flag discovery (guard-page access tracer + bit-flip logger, ChestMapper, Rank Finder)
- Debug toggles for isolating system issues (for dev purposes)
- Witch Time as an unlockable item
- Source-level Move ID hook for safe punch/kick/torture/angel arm/double jump restrictions
- Advanced combo filtering (captures composite strings like PPKP while preserving basic opener punches)
- Unified Move ID hook resolving conflicts with BayoHook trainer features
- Shop transition state trick for Accessories & Techniques (allows shop checks for AP-granted items)
- Crash dump generator (`.dmp`) & organized `Archipelago Logs` directory

### In Progress
- Co-op expansion (Equipment, HP/MP sync, costume sync, enemy position/health sync)

### To Do
- Filter in-game tracker to show only "in logic" checks
- Restore native Rodin LP turn-in shop menu without item auto-deletion
- Fix cosmetic Torture Attack prompt & magic consumption when locked
- Weapon switching as an unlockable item
- APWorld package polish (logic, options, location/item pools) for submission

### Not Planned / Stretch
- Randomized interactables (keys, levers, Temporal Witch Power Statues, etc.)
- Witch Heart / Moon Pearl World-pickup Locations

---

## Requirements

- **Bayonetta 1 (Steam version)**
- **BayoHook** — the trainer/hook DLL (`dinput8.dll`). This client is built as a module inside BayoHook.
- **z.dll** — required dependency for BayoHook.
- An **Archipelago server** hosting a multiworld session.

### Highly Recommended Additions
For the most stable, crash-free experience, we strongly recommend installing these alongside the AP mod:
- **[Bayonetta 4GB Patch](https://github.com/oceaniccamred/Bayonetta-4GB-Patched-EXE)** — Highly recommended to prevent out-of-memory crashes during extended modded runs.
- **[Bayonetta Essentials](https://gamebanana.com/mods/533310)** — Community performance and stability enhancements.

---

## Installation

1. Download the latest `dinput8.dll` and `z.dll` from the [Releases](https://github.com/Boi-027/BayonettaArchipelago/releases) page.
2. Place them into your Bayonetta game folder (the one containing `Bayonetta.exe`).
3. Launch the game — BayoHook and the Archipelago client load automatically.
4. **Make sure to back up your saves before playing!**

## APWorld Setup

1. Place the `bayonetta.apworld` file into your Archipelago `custom_worlds` folder.
2. Generate your multiworld as usual — the Bayonetta world will now be available.
3. All YAML options (include/exclude toggles, goal settings, link protocols, trap percentage, etc.) are fully functional.

## Playing

1. Start Bayonetta.
2. Open BayoHook's UI (**Delete** or **Insert** key).
3. Go to the **Archipelago → Connect** tab, enter your server details, and connect.
4. Start a new game (or load your randomizer save) and play. Checks fire automatically as you complete verses, chapters, open chests, collect tears, and make shop purchases.
5. Notifications appear in the top-left corner even when the menu is closed.
6. Visit the Gates of Hell to auto-scout shop items — no hint points needed!

### Co-op Setup
1. Both players launch the game and open the BayoHook UI (**Delete** key).
2. Go to the **Archipelago → Co-op** tab.
3. Player 1 clicks **Create Room**, then clicks **Copy ID** to share the 64-bit Room ID with Player 2.
4. Player 2 pastes the ID into the **Room ID** box and clicks **Join Room**.
5. Once connected enter the same Chapter then go to the **Extra** tab, then click on **Spawn Player 2** (TEMPORARY)
6. Once you are both are in the same Chapter and Verse you will get to see each other in real-time!

---

## Troubleshooting

- **DLL doesn't load / Error `0xc0e90002`:**
  - Right-click `dinput8.dll` and `z.dll`, select **Properties**, check the **Unblock** box at the bottom, and click Apply.
  - Add your Bayonetta game folder to your antivirus / Windows Defender exclusion list.
  - If using Windows 11, disable Smart App Control or S-Mode if untrusted DLLs are blocked.
- **Visual C++ Error:** Make sure you have the [Visual C++ 2015-2022 redistributable](https://aka.ms/vc14/vc_redist.x86.exe) installed.
- **The GUI isn't loading:** Try disabling overlays like GeForce Experience, RivaTuner, MSI Afterburner, ReShade, etc.
- **Newly acquired chapters don't appear:** Back out of the Chapter Select menu and re-enter it.
- **Game Crashes:** Check the `Archipelago Logs/` folder located next to `Bayonetta.exe`. Send the `BayoHook_CrashDump.dmp` and `BayoHook_Archipelago.log` files when opening an issue.
- **"Unknown ID" in the tracker:** The APWorld needs regenerating with the latest `.py` files. Please open an Issue with the Unknown ID it shows.
- **Something else, or an idea for the project?** Open an Issue and let me know!

---

## Credits

- Built on top of [BayoHook](https://github.com/SSSiyan/BayoHook) by SSSiyan and others, used and redistributed with permission.
- Archipelago integration, P2P networking, and chapter-blocking by **Boi**.
- Huge thanks to the community for countless bug reports!

---

## AI Usage Disclosure

- The integration of Archipelago within BayoHook was built through hands-on reverse-engineering and manual C++ development.
- All core code — memory hooks, chapter-blocking, P2P telemetry sync, and debugging tools — was written by me through direct reverse-engineering and testing.
- I have used LLMs (Claude and DeepSeek) as a way to expand my ideas and for confirming small issues I've had:
  - Checking my math when working out memory addresses.
  - Making sure I understood how APWorld options are supposed to work.
  - Catching small mistakes that I really shouldn't have made.
- No AI-generated code was used in this project without being reviewed, tested in-game, and modified by me first.
