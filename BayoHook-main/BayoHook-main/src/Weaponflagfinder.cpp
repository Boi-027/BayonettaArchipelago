// ============================================================================
// WeaponFlagFinder - a targeted memory-diff helper to locate the weapon
// OWNERSHIP flag(s) that the in-game weapon wheel reads.
//
// WHY THIS INSTEAD OF CHEAT ENGINE:
// CE value-scans scan the whole process, so a change to ~11-19 ownership bytes
// drowns in tens of thousands of coincidental changes (score, timers, physics).
// This helper snapshots ONLY a chosen region, then after you change exactly one
// thing in-game (own one more weapon), reports precisely which bytes changed in
// that region - a clean, tiny, controlled diff.
//
// HOW TO USE (from your ImGui Archipelago/Debug tab - hook up two buttons):
//   1. With a weapon UNOWNED, click "Snapshot".  -> WeaponFinder_Snapshot()
//   2. In-game, OWN exactly one more weapon (buy it in the Gates of Hell, or use
//      the telephone-booth unlock for ONE weapon if possible). Change nothing
//      else - stand still in a menu so timers/score aren't moving.
//   3. Click "Diff".                              -> WeaponFinder_Diff()
//   4. Read BayoHook_Archipelago.log: it lists every byte that changed, its old
//      and new value, and the absolute address. An ownership flag shows up as a
//      byte going 0 -> 1 (or a bit being set). Because you changed only one
//      weapon in a static scene, the list should be short.
//
// Then repeat owning a DIFFERENT weapon and diff again: if a DIFFERENT nearby
// address flips each time, you've found a per-weapon ownership table; if the
// SAME byte changes bits, it's a bitfield. Either way the address is revealed.
//
// WHICH REGION TO SCAN:
// The confirmed live inventory/save block is around 0x5AA74xx (magic, halos,
// consumables, heart/pearl counts). Weapon ownership is plausibly in the wider
// save block. We scan a broad window by default; narrow it once you see hits.
// You can also point it at any region you suspect.
// ============================================================================

#include <windows.h>
#include <vector>
#include <cstdint>
#include <string>
#include <fstream>
#include <ctime>

namespace WeaponFinder {

    // ---- configure the region to watch ----
    // Default: a broad window over the save-data block. Adjust START/SIZE if you
    // want to widen or narrow. Keep SIZE reasonable (a few KB) so the diff is fast
    // and the log stays readable.
    static uintptr_t g_scanStart = 0x5AA7000;   // a bit before the known inventory block
    static size_t    g_scanSize = 0x1000;       // 4 KB window (covers 0x5AA7000-0x5AA8000)

    static std::vector<uint8_t> g_snapshot;      // captured bytes
    static bool g_haveSnapshot = false;

    static void Log(const std::string& msg) {
        std::time_t t = std::time(nullptr);
        char buf[16];
        std::tm tmBuf;
        localtime_s(&tmBuf, &t);
        std::strftime(buf, sizeof(buf), "%H:%M:%S", &tmBuf);
        std::ofstream f("BayoHook_Archipelago.log", std::ios::app);
        if (f.is_open()) f << "[" << buf << "] [WeaponFinder] " << msg << "\n";
    }

    // Safely read the configured region into a buffer. Uses ReadProcessMemory so a
    // bad page doesn't crash us (returns false for unreadable spans).
    static bool ReadRegion(std::vector<uint8_t>& out) {
        out.assign(g_scanSize, 0);
        SIZE_T read = 0;
        // Read in one shot; if it fails (e.g. part of the range is unmapped), fall
        // back to page-by-page so we capture what we can.
        if (ReadProcessMemory(GetCurrentProcess(), (LPCVOID)g_scanStart,
            out.data(), g_scanSize, &read) && read == g_scanSize) {
            return true;
        }
        // Page-by-page fallback.
        const size_t page = 0x1000;
        bool any = false;
        for (size_t off = 0; off < g_scanSize; off += page) {
            size_t chunk = (g_scanSize - off < page) ? (g_scanSize - off) : page;
            SIZE_T r = 0;
            if (ReadProcessMemory(GetCurrentProcess(), (LPCVOID)(g_scanStart + off),
                out.data() + off, chunk, &r) && r > 0) {
                any = true;
            }
        }
        return any;
    }

    // Snapshot the region. Call this with the weapon UNOWNED.
    void Snapshot() {
        if (!ReadRegion(g_snapshot)) {
            Log("Snapshot failed - couldn't read the region.");
            g_haveSnapshot = false;
            return;
        }
        g_haveSnapshot = true;
        char msg[128];
        sprintf_s(msg, "Snapshot taken: 0x%zX bytes at 0x%zX. Now own ONE weapon, then Diff.",
            g_scanSize, (size_t)g_scanStart);
        Log(msg);
    }

    // Compare current memory to the snapshot and log every changed byte. Call this
    // AFTER owning exactly one more weapon.
    void Diff() {
        if (!g_haveSnapshot) {
            Log("Diff: no snapshot yet - click Snapshot first (with the weapon unowned).");
            return;
        }
        std::vector<uint8_t> now;
        if (!ReadRegion(now)) {
            Log("Diff failed - couldn't read the region.");
            return;
        }

        int changes = 0;
        for (size_t i = 0; i < g_scanSize && i < now.size() && i < g_snapshot.size(); ++i) {
            if (now[i] != g_snapshot[i]) {
                uintptr_t addr = g_scanStart + i;
                char msg[160];
                sprintf_s(msg, "CHANGED 0x%zX : %u -> %u  (delta %+d)",
                    (size_t)addr, (unsigned)g_snapshot[i], (unsigned)now[i],
                    (int)now[i] - (int)g_snapshot[i]);
                Log(msg);
                changes++;
                if (changes >= 200) { // safety cap so the log isn't flooded
                    Log("... (200+ changes; region too noisy - narrow g_scanSize or "
                        "do this in a static menu so nothing else moves)");
                    break;
                }
            }
        }
        char summary[128];
        sprintf_s(summary, "Diff complete: %d changed byte(s). A 0->1 flip is a likely "
            "ownership flag.", changes);
        Log(summary);

        // Re-snapshot so you can immediately own the NEXT weapon and diff again to
        // map the whole table.
        g_snapshot = std::move(now);
        Log("Re-snapshotted current state - own the next weapon and Diff again to "
            "find its flag (should be an adjacent address).");
    }

    // Optional: point the scanner at a different region at runtime.
    void SetRegion(uintptr_t start, size_t size) {
        g_scanStart = start;
        g_scanSize = size;
        g_haveSnapshot = false;
        char msg[128];
        sprintf_s(msg, "Region set to 0x%zX size 0x%zX. Snapshot again.",
            (size_t)start, size);
        Log(msg);
    }

    // ---- Weapon ownership bitfield decoder ----
    // The ownership field is a packed bitmask starting at 0x5AA7459: each set bit =
    // one owned weapon. This reads a few bytes there and logs exactly which GLOBAL
    // bit indices are set (bit 0 = 0x7459 bit0, bit 8 = 0x745A bit0, etc.), so you
    // don't have to do binary math. Workflow to map weapons:
    //   1. Fresh save. Click "WF: Decode" -> logs current owned bits (baseline).
    //   2. Buy/unlock ONE weapon. Click "WF: Decode" again.
    //   3. The newly-appearing bit index = that weapon's bit. Note it.
    //   4. Repeat for each weapon to build the full bit->weapon map.
    // With ALL weapons unlocked (telephone booth), one Decode shows the full width.
    static constexpr uintptr_t OWNERSHIP_FIELD_ADDR = 0x5AA7459;
    static constexpr size_t    OWNERSHIP_FIELD_BYTES = 4; // covers up to 32 weapons

    void DecodeOwnership() {
        uint8_t bytes[OWNERSHIP_FIELD_BYTES] = { 0 };
        SIZE_T read = 0;
        if (!ReadProcessMemory(GetCurrentProcess(), (LPCVOID)OWNERSHIP_FIELD_ADDR,
            bytes, OWNERSHIP_FIELD_BYTES, &read) || read != OWNERSHIP_FIELD_BYTES) {
            Log("Decode failed - couldn't read the ownership field.");
            return;
        }

        // Build a readable "bits set: a, b, c" list and the raw bytes.
        std::string bitList;
        for (size_t b = 0; b < OWNERSHIP_FIELD_BYTES; ++b) {
            for (int bit = 0; bit < 8; ++bit) {
                if (bytes[b] & (1 << bit)) {
                    int globalBit = (int)(b * 8) + bit;
                    if (!bitList.empty()) bitList += ", ";
                    bitList += std::to_string(globalBit);
                }
            }
        }
        char raw[96];
        sprintf_s(raw, "Ownership @0x%zX raw bytes: %02X %02X %02X %02X",
            (size_t)OWNERSHIP_FIELD_ADDR, bytes[0], bytes[1], bytes[2], bytes[3]);
        Log(raw);
        Log(std::string("Ownership bits set: ") + (bitList.empty() ? "(none)" : bitList));
    }

} // namespace WeaponFinder