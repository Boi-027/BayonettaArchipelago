// ============================================================================
// Archipelago.cpp - BayoHook Archipelago client
// ============================================================================
#include <windows.h>
#include <TlHelp32.h>    
#include <d3d9.h>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "ap_logo.h"        
#include "Archipelago.hpp"
#include "imgui/imgui.h"
#include "apclient.hpp"
#include <thread>
#include <mutex>
#include <atomic>
#include <queue>
#include <deque>
#include <list>
#include <vector>
#include <set>
#include <map>
#include <algorithm>
#include <string>
#include <fstream>
#include <chrono>
#include <ctime>
#include "GameHook.hpp" 
#include <DbgHelp.h>
#include "steam/steam_api.h"
#include "steam/isteammatchmaking.h"
#include "steam/isteamnetworking.h"
#include "steam/isteamclient.h" 
#include "steam/isteamfriends.h"
#include "steam/isteamuser.h"
#include <delayimp.h>
// --- STANDALONE DINPUT8 PROXY WRAPPER ---
#pragma comment(linker, "/EXPORT:DirectInput8Create=_DirectInput8Create@20")
extern "C" {
    HRESULT WINAPI DirectInput8Create(HINSTANCE hinst, DWORD dwVersion, REFIID riidltf, LPVOID* ppvOut, LPUNKNOWN punkOuter) {
        static HMODULE hRealDInput = nullptr;
        if (!hRealDInput) {
            char sysPath[MAX_PATH];
            GetSystemDirectoryA(sysPath, MAX_PATH);
            strcat_s(sysPath, "\\dinput8.dll");
            hRealDInput = LoadLibraryA(sysPath);
        }
        if (!hRealDInput) return E_FAIL;
        auto realCreate = (HRESULT(WINAPI*)(HINSTANCE, DWORD, REFIID, LPVOID*, LPUNKNOWN))
            GetProcAddress(hRealDInput, "DirectInput8Create");
        if (!realCreate) return E_FAIL;
        return realCreate(hinst, dwVersion, riidltf, ppvOut, punkOuter);
    }
}
// --- DELAY-LOAD HOOK: bind steam_api.dll to the copy the game already loaded ---
static FARPROC WINAPI BayoDelayLoadHook(unsigned dliNotify, PDelayLoadInfo pdli) {
    if (dliNotify == dliNotePreLoadLibrary) {
        if (pdli && pdli->szDll && _stricmp(pdli->szDll, "steam_api.dll") == 0) {
            HMODULE h = GetModuleHandleA("steam_api.dll");
            if (h) return (FARPROC)h;
        }
    }
    return nullptr;
}
extern "C" const PfnDliHook __pfnDliNotifyHook2 = BayoDelayLoadHook;
// --- STEAM INTERFACE HELPERS (static, delay-loaded) ---
static ISteamClient* GetBayoSteamClient() {
    return (ISteamClient*)SteamInternal_CreateInterface(STEAMCLIENT_INTERFACE_VERSION);
}
static ISteamMatchmaking* GetBayoMatchmaking() {
    return GetBayoSteamClient()->GetISteamMatchmaking(SteamAPI_GetHSteamUser(), SteamAPI_GetHSteamPipe(), STEAMMATCHMAKING_INTERFACE_VERSION);
}
static ISteamNetworking* GetBayoNetworking() {
    return GetBayoSteamClient()->GetISteamNetworking(SteamAPI_GetHSteamUser(), SteamAPI_GetHSteamPipe(), STEAMNETWORKING_INTERFACE_VERSION);
}
static ISteamUser* GetBayoSteamUser() {
    ISteamClient* client = GetBayoSteamClient();
    if (!client) return nullptr;
    return client->GetISteamUser(SteamAPI_GetHSteamUser(), SteamAPI_GetHSteamPipe(), STEAMUSER_INTERFACE_VERSION);
}
static ISteamFriends* GetBayoSteamFriends() {
    ISteamClient* client = GetBayoSteamClient();
    if (!client) return nullptr;
    return client->GetISteamFriends(SteamAPI_GetHSteamUser(), SteamAPI_GetHSteamPipe(), STEAMFRIENDS_INTERFACE_VERSION);
}
// ----------------------------
#pragma comment(lib, "dbghelp.lib")
#pragma comment(lib, "ws2_32.lib")

namespace WeaponFinder {
    void Snapshot();
    void Diff();
    void SetRegion(uintptr_t start, size_t size);
    void DecodeOwnership();
}

namespace Archipelago {
    enum ApLogCat : uint8_t { LOGCAT_SYSTEM = 0, LOGCAT_ITEMS = 1, LOGCAT_HINTS = 2, LOGCAT_CHAT = 3 };
    struct ApLogSeg { ImU32 col; std::string text; };
    struct ApLogLine { uint8_t cat; std::vector<ApLogSeg> segs; };

    static constexpr ImU32 LOGCOL_DEFAULT = IM_COL32(220, 220, 220, 255);
    static constexpr ImU32 LOGCOL_TIME = IM_COL32(130, 130, 130, 255);
    static constexpr ImU32 LOGCOL_SELF = IM_COL32(255, 110, 255, 255);
    static constexpr ImU32 LOGCOL_PLAYER = IM_COL32(250, 215, 90, 255);
    static constexpr ImU32 LOGCOL_LOCATION = IM_COL32(60, 230, 120, 255);
    static constexpr ImU32 LOGCOL_ENTRANCE = IM_COL32(110, 150, 255, 255);

    static std::mutex g_apLogMutex;
    static std::deque<ApLogLine> g_apLogLines;
    static bool g_apLogShowItems = true;
    static bool g_apLogShowHints = true;
    static bool g_apLogShowChat = true;
    static bool g_apLogShowSystem = true;
    static bool g_apToastAllSends = true;

    static std::string g_apLogFolderPath = "";

    static CSteamID g_CurrentLobbyID;
    static CSteamID g_RemotePlayerID;
    static bool g_IsHost = false;

    static char g_coopRoomIDInput[64] = "";
    static std::string g_coopStatus = "Steam API OK - Disconnected";

    // --- CO-OP CONFIG & CHAT ---
    static char g_coopPlayerName[64] = "BayonettaPlayer";
    static std::vector<std::string> g_coopChatMessages;

    // --- FORWARD DECLARATIONS FOR CLIENT STATE ---
    static APClient* g_apClient = nullptr;
    static bool g_apConnected = false;

    // --- FORWARD DECLARATION FOR LOGGING ---
    static void Log(const std::string& msg);

    // --- OFFLINE CHECK CACHING ---
    static std::set<int64_t> g_apOfflineCheckQueue;
    static std::mutex g_apOfflineMutex;

    // Safe wrapper for sending checks with robust offline fallback caching
    static void SendLocationCheckSafe(int64_t locId) {
        std::lock_guard<std::mutex> lock(g_apOfflineMutex);
        if (g_apClient && g_apConnected) {
            try {
                g_apClient->LocationChecks({ locId });
            }
            catch (...) {
                g_apOfflineCheckQueue.insert(locId);
                Log("[offline] Exception sending check - cached location check " + std::to_string(locId));
            }
        }
        else {
            g_apOfflineCheckQueue.insert(locId);
            Log("[offline] Disconnected - cached location check " + std::to_string(locId) + " for reconnection.");
        }
    }

    // --- DECLARED EARLY FOR RESETSESSIONSTATE VISIBILITY ---
    struct PendingRankCheck { int chapter; int verse; std::string label; int framesLeft; };
    static std::vector<PendingRankCheck> g_apPendingRankChecks;

    enum CoopPacketType : uint8_t {
        PACKET_TYPE_SYNC = 1,
        PACKET_TYPE_CHAT = 2
    };

    struct PlayerSyncPacket {
        uint8_t type = PACKET_TYPE_SYNC;
        float x, y, z;
        float rX, rY, rZ;
        int32_t moveID;
        int32_t movePart;
        float animFrame;
        int costumeId;
        char playerName[32];
    };

    struct CoopChatPacket {
        uint8_t type = PACKET_TYPE_CHAT;
        char sender[32];
        char message[128];
    };

    class SteamCoopManager {
    public:
        SteamCoopManager() :
            m_CallbackLobbyCreated(this, &SteamCoopManager::OnLobbyCreated),
            m_CallbackLobbyEntered(this, &SteamCoopManager::OnLobbyEntered),
            m_CallbackP2PRequest(this, &SteamCoopManager::OnP2PRequest) {
        }

        void HostRoom() {
            ISteamMatchmaking* mm = GetBayoMatchmaking();
            if (!mm) { g_coopStatus = "Steam not ready - can't host."; Log("Co-op host aborted: Steam matchmaking interface null."); return; }
            g_coopStatus = "Creating Lobby...";
            mm->CreateLobby(k_ELobbyTypePublic, 2);
        }
        void JoinRoom(uint64_t lobbyID) {
            ISteamMatchmaking* mm = GetBayoMatchmaking();
            if (!mm) { g_coopStatus = "Steam not ready - can't join."; Log("Co-op join aborted: Steam matchmaking interface null."); return; }
            g_coopStatus = "Joining Lobby...";
            CSteamID id(lobbyID);
            mm->JoinLobby(id);
        }

    private:
        STEAM_CALLBACK(SteamCoopManager, OnLobbyCreated, LobbyCreated_t, m_CallbackLobbyCreated);
        STEAM_CALLBACK(SteamCoopManager, OnLobbyEntered, LobbyEnter_t, m_CallbackLobbyEntered);
        STEAM_CALLBACK(SteamCoopManager, OnP2PRequest, P2PSessionRequest_t, m_CallbackP2PRequest);
    };

    void SteamCoopManager::OnLobbyCreated(LobbyCreated_t* pCallback) {
        if (pCallback->m_eResult == k_EResultOK) {
            g_CurrentLobbyID = CSteamID(pCallback->m_ulSteamIDLobby);
            g_IsHost = true;
            g_coopStatus = "Lobby Created! ID: " + std::to_string(g_CurrentLobbyID.ConvertToUint64());
        }
        else {
            g_coopStatus = "Failed to create lobby.";
        }
    }

    void SteamCoopManager::OnLobbyEntered(LobbyEnter_t* pCallback) {
        if (pCallback->m_EChatRoomEnterResponse == k_EChatRoomEnterResponseSuccess) {
            g_CurrentLobbyID = CSteamID(pCallback->m_ulSteamIDLobby);
            g_coopStatus = "Lobby Joined!";

            if (!g_IsHost) {
                g_RemotePlayerID = GetBayoMatchmaking()->GetLobbyOwner(g_CurrentLobbyID);
                g_coopStatus = "Connected to Host!";

                char ping = 1;
                GetBayoNetworking()->SendP2PPacket(g_RemotePlayerID, &ping, 1, k_EP2PSendReliable, 0);
            }
        }
        else {
            g_coopStatus = "Failed to join lobby.";
        }
    }

    void SteamCoopManager::OnP2PRequest(P2PSessionRequest_t* pCallback) {
        if (g_IsHost) {
            GetBayoNetworking()->AcceptP2PSessionWithUser(pCallback->m_steamIDRemote);
            g_RemotePlayerID = pCallback->m_steamIDRemote;
            g_coopStatus = "Player joined!";
        }
    }

    static SteamCoopManager* g_SteamManager = nullptr;

    static void SendP2PToAll(const void* data, uint32_t size, EP2PSend sendType) {
        if (!g_CurrentLobbyID.IsValid()) {
            if (g_RemotePlayerID.IsValid()) {
                GetBayoNetworking()->SendP2PPacket(g_RemotePlayerID, (void*)data, size, sendType, 0);
            }
            return;
        }
        int numMembers = GetBayoMatchmaking()->GetNumLobbyMembers(g_CurrentLobbyID);
        ISteamUser* pUser = GetBayoSteamUser();
        if (!pUser) return;
        CSteamID localUser = pUser->GetSteamID();
        for (int i = 0; i < numMembers; ++i) {
            CSteamID member = GetBayoMatchmaking()->GetLobbyMemberByIndex(g_CurrentLobbyID, i);
            if (member != localUser) {
                GetBayoNetworking()->SendP2PPacket(member, (void*)data, size, sendType, 0);
            }
        }
    }

    static void SendCoopSync() {
        if (!g_RemotePlayerID.IsValid() && !g_CurrentLobbyID.IsValid()) return;

        LocalPlayer* player = GameHook::GetLocalPlayer();
        if (!player) return;

        PlayerSyncPacket pkt;
        pkt.type = PACKET_TYPE_SYNC;
        pkt.x = player->pos.x;
        pkt.y = player->pos.y;
        pkt.z = player->pos.z;
        pkt.rX = player->rot.x;
        pkt.rY = player->rot.y;
        pkt.rZ = player->rot.z;
        pkt.moveID = player->moveID;
        pkt.movePart = player->movePart;
        pkt.animFrame = player->animFrame;
        pkt.costumeId = *(int*)(0x5AA74E4);
        strncpy_s(pkt.playerName, sizeof(pkt.playerName), g_coopPlayerName, _TRUNCATE);

        SendP2PToAll(&pkt, sizeof(pkt), k_EP2PSendUnreliable);
    }

    static void InitLogPath() {
        if (!g_apLogFolderPath.empty()) return;
        char exePath[MAX_PATH];
        GetModuleFileNameA(NULL, exePath, MAX_PATH);

        std::string path(exePath);
        size_t pos = path.find_last_of("\\/");
        if (pos != std::string::npos) {
            path = path.substr(0, pos + 1);
        }

        g_apLogFolderPath = path + "Archipelago Logs\\";
        CreateDirectoryA(g_apLogFolderPath.c_str(), NULL);
    }

    static char g_apHost[128] = "archipelago.gg";
    static char g_apPort[8] = "443";
    static char g_apSlot[64] = "";
    static char g_apPass[64] = "";

    static void LoadSavedConnection() {
        std::string path = g_apLogFolderPath + "ap_config.txt";
        std::ifstream f(path);
        if (!f.is_open()) return;

        std::string host, port, slot;
        if (std::getline(f, host)) strncpy_s(g_apHost, sizeof(g_apHost), host.c_str(), _TRUNCATE);
        if (std::getline(f, port)) strncpy_s(g_apPort, sizeof(g_apPort), port.c_str(), _TRUNCATE);
        if (std::getline(f, slot)) strncpy_s(g_apSlot, sizeof(g_apSlot), slot.c_str(), _TRUNCATE);
    }

    static void SaveConnection() {
        InitLogPath();
        std::string path = g_apLogFolderPath + "ap_config.txt";
        std::ofstream f(path);
        if (f.is_open()) {
            f << g_apHost << "\n" << g_apPort << "\n" << g_apSlot << "\n";
        }
    }

    static std::string LogTimestamp() {
        std::time_t t = std::time(nullptr);
        char buf[16];
        std::tm tmBuf;
        localtime_s(&tmBuf, &t);
        std::strftime(buf, sizeof(buf), "%H:%M:%S", &tmBuf);
        return std::string("[") + buf + "] ";
    }

    static void LogSegs(uint8_t cat, std::vector<ApLogSeg> segs) {
        std::string ts = LogTimestamp();
        std::string plain;
        for (const auto& s : segs) plain += s.text;

        ApLogLine line;
        line.cat = cat;
        line.segs.reserve(segs.size() + 1);
        line.segs.push_back({ LOGCOL_TIME, ts });
        for (auto& s : segs) line.segs.push_back(std::move(s));

        {
            std::lock_guard<std::mutex> lock(g_apLogMutex);
            g_apLogLines.push_back(std::move(line));
            while (g_apLogLines.size() > 400) g_apLogLines.pop_front();
        }

        InitLogPath();
        std::string fullPath = g_apLogFolderPath + "BayoHook_Archipelago.log";
        std::ofstream f(fullPath, std::ios::app);

        if (f.is_open()) f << ts << plain << "\n";
    }

    static void Log(const std::string& msg) {
        LogSegs(LOGCAT_SYSTEM, { { LOGCOL_DEFAULT, msg } });
    }

    static void ClearConsole() {
        {
            std::lock_guard<std::mutex> lock(g_apLogMutex);
            g_apLogLines.clear();
        }
        Log("Console cleared.");
    }

    LONG WINAPI BayoHookCrashHandler(EXCEPTION_POINTERS* pExceptionPointers) {
        char crashMsg[256];
        sprintf_s(crashMsg, "[CRASH] FATAL ERROR: Exception Code 0x%X at Address 0x%p",
            pExceptionPointers->ExceptionRecord->ExceptionCode,
            pExceptionPointers->ExceptionRecord->ExceptionAddress);
        Log(crashMsg);

        InitLogPath();
        std::string fullPath = g_apLogFolderPath + "BayoHook_CrashDump.dmp";
        HANDLE hFile = CreateFileA(fullPath.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

        if (hFile != INVALID_HANDLE_VALUE) {
            MINIDUMP_EXCEPTION_INFORMATION dumpInfo;
            dumpInfo.ThreadId = GetCurrentThreadId();
            dumpInfo.ExceptionPointers = pExceptionPointers;
            dumpInfo.ClientPointers = FALSE;

            MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), hFile, MiniDumpNormal, &dumpInfo, NULL, NULL);
            CloseHandle(hFile);
            Log("[CRASH] Dump file successfully saved to " + fullPath);
        }
        else {
            Log("[CRASH] Failed to create dump file!");
        }

        return EXCEPTION_CONTINUE_SEARCH;
    }

    struct HintEntry {
        std::string timestamp;
        std::string message;
    };
    static std::vector<HintEntry> g_hintHistory;
    static std::mutex g_hintHistoryMutex;

    static IDirect3DTexture9* g_apLogoTexture = nullptr;
    static int g_apLogoWidth = 0;
    static int g_apLogoHeight = 0;
    static float g_logoPosX = 0.501f;
    static float g_logoPosY = 0.495f;
    static float g_logoScale = 0.116f;

    static bool LoadTextureFromMemory(const unsigned char* buffer, int len, LPDIRECT3DDEVICE9 d3dDevice, IDirect3DTexture9** out_texture, int* out_width, int* out_height) {
        int image_width = 0;
        int image_height = 0;
        unsigned char* image_data = stbi_load_from_memory(buffer, len, &image_width, &image_height, NULL, 4);
        if (image_data == NULL) return false;

        IDirect3DTexture9* texture = nullptr;
        HRESULT hr = d3dDevice->CreateTexture(image_width, image_height, 1, D3DUSAGE_DYNAMIC, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &texture, NULL);
        if (FAILED(hr)) {
            stbi_image_free(image_data);
            return false;
        }

        D3DLOCKED_RECT rect;
        hr = texture->LockRect(0, &rect, NULL, 0);
        if (SUCCEEDED(hr)) {
            unsigned char* dest = (unsigned char*)rect.pBits;
            for (int y = 0; y < image_height; y++) {
                const unsigned char* src = image_data + (y * image_width * 4);
                unsigned char* dst = dest + (y * rect.Pitch);
                for (int x = 0; x < image_width; x++) {
                    dst[0] = src[2];
                    dst[1] = src[1];
                    dst[2] = src[0];
                    dst[3] = src[3];
                    src += 4;
                    dst += 4;
                }
            }
            texture->UnlockRect(0);
        }

        stbi_image_free(image_data);
        *out_texture = texture;
        *out_width = image_width;
        *out_height = image_height;
        return true;
    }

    void InitLogoTexture(LPDIRECT3DDEVICE9 pDevice) {
        if (g_apLogoTexture == nullptr) {
            LoadTextureFromMemory(ap_logo_png, ap_logo_len, pDevice, &g_apLogoTexture, &g_apLogoWidth, &g_apLogoHeight);
        }
    }

    struct Notification {
        std::string message;
        std::chrono::steady_clock::time_point timestamp;
        float duration;
        ImVec4 color;
    };

    static std::vector<Notification> g_notifications;
    static std::mutex g_notificationMutex;
    static constexpr float DEFAULT_NOTIFICATION_DURATION = 5.0f;
    static constexpr int MAX_NOTIFICATIONS = 5;

    static void AddNotification(const std::string& msg, const ImVec4& color = ImVec4(1.0f, 1.0f, 1.0f, 1.0f), float duration = DEFAULT_NOTIFICATION_DURATION) {
        std::lock_guard<std::mutex> lock(g_notificationMutex);
        g_notifications.push_back({ msg, std::chrono::steady_clock::now(), duration, color });
        while (g_notifications.size() > MAX_NOTIFICATIONS) {
            g_notifications.erase(g_notifications.begin());
        }
    }

    static bool ReadMem(uintptr_t addr, int32_t& value) {
        return ReadProcessMemory(GetCurrentProcess(), (LPCVOID)addr, &value, sizeof(value), nullptr);
    }
    static bool WriteMem(uintptr_t addr, int32_t value) {
        return WriteProcessMemory(GetCurrentProcess(), (LPVOID)addr, &value, sizeof(value), nullptr);
    }
    static bool ReadMemF(uintptr_t addr, float& value) {
        return ReadProcessMemory(GetCurrentProcess(), (LPCVOID)addr, &value, sizeof(value), nullptr);
    }
    static bool WriteMemF(uintptr_t addr, float value) {
        return WriteProcessMemory(GetCurrentProcess(), (LPVOID)addr, &value, sizeof(value), nullptr);
    }

    namespace ChestFinder {
        static constexpr uintptr_t REGION_START = 0x5AA7450;
        static constexpr uintptr_t REGION_END = 0x5AA7470;
        static constexpr uintptr_t FAMILY2_START = 0x5AA1900;
        static constexpr uintptr_t FAMILY2_END = 0x5AA1E00;

        static constexpr uintptr_t FLIP_START = 0x5A98560;
        static constexpr size_t    FLIP_SIZE = 0x11550;
        static constexpr uintptr_t FLIP_VERSE_ADDR = 0x5BB5A10;
        static constexpr int       FLIP_MUTE_AFTER = 6;

        static bool g_mapChests = false;
        static uint8_t g_chestShadow[FLIP_SIZE];
        static bool g_chestShadowInit = false;
        static std::set<uintptr_t> g_chestLogged;

        static void ChestMapStart() {
            SIZE_T bytesRead;
            ReadProcessMemory(GetCurrentProcess(), (LPCVOID)FLIP_START, g_chestShadow, FLIP_SIZE, &bytesRead);
            g_chestShadowInit = true;
            g_mapChests = true;
            g_chestLogged.clear();
            Log("[chestmapper] ACTIVE - watching full save block. Break chests!");
        }

        static void ChestMapStop() {
            g_mapChests = false;
            g_chestShadowInit = false;
            Log("[chestmapper] Stopped. " + std::to_string(g_chestLogged.size()) + " unique chests logged.");
        }

        static void ChestMapPoll(int chapterHint) {
            if (!g_mapChests || !g_chestShadowInit) return;

            static int lastVerse = -1;
            int32_t verse = 0;
            ReadMem(FLIP_VERSE_ADDR, verse);

            if (verse != lastVerse) {
                lastVerse = verse;
                SIZE_T bytesRead;
                ReadProcessMemory(GetCurrentProcess(), (LPCVOID)FLIP_START, g_chestShadow, FLIP_SIZE, &bytesRead);
                return;
            }

            static uint8_t live[FLIP_SIZE];
            SIZE_T bytesRead;
            if (!ReadProcessMemory(GetCurrentProcess(), (LPCVOID)FLIP_START, live, FLIP_SIZE, &bytesRead)) return;

            for (size_t i = 0; i < FLIP_SIZE; i++) {
                uint8_t newBits = live[i] & ~g_chestShadow[i];
                if (newBits) {
                    for (int b = 0; b < 8; b++) {
                        if (newBits & (1 << b)) {
                            uintptr_t addr = FLIP_START + i;
                            if (addr < 0x5AA7798 || addr >= REGION_END) continue;
                            if (g_chestLogged.count(addr)) continue;
                            g_chestLogged.insert(addr);

                            uint32_t fam1A10 = 0, fam1A28 = 0, fam1AA0 = 0;
                            ReadProcessMemory(GetCurrentProcess(), (LPCVOID)0x5AA1A10, &fam1A10, 4, nullptr);
                            ReadProcessMemory(GetCurrentProcess(), (LPCVOID)0x5AA1A28, &fam1A28, 4, nullptr);
                            ReadProcessMemory(GetCurrentProcess(), (LPCVOID)0x5AA1AA0, &fam1AA0, 4, nullptr);

                            char buf[256];
                            sprintf_s(buf, "[CHEST MAP] ch=%d verse=%d lane=0x%X bit=%d | fam: 1A10=0x%X 1A28=0x%X 1AA0=0x%X",
                                chapterHint, verse, (unsigned)addr, b, (unsigned)fam1A10, (unsigned)fam1A28, (unsigned)fam1AA0);
                            Log(buf);
                        }
                    }
                }
                g_chestShadow[i] = live[i];
            }
        }

        static std::atomic<bool> g_armed{ false };
        static PVOID     g_veh = nullptr;
        static uintptr_t g_pageBase = 0;
        static uintptr_t g_family2PageBase = 0;
        static SIZE_T    g_pageSpan = 0;
        static SIZE_T    g_family2PageSpan = 0;

        struct Hit {
            uintptr_t addr;
            uintptr_t eip;
            uint8_t isWrite;
            const char* region;
        };
        static constexpr int MAX_HITS = 4096;
        static Hit g_hits[MAX_HITS];
        static std::atomic<int> g_hitCount{ 0 };

        static void Arm() {
            DWORD old;
            if (g_pageBase && g_pageSpan)
                VirtualProtect((LPVOID)g_pageBase, g_pageSpan, PAGE_READWRITE | PAGE_GUARD, &old);
            if (g_family2PageBase && g_family2PageSpan)
                VirtualProtect((LPVOID)g_family2PageBase, g_family2PageSpan, PAGE_READWRITE | PAGE_GUARD, &old);
        }

        static LONG CALLBACK Veh(PEXCEPTION_POINTERS x) {
            DWORD code = x->ExceptionRecord->ExceptionCode;
            if (code == STATUS_GUARD_PAGE_VIOLATION) {
                uintptr_t rw = (uintptr_t)x->ExceptionRecord->ExceptionInformation[0];
                uintptr_t target = (uintptr_t)x->ExceptionRecord->ExceptionInformation[1];

                bool inLane = (target >= REGION_START && target < REGION_END);
                bool inFamily2 = (target >= FAMILY2_START && target < FAMILY2_END);

                if (inLane || inFamily2) {
                    int i = g_hitCount.fetch_add(1);
                    if (i < MAX_HITS) {
                        g_hits[i] = { target, (uintptr_t)x->ContextRecord->Eip, (uint8_t)rw, inLane ? "lane" : "family2" };
                    }
                    else {
                        g_hitCount.store(MAX_HITS);
                    }
                }
                x->ContextRecord->EFlags |= 0x100;
                return EXCEPTION_CONTINUE_EXECUTION;
            }
            if (code == STATUS_SINGLE_STEP) {
                if (g_armed.load()) Arm();
                return EXCEPTION_CONTINUE_EXECUTION;
            }
            return EXCEPTION_CONTINUE_SEARCH;
        }

        static void Start() {
            SYSTEM_INFO si; GetSystemInfo(&si);

            g_pageBase = REGION_START & ~((uintptr_t)si.dwPageSize - 1);
            uintptr_t laneEnd = (REGION_END + si.dwPageSize - 1) & ~((uintptr_t)si.dwPageSize - 1);
            g_pageSpan = laneEnd - g_pageBase;

            g_family2PageBase = FAMILY2_START & ~((uintptr_t)si.dwPageSize - 1);
            uintptr_t family2End = (FAMILY2_END + si.dwPageSize - 1) & ~((uintptr_t)si.dwPageSize - 1);
            g_family2PageSpan = family2End - g_family2PageBase;

            g_hitCount.store(0);
            if (!g_veh) g_veh = AddVectoredExceptionHandler(1, Veh);

            g_armed.store(true);
            Arm();
            Log("[chestfinder] ARMED - watching lane (0x5AA7700) and family2 (0x5AA1900).");
        }

        static void Stop() {
            g_armed.store(false);
            DWORD old;
            if (g_pageBase && g_pageSpan)
                VirtualProtect((LPVOID)g_pageBase, g_pageSpan, PAGE_READWRITE, &old);
            if (g_family2PageBase && g_family2PageSpan)
                VirtualProtect((LPVOID)g_family2PageBase, g_family2PageSpan, PAGE_READWRITE, &old);
            Log("[chestfinder] Disarmed.");
        }

        static void Dump() {
            int n = (std::min)(g_hitCount.load(), MAX_HITS);
            std::map<uintptr_t, std::set<uintptr_t>> laneHits;
            std::map<uintptr_t, std::set<uintptr_t>> family2Hits;

            for (int i = 0; i < n; ++i) {
                if (g_hits[i].eip > 0x1000000) continue;
                const char* region = g_hits[i].region ? g_hits[i].region : "?";
                if (strcmp(region, "lane") == 0) laneHits[g_hits[i].addr].insert(g_hits[i].eip);
                else family2Hits[g_hits[i].addr].insert(g_hits[i].eip);
            }

            Log("[chestfinder] " + std::to_string(n) + " raw hits across both regions");
            char buf[128];

            if (!laneHits.empty()) {
                Log("[chestfinder] --- LANE region (" + std::to_string(laneHits.size()) + " addresses) ---");
                for (auto& kv : laneHits) {
                    std::string eips;
                    for (auto e : kv.second) { sprintf_s(buf, "%X ", (unsigned)e); eips += buf; }
                    sprintf_s(buf, "  [lane] addr %X  accessed-by: ", (unsigned)kv.first);
                    Log(std::string(buf) + eips);
                }
            }

            if (!family2Hits.empty()) {
                Log("[chestfinder] --- FAMILY2 region (" + std::to_string(family2Hits.size()) + " addresses) ---");
                for (auto& kv : family2Hits) {
                    std::string eips;
                    for (auto e : kv.second) { sprintf_s(buf, "%X ", (unsigned)e); eips += buf; }
                    sprintf_s(buf, "  [family2] addr %X  accessed-by: ", (unsigned)kv.first);
                    Log(std::string(buf) + eips);
                }
            }

            if (laneHits.empty() && family2Hits.empty()) {
                Log("[chestfinder] No hits captured. Break a chest while armed.");
            }
        }

        static uint8_t  g_flipShadow[FLIP_SIZE];
        static uint16_t g_flipNoise[FLIP_SIZE];
        static bool     g_flipActive = false;

        static void FlipStart() {
            memcpy(g_flipShadow, (void*)FLIP_START, FLIP_SIZE);
            memset(g_flipNoise, 0, sizeof(g_flipNoise));
            g_flipActive = true;
            Log("[flipwatch] ACTIVE. Watching whole save block; counters auto-mute after a few flips.");
        }

        static void FlipStop() {
            g_flipActive = false;
            Log("[flipwatch] Stopped.");
        }

        static void FlipPoll(int chapterHint) {
            if (!g_flipActive) return;

            const uint8_t* live = (const uint8_t*)FLIP_START;
            int32_t verse = 0;
            ReadMem(FLIP_VERSE_ADDR, verse);

            for (size_t i = 0; i < FLIP_SIZE; ++i) {
                uint8_t newBits = (uint8_t)(live[i] & ~g_flipShadow[i]);
                if (newBits) {
                    if (g_flipNoise[i] >= FLIP_MUTE_AFTER) {
                        g_flipShadow[i] = live[i];
                        continue;
                    }
                    if (++g_flipNoise[i] == FLIP_MUTE_AFTER) {
                        char mb[96];
                        sprintf_s(mb, "[flipwatch] muting noisy addr %X (counter)", (unsigned)(FLIP_START + i));
                        Log(mb);
                    }

                    for (int b = 0; b < 8; ++b) {
                        if (newBits & (1 << b)) {
                            uintptr_t a = FLIP_START + i;
                            const char* zone;
                            if (a >= 0x5AA7798 && a < 0x5AA8080) zone = "lane";
                            else if (a >= 0x5AA1900 && a < 0x5AA1E00) zone = "family2";
                            else if (a >= 0x5AA0000 && a < 0x5AA7798) zone = "early";
                            else                                       zone = "?";

                            char buf[192];
                            sprintf_s(buf, "[flipwatch] ch=%d verse=%d addr=%X bit=%d [%s]",
                                chapterHint, verse, (unsigned)a, b, zone);
                            Log(buf);
                        }
                    }
                }
                g_flipShadow[i] = live[i];
            }
        }

        static uint8_t g_diffSnap[FLIP_SIZE];
        static bool    g_diffTaken = false;

        static void DiffSnap() {
            memcpy(g_diffSnap, (void*)FLIP_START, FLIP_SIZE);
            g_diffTaken = true;
            Log("[blockdiff] Snapshot taken. Break the chest, then click Diff.");
        }

        static void DiffCompare() {
            if (!g_diffTaken) { Log("[blockdiff] No snapshot yet."); return; }
            const uint8_t* live = (const uint8_t*)FLIP_START;
            int found = 0;
            for (size_t i = 0; i < FLIP_SIZE; ++i) {
                if (g_flipNoise[i] >= FLIP_MUTE_AFTER) continue;
                uint8_t newBits = (uint8_t)(live[i] & ~g_diffSnap[i]);
                if (!newBits) continue;
                for (int b = 0; b < 8; ++b) {
                    if (newBits & (1 << b)) {
                        char buf[128];
                        sprintf_s(buf, "[blockdiff] addr=%X bit=%d (0->1)", (unsigned)(FLIP_START + i), b);
                        Log(buf);
                        ++found;
                    }
                }
            }
            if (!found) Log("[blockdiff] No 0->1 flips since snapshot.");
        }

        static uint8_t g_rankSnap[FLIP_SIZE];
        static bool    g_rankSnapTaken = false;

        static void RankFindSnap() {
            memcpy(g_rankSnap, (void*)FLIP_START, FLIP_SIZE);
            g_rankSnapTaken = true;
            Log("[rankfind] Snapshot taken. Finish the verse (and dismiss the results screen), then click Diff.");
        }

        static void RankFindDiff() {
            if (!g_rankSnapTaken) { Log("[rankfind] No snapshot yet."); return; }
            const uint8_t* live = (const uint8_t*)FLIP_START;
            int candidates = 0;
            char buf[192];

            Log("[rankfind] Byte changes with new value in [0..9] (rank candidates):");
            for (size_t i = 0; i < FLIP_SIZE; ++i) {
                uint8_t before = g_rankSnap[i], after = live[i];
                if (before == after) continue;
                if (after > 9) continue;

                uintptr_t a = FLIP_START + i;
                const char* zone;
                if (a >= 0x5AA7798 && a < 0x5AA8080)     zone = "lane";
                else if (a >= 0x5AA1900 && a < 0x5AA1E00) zone = "family2";
                else if (a >= 0x5AA0000 && a < 0x5AA7798) zone = "early";
                else                                      zone = "?";

                sprintf_s(buf, "[rankfind]    addr=%X  %u -> %u  [%s]",
                    (unsigned)a, (unsigned)before, (unsigned)after, zone);
                Log(buf);
                if (++candidates >= 200) { Log("[rankfind] (truncated at 200 hits)"); break; }
            }
            if (candidates == 0) Log("[rankfind] No changed bytes with a rank-shaped value. Try a different rank next time.");
            else Log("[rankfind] " + std::to_string(candidates) + " candidate(s). Repeat at a different rank to narrow down.");
        }
    } // namespace ChestFinder

    static constexpr int32_t HP_BASE = 2000;
    static constexpr int32_t HP_PER_HEART = 400;
    static const uintptr_t HP_SOURCE_ADDRS[] = {
        0x5b7ad40, 0x5baf050, 0x5bb58c0,
    };
    static constexpr uintptr_t HP_WRITE_INSTR_ADDR = 0x8BC9E5;
    static constexpr size_t    HP_WRITE_INSTR_LEN = 6;
    static std::atomic<bool> g_apHpWritePatched{ false };
    static uint8_t g_apHpWriteOrigBytes[HP_WRITE_INSTR_LEN];
    static bool g_apHpWriteOrigSaved = false;

    static void PatchOutHPWrite() {
        if (g_apHpWritePatched.load()) return;
        DWORD oldProtect = 0;
        if (VirtualProtect((LPVOID)HP_WRITE_INSTR_ADDR, HP_WRITE_INSTR_LEN, PAGE_EXECUTE_READWRITE, &oldProtect)) {
            if (!g_apHpWriteOrigSaved) {
                memcpy(g_apHpWriteOrigBytes, (void*)HP_WRITE_INSTR_ADDR, HP_WRITE_INSTR_LEN);
                g_apHpWriteOrigSaved = true;
            }
            memset((void*)HP_WRITE_INSTR_ADDR, 0x90, HP_WRITE_INSTR_LEN);
            VirtualProtect((LPVOID)HP_WRITE_INSTR_ADDR, HP_WRITE_INSTR_LEN, oldProtect, &oldProtect);
            g_apHpWritePatched.store(true);
            Log("Patched out the game's max-HP write - our max HP will now stick.");
        }
        else {
            Log("Failed to patch max-HP write instruction (VirtualProtect failed).");
        }
    }

    static void RestoreHPWrite() {
        if (!g_apHpWritePatched.load() || !g_apHpWriteOrigSaved) return;
        DWORD oldProtect = 0;
        if (VirtualProtect((LPVOID)HP_WRITE_INSTR_ADDR, HP_WRITE_INSTR_LEN, PAGE_EXECUTE_READWRITE, &oldProtect)) {
            memcpy((void*)HP_WRITE_INSTR_ADDR, g_apHpWriteOrigBytes, HP_WRITE_INSTR_LEN);
            VirtualProtect((LPVOID)HP_WRITE_INSTR_ADDR, HP_WRITE_INSTR_LEN, oldProtect, &oldProtect);
            g_apHpWritePatched.store(false);
            Log("Restored the game's max-HP write (Shop workaround).");
        }
    }

    static constexpr uintptr_t PLAYER_POINTER_ADDR = 0xEF5A60;
    static constexpr uintptr_t HPMAX_OFFSET = 0x6BC;
    static constexpr uintptr_t HP_OFFSET = 0x6B4;
    static constexpr uintptr_t HPUNK_OFFSET = 0x93508;
    static std::atomic<int32_t> g_apHeartBonusCount{ 0 };
    static std::atomic<bool> g_apForceMaxHP{ false };
    static int g_apHeartAnimFrames = 0;
    static int g_apGhostCleanFrames = 0;
    static int32_t g_apPrevHeartFullSeen = -1;
    static bool IsRecognizedStage(int stageId);

    static bool HealToFull() {
        uint32_t playerPtr = 0;
        if (!ReadProcessMemory(GetCurrentProcess(), (LPCVOID)PLAYER_POINTER_ADDR,
            &playerPtr, sizeof(playerPtr), nullptr) || playerPtr == 0)
            return false;

        int32_t target = HP_BASE + g_apHeartBonusCount.load() * HP_PER_HEART;

        WriteMem((uintptr_t)playerPtr + HPMAX_OFFSET, target);
        WriteMem((uintptr_t)playerPtr + HP_OFFSET, target);
        WriteMem((uintptr_t)playerPtr + HPUNK_OFFSET, target);
        return true;
    }

    static void ApplyHeartUpgrade() {
        int32_t target = HP_BASE + g_apHeartBonusCount.load() * HP_PER_HEART;
        for (uintptr_t addr : HP_SOURCE_ADDRS) {
            WriteMem(addr, target);
        }
        Log("Max HP set to " + std::to_string(target) + ".");
    }

    static bool ReadPlayerBase(uintptr_t& baseOut);
    static void EnforceMaxHP() {
        if (!g_apForceMaxHP.load()) return;

        if (g_apHeartAnimFrames > 0) return;

        uintptr_t playerBase = 0;
        if (!ReadPlayerBase(playerBase)) return;

        int32_t target = HP_BASE + g_apHeartBonusCount.load() * HP_PER_HEART;

        for (uintptr_t addr : HP_SOURCE_ADDRS) {
            int32_t cur = 0;
            if (ReadMem(addr, cur) && cur < target) WriteMem(addr, target);
        }

        uint32_t playerPtr = 0;
        if (ReadProcessMemory(GetCurrentProcess(), (LPCVOID)PLAYER_POINTER_ADDR,
            &playerPtr, sizeof(playerPtr), nullptr) && playerPtr != 0) {
            uintptr_t hpMaxAddr = (uintptr_t)playerPtr + HPMAX_OFFSET;
            int32_t cur = 0;
            if (ReadMem(hpMaxAddr, cur) && cur < target) WriteMem(hpMaxAddr, target);
        }
    }

    static std::atomic<bool> g_apUnlockedPunch{ true };
    static std::atomic<bool> g_apUnlockedKick{ true };
    static std::atomic<bool> g_apUnlockedTorture{ true };
    static std::atomic<bool> g_apUnlockedAngelArms{ true };

    static constexpr uintptr_t MOVEID_OFFSET = 0x34C;
    static constexpr uintptr_t MOVEPART_OFFSET = 0x350;
    static constexpr uintptr_t INVINCIBILITY_OFFSET = 0x354;
    static constexpr uintptr_t ANIMFRAME_OFFSET = 0x3E4;
    static constexpr int32_t   DEATH_MOVE_ID = 451;
    static constexpr int32_t   DEATH_MOVE_MIN = 451;
    static constexpr int32_t   DEATH_MOVE_MAX = 462;

    static bool IsDeathMove(int32_t moveId) {
        return (moveId >= DEATH_MOVE_MIN && moveId <= DEATH_MOVE_MAX) || moveId == 405;
    }

    static bool IsAngelArmMove(int32_t moveId) {
        return (moveId >= 255 && moveId <= 281);
    }

    static bool IsTortureMove(int32_t moveId) {
        if (moveId == 288) return true;
        if (moveId >= 294 && moveId <= 298) return true;
        if (moveId >= 301 && moveId <= 303) return true;
        if (moveId == 306) return true;
        if (moveId == 319 || moveId == 320) return true;
        return false;
    }

    static bool IsPunchMove(int32_t moveId) {
        if (moveId == 49 || (moveId >= 48 && moveId <= 68)) return false;
        if (moveId >= 350 && moveId <= 370) return false;

        if (moveId == 50 || (moveId >= 55 && moveId <= 57) || (moveId == 67 || moveId == 68)) return true;
        if (moveId == 95 || moveId == 96 || moveId == 102 || moveId == 103 || moveId == 110 || moveId == 111) return true;
        if (moveId == 116 || moveId == 117 || moveId == 119 || moveId == 123) return true;
        if (moveId == 132 || moveId == 133 || moveId == 138 || moveId == 139 || moveId == 155) return true;
        if (moveId == 165 || moveId == 166 || moveId == 168 || moveId == 170 || moveId == 171) return true;
        if ((moveId >= 195 && moveId <= 200) || moveId == 210 || moveId == 211) return true;

        return false;
    }

    static bool IsKickMove(int32_t moveId) {
        if (moveId == 51 || moveId == 52 || (moveId >= 58 && moveId <= 66) || (moveId >= 69 && moveId <= 94)) return true;
        if ((moveId >= 97 && moveId <= 101) || (moveId >= 104 && moveId <= 109) || (moveId >= 112 && moveId <= 115)) return true;
        if ((moveId >= 118 && moveId <= 122) || (moveId >= 124 && moveId <= 131) || (moveId >= 134 && moveId <= 137)) return true;
        if ((moveId >= 140 && moveId <= 154) || (moveId >= 156 && moveId <= 164) || (moveId >= 167 && moveId <= 169)) return true;
        if ((moveId >= 172 && moveId <= 194) || (moveId >= 201 && moveId <= 209) || moveId >= 212) return true;
        return false;
    }

    static std::atomic<bool>    g_apDeathLinkEnabled{ false };
    static std::atomic<bool>    g_apDeathLinkPending{ false };
    static std::atomic<bool>    g_apSendDeathPending{ false };
    static std::string          g_apDeathCause;
    static std::mutex           g_apDeathMutex;
    static std::atomic<double>  g_apLastDeathTime{ 0.0 };
    static bool g_apPrevMoveWasDeath = false;
    static std::atomic<int> g_apDeathSuppressFrames{ 0 };
    static std::atomic<bool> g_apDeathLinkKillActive{ false };
    static int g_apDeathLinkAliveFrames = 0;

    static std::atomic<bool>    g_apDamageLinkEnabled{ false };
    static std::atomic<bool>    g_apTrapLinkEnabled{ false };
    static std::atomic<int>     g_apDamageSuppressFrames{ 0 };
    static std::atomic<double>  g_apLastDamageTime{ 0.0 };
    static std::atomic<double>  g_apLastTrapTime{ 0.0 };

    static std::atomic<bool>    g_apRingLinkEnabled{ false };
    static std::atomic<double>  g_apLastRingTime{ 0.0 };
    static int g_apPrevHalosForRing = -1;
    static int g_apRingGainAccumHalos = 0;
    static int g_apRingSendCooldown = 0;
    static int g_apRingRecvDisplayHalos = 0;
    static int g_apRingRecvDisplayFrames = 0;
    static int g_apRingSentDisplayRings = 0;
    static int g_apRingSentDisplayFrames = 0;

    static void RingLinkNoteOwnWrite() { g_apPrevHalosForRing = -1; }

    static std::list<std::string> BuildConnectTags();

    static bool ReadMoveId(int32_t& moveOut) {
        uint32_t playerPtr = 0;
        if (!ReadProcessMemory(GetCurrentProcess(), (LPCVOID)PLAYER_POINTER_ADDR,
            &playerPtr, sizeof(playerPtr), nullptr) || playerPtr == 0)
            return false;
        return ReadMem((uintptr_t)playerPtr + MOVEID_OFFSET, moveOut);
    }

    static bool KillPlayer() {
        uint32_t playerPtr = 0;
        if (!ReadProcessMemory(GetCurrentProcess(), (LPCVOID)PLAYER_POINTER_ADDR,
            &playerPtr, sizeof(playerPtr), nullptr) || playerPtr == 0)
            return false;

        g_apDeathLinkAliveFrames = 0;
        g_apDeathLinkKillActive.store(true);

        uintptr_t base = (uintptr_t)playerPtr;
        WriteMem(base + INVINCIBILITY_OFFSET, 0);
        WriteMem(base + HPUNK_OFFSET, 0);
        WriteMem(base + HP_OFFSET, 0);
        WriteMem(base + MOVEPART_OFFSET, 0);
        WriteMemF(base + ANIMFRAME_OFFSET, 0.0f);
        return WriteMem(base + MOVEID_OFFSET, DEATH_MOVE_ID);
    }

    static constexpr uintptr_t MAGIC_ORB_COUNT_ADDR = 0x5aa74b0;
    static constexpr int32_t    MAGIC_ORB_MAX = 24;
    static constexpr uintptr_t MP_CUR_ADDR = 0x5aa74ac;

    static void GiveMoonPearlOrb() {
        int32_t orbs = 0;
        if (ReadMem(MAGIC_ORB_COUNT_ADDR, orbs)) {
            if (orbs < MAGIC_ORB_MAX) orbs++;
            WriteMem(MAGIC_ORB_COUNT_ADDR, orbs);
            WriteMemF(MP_CUR_ADDR, (float)orbs * 50.0f);
        }
    }

    static constexpr uintptr_t WEAPON_OWNERSHIP_ADDR = 0x5AA7459;
    struct WeaponBit { int itemId; int bit; int altBit; const char* label; };

    static const WeaponBit g_apWeaponBits[] = {
        { 50001, 23, -1, "Scarborough Fair" },
        { 50002, 22, 10, "Onyx Roses" },
        { 50003, 21, -1, "Shuraba" },
        { 50004, 20, -1, "Kulshedra" },
        { 50005, 17,  9, "Durga" },
        { 50006, 15, -1, "Odette" },
        { 50007, 16,  8, "Lt. Col. Kilgore" },
        { 50008, 14, -1, "Sai Fung" },
        { 50009, 6,  -1, "Bazillions" },
        { 50010, 7,  -1, "Pillow Talk" },
        { 50011, 5,  -1, "Rodin" },
    };

    static bool ReadWeaponBit(int bit) {
        uintptr_t addr = WEAPON_OWNERSHIP_ADDR + (bit / 8);
        uint8_t b = 0;
        if (!ReadProcessMemory(GetCurrentProcess(), (LPCVOID)addr, &b, 1, nullptr)) return false;
        return (b & (1 << (bit % 8))) != 0;
    }

    static bool SetWeaponBit(int bit) {
        uintptr_t addr = WEAPON_OWNERSHIP_ADDR + (bit / 8);
        uint8_t b = 0;
        if (!ReadProcessMemory(GetCurrentProcess(), (LPCVOID)addr, &b, 1, nullptr)) return false;
        b |= (uint8_t)(1 << (bit % 8));
        return WriteProcessMemory(GetCurrentProcess(), (LPVOID)addr, &b, 1, nullptr) != 0;
    }

    static bool ClearWeaponBit(int bit) {
        if (bit < 0) return false;
        uintptr_t addr = WEAPON_OWNERSHIP_ADDR + (bit / 8);
        uint8_t b = 0;
        if (!ReadProcessMemory(GetCurrentProcess(), (LPCVOID)addr, &b, 1, nullptr)) return false;
        b &= (uint8_t)~(1 << (bit % 8));
        return WriteProcessMemory(GetCurrentProcess(), (LPVOID)addr, &b, 1, nullptr) != 0;
    }

    static std::atomic<uint32_t> g_apGrantedWeaponBits{ 0 };

    static void GrantWeapon(int64_t itemId) {
        const WeaponBit* wb = nullptr;
        for (const auto& w : g_apWeaponBits) if (w.itemId == itemId) { wb = &w; break; }
        if (!wb) return;

        bool ok = SetWeaponBit(wb->bit);
        if (wb->altBit >= 0) ok = SetWeaponBit(wb->altBit) && ok;

        if (ok) {
            Log(std::string("Granted weapon: ") + wb->label + " - now owned on the weapon wheel (from AP).");
        }
        else {
            Log(std::string("Failed to grant weapon: ") + wb->label + " (couldn't write ownership bit - player/save not loaded?).");
        }
    }

    struct LPMap { int64_t lpItemId; int64_t weaponItemId; const char* label; };
    static const LPMap g_apLPs[] = {
        { 50050, 50002, "LP - Trois Marches Militaires (Onyx Roses)" },
        { 50051, 50004, "LP - Fantaisie-Impromptu (Kulshedra)" },
        { 50052, 50005, "LP - Sonate in D K.448 (Durga)" },
        { 50053, 50006, "LP - Les Patineurs Waltz op.183 (Odette)" },
        { 50054, 50007, "LP - Walkurenritt (Lt. Col. Kilgore)" },
        { 50055, 50003, "LP - Quasi una Fantasia (Shuraba)" },
    };

    struct WeaponLoc {
        int64_t locationId;
        int baseBit;
        int altBit;
        const char* label;
    };

    static const WeaponLoc g_apWeaponLocs[] = {
        { 60001, 22, 10, "Onyx Roses" },
        { 60002, 21, -1, "Shuraba" },
        { 60003, 20, -1, "Kulshedra" },
        { 60004, 17,  9, "Durga" },
        { 60005, 15, -1, "Odette" },
        { 60006, 16,  8, "Lt. Col. Kilgore" },
    };

    static std::map<int64_t, int64_t> g_apLpToWeaponLoc;
    static std::set<int64_t> g_apReceivedLPs;
    static std::set<int64_t> g_apSentWeaponChecks;
    static std::set<int64_t> g_apPendingLPTurnIn;

    static void GrantLP(int64_t lpItemId) {
        g_apReceivedLPs.insert(lpItemId);

        bool checkSent = false;
        std::string locLabel = "Unknown Weapon";

        if (g_apClient && g_apConnected) {
            auto locIt = g_apLpToWeaponLoc.find(lpItemId);
            if (locIt != g_apLpToWeaponLoc.end()) {
                int64_t locId = locIt->second;

                for (const auto& wl : g_apWeaponLocs) {
                    if (wl.locationId == locId) { locLabel = wl.label; break; }
                }

                if (g_apSentWeaponChecks.count(locId) == 0) {
                    g_apSentWeaponChecks.insert(locId);
                    SendLocationCheckSafe(locId);
                    checkSent = true;
                    AddNotification("✓ Checked: " + locLabel, ImVec4(0.3f, 0.8f, 1.0f, 1.0f));
                }
                else {
                    Log("LP Shop Check Skipped: Location '" + locLabel + "' was already checked on the server.");
                }
            }
            else {
                Log("ERROR: LP ID " + std::to_string(lpItemId) + " not found in g_apLpToWeaponLoc mapping!");
            }
        }

        for (const auto& lp : g_apLPs) {
            if (lp.lpItemId == lpItemId) {
                for (const auto& w : g_apWeaponBits) {
                    if (w.itemId == lp.weaponItemId) {
                        g_apGrantedWeaponBits.fetch_or(1u << w.bit);
                        if (w.altBit >= 0) g_apGrantedWeaponBits.fetch_or(1u << w.altBit);
                        break;
                    }
                }

                GrantWeapon(lp.weaponItemId);

                if (checkSent) {
                    Log(std::string("Received LP ") + lp.label + " - weapon granted and location check sent.");
                }
                else {
                    Log(std::string("Received LP ") + lp.label + " - weapon granted (check skipped).");
                }
                return;
            }
        }
        Log("LP ID " + std::to_string(lpItemId) + " received but not mapped to a weapon - not recorded.");
    }

    struct TechniqueDef {
        int64_t  itemId;
        int64_t  locationId;
        uintptr_t addr;
        int      bit;
        uint32_t enabledMask;
        const char* label;
    };
    static constexpr uintptr_t TECHNIQUE_ENABLED_ADDR = 0x5AA74A4;

    static const TechniqueDef g_apTechniques[] = {
        { 50500, 60201, 0x5AA74A9, 7, (1u << 14) | (1u << 15), "After Burner Kick" },
        { 50501, 60202, 0x5AA74A9, 5, (1u << 13),             "Air Dodge" },
        { 50502, 0,     0x5AA74A7, 5, 0,                      "Beast Within" },
        { 50503, 60203, 0x5AA74AB, 3, 0,                      "Bat Within" },
        { 50504, 60204, 0x5AA74AB, 4, 0,                      "Crow Within" },
        { 50505, 60205, 0x5AA74AA, 0, (1u << 16),             "Breakdance" },
        { 50506, 0,     0x5AA74A6, 4, 0,                      "Bullet Climax" },
        { 50507, 60206, 0x5AA74AA, 4, (1u << 21),             "Heel Slide" },
        { 50508, 60207, 0x5AA74AB, 0, (1u << 24),             "Heel Stomp" },
        { 50509, 60208, 0x5AA74AA, 7, (1u << 23),             "Stiletto" },
        { 50510, 60209, 0x5AA74AB, 1, (1u << 25),             "Tetsuzanko" },
        { 50512, 60210, 0x5AA74AA, 1, (1u << 17),             "Umbran Portal Kick" },
        { 50513, 60211, 0x5AA74A9, 0, (1u << 8),              "Umbran Spear" },
        { 50514, 60212, 0x5AA74AA, 2, (1u << 18),             "Witch Twist" },
    };

    static bool ReadByteAt(uintptr_t addr, uint8_t& out) {
        return ReadProcessMemory(GetCurrentProcess(), (LPCVOID)addr, &out, 1, nullptr) != 0;
    }
    static bool WriteByteAt(uintptr_t addr, uint8_t val) {
        return WriteProcessMemory(GetCurrentProcess(), (LPVOID)addr, &val, 1, nullptr) != 0;
    }

    static bool SetTechniqueBit(const TechniqueDef& t) {
        uint8_t b = 0;
        if (!ReadByteAt(t.addr, b)) return false;
        b |= (uint8_t)(1 << t.bit);
        return WriteByteAt(t.addr, b);
    }
    static bool ClearTechniqueBit(const TechniqueDef& t) {
        uint8_t b = 0;
        if (!ReadByteAt(t.addr, b)) return false;
        b &= (uint8_t)~(1 << t.bit);
        return WriteByteAt(t.addr, b);
    }
    static bool ReadTechniqueBit(const TechniqueDef& t) {
        uint8_t b = 0;
        if (!ReadByteAt(t.addr, b)) return false;
        return (b & (1 << t.bit)) != 0;
    }

    static int32_t g_apLiveRecognizedStage = -1;
    static int32_t g_apLastChapterStage = -1;
    static int32_t g_apActiveChapterStage = -1;
    static int g_apLastStage = -1;

    static std::set<int64_t> g_apGrantedTechniques;
    static std::set<int64_t> g_apSentTechniqueChecks;
    static std::map<int64_t, int> g_apTechDisabledFrames;
    static int g_apSquishFrames = 0;
    static int g_apSquashFrames = 0;
    static int g_apSquashTotal = 0;

    static void GrantTechnique(int64_t itemId) {
        for (const auto& t : g_apTechniques) {
            if (t.itemId == itemId) {
                g_apGrantedTechniques.insert(itemId);

                if (g_apLiveRecognizedStage != 0xF01 && g_apLiveRecognizedStage != 0xA10) {
                    if (SetTechniqueBit(t))
                        Log(std::string("Granted technique: ") + t.label + " (unlocked from AP).");
                    else
                        Log(std::string("Failed to grant technique: ") + t.label + " (player/save not loaded?).");
                }
                else {
                    Log(std::string("Granted technique: ") + t.label + " (will unlock when leaving shop).");
                }
                return;
            }
        }
    }

    struct AccessoryDef {
        int64_t itemId;
        int64_t locationId;
        uintptr_t addr;
        int bit;
        const char* label;
    };

    static const AccessoryDef g_apAccessories[] = {
            { 50101, 0,     0x5AA745E, 4, "Climax Brace" },
            { 50102, 0,     0x5AA745E, 6, "Eternal Testimony" },
            { 50103, 0,     0x5AA745E, 5, "Bracelet of Time" },
            { 50104, 60012, 0x5AA745F, 1, "Evil Harvest Rosary" },
            { 50105, 60013, 0x5AA745F, 0, "Gaze of Despair" },
            { 50106, 60014, 0x5AA745F, 5, "Infernal Communicator" },
            { 50107, 60007, 0x5AA745E, 7, "Moon of Mahaa-Kalaa" },
            { 50108, 60008, 0x5AA745F, 4, "Pulley's Butterfly" },
            { 50109, 60015, 0x5AA745F, 3, "Selene's Light" },
            { 50110, 60016, 0x5AA745F, 2, "Star of Dineta" },
            { 50111, 60017, 0x5AA745F, 6, "Sergey's Lover" },
            { 50112, 60018, 0x5AA745E, 3, "Immortal Marionette" },
    };

    static bool SetAccessoryBit(const AccessoryDef& a) {
        uint8_t b = 0;
        if (!ReadByteAt(a.addr, b)) return false;
        b |= (uint8_t)(1 << a.bit);
        return WriteByteAt(a.addr, b);
    }
    static bool ClearAccessoryBit(const AccessoryDef& a) {
        uint8_t b = 0;
        if (!ReadByteAt(a.addr, b)) return false;
        b &= (uint8_t)~(1 << a.bit);
        return WriteByteAt(a.addr, b);
    }
    static bool ReadAccessoryBit(const AccessoryDef& a) {
        uint8_t b = 0;
        if (!ReadByteAt(a.addr, b)) return false;
        return (b & (1 << a.bit)) != 0;
    }

    static std::set<int64_t> g_apGrantedAccessories;
    static std::set<int64_t> g_apSentAccessoryChecks;

    static void GrantAccessory(int64_t itemId) {
        for (const auto& a : g_apAccessories) {
            if (a.itemId == itemId) {
                g_apGrantedAccessories.insert(itemId);

                if (g_apLiveRecognizedStage != 0xF01 && g_apLiveRecognizedStage != 0xA10) {
                    SetAccessoryBit(a);
                    Log(std::string("Granted accessory: ") + a.label + " (unlocked on wheel from AP).");
                }
                else {
                    Log(std::string("Granted accessory: ") + a.label + " (will unlock on wheel when leaving shop).");
                }
                return;
            }
        }
    }

    static constexpr uintptr_t AREA_JUMP_ADDR = 0x5A978E8;

    struct ChapterStage { int stageId; int chapter; };
    static const ChapterStage g_apChapterStages[] = {
        { 0x1C1, 0 }, // Prologue
        { 0x111, 1 }, { 0x112, 1 }, { 0x113, 1 }, { 0x114, 1 }, { 0x115, 1 },
        { 0x122, 2 }, { 0x123, 2 }, { 0x124, 2 }, { 0x125, 2 }, { 0x126, 2 },
        { 0x12B, 2 }, { 0x12C, 2 },
        { 0x132, 3 }, { 0x133, 3 }, { 0x134, 3 }, { 0x135, 3 }, { 0x137, 3 },
        { 0x151, 4 },
        { 0x200, 5 }, { 0x201, 5 }, { 0x202, 5 }, { 0x203, 5 }, { 0x204, 5 },
        { 0x211, 6 }, { 0x212, 6 }, { 0x213, 6 }, { 0x215, 6 },
        { 0x214, 7 },
        { 0x301, 8 },
        { 0x311, 9 }, { 0x315, 9 },
        { 0x321, 10 }, { 0x322, 10 }, { 0x323, 10 }, { 0x325, 10 },
        { 0x32A, 10 }, { 0x32B, 10 },
        { 0x320, 11 },
        { 0x401, 12 }, { 0x402, 12 }, { 0x403, 12 }, { 0x404, 12 },
        { 0x405, 12 }, { 0x408, 12 },
        { 0x420, 13 }, { 0x421, 13 },
        { 0x501, 14 }, { 0x512, 14 },
        { 0x521, 15 }, { 0x522, 15 }, { 0x523, 15 }, { 0x524, 15 }, { 0x525, 15 },
        { 0x532, 16 },
        { 0x5A1, 17 }, { 0x5A2, 17 },
    };

    static int ChapterForStage(int stageId) {
        for (const auto& cs : g_apChapterStages)
            if (cs.stageId == stageId) return cs.chapter;
        return -1;
    }

    static constexpr int RESULT_STAGE = 0xA20;

    static bool IsRecognizedStage(int stageId) {
        if (ChapterForStage(stageId) >= 0) return true;
        static const int recognized[] = { 0x0, 0xA00, 0xA10, 0xA20, 0x1A1, 0xB00, 0xC00, 0xF01 };
        for (int s : recognized) if (s == stageId) return true;
        return false;
    }

    static int64_t ChapterCompleteLocation(int chapter) {
        return 610000 + (int64_t)chapter * 100 + 99;
    }

    static constexpr int MAX_CHAPTER = 17;
    static constexpr int REQUIEM_CHAPTER = 17;

    static int g_apMacguffinsRequired = 10;
    static int g_apMacguffinCount = 0;
    static bool g_apGoalSent = false;

    static std::string ChapterLabel(int chapter) {
        if (chapter == 0) return "Prologue";
        if (chapter == REQUIEM_CHAPTER) return "Requiem";
        return "Chapter " + std::to_string(chapter);
    }

    static bool g_apChapterInit = false;
    static std::set<int> g_apCompletedChapters;

    static constexpr uintptr_t CURRENT_VERSE_ADDR = 0x5BB5A10;
    static constexpr uintptr_t VERSE_COMPLETE_ADDR = 0x5AA81FC;

    static int64_t VerseLocation(int chapter, int verse) {
        return 610000 + (int64_t)chapter * 100 + verse;
    }

    static bool InChapterStage() {
        if (ChapterForStage(g_apLiveRecognizedStage) < 0) return false;
        int32_t currentVerse = 0;
        ReadProcessMemory(GetCurrentProcess(), (LPCVOID)CURRENT_VERSE_ADDR, &currentVerse, sizeof(currentVerse), nullptr);
        return currentVerse > 0;
    }

    static int g_apLastVerse = -1;
    static int g_apVerseChapter = -1;
    static bool g_apVerseInit = false;
    static std::set<int> g_apCompletedVerses;
    static std::set<int64_t> g_apCompletedAlfheims;
    static std::set<int64_t> g_apSentRankChecks;
    static std::set<int64_t> g_apCheckedLocations;

    static int g_apVerseRankTarget = 0;
    static uintptr_t g_apRankAddrBase = 0;

    static constexpr uintptr_t RANK_TABLE_BASE = 0x5AA8228;
    static constexpr uintptr_t RANK_VERSE_STRIDE = 0x14;
    static constexpr int       RANK_MAX_VERSE = 20;

    static uintptr_t RankAddrFor(int /*chapter*/, int verse) {
        if (verse < 1 || verse > RANK_MAX_VERSE) return 0;
        return RANK_TABLE_BASE + (uintptr_t)(verse - 1) * RANK_VERSE_STRIDE;
    }

    static bool RankMeetsTarget(int rank, int target) {
        switch (target) {
        case 1: return rank >= 0;
        case 2: return rank >= 2;
        case 3: return rank >= 3;
        case 4: return rank >= 4;
        case 5: return rank >= 5;
        }
        return false;
    }

    static int64_t RankLocation(int chapter, int verse, int tier) {
        int offset = (tier - 1) * 2000;
        if (chapter == 0) return 620000 + offset + verse;
        return 620000 + offset + (chapter * 100) + verse;
    }

    struct ChestLocation {
        int chapter;
        int verse;
        uintptr_t laneAddr;
        int bit;
        int64_t locationId;
    };
    static const ChestLocation g_apChestLocations[] = {
        { 1,  1,  0x5AA77C1, 5, 60301 }, { 1,  4,  0x5AA77B1, 1, 60302 },
        { 2,  5,  0x5AA7821, 4, 60303 }, { 2,  5,  0x5AA7844, 6, 60304 },
        { 2,  5,  0x5AA7844, 5, 60305 }, { 2,  10, 0x5AA7852, 6, 60306 },
        { 3,  2,  0x5AA789C, 3, 60307 }, { 3,  5,  0x5AA78B5, 3, 60308 },
        { 3,  5,  0x5AA78B5, 2, 60309 }, { 3,  8,  0x5AA791B, 1, 60310 },
        { 3,  13, 0x5AA791D, 4, 60311 }, { 3,  13, 0x5AA791C, 2, 60312 },
        { 5,  6,  0x5AA7A1A, 2, 60313 }, { 5,  6,  0x5AA7A1A, 3, 60314 },
        { 5,  11, 0x5AA7A1A, 5, 60315 }, { 5,  12, 0x5AA7A1A, 4, 60316 },
        { 6,  1,  0x5AA7A9F, 6, 60317 }, { 6,  1,  0x5AA7A9F, 7, 60318 },
        { 6,  2,  0x5AA7A9F, 5, 60319 }, { 6,  10, 0x5AA7A98, 0, 60320 },
        { 9,  1,  0x5AA7BA3, 1, 60321 }, { 9,  2,  0x5AA7BA3, 2, 60322 },
        { 9,  5,  0x5AA7BAB, 4, 60323 }, { 9,  5,  0x5AA7BAB, 5, 60324 },
        { 9,  5,  0x5AA7BAB, 3, 60325 }, { 9,  5,  0x5AA7BAB, 2, 60326 },
        { 9,  5,  0x5AA7BAB, 6, 60327 }, { 9,  6,  0x5AA7B9A, 2, 60328 },
        { 9,  7,  0x5AA7B9A, 1, 60329 }, { 9,  10, 0x5AA7BB5, 3, 60330 },
        { 9,  10, 0x5AA7BB5, 2, 60331 }, { 10, 2,  0x5AA7C2D, 6, 60332 },
        { 10, 3,  0x5AA7C33, 6, 60333 }, { 10, 8,  0x5AA7C48, 1, 60334 },
        { 10, 8,  0x5AA7C48, 3, 60335 }, { 10, 10, 0x5AA7C3C, 3, 60336 },
        { 12, 1,  0x5AA7CA6, 7, 60337 }, { 12, 1,  0x5AA7CA7, 0, 60338 },
        { 12, 1,  0x5AA7CA6, 6, 60339 }, { 12, 3,  0x5AA7CAA, 7, 60340 },
        { 15, 1,  0x5AA7E9B, 0, 60341 }, { 15, 11, 0x5AA7EBE, 3, 60342 },
        { 15, 11, 0x5AA7EBE, 2, 60343 }, { 15, 13, 0x5AA7EBE, 1, 60344 },
        { 15, 13, 0x5AA7EBE, 0, 60345 },
    };
    static std::set<int64_t> g_apCompletedChests;

    struct TearLocation {
        int chapter;
        uintptr_t laneAddr;
        int bit;
        int64_t locationId;
    };

    static const std::vector<TearLocation> g_apTearLocations = {
        { 1, 0x5AA77A0, 1, 630001 },
        { 1, 0x5AA77B6, 2, 630002 },
        { 2, 0x5AA781B, 6, 630003 },
        { 2, 0x5AA781B, 5, 630004 },
        { 3, 0x5AA789A, 7, 630005 },
        { 3, 0x5AA791B, 6, 630006 },
        { 5, 0x5AA7A19, 6, 630007 },
        { 5, 0x5AA7A28, 4, 630008 },
        { 6, 0x5AA7AA2, 3, 630009 },
        { 6, 0x5AA7AC7, 3, 630010 },
        { 9, 0x5AA7B9D, 5, 630011 },
        { 9, 0x5AA7BA5, 7, 630012 },
        { 9, 0x5AA7BAE, 6, 630013 },
        { 10, 0x5AA7C35, 7, 630014 },
        { 10, 0x5AA7C3C, 6, 630015 },
        { 12, 0x5AA7CA3, 1, 630016 },
        { 15, 0x5AA7EB4, 0, 630017 },
        { 15, 0x5AA7EB5, 2, 630018 },
    };
    static std::set<int64_t> g_apCompletedTears;

    struct GameItem {
        uintptr_t address;
        int maxQuantity;
        int grantAmount;
        const char* label;
        uintptr_t discoverAddr;
        int discoverBit;
    };
    static std::map<int64_t, GameItem> g_apItemMap;

    static std::atomic<uint32_t> g_apUnlockedChapterMask{ 0 };

    enum class StatKind { None, MaxHP, MaxMP };
    struct FragmentUpgrade {
        uintptr_t fragmentAddr;
        int fragmentsPerFull;
        uintptr_t fullCountAddr;
        StatKind statKind;
        int32_t statBonusPerFull;
        const char* label;
    };
    static std::map<int64_t, FragmentUpgrade> g_apFragmentMap;

    static void InitItemMaps() {
        g_apItemMap[50300] = { 0x5aa74d4, -1, 1, "Green Herb Lollipop",      0x5AA75F3, 4 };
        g_apItemMap[50301] = { 0x5aa74d8, -1, 1, "Mega Green Herb Lollipop", 0x5AA75F3, 3 };
        g_apItemMap[50302] = { 0x5aa74dc, -1, 1, "Purple Magic Lollipop",    0x5AA75F3, 2 };
        g_apItemMap[50303] = { 0x5aa74e0, -1, 1, "Mega Purple Magic Lollipop", 0x5AA75F3, 1 };
        g_apItemMap[50304] = { 0x5aa74e4, -1, 1, "Bloody Rose Lollipop",     0x5AA75F3, 0 };
        g_apItemMap[50305] = { 0x5aa74e8, -1, 1, "Mega Bloody Rose Lollipop", 0x5AA75F2, 7 };
        g_apItemMap[50306] = { 0x5aa74ec, -1, 1, "Yellow Moon Lollipop",     0x5AA75F2, 6 };
        g_apItemMap[50307] = { 0x5aa74f0, -1, 1, "Mega Yellow Moon Lollipop", 0x5AA75F2, 5 };
        g_apItemMap[50308] = { 0x5aa74f4, -1, 1, "Magic Flute",              0x0, 0 };
        g_apItemMap[50309] = { 0x5aa74fc, -1, 1, "Red Hot Shot",             0x5AA75F2, 2 };

        g_apItemMap[50320] = { 0x5aa74d0, -1, 5,  "Unicorn Horn x5",         0x0, 0 };
        g_apItemMap[50321] = { 0x5aa74d0, -1, 10, "Unicorn Horn x10",        0x0, 0 };
        g_apItemMap[50322] = { 0x5aa74d0, -1, 15, "Unicorn Horn x15",        0x0, 0 };
        g_apItemMap[50323] = { 0x5aa74c8, -1, 5,  "Baked Gecko x5",          0x0, 0 };
        g_apItemMap[50324] = { 0x5aa74c8, -1, 10, "Baked Gecko x10",         0x0, 0 };
        g_apItemMap[50325] = { 0x5aa74c8, -1, 15, "Baked Gecko x15",         0x0, 0 };
        g_apItemMap[50326] = { 0x5aa74cc, -1, 5,  "Mandragora Root x5",      0x0, 0 };
        g_apItemMap[50327] = { 0x5aa74cc, -1, 10, "Mandragora Root x10",     0x0, 0 };
        g_apItemMap[50328] = { 0x5aa74cc, -1, 15, "Mandragora Root x15",     0x0, 0 };

        g_apItemMap[50400] = { 0x5aa74b4, -1, 5000,    "5,000 Halos",          0x0, 0 };
        g_apItemMap[50401] = { 0x5aa74b4, -1, 10000,   "10,000 Halos",        0x0, 0 };
        g_apItemMap[50402] = { 0x5aa74b4, -1, 25000,   "25,000 Halos",        0x0, 0 };
        g_apItemMap[50403] = { 0x5aa74b4, -1, 50000,   "50,000 Halos",        0x0, 0 };
        g_apItemMap[50404] = { 0x5aa74b4, -1, 100000,  "100,000 Halos",       0x0, 0 };
        g_apItemMap[50405] = { 0x5aa74b4, -1, 1000,    "1,000 Halos",         0x0, 0 };
        g_apItemMap[50406] = { 0x5aa74b4, -1, 2500,    "2,500 Halos",         0x0, 0 };
        g_apItemMap[50407] = { 0x5aa74b4, -1, 7500,    "7,500 Halos",         0x0, 0 };
        g_apItemMap[50408] = { 0x5aa74b4, -1, 20000,   "20,000 Halos",        0x0, 0 };
        g_apItemMap[50409] = { 0x5aa74b4, -1, 30000,   "30,000 Halos",        0x0, 0 };
        g_apItemMap[50410] = { 0x5aa74b4, -1, 75000,   "75,000 Halos",        0x0, 0 };
        g_apItemMap[50411] = { 0x5aa74b4, -1, 200000,  "200,000 Halos",       0x0, 0 };
        g_apItemMap[50412] = { 0x5aa74b4, -1, 137,     "Enzo's Pocket Change", 0x0, 0 };
        g_apItemMap[50413] = { 0x5aa74b4, -1, 300000,  "300,000 Halos",       0x0, 0 };
        g_apItemMap[50414] = { 0x5aa74b4, -1, 500000,  "500,000 Halos",       0x0, 0 };
        g_apItemMap[50415] = { 0x5aa74b4, -1, 1000000, "1,000,000 Halos",     0x0, 0 };

        g_apFragmentMap[50200] = { 0x5aa7504, 4, 0x5aa7500, StatKind::MaxHP, 400, "Broken Witch Heart" };
        g_apFragmentMap[50202] = { 0x5aa750c, 2, 0x5aa7508, StatKind::MaxMP, 50,  "Broken Moon Pearl" };

        for (const auto& lp : g_apLPs) {
            for (const auto& w : g_apWeaponBits) {
                if (w.itemId == lp.weaponItemId) {
                    for (const auto& wl : g_apWeaponLocs) {
                        if (wl.baseBit == w.bit) {
                            g_apLpToWeaponLoc[lp.lpItemId] = wl.locationId;
                            break;
                        }
                    }
                    break;
                }
            }
        }
    }

    static constexpr uintptr_t HALO_COUNT_ADDR = 0x5aa74b4;

    struct ConsumableSlot { uintptr_t addr; const char* label; };
    static const ConsumableSlot g_apConsumableSlots[] = {
        { 0x5aa74d4, "Green Herb Lollipop" },      { 0x5aa74d8, "Mega Green Herb Lollipop" },
        { 0x5aa74dc, "Purple Magic Lollipop" },    { 0x5aa74e0, "Mega Purple Magic Lollipop" },
        { 0x5aa74e4, "Bloody Rose Lollipop" },     { 0x5aa74e8, "Mega Bloody Rose Lollipop" },
        { 0x5aa74ec, "Yellow Moon Lollipop" },     { 0x5aa74f0, "Mega Yellow Moon Lollipop" },
        { 0x5aa74f4, "Magic Flute" },              { 0x5aa74fc, "Red Hot Shot" },
    };

    static bool ReadPlayerBase(uintptr_t& baseOut) {
        uint32_t playerPtr = 0;
        if (!ReadProcessMemory(GetCurrentProcess(), (LPCVOID)PLAYER_POINTER_ADDR,
            &playerPtr, sizeof(playerPtr), nullptr) || playerPtr == 0)
            return false;
        baseOut = (uintptr_t)playerPtr;
        return true;
    }

    struct TrapNameEntry { int64_t id; const char* name; };
    static const TrapNameEntry g_apTrapNames[] = {
            { 50600, "Pickpocket Trap" },    { 50601, "Bloodletting Trap" },
            { 50602, "Fragile Witch Trap" }, { 50603, "Magic Drain Trap" },
            { 50604, "Amnesia Trap" },       { 50605, "Sticky Fingers Trap" },
            { 50606, "Squish Trap" },        { 50607, "Angel Ambush Trap" },
            { 50608, "Grace & Glory Trap" }, { 50609, "Nemesis Trap" },
            { 50610, "Alfheim Curse Trap" }, { 50611, "Berserk Trap" },
            { 50612, "Gracious & Glorious Trap" },
            { 50613, "Fairness & Fearless Trap" },
            { 50614, "Squash Trap" },
    };

    static int64_t TrapIdForName(const std::string& name) {
        for (const auto& t : g_apTrapNames)
            if (name == t.name) return t.id;
        return 0;
    }

    static const char* TrapNameForId(int64_t id) {
        for (const auto& t : g_apTrapNames)
            if (id == t.id) return t.name;
        return nullptr;
    }

    static void SendTrapLink(int64_t itemId);
    static void EvaluateGoal();
    static bool SyncGraceOver();
    static const ImVec4 TRAP_COLOR{ 1.0f, 0.35f, 0.35f, 1.0f };

#ifndef SPEEDRUN_BUILD
    struct ApPendingSpawn { int entityId; int variant; };
    static std::deque<ApPendingSpawn> g_apPendingSpawns;
    static int g_apSpawnCooldownFrames = 0;

    static void QueueTrapSpawns(const ApPendingSpawn* pool, int poolSize, int count) {
        for (int i = 0; i < count; ++i) {
            if (g_apPendingSpawns.size() >= 9) {
                Log("Spawn queue full - extra ambush enemies dropped.");
                return;
            }
            g_apPendingSpawns.push_back(pool[(GetTickCount() / 7 + (unsigned)i * 13u) % (unsigned)poolSize]);
        }
    }

    static void ProcessPendingSpawns() {
        if (g_apPendingSpawns.empty()) return;
        if (g_apSpawnCooldownFrames > 0) { --g_apSpawnCooldownFrames; return; }
        if (g_apDeathLinkKillActive.load()) return;
        if (GameHook::spawnEntityFromHotkey) return;

        LocalPlayer* player = GameHook::GetLocalPlayer();
        if (!player) return;

        const ApPendingSpawn s = g_apPendingSpawns.front();
        g_apPendingSpawns.pop_front();

        GameHook::hotkeyEntitySpawn.entityID = s.entityId;
        GameHook::hotkeyEntitySpawn.settings.int_4_Variant = s.variant;
        GameHook::hotkeyEntitySpawn.settings.int_8_SpawnModifier = 0;
        GameHook::hotkeyEntitySpawn.settings.float_70_X = player->pos.x;
        GameHook::hotkeyEntitySpawn.settings.float_74_Y = player->pos.y + 1.0f;
        GameHook::hotkeyEntitySpawn.settings.float_78_Z = player->pos.z;
        GameHook::spawnEntityFromHotkey = true;
        g_apSpawnCooldownFrames = 25;
    }
#endif

    static int g_apCurseHitsLeft = 0;
    static int g_apCursePrevHP = -1;
    static void PollCurseTrap() {
        if (g_apCurseHitsLeft <= 0) return;
        uintptr_t base = 0;
        if (!ReadPlayerBase(base)) { g_apCursePrevHP = -1; return; }
        int32_t hp = 0;
        if (!ReadMem(base + HP_OFFSET, hp)) return;
        if (g_apCursePrevHP < 0) { g_apCursePrevHP = hp; return; }

        int delta = g_apCursePrevHP - hp;
        g_apCursePrevHP = hp;

        if (delta >= 50) {
            g_apCurseHitsLeft--;
            if (g_apCurseHitsLeft <= 0) {
                WriteMem(base + HPUNK_OFFSET, 1);
                WriteMem(base + HP_OFFSET, 1);
                AddNotification("\xE2\x98\xA0 Curse fulfilled!", TRAP_COLOR, 6.0f);
                Log("Alfheim Curse: reduced to 1 HP.");
                g_apCurseHitsLeft = 0;
            }
            else {
                AddNotification("\xE2\x98\xA0 " + std::to_string(g_apCurseHitsLeft) + " hits left...", TRAP_COLOR, 4.0f);
            }
        }
    }

    static void EnrageAllEnemies() {
        uintptr_t enemyListPtr = 0;
        if (!ReadProcessMemory(GetCurrentProcess(), (LPCVOID)0x5A56A88, &enemyListPtr, sizeof(enemyListPtr), nullptr)) return;
        if (enemyListPtr == 0) return;

        int enemyCount = 0;
        ReadProcessMemory(GetCurrentProcess(), (LPCVOID)(enemyListPtr + 0x30), &enemyCount, sizeof(enemyCount), nullptr);
        if (enemyCount <= 0 || enemyCount > 64) return;

        uintptr_t enemyArray = 0;
        ReadProcessMemory(GetCurrentProcess(), (LPCVOID)(enemyListPtr + 0x34), &enemyArray, sizeof(enemyArray), nullptr);
        if (enemyArray == 0) return;

        constexpr int ENRAGE_OFFSET = 0x5A4;
        for (int i = 0; i < enemyCount; ++i) {
            uintptr_t enemy = 0;
            ReadProcessMemory(GetCurrentProcess(), (LPCVOID)(enemyArray + i * 4), &enemy, sizeof(enemy), nullptr);
            if (enemy == 0) continue;

            float enrageTime = 600.0f;
            WriteProcessMemory(GetCurrentProcess(), (LPVOID)(enemy + ENRAGE_OFFSET), &enrageTime, sizeof(enrageTime), nullptr);
        }
    }

    static void ProcessTrap(int64_t itemId) {
        switch (itemId) {
        case 50600: {
            int32_t halos = 0;
            if (ReadMem(HALO_COUNT_ADDR, halos) && halos > 0) {
                int32_t take = halos < 10000 ? halos : 10000;
                WriteMem(HALO_COUNT_ADDR, halos - take);
                RingLinkNoteOwnWrite();
                AddNotification("\xE2\x98\xA0 Pickpocket Trap! Lost " + std::to_string(take) + " Halos.", TRAP_COLOR, 6.0f);
                Log("Pickpocket Trap: -" + std::to_string(take) + " Halos.");
            }
            else Log("Pickpocket Trap fizzled (no Halos to steal).");
            return;
        }
        case 50601: {
            uintptr_t base = 0; int32_t hp = 0;
            if (ReadPlayerBase(base) && ReadMem(base + HP_OFFSET, hp) && hp > 2) {
                g_apDamageSuppressFrames.store(90);
                WriteMem(base + HPUNK_OFFSET, hp / 2);
                WriteMem(base + HP_OFFSET, hp / 2);
                AddNotification("\xE2\x98\xA0 Bloodletting Trap! HP halved.", TRAP_COLOR, 6.0f);
                Log("Bloodletting Trap: HP " + std::to_string(hp) + " -> " + std::to_string(hp / 2) + ".");
            }
            else Log("Bloodletting Trap fizzled (not in gameplay or HP too low).");
            return;
        }
        case 50602: {
            uintptr_t base = 0; int32_t hp = 0;
            if (ReadPlayerBase(base) && ReadMem(base + HP_OFFSET, hp) && hp > 1) {
                g_apDamageSuppressFrames.store(90);
                WriteMem(base + HPUNK_OFFSET, 1);
                WriteMem(base + HP_OFFSET, 1);
                AddNotification("\xE2\x98\xA0 Fragile Witch Trap! 1 HP - don't get hit!", TRAP_COLOR, 6.0f);
                Log("Fragile Witch Trap: HP set to 1.");
            }
            else Log("Fragile Witch Trap fizzled (not in gameplay or already at 1 HP).");
            return;
        }
        case 50603: {
            WriteMemF(MP_CUR_ADDR, 0.0f);
            AddNotification("\xE2\x98\xA0 Magic Drain Trap! Magic gauge emptied.", TRAP_COLOR, 6.0f);
            Log("Magic Drain Trap: magic gauge set to 0.");
            return;
        }
        case 50604: {
            int ownedIdx[32]; int ownedCount = 0;
            int total = (int)(sizeof(g_apTechniques) / sizeof(g_apTechniques[0]));
            for (int i = 0; i < total; ++i) {
                if (g_apTechDisabledFrames.count(g_apTechniques[i].itemId)) continue;
                if (ReadTechniqueBit(g_apTechniques[i])) ownedIdx[ownedCount++] = i;
            }
            if (ownedCount == 0) { Log("Amnesia Trap fizzled (no techniques to forget)."); return; }
            const TechniqueDef& t = g_apTechniques[ownedIdx[GetTickCount() % ownedCount]];

            ClearTechniqueBit(t);
            if (t.enabledMask != 0) {
                uint32_t mask = 0;
                ReadProcessMemory(GetCurrentProcess(), (LPCVOID)TECHNIQUE_ENABLED_ADDR, &mask, sizeof(mask), nullptr);
                mask &= ~t.enabledMask;
                WriteProcessMemory(GetCurrentProcess(), (LPVOID)TECHNIQUE_ENABLED_ADDR, &mask, sizeof(mask), nullptr);
            }
            g_apTechDisabledFrames[t.itemId] = 30 * 60;
            AddNotification("\xE2\x98\xA0 Amnesia Trap! Forgot " + std::string(t.label) + " for 30 seconds.", TRAP_COLOR, 8.0f);
            Log("Amnesia Trap: " + std::string(t.label) + " disabled for 30 seconds.");
            return;
        }
        case 50605: {
            int haveIdx[16]; int haveCount = 0; int32_t counts[16];
            int total = (int)(sizeof(g_apConsumableSlots) / sizeof(g_apConsumableSlots[0]));
            for (int i = 0; i < total; ++i) {
                int32_t c = 0;
                if (ReadMem(g_apConsumableSlots[i].addr, c) && c > 0) { counts[haveCount] = c; haveIdx[haveCount++] = i; }
            }
            if (haveCount == 0) { Log("Sticky Fingers Trap fizzled (nothing to steal)."); return; }
            int pick = (int)(GetTickCount() % haveCount);
            const ConsumableSlot& s = g_apConsumableSlots[haveIdx[pick]];

            int32_t take = counts[pick] < 3 ? counts[pick] : 3;
            WriteMem(s.addr, counts[pick] - take);
            AddNotification("\xE2\x98\xA0 Sticky Fingers Trap! Lost " + std::to_string(take) + "x " + s.label + ".", TRAP_COLOR, 6.0f);
            Log("Sticky Fingers Trap: -" + std::to_string(take) + " " + std::string(s.label) + ".");
            return;
        }
        case 50606: {
            uintptr_t base = 0;
            if (!ReadPlayerBase(base)) { Log("Squish Trap fizzled (not in gameplay)."); return; }
            g_apSquishFrames = 20 * 60;
            AddNotification("\xE2\x98\xA0 Squish Trap! Flattened for 20 seconds.", TRAP_COLOR, 6.0f);
            Log("Squish Trap: player scale squashed for 20 seconds.");
            return;
        }
        case 50607: {
#ifndef SPEEDRUN_BUILD
            static const ApPendingSpawn kAmbushPool[] = {
                { 0x20000, 1 }, { 0x20000, 2 }, { 0x20000, 9 },
                { 0x20000, 4 }, { 0x20000, 5 }, { 0x20000, 6 },
            };
            QueueTrapSpawns(kAmbushPool, 6, 3);
            AddNotification("\xE2\x98\xA0 Angel Ambush! They found you.", TRAP_COLOR, 8.0f);
            Log("Angel Ambush Trap: 3 angels incoming.");
#else
            Log("Angel Ambush Trap ignored (speedrun build).");
#endif
            return;
        }
        case 50608: {
#ifndef SPEEDRUN_BUILD
            static const ApPendingSpawn kDuo[] = { { 0x20040, 0 }, { 0x20041, 0 } };
            if (g_apPendingSpawns.size() + 2 <= 9) {
                g_apPendingSpawns.push_back(kDuo[0]);
                g_apPendingSpawns.push_back(kDuo[1]);
            }
            else Log("Spawn queue full - Grace & Glory dropped.");
            AddNotification("\xE2\x98\xA0 Grace & Glory have entered the hunt!", TRAP_COLOR, 8.0f);
            Log("Grace & Glory Trap: duo incoming.");
#else
            Log("Grace & Glory Trap ignored (speedrun build).");
#endif
            return;
        }
        case 50609: {
#ifndef SPEEDRUN_BUILD
            static const ApPendingSpawn kJeanne[] = { { 0x21000, 0 } };
            if (g_apPendingSpawns.size() < 9) g_apPendingSpawns.push_back(kJeanne[0]);
            else Log("Spawn queue full - Nemesis dropped.");
            AddNotification("\xE2\x98\xA0 Nemesis Trap! Jeanne has come for you.", TRAP_COLOR, 8.0f);
            Log("Nemesis Trap: enemy Jeanne incoming.");
#else
            Log("Nemesis Trap ignored (speedrun build).");
#endif
            return;
        }
        case 50610: {
            g_apCurseHitsLeft = 3; g_apCursePrevHP = -1;
            AddNotification("\xE2\x98\xA0 Alfheim Curse! 3 hits until doom.", TRAP_COLOR, 8.0f);
            Log("Alfheim Curse activated.");
            return;
        }
        case 50611: {
            EnrageAllEnemies();
            AddNotification("\xE2\x98\xA0 Berserk! All enemies enraged.", TRAP_COLOR, 8.0f);
            Log("Berserk Trap fired.");
            return;
        }
        case 50612: {
#ifndef SPEEDRUN_BUILD
            static const ApPendingSpawn kDuo[] = { { 0x20042, 0 }, { 0x20043, 0 } };
            if (g_apPendingSpawns.size() + 2 <= 9) {
                g_apPendingSpawns.push_back(kDuo[0]);
                g_apPendingSpawns.push_back(kDuo[1]);
            }
            else Log("Spawn queue full - Gracious & Glorious dropped.");
            AddNotification("\xE2\x98\xA0 Gracious & Glorious join the fray!", TRAP_COLOR, 8.0f);
            Log("Gracious & Glorious Trap: duo spawned.");
#else
            Log("Gracious & Glorious Trap ignored (speedrun).");
#endif
            return;
        }
        case 50613: {
#ifndef SPEEDRUN_BUILD
            static const ApPendingSpawn kFairnessFearlessDuo[] = { { 0x20050, 0 }, { 0x20051, 0 } };
            if (g_apPendingSpawns.size() + 2 <= 9) {
                g_apPendingSpawns.push_back(kFairnessFearlessDuo[0]);
                g_apPendingSpawns.push_back(kFairnessFearlessDuo[1]);
            }
            else Log("Spawn queue full - Fairness & Fearless dropped.");
            AddNotification("\xE2\x98\xA0 Fairness & Fearless have entered the hunt!", TRAP_COLOR, 8.0f);
            Log("Fairness & Fearless Trap: duo incoming.");
#else
            Log("Fairness & Fearless Trap ignored (speedrun build).");
#endif
            return;
        }
        case 50614: {
            uintptr_t base = 0;
            if (!ReadPlayerBase(base)) { Log("Squash Trap fizzled (not in gameplay)."); return; }
            WriteMem(base + 0x34C, 439);
            WriteMem(base + 0x350, 0);
            WriteMemF(base + 0x3E4, 0.0f);
            AddNotification("\xE2\x98\xA0 Squash Trap! Cartoon flattened!", TRAP_COLOR, 6.0f);
            Log("Squash Trap: player forced into Flattened state.");
            return;
        }
        }
    }

    static constexpr uintptr_t PLAYER_SCALE_OFFSET = 0xF0;
    static void ProcessTrapTimers() {
#ifndef SPEEDRUN_BUILD
        ProcessPendingSpawns();
#endif
        if (g_apSquishFrames > 0) {
            uintptr_t base = 0;
            bool havePlayer = ReadPlayerBase(base);
            if (havePlayer) {
                WriteMemF(base + PLAYER_SCALE_OFFSET + 0x0, 1.3f);
                WriteMemF(base + PLAYER_SCALE_OFFSET + 0x4, 0.2f);
                WriteMemF(base + PLAYER_SCALE_OFFSET + 0x8, 1.3f);
            }
            if (--g_apSquishFrames == 0) {
                if (havePlayer) {
                    WriteMemF(base + PLAYER_SCALE_OFFSET + 0x0, 1.0f);
                    WriteMemF(base + PLAYER_SCALE_OFFSET + 0x4, 1.0f);
                    WriteMemF(base + PLAYER_SCALE_OFFSET + 0x8, 1.0f);
                }
                AddNotification("Back to full height!", ImVec4(0.4f, 1.0f, 0.4f, 1.0f), 5.0f);
                Log("Squish Trap expired: player scale restored.");
            }
        }
        if (g_apSquashFrames > 0) {
            LocalPlayer* player = GameHook::GetLocalPlayer();
            if (player) {
                int elapsed = g_apSquashTotal - g_apSquashFrames;
                int desiredPart = (elapsed < 30) ? 3 : (elapsed < 90) ? 5 : 7;
                if (player->moveID != 439 || player->movePart != desiredPart) {
                    player->moveID = 439;
                    player->movePart = desiredPart;
                    player->animFrame = 0.0f;
                }
            }
            if (--g_apSquashFrames == 0) {
                if (player && player->moveID == 439) {
                    player->moveID = 0;
                    player->movePart = 1;
                }
                AddNotification("Back to normal!", ImVec4(0.4f, 1.0f, 0.4f, 1.0f), 5.0f);
                Log("Squash Trap expired.");
            }
        }

        if (g_apTechDisabledFrames.empty()) return;

        for (auto it = g_apTechDisabledFrames.begin(); it != g_apTechDisabledFrames.end(); ) {
            if (--(it->second) > 0) {
                ++it;
                continue;
            }

            int64_t itemId = it->first;
            g_apTechDisabledFrames.erase(it++);

            for (const auto& t : g_apTechniques) {
                if (t.itemId != itemId) continue;
                SetTechniqueBit(t);
                if (t.enabledMask != 0) {
                    uint32_t mask = 0;
                    ReadProcessMemory(GetCurrentProcess(), (LPCVOID)TECHNIQUE_ENABLED_ADDR, &mask, sizeof(mask), nullptr);
                    mask |= t.enabledMask;
                    WriteProcessMemory(GetCurrentProcess(), (LPVOID)TECHNIQUE_ENABLED_ADDR, &mask, sizeof(mask), nullptr);
                }
                AddNotification("You remembered " + std::string(t.label) + "!", ImVec4(0.4f, 1.0f, 0.4f, 1.0f), 5.0f);
                Log("Amnesia Trap expired: " + std::string(t.label) + " restored.");
                break;
            }
        }
    }

    static std::mutex g_apQueueMutex;
    static std::queue<int64_t> g_apItemQueue;
    static std::queue<int64_t> g_apTrapQueue;
    static std::atomic<bool> g_apTrackerDirty{ true };

    static void ProcessItem(int64_t itemId) {
        if (itemId == 50515) {
            g_apWitchTimeLocked = false;
            Log("Granted Witch Time (from AP).");
            AddNotification("Witch Time unlocked!", ImVec4(0.4f, 1.0f, 0.4f, 1.0f), 6.0f);
            g_apTrackerDirty.store(true);
            return;
        }

        if (itemId == 50800) {
            g_apUnlockedPunch.store(true);
            Log("Punch Unlocked!");
            AddNotification("Punch Unlocked!", ImVec4(0.4f, 1.0f, 0.4f, 1.0f), 6.0f);
            g_apTrackerDirty.store(true);
            return;
        }
        if (itemId == 50801) {
            g_apUnlockedKick.store(true);
            Log("Kick Unlocked!");
            AddNotification("Kick Unlocked!", ImVec4(0.4f, 1.0f, 0.4f, 1.0f), 6.0f);
            g_apTrackerDirty.store(true);
            return;
        }
        if (itemId == 50803) {
            g_apUnlockedTorture.store(true);
            Log("Torture Attacks Unlocked!");
            AddNotification("Torture Attacks Unlocked!", ImVec4(0.4f, 1.0f, 0.4f, 1.0f), 6.0f);
            g_apTrackerDirty.store(true);
            return;
        }
        if (itemId == 50804) {
            g_apUnlockedAngelArms.store(true);
            Log("Angel Arms Unlocked!");
            AddNotification("Angel Arms Unlocked!", ImVec4(0.4f, 1.0f, 0.4f, 1.0f), 6.0f);
            g_apTrackerDirty.store(true);
            return;
        }

        auto fragIt = g_apFragmentMap.find(itemId);
        if (fragIt != g_apFragmentMap.end()) {
            const auto& cfg = fragIt->second;
            int32_t frags = 0;
            ReadMem(cfg.fragmentAddr, frags);
            frags++;
            bool completed = false;
            if (frags >= cfg.fragmentsPerFull) { frags = 0; completed = true; }
            WriteMem(cfg.fragmentAddr, frags);

            if (completed) {
                if (cfg.statKind == StatKind::MaxHP) {
                    g_apHeartAnimFrames = 180;
                    g_apHeartBonusCount.fetch_add(1);
                    g_apForceMaxHP.store(true);
                    ApplyHeartUpgrade();
                    WriteMem(0x5AA7500, 0);
                    g_apGhostCleanFrames = 180;
                    int32_t newMaxHP = HP_BASE + g_apHeartBonusCount.load() * HP_PER_HEART;
                    Log(std::string("Completed a ") + cfg.label + " - max HP now " +
                        std::to_string(newMaxHP) + " (enforced each frame).");
                }
                else if (cfg.statKind == StatKind::MaxMP) {
                    GiveMoonPearlOrb();
                    g_apGhostCleanFrames = 180;
                    Log(std::string("Completed a ") + cfg.label + " - +1 magic orb (+50 MP).");
                }
                else {
                    Log(std::string("Completed a ") + cfg.label + ".");
                }
            }
            else {
                Log(std::string("Gave 1 ") + cfg.label + " piece (" +
                    std::to_string(frags) + "/" + std::to_string(cfg.fragmentsPerFull) + ").");
            }
            return;
        }

        if (itemId >= 51001 && itemId <= 51017) {
            int chapterNum = (int)(itemId - 51000);
            g_apUnlockedChapterMask.fetch_or(1u << chapterNum);
            Log(ChapterLabel(chapterNum) + " unlocked.");
            g_apTrackerDirty.store(true);
            return;
        }

        if (itemId == 50700) {
            g_apMacguffinCount++;
            AddNotification("✦ Memory Fragment (" + std::to_string(g_apMacguffinCount) +
                "/" + std::to_string(g_apMacguffinsRequired) + ")", ImVec4(0.85f, 0.75f, 1.0f, 1.0f), 8.0f);
            Log("Memory Fragment received (" + std::to_string(g_apMacguffinCount) +
                "/" + std::to_string(g_apMacguffinsRequired) + ").");
            g_apTrackerDirty.store(true);
            EvaluateGoal();
            return;
        }

        if (itemId >= 50600 && itemId <= 50649) {
            if (!SyncGraceOver()) {
                Log("Skipped trap during initial sync (already applied in a previous session).");
                return;
            }
            ProcessTrap(itemId);
            SendTrapLink(itemId);
            return;
        }

        if (itemId >= 50050 && itemId <= 50055) { GrantLP(itemId); return; }

        if (itemId >= 50001 && itemId <= 50011) {
            for (const auto& w : g_apWeaponBits) {
                if (w.itemId == itemId) {
                    g_apGrantedWeaponBits.fetch_or(1u << w.bit);
                    if (w.altBit >= 0) g_apGrantedWeaponBits.fetch_or(1u << w.altBit);
                    break;
                }
            }
            GrantWeapon(itemId);
            return;
        }

        if (itemId >= 50500 && itemId <= 50514) {
            for (const auto& t : g_apTechniques) {
                if (t.itemId == itemId) { GrantTechnique(itemId); g_apTrackerDirty.store(true); return; }
            }
            Log("Technique ID " + std::to_string(itemId) + " received but not mapped.");
            return;
        }

        if (itemId >= 50100 && itemId <= 50112) {
            for (const auto& a : g_apAccessories) {
                if (a.itemId == itemId) { GrantAccessory(itemId); return; }
            }
            Log("Accessory ID " + std::to_string(itemId) + " received but not mapped.");
            return;
        }

        auto it = g_apItemMap.find(itemId);
        if (it != g_apItemMap.end()) {
            if (it->second.address == 0) {
                Log(std::string(it->second.label) + " received, but address is unmapped.");
                return;
            }

            int32_t cur = 0;
            ReadMem(it->second.address, cur);

            int32_t next = cur + it->second.grantAmount;
            if (it->second.maxQuantity > 0 && next > it->second.maxQuantity) next = it->second.maxQuantity;
            WriteMem(it->second.address, next);

            if (it->second.discoverAddr != 0) {
                uint8_t discByte = 0;
                if (ReadProcessMemory(GetCurrentProcess(), (LPCVOID)it->second.discoverAddr, &discByte, 1, nullptr)) {
                    discByte |= (uint8_t)(1 << it->second.discoverBit);
                    WriteProcessMemory(GetCurrentProcess(), (LPVOID)it->second.discoverAddr, &discByte, 1, nullptr);
                }
            }

            if (it->second.address == HALO_COUNT_ADDR) RingLinkNoteOwnWrite();

            Log(std::string("Gave ") + it->second.label + " (" + std::to_string(cur) + " -> " + std::to_string(next) + ").");
            return;
        }
    }

    static void QueueItem(int64_t itemId) {
        std::lock_guard<std::mutex> lock(g_apQueueMutex);
        if (itemId >= 50600 && itemId <= 50649) {
            g_apTrapQueue.push(itemId);
        }
        else {
            g_apItemQueue.push(itemId);
        }
    }

    static void DrainItemQueue() {
        for (int n = 0; n < 64; ++n) {
            int64_t id = -1;
            {
                std::lock_guard<std::mutex> lock(g_apQueueMutex);
                if (g_apItemQueue.empty()) break;
                id = g_apItemQueue.front();
                g_apItemQueue.pop();
            }
            ProcessItem(id);
        }
    }

    static void DrainTrapQueue() {
        if (!InChapterStage()) return;
        for (int n = 0; n < 16; ++n) {
            int64_t id = -1;
            {
                std::lock_guard<std::mutex> lock(g_apQueueMutex);
                if (g_apTrapQueue.empty()) break;
                id = g_apTrapQueue.front();
                g_apTrapQueue.pop();
            }
            ProcessItem(id);
        }
    }

    static bool g_apConnecting = false;
    static bool g_wsaInitialized = false;

    static std::string g_apStatus = "Not connected";

    static int g_apGoal = 0;
    static int g_apGoalChapterCount = 8;
    static bool g_apIncludeChests = true;
    static bool g_apIncludeTears = false;
    static bool g_apIncludeWeapons = true;
    static bool g_apIncludeTechniques = true;
    static bool g_apIncludeAccessories = true;

    static std::atomic<int> g_apSyncGraceFrames{ 0 };

    static bool SyncGraceOver() {
        if (g_apSyncGraceFrames.load() > 0) return false;
        std::lock_guard<std::mutex> lock(g_apQueueMutex);
        return g_apItemQueue.empty() && g_apTrapQueue.empty();
    }

    static bool ShopSyncOver() {
        if (g_apSyncGraceFrames.load() > 0) return false;
        std::lock_guard<std::mutex> lock(g_apQueueMutex);
        return g_apItemQueue.empty();
    }

    static bool ReadWeaponBitfield(uint32_t& out) {
        uint8_t b[3] = { 0, 0, 0 };
        if (!ReadProcessMemory(GetCurrentProcess(), (LPCVOID)WEAPON_OWNERSHIP_ADDR, b, 3, nullptr)) return false;
        out = (uint32_t)b[0] | ((uint32_t)b[1] << 8) | ((uint32_t)b[2] << 16);
        return true;
    }

    static void SendTechniqueLearnCheck(int64_t itemId) {
        if (!g_apClient || !g_apConnected) return;
        for (const auto& t : g_apTechniques) {
            if (t.itemId == itemId) {
                if (t.locationId != 0 && g_apSentTechniqueChecks.count(t.locationId) == 0) {
                    g_apSentTechniqueChecks.insert(t.locationId);
                    SendLocationCheckSafe(t.locationId);
                }
                return;
            }
        }
    }

    static constexpr uintptr_t WEAPON_SLOT_A_HAND = 0x5AA741C;
    static constexpr uintptr_t WEAPON_SLOT_A_FEET = 0x5AA7420;
    static constexpr uintptr_t WEAPON_SLOT_B_HAND = 0x5AA7424;
    static constexpr uintptr_t WEAPON_SLOT_B_FEET = 0x5AA7428;
    static constexpr int32_t   HANDGUNS_WEAPON_ID = 11;

    static void PollWeaponPurchases() {
        if (!g_apClient || !g_apConnected) return;

        bool inShop = (g_apLiveRecognizedStage == 0xF01 || g_apLiveRecognizedStage == 0xA10);
        int chapter = ChapterForStage(g_apLiveRecognizedStage);

        if (chapter < 0 && !inShop) return;

        uintptr_t playerBase = 0;
        if (!ReadPlayerBase(playerBase) || playerBase == 0) return;

        static int throttle = 0;
        if (++throttle < 10) return;
        throttle = 0;

        uint32_t bits = 0;
        if (!ReadWeaponBitfield(bits)) return;

        uint32_t granted = g_apGrantedWeaponBits.load();

        static std::map<int32_t, bool> s_prevBaseOwned;
        static std::map<int32_t, bool> s_prevAltOwned;

        for (const auto& w : g_apWeaponLocs) {
            bool baseOwned = (bits & (1u << w.baseBit)) != 0;
            bool altOwned = (w.altBit >= 0) && ((bits & (1u << w.altBit)) != 0);

            bool apGrantedBase = (granted & (1u << w.baseBit)) != 0;
            bool apGrantedAlt = (w.altBit >= 0) && ((granted & (1u << w.altBit)) != 0);

            bool shopChecked = g_apSentWeaponChecks.count(w.locationId) != 0;

            bool wasBaseOwned = s_prevBaseOwned[w.locationId];
            bool wasAltOwned = s_prevAltOwned[w.locationId];

            if ((baseOwned && !wasBaseOwned) || (altOwned && !wasAltOwned)) {
                if (!shopChecked && inShop) {
                    g_apSentWeaponChecks.insert(w.locationId);
                    SendLocationCheckSafe(w.locationId);
                    AddNotification("✓ Purchased & Checked: " + std::string(w.label), ImVec4(0.3f, 0.8f, 1.0f, 1.0f));
                    Log("Shop purchase detected for " + std::string(w.label) + " - sending location check.");
                    shopChecked = true;
                }
            }

            s_prevBaseOwned[w.locationId] = baseOwned;
            s_prevAltOwned[w.locationId] = altOwned;

            if (!inShop || shopChecked) {
                if (apGrantedBase && !baseOwned) {
                    SetWeaponBit(w.baseBit);
                    bits |= (1u << w.baseBit);
                }
                else if (!apGrantedBase && baseOwned && g_apIncludeWeapons) {
                    ClearWeaponBit(w.baseBit);
                    bits &= ~(1u << w.baseBit);
                }

                if (w.altBit >= 0) {
                    if (apGrantedAlt && !altOwned) {
                        SetWeaponBit(w.altBit);
                        bits |= (1u << w.altBit);
                    }
                    else if (!apGrantedAlt && altOwned && g_apIncludeWeapons) {
                        ClearWeaponBit(w.altBit);
                        bits &= ~(1u << w.altBit);
                    }
                }
            }
        }

        uint32_t scarboroughFairBit = 23;
        bool sfGranted = (granted & (1u << scarboroughFairBit)) != 0;
        bool sfOwned = (bits & (1u << scarboroughFairBit)) != 0;
        if (!sfGranted && sfOwned && g_apIncludeWeapons) {
            ClearWeaponBit(scarboroughFairBit);
            bits &= ~(1u << scarboroughFairBit);
        }

        static bool handgunsInitialized = false;
        if (!inShop && !sfGranted && granted == 0) {
            if (!handgunsInitialized) {
                WriteMem(WEAPON_SLOT_A_HAND, HANDGUNS_WEAPON_ID);
                WriteMem(WEAPON_SLOT_A_FEET, HANDGUNS_WEAPON_ID);
                WriteMem(WEAPON_SLOT_B_HAND, HANDGUNS_WEAPON_ID);
                WriteMem(WEAPON_SLOT_B_FEET, HANDGUNS_WEAPON_ID);
                handgunsInitialized = true;
            }
        }
        else {
            handgunsInitialized = false;
        }
    }

    static void PollTechniqueLearns() {
        if (!g_apClient || !g_apConnected) return;

        bool inShop = (g_apLiveRecognizedStage == 0xF01 || g_apLiveRecognizedStage == 0xA10);
        int chapter = ChapterForStage(g_apLiveRecognizedStage);
        if (chapter < 0 && !inShop) return;

        static int throttle = 0;
        if (++throttle < 10) return;
        throttle = 0;

        uint32_t enabledMask = 0;
        ReadProcessMemory(GetCurrentProcess(), (LPCVOID)TECHNIQUE_ENABLED_ADDR,
            &enabledMask, sizeof(enabledMask), nullptr);
        bool maskDirty = false;

        static std::map<int32_t, bool> s_prevTechMemory;

        for (const auto& t : g_apTechniques) {
            if (g_apTechDisabledFrames.count(t.itemId)) continue;

            bool memoryOwned = ReadTechniqueBit(t);
            bool apGranted = g_apGrantedTechniques.count(t.itemId) != 0;
            bool shopChecked = (t.locationId != 0) && g_apSentTechniqueChecks.count(t.locationId) != 0;

            bool wasOwned = s_prevTechMemory[t.itemId];

            if (t.locationId != 0 && memoryOwned && !wasOwned && !shopChecked && inShop) {
                g_apSentTechniqueChecks.insert(t.locationId);
                SendLocationCheckSafe(t.locationId);
                AddNotification("✓ Learned & Checked: " + std::string(t.label), ImVec4(0.3f, 0.8f, 1.0f, 1.0f));
                Log("Shop purchase detected for technique " + std::string(t.label) + " - sending location check.");
                shopChecked = true;
            }

            s_prevTechMemory[t.itemId] = memoryOwned;

            if (!inShop || shopChecked) {
                if (apGranted && !memoryOwned) {
                    SetTechniqueBit(t);
                    if (t.enabledMask != 0 && (enabledMask & t.enabledMask) == 0) {
                        enabledMask |= t.enabledMask;
                        maskDirty = true;
                    }
                }
                else if (!apGranted && memoryOwned && g_apIncludeTechniques) {
                    ClearTechniqueBit(t);
                    if (t.enabledMask != 0 && (enabledMask & t.enabledMask)) {
                        enabledMask &= ~t.enabledMask;
                        maskDirty = true;
                    }
                }
            }
        }

        if (maskDirty) {
            WriteProcessMemory(GetCurrentProcess(), (LPVOID)TECHNIQUE_ENABLED_ADDR,
                &enabledMask, sizeof(enabledMask), nullptr);
        }
    }

    static void PollAccessoryObtains() {
        if (!g_apClient || !g_apConnected) return;

        bool inShop = (g_apLiveRecognizedStage == 0xF01 || g_apLiveRecognizedStage == 0xA10);

        for (const auto& a : g_apAccessories) {
            if (a.locationId == 0) continue;

            bool memoryOwned = ReadAccessoryBit(a);
            bool apGranted = g_apGrantedAccessories.count(a.itemId) != 0;
            bool shopChecked = g_apSentAccessoryChecks.count(a.locationId) != 0;

            if (memoryOwned && !shopChecked) {
                g_apSentAccessoryChecks.insert(a.locationId);
                SendLocationCheckSafe(a.locationId);
                AddNotification("✓ Bought & Checked: " + std::string(a.label), ImVec4(0.3f, 0.8f, 1.0f, 1.0f));
                Log("Purchase detected for " + std::string(a.label) + " - sending location check.");
                shopChecked = true;
            }

            if (!inShop || shopChecked) {
                if (apGranted && !memoryOwned) {
                    SetAccessoryBit(a);
                }
                else if (!apGranted && memoryOwned && g_apIncludeAccessories) {
                    ClearAccessoryBit(a);
                }
            }
        }
    }

    static void FireFinalVerse(int chapter);

    static void EvaluateGoal() {
        if (!g_apClient || !g_apConnected || g_apGoalSent) return;

        bool goal_met = false;
        int completed_count = (int)g_apCompletedChapters.size();
        bool requiem_done = (g_apCompletedChapters.count(REQUIEM_CHAPTER) > 0);

        switch (g_apGoal) {
        case 0:
            goal_met = (completed_count >= 18);
            break;
        case 1:
            goal_met = (requiem_done && completed_count >= g_apGoalChapterCount);
            break;
        case 2:
            goal_met = (requiem_done &&
                g_apCompletedChapters.count(4) > 0 &&
                g_apCompletedChapters.count(7) > 0 &&
                g_apCompletedChapters.count(11) > 0 &&
                g_apCompletedChapters.count(13) > 0 &&
                g_apCompletedChapters.count(14) > 0 &&
                g_apCompletedChapters.count(16) > 0);
            break;
        case 3:
            goal_met = (completed_count >= g_apGoalChapterCount);
            break;
        case 4:
            goal_met = (g_apMacguffinCount >= g_apMacguffinsRequired);
            break;
        }

        if (goal_met) {
            g_apGoalSent = true;
            g_apClient->StatusUpdate(APClient::ClientStatus::GOAL);
            Log("Goal reached! Sent Victory StatusUpdate to server.");
        }
    }

    static void PollChapterCompletion() {
        if (!g_apClient || !g_apConnected) return;
        if (g_apLiveRecognizedStage == -1) return;
        if (g_apDeathLinkKillActive.load()) return;

        if (g_apLiveRecognizedStage == 0x5A1 || g_apLiveRecognizedStage == 0x5A2) return;

        if (!g_apChapterInit) {
            g_apLastStage = g_apLiveRecognizedStage;
            if (ChapterForStage(g_apLiveRecognizedStage) >= 0) {
                g_apActiveChapterStage = g_apLiveRecognizedStage;
            }
            g_apChapterInit = true;
            return;
        }

        if (g_apLiveRecognizedStage == g_apLastStage) return;

        int prevStage = g_apLastStage;
        g_apLastStage = g_apLiveRecognizedStage;

        int liveCh = ChapterForStage(g_apLiveRecognizedStage);
        if (liveCh >= 0) {
            int32_t currentVerse = 0;
            ReadProcessMemory(GetCurrentProcess(), (LPCVOID)CURRENT_VERSE_ADDR, &currentVerse, sizeof(currentVerse), nullptr);
            if (currentVerse > 0) {
                g_apActiveChapterStage = g_apLiveRecognizedStage;
            }
        }

        if (g_apLiveRecognizedStage != RESULT_STAGE) {
            if (prevStage == 0x1C1 && ChapterForStage(g_apLiveRecognizedStage) == 1) {
                if (g_apCompletedChapters.count(0) == 0) {
                    g_apCompletedChapters.insert(0);
                    FireFinalVerse(0);
                    int64_t loc = ChapterCompleteLocation(0);
                    SendLocationCheckSafe(loc);
                    EvaluateGoal();
                    AddNotification("✓ " + ChapterLabel(0) + " Complete!", ImVec4(1.0f, 0.8f, 0.2f, 1.0f), 10.0f);
                }
            }
            return;
        }

        int finished = g_apVerseChapter;

        if (finished < 0 && prevStage == 0x1C1) {
            finished = 0;
        }

        if (finished < 0 || finished > MAX_CHAPTER) return;
        if (g_apCompletedChapters.count(finished)) return;

        g_apCompletedChapters.insert(finished);
        FireFinalVerse(finished);

        int64_t loc = ChapterCompleteLocation(finished);
        SendLocationCheckSafe(loc);
        EvaluateGoal();

        AddNotification("✓ " + ChapterLabel(finished) + " Complete!", ImVec4(1.0f, 0.8f, 0.2f, 1.0f), 10.0f);

        g_apVerseChapter = -1;
        g_apLastVerse = -1;
    }

    struct AlfheimVerse { int chapter; int verse; int64_t locationId; int alfheimNum; };
    static const AlfheimVerse g_apAlfheimVerses[] = {
        {  1,  3, 60101, 1  }, {  2,  5, 60102, 2  }, {  2,  6, 60103, 3  },
        {  2,  8, 60104, 4  }, {  3,  2, 60105, 5  }, {  3,  5, 60106, 6  },
        {  3, 10, 60107, 7  }, {  3, 11, 60108, 8  }, {  5,  3, 60109, 9  },
        {  5,  8, 60110, 10 }, {  5, 10, 60111, 11 }, {  6,  4, 60112, 12 },
        {  9,  2, 60113, 13 }, {  9,  7, 60114, 14 }, {  9, 11, 60115, 15 },
        { 10,  3, 60116, 16 }, { 10,  9, 60117, 17 }, { 12,  4, 60118, 18 },
        { 12,  7, 60119, 19 }, { 15,  9, 60120, 20 }, { 17,  5, 60121, 21 },
    };

    static bool LocationExists(int64_t loc) {
        if (!g_apClient) return true;
        const auto& missing = g_apClient->get_missing_locations();
        const auto& checked = g_apClient->get_checked_locations();
        if (missing.empty() && checked.empty()) return true;
        if (missing.find(loc) != missing.end()) return true;
        return checked.find(loc) != checked.end();
    }

    static void FireAlfheimForVerse(int chapter, int verse, const std::string& label) {
        for (const auto& av : g_apAlfheimVerses) {
            if (av.chapter != chapter || av.verse != verse) continue;
            if (g_apCompletedAlfheims.count(av.locationId)) return;

            g_apCompletedAlfheims.insert(av.locationId);
            SendLocationCheckSafe(av.locationId);
            AddNotification("✓ Alfheim " + std::to_string(av.alfheimNum) + " Complete!", ImVec4(1.0f, 0.6f, 0.0f, 1.0f));
            return;
        }
    }

    static void FireVerseCheck(int chapter, int verse, uint8_t rankByte) {
        if (chapter < 0 || chapter > MAX_CHAPTER || verse < 1) return;
        int key = chapter * 100 + verse;
        std::string label = ChapterLabel(chapter);

        if (g_apCompletedVerses.count(key) == 0) {
            g_apCompletedVerses.insert(key);
            int64_t loc = VerseLocation(chapter, verse);
            if (!LocationExists(loc)) {
                Log("[verse] " + label + " Verse " + std::to_string(verse) +
                    " -> location " + std::to_string(loc) +
                    " is not in this seed's datapackage.");
            }
            else {
                SendLocationCheckSafe(loc);
                AddNotification("✓ " + label + " Verse " + std::to_string(verse), ImVec4(0.3f, 0.8f, 1.0f, 1.0f));
                FireAlfheimForVerse(chapter, verse, label);
            }
        }

        if (rankByte >= 4 && rankByte <= 8) {
            int earnedTier = rankByte - 3;
            std::list<int64_t> batch;

            for (int t = 1; t <= earnedTier; t++) {
                int64_t rankLoc = RankLocation(chapter, verse, t);

                if (LocationExists(rankLoc) && g_apCheckedLocations.count(rankLoc) == 0) {
                    batch.push_back(rankLoc);
                    g_apCheckedLocations.insert(rankLoc);

                    std::string tierNames[] = { "", "Bronze", "Silver", "Gold", "Platinum", "Pure Platinum" };
                    AddNotification("✓ Verse Rank: " + tierNames[t], ImVec4(0.9f, 0.9f, 0.2f, 1.0f), 6.0f);
                }
            }

            for (int64_t rLoc : batch) {
                SendLocationCheckSafe(rLoc);
            }
        }
    }

    static void PollPendingRankChecks() {
        if (g_apPendingRankChecks.empty()) return;

        for (auto it = g_apPendingRankChecks.begin(); it != g_apPendingRankChecks.end(); ) {
            bool remove = false;
            uintptr_t addr = RankAddrFor(it->chapter, it->verse);
            uint8_t rankByte = 0;
            bool readOk = (addr != 0) && ReadProcessMemory(GetCurrentProcess(), (LPCVOID)addr, &rankByte, 1, nullptr);

            if (readOk && rankByte >= 4 && rankByte <= 8) {
                if (g_apClient && g_apConnected) {
                    FireVerseCheck(it->chapter, it->verse, rankByte);
                }
                remove = true;
            }

            if (!remove && --it->framesLeft <= 0) {
                if (readOk && rankByte >= 4 && rankByte <= 8) {
                    if (g_apClient && g_apConnected) {
                        FireVerseCheck(it->chapter, it->verse, rankByte);
                    }
                }
                remove = true;
            }

            if (remove) it = g_apPendingRankChecks.erase(it);
            else ++it;
        }
    }

    static void PollVerseCompletion() {
        if (!g_apClient || !g_apConnected) return;
        if (g_apLiveRecognizedStage == 0x5A1 || g_apLiveRecognizedStage == 0x5A2) return;

        if (g_apDeathLinkKillActive.load()) {
            int32_t verse = 0;
            ReadProcessMemory(GetCurrentProcess(), (LPCVOID)VERSE_COMPLETE_ADDR,
                &verse, sizeof(verse), nullptr);
            if (verse == 0) {
                g_apDeathLinkKillActive.store(false);
                g_apLastVerse = 0;
                g_apVerseInit = false;
            }
            return;
        }

        int32_t verse = 0;
        if (!ReadProcessMemory(GetCurrentProcess(), (LPCVOID)VERSE_COMPLETE_ADDR,
            &verse, sizeof(verse), nullptr)) return;

        int chapter = -1;
        if (g_apLastChapterStage != -1) chapter = ChapterForStage(g_apLastChapterStage);
        else if (g_apLiveRecognizedStage != -1) chapter = ChapterForStage(g_apLiveRecognizedStage);

        if (!g_apVerseInit) { g_apLastVerse = verse; g_apVerseChapter = chapter; g_apVerseInit = true; return; }
        if (verse >= 1 && chapter >= 0 && (g_apLastVerse < 1 || g_apVerseChapter < 0)) {
            g_apVerseChapter = chapter;
        }

        if (g_apLastVerse >= 1 && verse == 0) {
            int ch = g_apVerseChapter;
            if (ch >= 0) {
                g_apPendingRankChecks.push_back({ ch, g_apLastVerse, ChapterLabel(ch), 300 });
            }
        }
        g_apLastVerse = verse;
    }

    static void FireFinalVerse(int chapter) {
        if (chapter < 0 || chapter > MAX_CHAPTER) return;
        if (g_apVerseChapter == chapter && g_apLastVerse >= 1) {
            g_apPendingRankChecks.push_back({ chapter, g_apLastVerse, ChapterLabel(chapter), 300 });
        }
    }

    static void CheckAutoUnlockRequiem() {
        if (!g_apClient || !g_apConnected) return;
        if (g_apGoal != 1) return;

        int completed = (int)g_apCompletedChapters.size();
        bool requiem_done = (g_apCompletedChapters.count(REQUIEM_CHAPTER) > 0);
        int non_requiem_count = requiem_done ? completed - 1 : completed;

        if (non_requiem_count >= g_apGoalChapterCount - 1) {
            uint32_t mask = g_apUnlockedChapterMask.load();
            if ((mask & (1u << REQUIEM_CHAPTER)) == 0) {
                g_apUnlockedChapterMask.fetch_or(1u << REQUIEM_CHAPTER);
                Log("Requiem automatically unlocked (Chapter Count requirement met).");
                AddNotification("Requiem Unlocked!", ImVec4(0.4f, 1.0f, 0.4f, 1.0f), 8.0f);
                g_apTrackerDirty.store(true);
            }
        }
    }

    static constexpr uintptr_t MODULE_BASE = 0x400000;
    static constexpr uintptr_t PTR_STATIC = MODULE_BASE + 0x576A40C;
    static constexpr uintptr_t OFFSET_1 = 0x98;
    static constexpr uintptr_t OFFSET_2 = 0x70;
    static constexpr uintptr_t UNLOCK_CHECK_ADDR = MODULE_BASE + 0x1E350;

    static uint8_t  g_apUnlockCheckOrigBytes[5] = { 0 };
    static bool     g_apUnlockCheckHooked = false;

    static int __cdecl UnlockCheckHook(int chapterIndex, int variant) {
        (void)variant;
        int allowed = 0;
        if (chapterIndex >= 0 && chapterIndex <= MAX_CHAPTER + 1) {
            if (chapterIndex <= 2) { allowed = 1; }
            else {
                int apChapter = chapterIndex - 1;
                uint32_t mask = g_apUnlockedChapterMask.load(std::memory_order_relaxed);
                allowed = (mask & (1u << apChapter)) ? 1 : 0;
            }
        }
        return allowed;
    }

    static __declspec(naked) void UnlockCheckBridge() {
        __asm {
            mov  eax, [esp + 4]
            mov  edx, [esp + 8]
            push edx
            push eax
            call UnlockCheckHook
            add  esp, 8
            ret
        }
    }

    static void InstallChapterBlockHook() {
        if (g_apUnlockCheckHooked) return;

        ReadProcessMemory(GetCurrentProcess(), (LPCVOID)UNLOCK_CHECK_ADDR,
            g_apUnlockCheckOrigBytes, 5, nullptr);

        uint8_t patch[5];
        patch[0] = 0xE9;
        uint32_t rel = (uint32_t)((uintptr_t)UnlockCheckBridge - (UNLOCK_CHECK_ADDR + 5));
        *(uint32_t*)(patch + 1) = rel;

        DWORD oldProtect;
        if (VirtualProtect((LPVOID)UNLOCK_CHECK_ADDR, 5, PAGE_EXECUTE_READWRITE, &oldProtect)) {
            WriteProcessMemory(GetCurrentProcess(), (LPVOID)UNLOCK_CHECK_ADDR, patch, 5, nullptr);
            VirtualProtect((LPVOID)UNLOCK_CHECK_ADDR, 5, oldProtect, &oldProtect);
            g_apUnlockCheckHooked = true;
        }
    }

    // Kick-band punch IDs
    static bool IsLowRangePunch(int32_t moveId) {
        return moveId == 55 || moveId == 56 || moveId == 57
            || moveId == 67 || moveId == 68;
    }

    int32_t __cdecl FilterMoveID(int32_t moveId) {
        // 1. Torture Attacks
        if (IsTortureMove(moveId)) {
            return g_apUnlockedTorture.load() ? moveId : 0;
        }
        // 2. Angel Arms (Strictly targeted to actual weapon summons 255-281 to keep grabs/slams open)
        if (IsAngelArmMove(moveId)) {
            if (!g_apUnlockedAngelArms.load()) return 0;
            return moveId;
        }
        // 3. Strict Punch Lock
        if (!g_apUnlockedPunch.load() && (moveId == 49 || moveId == 50)) {
            return 0;
        }
        // 4. Kick Lock, carving punch IDs back out of the kick band
        if (!g_apUnlockedKick.load() && (moveId >= 51 && moveId <= 90)) {
            if (g_apUnlockedPunch.load() && IsLowRangePunch(moveId)) return moveId;
            return 0;
        }
        return moveId;
    }

    static uintptr_t g_MoveIDHook_Ret = MODULE_BASE + 0xBD059;
    static __declspec(naked) void MoveIDBridge() {
        __asm {
            push eax
            mov eax, 0xEF5A60
            mov eax, [eax]
            cmp ecx, eax
            pop eax
            jne skip_filter

            push eax
            push ecx

            push edx
            call FilterMoveID
            add esp, 4

            mov edx, eax

            pop ecx
            pop eax

            skip_filter :
            mov[ecx + 0x34C], edx
                jmp[g_MoveIDHook_Ret]
        }
    }

    static bool g_apMoveIDHookInstalled = false;
    static void InstallMoveIDHook() {
        if (g_apMoveIDHookInstalled) return;

        DWORD oldProtect;
        uintptr_t hookAddr = MODULE_BASE + 0xBD053;

        if (VirtualProtect((LPVOID)hookAddr, 6, PAGE_EXECUTE_READWRITE, &oldProtect)) {
            uint8_t patch[6];
            patch[0] = 0xE9;
            *(uint32_t*)(patch + 1) = (uint32_t)((uintptr_t)MoveIDBridge - (hookAddr + 5));
            patch[5] = 0x90;

            WriteProcessMemory(GetCurrentProcess(), (LPVOID)hookAddr, patch, 6, nullptr);
            VirtualProtect((LPVOID)hookAddr, 6, oldProtect, &oldProtect);

            g_apMoveIDHookInstalled = true;
        }
    }

    static int32_t __cdecl FilterNextStage(int32_t proposedStage) {
        int ch = ChapterForStage(proposedStage);
        if (ch < 0) return proposedStage;

        uint32_t mask = g_apUnlockedChapterMask.load();
        if (ch <= 1 || (mask & (1u << ch)) != 0) {
            return proposedStage;
        }

        Log("Blocked seamless transition into locked Chapter " + std::to_string(ch) + ". Redirecting to Main Menu.");
        return 0xA10;
    }

    static uintptr_t g_StageJumpHook_Ret = MODULE_BASE + 0xFC48C;

    static __declspec(naked) void StageJumpBridge() {
        __asm {
            push ecx
            push edx

            push eax
            call FilterNextStage
            add esp, 4

            pop edx
            pop ecx

            mov[esi + 0x828], eax

            jmp[g_StageJumpHook_Ret]
        }
    }

    static bool g_apStageJumpHookInstalled = false;
    static void InstallStageJumpHook() {
        if (g_apStageJumpHookInstalled) return;

        DWORD oldProtect;
        uintptr_t hookAddr = MODULE_BASE + 0xFC486;
        int instructionLength = 6;

        if (VirtualProtect((LPVOID)hookAddr, instructionLength, PAGE_EXECUTE_READWRITE, &oldProtect)) {
            uint8_t patch[16];
            memset(patch, 0x90, instructionLength);

            patch[0] = 0xE9;
            *(uint32_t*)(patch + 1) = (uint32_t)((uintptr_t)StageJumpBridge - (hookAddr + 5));

            WriteProcessMemory(GetCurrentProcess(), (LPVOID)hookAddr, patch, instructionLength, nullptr);
            VirtualProtect((LPVOID)hookAddr, instructionLength, oldProtect, &oldProtect);

            g_apStageJumpHookInstalled = true;
        }
    }

    static void RemoveChapterBlockHook() {
        if (!g_apUnlockCheckHooked) return;
        DWORD oldProtect;
        if (VirtualProtect((LPVOID)UNLOCK_CHECK_ADDR, 5, PAGE_EXECUTE_READWRITE, &oldProtect)) {
            WriteProcessMemory(GetCurrentProcess(), (LPVOID)UNLOCK_CHECK_ADDR,
                g_apUnlockCheckOrigBytes, 5, nullptr);
            VirtualProtect((LPVOID)UNLOCK_CHECK_ADDR, 5, oldProtect, &oldProtect);
            g_apUnlockCheckHooked = false;
        }
    }

    static void PollChapterBlock() {
        if (!g_apClient || !g_apConnected) return;

        InstallChapterBlockHook();
        InstallMoveIDHook();
        InstallStageJumpHook();

        uint32_t mask = g_apUnlockedChapterMask.load();
        int highestUnlocked = 2;
        for (int ch = MAX_CHAPTER; ch >= 3; --ch) {
            if (mask & (1u << ch)) { highestUnlocked = ch; break; }
        }

        static int lastWrittenLimit = -1;
        int32_t limit = highestUnlocked + 1;
        if (limit == lastWrittenLimit) return;

        uintptr_t limitPtr = 0;
        if (ReadProcessMemory(GetCurrentProcess(), (LPCVOID)PTR_STATIC, &limitPtr, sizeof(limitPtr), nullptr) && limitPtr != 0) {
            limitPtr += OFFSET_1;
            if (ReadProcessMemory(GetCurrentProcess(), (LPCVOID)limitPtr, &limitPtr, sizeof(limitPtr), nullptr) && limitPtr != 0) {
                limitPtr += OFFSET_2;
                int32_t currentLimit = 0;
                if (ReadProcessMemory(GetCurrentProcess(), (LPCVOID)limitPtr, &currentLimit, sizeof(currentLimit), nullptr)) {
                    if (currentLimit != limit && currentLimit != 0) {
                        WriteProcessMemory(GetCurrentProcess(), (LPVOID)limitPtr, &limit, sizeof(limit), nullptr);
                        lastWrittenLimit = limit;
                    }
                }
            }
        }
    }

    static void PollChests() {
        if (!g_apClient || !g_apConnected) return;
        if (!g_apIncludeChests) return;

        int chapter = -1;
        if (g_apLastChapterStage != -1) chapter = ChapterForStage(g_apLastChapterStage);
        else if (g_apLiveRecognizedStage != -1) chapter = ChapterForStage(g_apLiveRecognizedStage);

        if (chapter < 0) return;

        for (const auto& chest : g_apChestLocations) {
            if (chest.chapter != chapter) continue;
            if (g_apCompletedChests.count(chest.locationId)) continue;

            uint8_t byte = 0;
            if (!ReadProcessMemory(GetCurrentProcess(), (LPCVOID)chest.laneAddr, &byte, 1, nullptr)) continue;

            if (byte & (1 << chest.bit)) {
                g_apCompletedChests.insert(chest.locationId);
                SendLocationCheckSafe(chest.locationId);

                std::string locName = g_apClient->get_location_name(chest.locationId, "Bayonetta");
                AddNotification("✓ Chest: " + locName, ImVec4(0.8f, 0.6f, 0.2f, 1.0f));
            }
        }
    }

    static void PollTears() {
        if (!g_apClient || !g_apConnected) return;
        if (!g_apIncludeTears) return;

        int chapter = -1;
        if (g_apLastChapterStage != -1) chapter = ChapterForStage(g_apLastChapterStage);
        else if (g_apLiveRecognizedStage != -1) chapter = ChapterForStage(g_apLiveRecognizedStage);

        if (chapter < 0) return;

        for (const auto& tear : g_apTearLocations) {
            if (tear.chapter != chapter) continue;
            if (g_apCompletedTears.count(tear.locationId)) continue;

            uint8_t byte = 0;
            if (!ReadProcessMemory(GetCurrentProcess(), (LPCVOID)tear.laneAddr, &byte, 1, nullptr)) continue;

            if (byte & (1 << tear.bit)) {
                g_apCompletedTears.insert(tear.locationId);
                SendLocationCheckSafe(tear.locationId);

                std::string locName = g_apClient->get_location_name(tear.locationId, "Bayonetta");
                AddNotification("✓ Tear: " + locName, ImVec4(0.9f, 0.2f, 0.2f, 1.0f));
            }
        }
    }

    static std::set<int64_t> g_apAutoHintedShops;
    static bool g_shopScouted = false;

    static void AutoHintShop() {
        if (!g_apClient || !g_apConnected) return;
        if (g_apLiveRecognizedStage != 0xF01 && g_apLiveRecognizedStage != 0xA10) { g_shopScouted = false; return; }
        if (g_shopScouted) return;

        static const int64_t shopLocs[] = {
            60001, 60002, 60003, 60004, 60005, 60006, 60007, 60008,
            60201, 60202, 60203, 60204, 60205, 60206, 60207, 60208,
            60209, 60210, 60211, 60212, 60213, 60214
        };
        std::list<int64_t> toScout;
        const auto& missing = g_apClient->get_missing_locations();

        for (int64_t loc : shopLocs) {
            if (g_apAutoHintedShops.count(loc)) continue;
            if (missing.find(loc) == missing.end()) { g_apAutoHintedShops.insert(loc); continue; }
            toScout.push_back(loc);
        }

        if (!toScout.empty()) {
            g_apClient->LocationScouts(toScout, 0);
            Log("Scouting " + std::to_string(toScout.size()) + " shop items...");
        }
        g_shopScouted = true;
    }

    static int   g_apPrevHPForDamage = -1;
    static int   g_apPrevMaxHPForDamage = -1;
    static float g_apDamageAccumFrac = 0.0f;
    static int   g_apDamageSendCooldown = 0;

    static void PollDamageLink() {
        if (!g_apDamageLinkEnabled.load() || !g_apClient || !g_apConnected) return;
        if (!InChapterStage()) { g_apPrevHPForDamage = -1; return; }

        uintptr_t base = 0;
        if (!ReadPlayerBase(base)) { g_apPrevHPForDamage = -1; return; }

        int32_t hp = 0, maxhp = 0;
        if (!ReadMem(base + HP_OFFSET, hp) || !ReadMem(base + HPMAX_OFFSET, maxhp) || maxhp <= 0) {
            g_apPrevHPForDamage = -1;
            return;
        }

        int suppress = g_apDamageSuppressFrames.load();
        if (suppress > 0) g_apDamageSuppressFrames.store(suppress - 1);

        if (suppress == 0 && g_apPrevHPForDamage > 0 && hp < g_apPrevHPForDamage) {
            float lostFrac = (float)(g_apPrevHPForDamage - hp) / (float)maxhp;
            g_apDamageAccumFrac += lostFrac;
        }

        g_apPrevHPForDamage = hp;
        g_apPrevMaxHPForDamage = maxhp;

        if (g_apDamageSendCooldown > 0) { --g_apDamageSendCooldown; return; }

        if (g_apDamageAccumFrac >= 0.01f) {
            double now = g_apClient->get_server_time();
            g_apLastDamageTime.store(now);
            std::string who = (g_apSlot[0] ? std::string(g_apSlot) : std::string("Bayonetta"));
            nlohmann::json data = { { "time", now }, { "source", who }, { "damage", (double)g_apDamageAccumFrac } };
            g_apClient->Bounce(data, {}, {}, { "DamageLink" });
            Log("DamageLink sent (" + std::to_string((int)(g_apDamageAccumFrac * 100.0f)) + "% HP).");

            g_apDamageAccumFrac = 0.0f;
            g_apDamageSendCooldown = 60;
        }
    }

    static void PollRingLink() {
        if (g_apRingRecvDisplayFrames > 0 && --g_apRingRecvDisplayFrames == 0 && g_apRingRecvDisplayHalos != 0) {
            std::string sign = g_apRingRecvDisplayHalos > 0 ? "+" : "";
            AddNotification("Ring Link: " + sign + std::to_string(g_apRingRecvDisplayHalos) + " Halos.",
                ImVec4(1.0f, 0.85f, 0.3f, 1.0f), 5.0f);
            Log("Ring Link total applied: " + sign + std::to_string(g_apRingRecvDisplayHalos) + " Halos.");
            g_apRingRecvDisplayHalos = 0;
        }
        if (g_apRingSentDisplayFrames > 0 && --g_apRingSentDisplayFrames == 0 && g_apRingSentDisplayRings > 0) {
            Log("Ring Link: sent +" + std::to_string(g_apRingSentDisplayRings) + " rings (" +
                std::to_string(g_apRingSentDisplayRings * 100) + " Halos collected).");
            g_apRingSentDisplayRings = 0;
        }
        if (!g_apRingLinkEnabled.load() || !g_apClient || !g_apConnected) return;

        int32_t halos = 0;
        if (!ReadMem(HALO_COUNT_ADDR, halos) || halos < 0) { g_apPrevHalosForRing = -1; return; }

        if (g_apPrevHalosForRing < 0) { g_apPrevHalosForRing = halos; return; }

        int32_t delta = halos - g_apPrevHalosForRing;
        g_apPrevHalosForRing = halos;

        if (delta > 0) g_apRingGainAccumHalos += delta;
        if (g_apRingSendCooldown > 0) { --g_apRingSendCooldown; return; }

        int rings = g_apRingGainAccumHalos / 100;
        if (rings > 0) {
            g_apRingGainAccumHalos -= rings * 100;
            double now = g_apClient->get_server_time();
            g_apLastRingTime.store(now);
            nlohmann::json data = { { "time", now }, { "source", g_apClient->get_player_number() }, { "amount", rings } };
            g_apClient->Bounce(data, {}, {}, { "RingLink" });

            g_apRingSentDisplayRings += rings;
            g_apRingSentDisplayFrames = 5 * 60;
            g_apRingSendCooldown = 60;
        }
    }

    static void SendTrapLink(int64_t itemId) {
        if (!g_apTrapLinkEnabled.load() || !g_apClient || !g_apConnected) return;
        const char* tn = TrapNameForId(itemId);
        if (!tn) return;

        double now = g_apClient->get_server_time();
        g_apLastTrapTime.store(now);
        std::string who = (g_apSlot[0] ? std::string(g_apSlot) : std::string("Bayonetta"));
        nlohmann::json data = { { "time", now }, { "source", who }, { "trap_name", std::string(tn) } };
        g_apClient->Bounce(data, {}, {}, { "TrapLink" });

        Log(std::string("TrapLink sent: ") + tn + ".");
    }

    static void SendDeathLink() {
        if (!g_apClient || !g_apConnected) return;

        double now = g_apClient->get_server_time();
        g_apLastDeathTime.store(now);
        std::string who = (g_apSlot[0] ? std::string(g_apSlot) : std::string("Bayonetta"));
        nlohmann::json data = { { "time", now }, { "source", who }, { "cause", who + " was slain." } };
        g_apClient->Bounce(data, {}, {}, { "DeathLink" });
        AddNotification("☠ You died!", ImVec4(1.0f, 0.2f, 0.2f, 1.0f), 6.0f);
        Log("DeathLink sent (we died).");
    }

    static void HandleBounce(const nlohmann::json& command) {
        if (!command.contains("data") || !command["data"].is_object()) return;
        const auto& data = command["data"];
        bool tagDeath = false, tagDamage = false, tagTrap = false, tagRing = false, anyTag = false;

        auto tagsIt = command.find("tags");
        if (tagsIt != command.end() && tagsIt->is_array()) {
            for (const auto& t : *tagsIt) {
                if (!t.is_string()) continue;
                anyTag = true;
                const std::string tv = t.get<std::string>();
                if (tv == "DeathLink") tagDeath = true;
                else if (tv == "DamageLink") tagDamage = true;
                else if (tv == "TrapLink") tagTrap = true;
                else if (tv == "RingLink") tagRing = true;
            }
        }
        if (!anyTag && data.contains("time") && data.contains("source") &&
            !data.contains("damage") && !data.contains("trap_name") && !data.contains("amount"))
            tagDeath = true;

        const double t = data.value("time", 0.0);
        const std::string source = data.value("source", std::string("Someone"));
        const std::string ourName = (g_apSlot[0] ? std::string(g_apSlot) : std::string("Bayonetta"));

        if (tagDeath && g_apDeathLinkEnabled.load()) {
            if (t != 0.0 && t == g_apLastDeathTime.load()) return;
            if (source == ourName) return;

            g_apLastDeathTime.store(t);
            std::string cause = data.value("cause", source + " died.");
            { std::lock_guard<std::mutex> lock(g_apDeathMutex); g_apDeathCause = cause; }

            g_apDeathLinkPending.store(true);
            g_apPrevMoveWasDeath = false;
            g_apDeathSuppressFrames.store(600);
            AddNotification("☠ " + source + " died: " + cause, ImVec4(1.0f, 0.2f, 0.2f, 1.0f), 8.0f);
            Log("DeathLink received: " + cause);
            return;
        }

        if (tagDamage && g_apDamageLinkEnabled.load()) {
            if (t != 0.0 && t == g_apLastDamageTime.load()) return;
            if (source == ourName) return;
            g_apLastDamageTime.store(t);

            double frac = data.value("damage", 0.0);
            if (frac <= 0.0) return;
            if (frac > 0.5) frac = 0.5;

            uintptr_t base = 0; int32_t hp = 0, maxhp = 0;
            if (!ReadPlayerBase(base) || !ReadMem(base + HP_OFFSET, hp) ||
                !ReadMem(base + HPMAX_OFFSET, maxhp) || maxhp <= 0 || hp <= 1)
                return;

            int32_t dmg = (int32_t)(frac * (double)maxhp);
            if (dmg < 1) dmg = 1;
            int32_t newHp = hp - dmg;
            if (newHp < 1) newHp = 1;

            g_apDamageSuppressFrames.store(90);
            WriteMem(base + HPUNK_OFFSET, newHp);
            WriteMem(base + HP_OFFSET, newHp);
            AddNotification("☠ Shared pain from " + source + " (-" + std::to_string(hp - newHp) + " HP).", ImVec4(1.0f, 0.5f, 0.3f, 1.0f), 6.0f);
            Log("DamageLink received from " + source + ": -" + std::to_string(hp - newHp) + " HP.");
            return;
        }

        if (tagTrap && g_apTrapLinkEnabled.load()) {
            if (t != 0.0 && t == g_apLastTrapTime.load()) return;
            if (source == ourName) return;
            g_apLastTrapTime.store(t);

            const std::string trapName = data.value("trap_name", std::string());
            int64_t id = TrapIdForName(trapName);
            if (id != 0) {
                Log("TrapLink received from " + source + ": " + trapName + ".");
            }
            else {
                const int total = (int)(sizeof(g_apTrapNames) / sizeof(g_apTrapNames[0]));
                id = g_apTrapNames[GetTickCount() % (unsigned)total].id;
                Log("TrapLink from " + source + (trapName.empty() ? std::string("") : (": '" + trapName + "'")) +
                    " - no Bayonetta match, firing a random trap instead.");
            }
            ProcessTrap(id);
            return;
        }

        if (tagRing && g_apRingLinkEnabled.load()) {
            if (t != 0.0 && t == g_apLastRingTime.load()) return;
            if (data["source"].is_number_integer() && g_apClient &&
                data["source"].get<int>() == g_apClient->get_player_number()) return;
            if (data["source"].is_string() && source == ourName) return;

            g_apLastRingTime.store(t);

            if (!data.contains("amount") || !data["amount"].is_number()) return;
            int32_t deltaHalos = (int32_t)(data["amount"].get<double>() * 100.0);
            if (deltaHalos == 0) return;

            int32_t halos = 0;
            if (!ReadMem(HALO_COUNT_ADDR, halos)) return;

            int32_t next = halos + deltaHalos;
            if (next < 0) next = 0;
            WriteMem(HALO_COUNT_ADDR, next);
            RingLinkNoteOwnWrite();

            g_apRingRecvDisplayHalos += (next - halos);
            g_apRingRecvDisplayFrames = 5 * 60;
        }
    }

    struct TrackedLocation { int64_t id; std::string name; bool checked; bool reachable; };
    static struct TrackerGroup { std::string name; std::vector<TrackedLocation> locations; int checkedCount = 0; };

    static std::vector<TrackerGroup> g_apTrackerGroups;
    static int g_apTrackerTotalChecked = 0;
    static int g_apTrackerTotalLocations = 0;
    static int g_apTrackerAvailableNow = 0;

    static std::map<std::string, int> g_apReceivedItems;
    static std::mutex g_apReceivedMutex;

    static std::string GroupOf(const std::string& name) {
        if (name.rfind("Learn ", 0) == 0) return "Techniques";
        if (name.rfind("Prologue", 0) == 0) return "Prologue";
        auto colon = name.find(':');
        std::string g = (colon != std::string::npos) ? name.substr(0, colon) : name;
        while (!g.empty() && g.back() == ' ') g.pop_back();
        return g.empty() ? "Other" : g;
    }

    static int RomanValue(const std::string& s) {
        static const std::map<char, int> vals = { {'I',1},{'V',5},{'X',10},{'L',50},{'C',100},{'D',500},{'M',1000} };
        int total = 0, prev = 0;
        for (auto it = s.rbegin(); it != s.rend(); ++it) {
            auto vi = vals.find(*it); if (vi == vals.end()) return -1;
            int v = vi->second;
            if (v < prev) total -= v; else { total += v; prev = v; }
        }
        return total > 0 ? total : -1;
    }

    static std::pair<int, int> GroupSortKey(const std::string& groupName) {
        const std::string prefix = "Chapter ";
        if (groupName.rfind(prefix, 0) == 0) { int rv = RomanValue(groupName.substr(prefix.size())); if (rv > 0) return { 0, rv }; }
        return { 1, 0 };
    }

    static std::pair<int, int> LocationSortKey(const std::string& name) {
        int num = 0; bool hasNum = false;
        int i = (int)name.size() - 1;
        while (i >= 0 && name[i] == ' ') --i;
        int end = i;
        while (i >= 0 && name[i] >= '0' && name[i] <= '9') --i;
        if (i < end) { hasNum = true; num = std::atoi(name.c_str() + i + 1); }

        int bucket;
        if (name.find("Prologue Verse") != std::string::npos) bucket = 0;
        else if (name.find("Verse") != std::string::npos)    bucket = 1;
        else bucket = 2;

        return { bucket, hasNum ? num : 0 };
    }

    static int ChapterNumberOfGroup(const std::string& groupName) {
        if (groupName == "Requiem") return REQUIEM_CHAPTER;
        const std::string prefix = "Chapter ";
        if (groupName.rfind(prefix, 0) == 0) { int rv = RomanValue(groupName.substr(prefix.size())); if (rv > 0) return rv; }
        return -1;
    }

    static bool LocationReachable(const std::string& locName, const std::string& groupKey) {
        int chapterNum = ChapterNumberOfGroup(groupKey);
        if (chapterNum > 0) {
            if ((g_apUnlockedChapterMask.load() & (1u << chapterNum)) == 0) return false;
        }

        if (locName == "Chapter V: The Lost Holy Grounds - Complete") { if (g_apGrantedTechniques.count(50502) == 0) return false; }

        static const std::map<std::string, int64_t> shopLP = {
             {"Shop: Buy Onyx Roses", 50050},
             {"Shop: Buy Shuraba", 50055},
             {"Shop: Buy Kulshedra", 50051},
             {"Shop: Buy Durga", 50052},
             {"Shop: Buy Odette", 50053},
             {"Shop: Buy Kilgore", 50054},
        };
        auto lpIt = shopLP.find(locName);
        if (lpIt != shopLP.end() && g_apReceivedLPs.count(lpIt->second) == 0) return false;

        return true;
    }

    static void RebuildTrackerSnapshot() {
        g_apTrackerGroups.clear();
        g_apTrackerTotalChecked = 0;
        g_apTrackerTotalLocations = 0;
        g_apTrackerAvailableNow = 0;

        if (!g_apClient) return;

        const std::string game = "Bayonetta";
        std::set<int64_t> checked = g_apClient->get_checked_locations();
        std::set<int64_t> missing = g_apClient->get_missing_locations();

        std::map<std::string, TrackerGroup> groups;

        auto addLoc = [&](int64_t id, bool isChecked) {
            std::string name = g_apClient->get_location_name(id, game);
            std::string key = GroupOf(name);

            g_apTrackerTotalLocations++;
            if (isChecked) g_apTrackerTotalChecked++;

            bool reachable = LocationReachable(name, key);
            if (!isChecked && reachable) g_apTrackerAvailableNow++;

            auto& grp = groups[key];
            if (grp.name.empty()) grp.name = key;
            grp.locations.push_back({ id, name, isChecked, reachable });
            if (isChecked) grp.checkedCount++;
            };

        for (int64_t id : checked) addLoc(id, true);
        for (int64_t id : missing) addLoc(id, false);

        for (auto& kv : groups) {
            std::sort(kv.second.locations.begin(), kv.second.locations.end(),
                [](const TrackedLocation& a, const TrackedLocation& b) {
                    auto ka = LocationSortKey(a.name); auto kb = LocationSortKey(b.name);
                    if (ka != kb) return ka < kb; return a.name < b.name;
                });
            g_apTrackerGroups.push_back(std::move(kv.second));
        }

        std::sort(g_apTrackerGroups.begin(), g_apTrackerGroups.end(),
            [](const TrackerGroup& a, const TrackerGroup& b) {
                auto ka = GroupSortKey(a.name); auto kb = GroupSortKey(b.name);
                if (ka != kb) return ka < kb; return a.name < b.name;
            });

        g_apTrackerDirty.store(false);
    }

    static std::string g_apSessionKey;
    static int g_apNextItemIndex = 0;

    volatile bool g_apWitchTimeLocked = true;
    static bool g_apLastWitchTimeLocked = true;

    static void ResetSessionState() {
        g_apWitchTimeLocked = true;
        g_apGrantedWeaponBits.store(0);
        g_apSentWeaponChecks.clear();
        g_apGrantedTechniques.clear(); g_apSentTechniqueChecks.clear();
        g_apGrantedAccessories.clear(); g_apSentAccessoryChecks.clear();
        g_apCompletedChapters.clear(); g_apChapterInit = false;
        g_apCompletedVerses.clear(); g_apVerseInit = false;
        g_apLastVerse = -1; g_apVerseChapter = -1;
        g_apCompletedAlfheims.clear();
        g_apSentRankChecks.clear();
        g_apPendingRankChecks.clear();
        g_apCompletedChests.clear();
        g_apCompletedTears.clear();
        g_apUnlockedChapterMask.store(0);
        g_apAutoHintedShops.clear(); g_shopScouted = false;

        g_apUnlockedPunch.store(true);
        g_apUnlockedKick.store(true);
        g_apUnlockedTorture.store(true);
        g_apUnlockedAngelArms.store(true);

        { std::lock_guard<std::mutex> lock(g_apReceivedMutex); g_apReceivedItems.clear(); }
        {
            std::lock_guard<std::mutex> lock(g_apQueueMutex);
            while (!g_apItemQueue.empty()) g_apItemQueue.pop();
            while (!g_apTrapQueue.empty()) g_apTrapQueue.pop();
        }

        g_apMacguffinCount = 0;
        g_apGoalSent = false;
        g_apNextItemIndex = 0;
        g_apTrackerDirty.store(true);
        g_apCurseHitsLeft = 0; g_apCursePrevHP = -1;
        g_apSquishFrames = 0;
        g_apSquashFrames = 0;
        g_apSquashTotal = 0;
        g_apReceivedLPs.clear();

        {
            std::lock_guard<std::mutex> lock(g_hintHistoryMutex);
            g_hintHistory.clear();
        }
    }

    static void SeedCompletedFromServer() {
        if (!g_apClient) return;
        for (int64_t loc : g_apClient->get_checked_locations()) {
            if (loc >= 60301 && loc <= 60345) { g_apCompletedChests.insert(loc); }
            else if (loc >= 630001 && loc <= 630051) { g_apCompletedTears.insert(loc); }
            else if (loc >= 60101 && loc <= 60121) { g_apCompletedAlfheims.insert(loc); }
            else if (loc >= 620000 && loc < 630000) { g_apSentRankChecks.insert(loc); }
            else if (loc >= 60001 && loc <= 60006) { g_apSentWeaponChecks.insert(loc); }
            else if (loc == 60007 || loc == 60008) {
                for (const auto& a : g_apAccessories)
                    if (a.locationId == loc) { g_apSentAccessoryChecks.insert(a.itemId); break; }
            }
            else if (loc >= 60201 && loc <= 60214) {
                g_apSentTechniqueChecks.insert(loc);
            }
            else if (loc >= 610000) {
                int rem = (int)(loc - 610000);
                int ch = rem / 100, v = rem % 100;
                if (ch >= 0 && ch <= MAX_CHAPTER) {
                    if (v == 99) g_apCompletedChapters.insert(ch);
                    else g_apCompletedVerses.insert(ch * 100 + v);
                }
            }
        }
    }

    static std::list<std::string> BuildConnectTags() {
        std::list<std::string> tags = { "AP" };
        if (g_apDeathLinkEnabled.load()) tags.push_back("DeathLink");
        if (g_apDamageLinkEnabled.load()) tags.push_back("DamageLink");
        if (g_apTrapLinkEnabled.load()) tags.push_back("TrapLink");
        if (g_apRingLinkEnabled.load()) tags.push_back("RingLink");
        return tags;
    }

    static ImU32 ItemFlagColor(unsigned flags) {
        if (flags & APClient::FLAG_ADVANCEMENT)   return IM_COL32(221, 160, 221, 255);
        if (flags & APClient::FLAG_NEVER_EXCLUDE) return IM_COL32(140, 125, 235, 255);
        if (flags & APClient::FLAG_TRAP)          return IM_COL32(250, 128, 114, 255);
        return IM_COL32(0, 238, 238, 255);
    }

    static ImU32 ApLogColorByName(const std::string& name) {
        if (name == "red")       return IM_COL32(255, 96, 96, 255);
        if (name == "green")     return IM_COL32(96, 255, 96, 255);
        if (name == "yellow")    return IM_COL32(255, 255, 110, 255);
        if (name == "blue")      return IM_COL32(110, 150, 255, 255);
        if (name == "magenta")   return IM_COL32(255, 110, 255, 255);
        if (name == "cyan")      return IM_COL32(110, 255, 255, 255);
        if (name == "slateblue") return IM_COL32(140, 125, 235, 255);
        if (name == "plum")      return IM_COL32(221, 160, 221, 255);
        if (name == "salmon")    return IM_COL32(250, 128, 114, 255);
        if (name == "orange")    return IM_COL32(255, 165, 0, 255);
        if (name == "white")     return IM_COL32(255, 255, 255, 255);
        if (name == "black")     return IM_COL32(160, 160, 160, 255);
        return LOGCOL_DEFAULT;
    }

    static void JsonNodesToSegs(const std::list<APClient::TextNode>& nodes, std::vector<ApLogSeg>& out) {
        int selfSlot = g_apClient ? g_apClient->get_player_number() : -1;
        for (const auto& node : nodes) {
            ImU32 col = LOGCOL_DEFAULT;
            std::string text = node.text;

            if (node.type == "player_id") {
                int slot = atoi(node.text.c_str());
                if (g_apClient) text = g_apClient->get_player_alias(slot);
                col = (slot == selfSlot) ? LOGCOL_SELF : LOGCOL_PLAYER;
            }
            else if (node.type == "item_id") {
                long long id = strtoll(node.text.c_str(), nullptr, 10);
                if (g_apClient) text = g_apClient->get_item_name(id, g_apClient->get_player_game(node.player));
                col = ItemFlagColor(node.flags);
            }
            else if (node.type == "item_name") {
                col = ItemFlagColor(node.flags);
            }
            else if (node.type == "location_id") {
                long long id = strtoll(node.text.c_str(), nullptr, 10);
                if (g_apClient) text = g_apClient->get_location_name(id, g_apClient->get_player_game(node.player));
                col = LOGCOL_LOCATION;
            }
            else if (node.type == "location_name") {
                col = LOGCOL_LOCATION;
            }
            else if (node.type == "entrance_name") {
                col = LOGCOL_ENTRANCE;
            }
            else if (node.type == "color") {
                col = ApLogColorByName(node.color);
            }

            if (!text.empty()) out.push_back({ col, text });
        }
    }

    static void Connect() {
        if (g_apConnecting || g_apConnected) return;

        auto trim = [](char* buf, size_t cap) {
            std::string s(buf);
            size_t b = s.find_first_not_of(" \t\r\n");
            size_t e = s.find_last_not_of(" \t\r\n");
            s = (b == std::string::npos) ? std::string() : s.substr(b, e - b + 1);
            strncpy_s(buf, cap, s.c_str(), _TRUNCATE);
            };

        trim(g_apHost, sizeof(g_apHost));
        trim(g_apPort, sizeof(g_apPort));
        trim(g_apSlot, sizeof(g_apSlot));

        if (g_apSlot[0] == '\0') {
            g_apStatus = "Enter a slot name first";
            Log("Connect aborted - slot name is empty.");
            return;
        }

        SaveConnection();

        g_apConnecting = true;
        g_apStatus = "Connecting...";

        if (!g_wsaInitialized) { WSADATA wsa; WSAStartup(MAKEWORD(2, 2), &wsa); g_wsaInitialized = true; }

        std::string host = g_apHost;
        std::string port = g_apPort;
        std::string uri;

        if (host.rfind("ws://", 0) == 0 || host.rfind("wss://", 0) == 0) {
            uri = host + ":" + port;
        }
        else {
            bool secure = (host.find("archipelago.gg") != std::string::npos) || port == "443";
            uri = (secure ? "wss://" : "ws://") + host + ":" + port;
        }

        if (g_apClient) { delete g_apClient; g_apClient = nullptr; }
        g_apClient = new APClient("bayohook_ap_uuid", "Bayonetta", uri);

        g_apClient->set_room_info_handler([]() {
            Log("Room info received, authenticating...");
            g_apClient->ConnectSlot(g_apSlot, g_apPass, 7, BuildConnectTags(), { 0, 7, 0 });
            });

        g_apClient->set_slot_connected_handler([](const nlohmann::json& slot_data) {
            g_apConnected = true; g_apConnecting = false; g_apStatus = "Connected";

            std::string sessionKey = g_apClient->get_seed() + "|" + g_apSlot;
            if (sessionKey != g_apSessionKey) {
                g_apSessionKey = sessionKey;
                ResetSessionState();
                Log("New seed/slot - starting with fresh session state.");
            }

            SeedCompletedFromServer();

            // --- FLUSH OFFLINE CACHED CHECKS ON RECONNECT ---
            {
                std::lock_guard<std::mutex> lock(g_apOfflineMutex);
                if (!g_apOfflineCheckQueue.empty()) {
                    std::list<int64_t> batch(g_apOfflineCheckQueue.begin(), g_apOfflineCheckQueue.end());
                    g_apClient->LocationChecks(batch);
                    Log("[offline] Flushed " + std::to_string(batch.size()) + " cached offline checks to server.");
                    g_apOfflineCheckQueue.clear();
                }
            }

            g_apSyncGraceFrames.store(300);

            g_apGoal = 0;
            g_apGoalChapterCount = 8;
            g_apMacguffinsRequired = 10;
            g_apGoalSent = false;
            g_apIncludeChests = true;
            g_apIncludeWeapons = g_apIncludeTechniques = g_apIncludeAccessories = true;

            if (slot_data.contains("include_chests") && slot_data["include_chests"].is_number_integer())
                g_apIncludeChests = slot_data["include_chests"].get<int>() != 0;
            if (slot_data.contains("include_tears") && slot_data["include_tears"].is_number_integer())
                g_apIncludeTears = slot_data["include_tears"].get<int>() != 0;
            if (slot_data.contains("include_weapons") && slot_data["include_weapons"].is_number_integer())
                g_apIncludeWeapons = slot_data["include_weapons"].get<int>() != 0;
            if (slot_data.contains("include_techniques") && slot_data["include_techniques"].is_number_integer())
                g_apIncludeTechniques = slot_data["include_techniques"].get<int>() != 0;
            if (slot_data.contains("include_accessories") && slot_data["include_accessories"].is_number_integer())
                g_apIncludeAccessories = slot_data["include_accessories"].get<int>() != 0;

            auto is_option_enabled = [&](const char* key) -> bool {
                if (!slot_data.contains(key)) return false;
                if (slot_data[key].is_boolean()) return slot_data[key].get<bool>();
                if (slot_data[key].is_number()) return slot_data[key].get<int>() != 0;
                return false;
                };

            g_apUnlockedPunch.store(!is_option_enabled("include_punches"));
            g_apUnlockedKick.store(!is_option_enabled("include_kicks"));
            g_apUnlockedTorture.store(!is_option_enabled("include_torture_attacks"));
            g_apUnlockedAngelArms.store(!is_option_enabled("include_angel_arms"));

            if (slot_data.contains("death_link") && slot_data["death_link"].is_number_integer())
                g_apDeathLinkEnabled.store(slot_data["death_link"].get<int>() != 0);
            if (slot_data.contains("damage_link") && slot_data["damage_link"].is_number_integer())
                g_apDamageLinkEnabled.store(slot_data["damage_link"].get<int>() != 0);
            if (slot_data.contains("trap_link") && slot_data["trap_link"].is_number_integer())
                g_apTrapLinkEnabled.store(slot_data["trap_link"].get<int>() != 0);
            if (slot_data.contains("ring_link") && slot_data["ring_link"].is_number_integer())
                g_apRingLinkEnabled.store(slot_data["ring_link"].get<int>() != 0);

            g_apPrevHalosForRing = -1;

            if (g_apClient) g_apClient->ConnectUpdate(false, 7, true, BuildConnectTags());

            if (slot_data.contains("goal") && slot_data["goal"].is_number_integer())
                g_apGoal = slot_data["goal"].get<int>();
            if (slot_data.contains("goal_chapter_count") && slot_data["goal_chapter_count"].is_number_integer())
                g_apGoalChapterCount = slot_data["goal_chapter_count"].get<int>();
            if (slot_data.contains("macguffins_required") && slot_data["macguffins_required"].is_number_integer())
                g_apMacguffinsRequired = slot_data["macguffins_required"].get<int>();
            if (slot_data.contains("verse_rank_target") && slot_data["verse_rank_target"].is_number_integer())
                g_apVerseRankTarget = slot_data["verse_rank_target"].get<int>();

            g_apTrackerDirty.store(true);
            Log("Connected to Archipelago server. Active Goal ID: " + std::to_string(g_apGoal));
            });

        g_apClient->set_slot_refused_handler([](const std::list<std::string>& errs) {
            g_apConnecting = false; g_apStatus = "Refused";
            for (auto& e : errs) Log("Slot refused: " + e);
            });

        g_apClient->set_socket_error_handler([](const std::string& msg) {
            g_apConnecting = false; g_apStatus = "Socket error"; Log("Socket error: " + msg);
            });

        g_apClient->set_socket_disconnected_handler([]() {
            if (g_apConnected) {
                g_apConnected = false;
                g_apStatus = "Reconnecting...";
                Log("Socket lost - APClient will retry. Session state is kept.");
            }
            });

        g_apClient->set_items_received_handler([](const std::list<APClient::NetworkItem>& items) {
            for (auto& item : items) {
                if (item.index >= 0) {
                    if (item.index < g_apNextItemIndex) continue;
                    g_apNextItemIndex = item.index + 1;
                }

                std::string name = g_apClient->get_item_name(item.item, "Bayonetta");

                QueueItem(item.item);

                {
                    std::lock_guard<std::mutex> lock(g_apReceivedMutex);
                    g_apReceivedItems[name]++;
                }

                if (item.player == 0) {
                    std::string displayName = (name == "Unknown" || name.find_first_not_of("0123456789") == std::string::npos)
                        ? ("Item " + std::to_string(item.item))
                        : name;
                    Log("Received " + displayName + " from the server (starting inventory).");
                    AddNotification("Received " + displayName, ImVec4(0.3f, 1.0f, 0.3f, 1.0f));
                }
            }
            g_apTrackerDirty.store(true);
            });

        g_apClient->set_location_checked_handler([](const std::list<int64_t>&) { g_apTrackerDirty.store(true); });

        g_apClient->set_print_json_handler([](const APClient::PrintJSONArgs& args) {
            std::vector<ApLogSeg> segs;
            JsonNodesToSegs(args.data, segs);
            if (segs.empty()) return;

            std::string plain;
            for (const auto& s : segs) plain += s.text;

            uint8_t cat = LOGCAT_CHAT;
            const bool isSend = (args.type == "ItemSend" || args.type == "ItemCheat");

            if (isSend) {
                cat = LOGCAT_ITEMS;
                segs.insert(segs.begin(), { LOGCOL_DEFAULT, "(Team #1) " });
            }
            else if (args.type == "Hint") {
                cat = LOGCAT_HINTS;
                segs.insert(segs.begin(), { LOGCOL_DEFAULT, "Notice (Team #1): " });

                std::string ts = LogTimestamp();
                HintEntry entry;
                entry.timestamp = ts;
                entry.message = plain;
                {
                    std::lock_guard<std::mutex> lock(g_hintHistoryMutex);
                    g_hintHistory.push_back(entry);
                    if (g_hintHistory.size() > 200)
                        g_hintHistory.erase(g_hintHistory.begin());
                }
            }
            else if (args.type == "Goal" || args.type == "Release" ||
                args.type == "Collect" || args.type == "Countdown") {
                cat = LOGCAT_ITEMS;
            }

            LogSegs(cat, std::move(segs));

            if (isSend) {
                int self = g_apClient ? g_apClient->get_player_number() : -1;
                int receiver = args.receiving ? *args.receiving : -1;
                int sender = args.item ? args.item->player : -1;

                bool involvesMe = (receiver == self || sender == self);
                if (involvesMe || g_apToastAllSends) {
                    ImVec4 c = (receiver == self) ? ImVec4(0.3f, 1.0f, 0.3f, 1.0f)
                        : (sender == self) ? ImVec4(1.0f, 0.8f, 0.3f, 1.0f)
                        : ImVec4(0.85f, 0.85f, 0.85f, 1.0f);
                    AddNotification(plain, c);
                }
            }
            else if (args.type == "Hint") {
                AddNotification(plain, ImVec4(1.0f, 1.0f, 0.3f, 1.0f));
            }
            else if (args.type == "Join" || args.type == "Part") {
                AddNotification(plain, ImVec4(0.7f, 0.7f, 1.0f, 1.0f));
            }
            else if (args.type == "Goal" || args.type == "Release" || args.type == "Collect") {
                AddNotification(plain, ImVec4(1.0f, 0.8f, 0.2f, 1.0f), 10.0f);
            }
            });

        g_apClient->set_bounced_handler([](const nlohmann::json& command) { HandleBounce(command); });

        g_apClient->set_location_info_handler([](const std::list<APClient::NetworkItem>& items) {
            for (const auto& item : items) {
                std::string itemName = g_apClient->get_item_name(item.item, g_apClient->get_player_game(item.player));
                std::string playerName = g_apClient->get_player_alias(item.player);
                std::string locName = g_apClient->get_location_name(item.location, "Bayonetta");

                std::string classification;
                if (item.flags & APClient::FLAG_ADVANCEMENT)
                    classification = "(Progression)";
                else if (item.flags & APClient::FLAG_NEVER_EXCLUDE)
                    classification = "(Useful)";
                else if (item.flags & APClient::FLAG_TRAP)
                    classification = "(Trap)";
                else
                    classification = "(Filler)";

                Log("  Shop: " + locName + " - " + playerName + "'s " + itemName + " " + classification);
                g_apAutoHintedShops.insert(item.location);
            }
            });
    }

    static void Disconnect() {
        if (g_apClient) { delete g_apClient; g_apClient = nullptr; }
        g_apConnected = false; g_apConnecting = false; g_apStatus = "Not connected";
        g_apDeathLinkPending.store(false);
        g_apSendDeathPending.store(false);
        g_apDeathSuppressFrames.store(0);
        g_apPrevMoveWasDeath = false;
        g_apDeathLinkKillActive.store(false);
        g_apSyncGraceFrames.store(0);

        g_apTrackerGroups.clear();
        g_apTrackerTotalChecked = 0;
        g_apTrackerTotalLocations = 0;
        g_apTrackerAvailableNow = 0;
        g_apTrackerDirty.store(true);

        g_apChapterInit = false;
        g_apVerseInit = false;

        g_shopScouted = false;

        RemoveChapterBlockHook();

        Log("Disconnected.");
    }

    static void SendChat(const std::string& msg) {
        if (!g_apClient || !g_apConnected) { Log("Cannot send - not connected."); return; }
        if (msg.empty()) return;

        if (msg == "/resetshop") { g_apAutoHintedShops.clear(); Log("Shop hints reset."); return; }
        if (msg == "/hintshop") { g_apAutoHintedShops.clear(); if (g_apLiveRecognizedStage == 0xF01) AutoHintShop(); else Log("You need to be in the Gates of Hell to hint shop items."); return; }

        g_apClient->Say(msg);
    }

    static void DrawNotifications() {
        std::lock_guard<std::mutex> lock(g_notificationMutex);
        if (g_notifications.empty()) return;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 5.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10, 10));

        auto now = std::chrono::steady_clock::now();
        g_notifications.erase(
            std::remove_if(g_notifications.begin(), g_notifications.end(),
                [now](const Notification& n) {
                    auto elapsed = std::chrono::duration_cast<std::chrono::duration<float>>(now - n.timestamp).count();
                    return elapsed >= n.duration;
                }
            ),
            g_notifications.end()
        );

        if (g_notifications.empty()) {
            ImGui::PopStyleVar(2);
            return;
        }

        ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(400, 0), ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.7f);

        ImGui::Begin("##Notifications", nullptr,
            ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoFocusOnAppearing |
            ImGuiWindowFlags_NoInputs |
            ImGuiWindowFlags_NoNav |
            ImGuiWindowFlags_NoScrollWithMouse |
            ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_AlwaysAutoResize);

        for (size_t i = 0; i < g_notifications.size(); i++) {
            const auto& notif = g_notifications[i];
            auto elapsed = std::chrono::duration_cast<std::chrono::duration<float>>(now - notif.timestamp).count();

            float alpha = 1.0f;
            if (notif.duration - elapsed < 1.0f) {
                alpha = notif.duration - elapsed;
            }

            ImVec4 color = notif.color;
            color.w *= alpha;

            ImGui::PushStyleColor(ImGuiCol_Text, color);
            ImGui::TextWrapped("%s", notif.message.c_str());
            ImGui::PopStyleColor();

            if (i < g_notifications.size() - 1) {
                ImGui::Spacing();
            }
        }

        ImGui::End();
        ImGui::PopStyleVar(2);
    }

    static void DrawConnectTab() {
        ImGui::Text("Status: %s", g_apStatus.c_str());
        ImGui::Separator();

        bool lockInputs = g_apConnected || g_apConnecting;
        if (lockInputs) ImGui::BeginDisabled();
        ImGui::InputText("Server##APServer", g_apHost, sizeof(g_apHost));
        ImGui::InputText("Port##APPort", g_apPort, sizeof(g_apPort));
        ImGui::InputText("Slot Name##APSlot", g_apSlot, sizeof(g_apSlot));
        ImGui::InputText("Password##APPass", g_apPass, sizeof(g_apPass), ImGuiInputTextFlags_Password);
        if (lockInputs) ImGui::EndDisabled();

        if (!g_apConnected && !g_apConnecting) {
            if (ImGui::Button("Connect##APConnect")) Connect();
        }
        else {
            if (ImGui::Button("Disconnect##APDisconnect")) Disconnect();
        }
        ImGui::Separator();

        {
            bool dl = g_apDeathLinkEnabled.load();
            if (ImGui::Checkbox("Death Link##APDeathLink", &dl)) {
                g_apDeathLinkEnabled.store(dl);
                Log(dl ? "Death Link enabled." : "Death Link disabled.");
                g_apPrevMoveWasDeath = false;
                g_apDeathSuppressFrames.store(0);
                if (g_apConnected && g_apClient) {
                    g_apClient->ConnectUpdate(false, 7, true, BuildConnectTags());
                }
            }
            ImGui::SameLine();
            ImGui::TextDisabled("(?)");
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("When on, dying sends a death to everyone with Death Link,\n"
                    "and their deaths kill you. Uses the same HP system as\n"
                    "Witch Hearts. Toggle before connecting for best results.");

            bool dmgl = g_apDamageLinkEnabled.load();
            if (ImGui::Checkbox("Damage Link##APDamageLink", &dmgl)) {
                g_apDamageLinkEnabled.store(dmgl);
                Log(dmgl ? "Damage Link enabled." : "Damage Link disabled.");
                g_apDamageSuppressFrames.store(0);
                g_apPrevHPForDamage = -1;
                g_apDamageAccumFrac = 0.0f;
                if (g_apConnected && g_apClient)
                    g_apClient->ConnectUpdate(false, 7, true, BuildConnectTags());
            }
            ImGui::SameLine();
            ImGui::TextDisabled("(?)");
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Damage you take is shared with everyone on Damage Link,\n"
                    "and their damage hurts you. Never lethal on its own -\n"
                    "shared damage always leaves you at least 1 HP.");

            bool trpl = g_apTrapLinkEnabled.load();
            if (ImGui::Checkbox("Trap Link##APTrapLink", &trpl)) {
                g_apTrapLinkEnabled.store(trpl);
                Log(trpl ? "Trap Link enabled." : "Trap Link disabled.");
                if (g_apConnected && g_apClient)
                    g_apClient->ConnectUpdate(false, 7, true, BuildConnectTags());
            }
            ImGui::SameLine();
            ImGui::TextDisabled("(?)");
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Traps you receive also fire for everyone on Trap Link,\n"
                    "and their traps fire on you - matched by name when\n"
                    "possible, or a random Bayonetta trap otherwise.");

            bool ringl = g_apRingLinkEnabled.load();
            if (ImGui::Checkbox("Ring Link##APRingLink", &ringl)) {
                g_apRingLinkEnabled.store(ringl);
                Log(ringl ? "Ring Link enabled." : "Ring Link disabled.");
                g_apPrevHalosForRing = -1;
                g_apRingGainAccumHalos = 0;
                if (g_apConnected && g_apClient)
                    g_apClient->ConnectUpdate(false, 7, true, BuildConnectTags());
            }
            ImGui::SameLine();
            ImGui::TextDisabled("(?)");
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Halos you collect are shared as rings with everyone on\n"
                    "Ring Link (1 ring = 100 Halos), and their ring changes\n"
                    "become Halos for you. Shop purchases are not shared.");
        }

        ImGui::Separator();
        if (ImGui::CollapsingHeader("Admin (DEV ONLY)")) {
            static int debugId = 50200;
            ImGui::SetNextItemWidth(150.0f);
            ImGui::InputInt("Item ID##APDebugID", &debugId);
            ImGui::SameLine();
            if (ImGui::Button("Give##APDebugGive")) QueueItem(debugId);
            ImGui::TextDisabled("Give any AP item by its numeric ID.");

            ImGui::Separator();
            ImGui::TextDisabled("Weapon / flag finder (see log for results):");
            if (ImGui::Button("WF: Snapshot##APWFSnap")) WeaponFinder::Snapshot();
            ImGui::SameLine();
            if (ImGui::Button("WF: Diff##APWFDiff")) WeaponFinder::Diff();
            ImGui::SameLine();
            if (ImGui::Button("WF: Widen region##APWFWiden")) {
                WeaponFinder::SetRegion(0x5AA0000, 0x20000);
            }
            ImGui::TextDisabled("Snapshot in static menu (Gates of Hell), make ONE change,");
            ImGui::TextDisabled("then return to the same menu and Diff. Look for 0->1 flips.");

            ImGui::Separator();
            ImGui::TextDisabled("Chest finder (see log for results):");
            if (ImGui::Button("CF: Arm##APCFArm"))    ChestFinder::Start();
            ImGui::SameLine();
            if (ImGui::Button("CF: Disarm##APCFStop"))  ChestFinder::Stop();
            ImGui::SameLine();
            if (ImGui::Button("CF: Dump##APCFDump"))    ChestFinder::Dump();
            ImGui::TextDisabled("Tracer: Arm at chapter select -> load chapter -> Disarm -> Dump.");

            ImGui::Separator();
            if (ImGui::Button("CF: FlipWatch##APCFFlip")) ChestFinder::FlipStart();
            ImGui::SameLine();
            if (ImGui::Button("CF: FlipStop##APCFFlipS")) ChestFinder::FlipStop();
            ImGui::TextDisabled("FlipWatch: turn on once, wait 2s for counter mutes, then smash.");

            ImGui::Separator();
            if (ImGui::Button("CF: DiffSnap##APCFDS")) ChestFinder::DiffSnap();
            ImGui::SameLine();
            if (ImGui::Button("CF: DiffCmp##APCFDC"))  ChestFinder::DiffCompare();
            ImGui::TextDisabled("For single checks: Snap -> break chest -> DiffCmp.");

            ImGui::SeparatorText("Chest Mapper");
            if (ImGui::Button("CM: Start##APCMStart")) ChestFinder::ChestMapStart();
            ImGui::SameLine();
            if (ImGui::Button("CM: Stop##APCMStop")) ChestFinder::ChestMapStop();
            ImGui::TextDisabled("Start, break chests, Stop. Check log for results.");

            ImGui::SeparatorText("Rank Finder");
            if (ImGui::Button("RF: Snap##APRFSnap")) ChestFinder::RankFindSnap();
            ImGui::SameLine();
            if (ImGui::Button("RF: Diff##APRFDiff")) ChestFinder::RankFindDiff();
            ImGui::TextDisabled("Snap BEFORE finishing a verse. Play + finish. Snap AGAIN after");
            ImGui::TextDisabled("the results screen closes, then Diff. Aim for a specific rank");
            ImGui::TextDisabled("(e.g. Bronze/Silver) so the byte you're hunting has a known value.");
        }
    }

    static void DrawConsoleTab() {
        ImGui::Checkbox("Items##APLogFI", &g_apLogShowItems); ImGui::SameLine();
        ImGui::Checkbox("Hints##APLogFH", &g_apLogShowHints); ImGui::SameLine();
        ImGui::Checkbox("Chat##APLogFC", &g_apLogShowChat); ImGui::SameLine();
        ImGui::Checkbox("System##APLogFS", &g_apLogShowSystem); ImGui::SameLine();
        ImGui::TextDisabled("|"); ImGui::SameLine();
        ImGui::Checkbox("Toast all sends##APLogFT", &g_apToastAllSends);

        ImGui::BeginChild("APConsoleChild", ImVec2(0, -ImGui::GetFrameHeightWithSpacing()), true, ImGuiWindowFlags_HorizontalScrollbar);
        {
            std::lock_guard<std::mutex> lock(g_apLogMutex);
            static std::vector<const ApLogLine*> visible;
            visible.clear();
            for (const auto& line : g_apLogLines) {
                bool show = (line.cat == LOGCAT_ITEMS) ? g_apLogShowItems
                    : (line.cat == LOGCAT_HINTS) ? g_apLogShowHints
                    : (line.cat == LOGCAT_CHAT) ? g_apLogShowChat
                    : g_apLogShowSystem;
                if (show) visible.push_back(&line);
            }

            ImGuiListClipper clipper;
            clipper.Begin((int)visible.size());
            while (clipper.Step()) {
                for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
                    const ApLogLine& line = *visible[i];
                    for (size_t s = 0; s < line.segs.size(); ++s) {
                        if (s > 0) ImGui::SameLine(0.0f, 0.0f);
                        ImGui::PushStyleColor(ImGuiCol_Text, line.segs[s].col);
                        ImGui::TextUnformatted(line.segs[s].text.c_str());
                        ImGui::PopStyleColor();
                    }
                }
            }
            clipper.End();
        }
        if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) ImGui::SetScrollHereY(1.0f);
        ImGui::EndChild();

        static char consoleInput[256] = "";
        auto submit = [&]() {
            if (consoleInput[0] == '\0') return;
            std::string msg(consoleInput);
            if (msg == "/clear") ClearConsole();
            else SendChat(msg);
            consoleInput[0] = '\0';
            };

        ImGui::SetNextItemWidth(-140.0f);
        bool entered = ImGui::InputText("##APConsoleInput", consoleInput, sizeof(consoleInput), ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::SameLine();
        if (ImGui::Button("Send##APConsoleSend")) submit();
        ImGui::SameLine();
        if (ImGui::Button("Clear##APConsoleClear")) ClearConsole();
        if (entered) submit();
    }

    static void DrawHintsTab() {
        ImGui::TextWrapped("Request a hint for one of your items. This sends a "
            "\"!hint\" command to the server; the result appears in the Console tab.");
        ImGui::Separator();

        static char hintItem[128] = "";
        ImGui::SetNextItemWidth(-120.0f);
        bool entered = ImGui::InputText("Item Name##APHintItem", hintItem, sizeof(hintItem), ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::SameLine();
        bool clicked = ImGui::Button("Send Hint##APHintSend");

        if ((entered || clicked) && hintItem[0] != '\0') {
            SendChat(std::string("!hint ") + hintItem);
            hintItem[0] = '\0';
        }
        ImGui::Spacing();

        ImGui::TextDisabled("Other useful commands you can type in the Console tab:");
        ImGui::BulletText("!hint_location <location>  - hint what's at a location");
        ImGui::BulletText("!hint  (no args)           - list your current hints");
        ImGui::BulletText("!missing                   - list your unchecked locations");

        ImGui::Separator();
        ImGui::Text("Hint History:");
        ImGui::BeginChild("HintHistory", ImVec2(0, 200), true);
        {
            std::lock_guard<std::mutex> lock(g_hintHistoryMutex);
            if (g_hintHistory.empty()) {
                ImGui::TextDisabled("No hints yet.");
            }
            else {
                for (const auto& hint : g_hintHistory) {
                    ImGui::TextWrapped("%s %s", hint.timestamp.c_str(), hint.message.c_str());
                }
            }
        }
        if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
            ImGui::SetScrollHereY(1.0f);
        ImGui::EndChild();
    }

    static bool InGoMode() {
        int completed = (int)g_apCompletedChapters.size();
        bool requiem_done = (g_apCompletedChapters.count(REQUIEM_CHAPTER) > 0);
        switch (g_apGoal) {
        case 0: return completed >= 17;
        case 1: return completed + 1 >= g_apGoalChapterCount || (!requiem_done && completed >= g_apGoalChapterCount - 1);
        case 2: {
            int have = (int)(g_apCompletedChapters.count(4) + g_apCompletedChapters.count(7) +
                g_apCompletedChapters.count(11) + g_apCompletedChapters.count(13) +
                g_apCompletedChapters.count(14) + g_apCompletedChapters.count(16));
            int need = 6 + (requiem_done ? 0 : 1);
            return have >= need - 1;
        }
        case 3: return completed >= g_apGoalChapterCount - 1;
        case 4: return g_apMacguffinCount >= g_apMacguffinsRequired - 1;
        }
        return false;
    }

    static void DrawTrackerTab() {
        if (!g_apConnected) {
            ImGui::TextDisabled("Connect to a server to see tracker data.");
            return;
        }
        if (g_apTrackerDirty.load()) RebuildTrackerSnapshot();

        if (g_apGoal == 4) {
            ImVec4 col = (g_apMacguffinCount >= g_apMacguffinsRequired)
                ? ImVec4(0.55f, 0.90f, 0.55f, 1.0f)
                : ImVec4(0.85f, 0.75f, 1.0f, 1.0f);
            ImGui::PushStyleColor(ImGuiCol_Text, col);
            ImGui::Text("✦ Memory Fragments: %d / %d",
                g_apMacguffinCount, g_apMacguffinsRequired);
            ImGui::PopStyleColor();
        }

        if (InGoMode()) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.85f, 0.2f, 1.0f));
            ImGui::TextUnformatted("★ GO MODE - your goal is one step away!");
            ImGui::PopStyleColor();
            ImGui::Separator();
        }

        int availNow = g_apTrackerAvailableNow;
        if (availNow > 0)
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.90f, 0.55f, 1.0f));
        if (availNow == 1)
            ImGui::Text("You have 1 check you can do right now.");
        else if (availNow == 0)
            ImGui::TextDisabled("No checks available right now - unlock more chapters.");
        else
            ImGui::Text("You have %d checks you can do right now.", availNow);
        if (availNow > 0)
            ImGui::PopStyleColor();

        int chaptersInLogic = 0;
        std::string chapterList;
        uint32_t unlockedMask = g_apUnlockedChapterMask.load();
        for (int ch = 0; ch <= MAX_CHAPTER; ++ch) {
            bool open = (ch <= 1) || (unlockedMask & (1u << ch)) != 0;
            if (open) {
                chaptersInLogic++;
                if (!chapterList.empty()) chapterList += ", ";
                chapterList += ChapterLabel(ch);
            }
        }
        ImGui::TextDisabled("In logic: %d chapter%s", chaptersInLogic,
            chaptersInLogic == 1 ? "" : "s");
        if (ImGui::IsItemHovered() && !chapterList.empty())
            ImGui::SetTooltip("%s", chapterList.c_str());

        ImGui::SameLine();
        ImGui::TextDisabled("  -  %d / %d total checks done",
            g_apTrackerTotalChecked, g_apTrackerTotalLocations);

        if (g_apTrackerTotalLocations > 0) {
            float frac = (float)g_apTrackerTotalChecked / (float)g_apTrackerTotalLocations;
            ImGui::ProgressBar(frac, ImVec2(-1.0f, 0.0f));
        }

        static bool hideChecked = false;
        static bool showLocked = false;
        ImGui::Checkbox("Hide checked", &hideChecked);
        ImGui::SameLine();
        ImGui::Checkbox("Show locked chapters", &showLocked);

        ImGui::Separator();

        ImGui::BeginChild("APTrackerLocChild", ImVec2(0, 240), true);
        for (const auto& grp : g_apTrackerGroups) {
            int total = (int)grp.locations.size();

            int visibleCount = 0;
            for (const auto& loc : grp.locations) {
                if (hideChecked && loc.checked) continue;
                if (!loc.checked && !loc.reachable && !showLocked) continue;
                visibleCount++;
            }
            if (visibleCount == 0) continue;

            std::string header = grp.name + " (" + std::to_string(grp.checkedCount) +
                "/" + std::to_string(total) + ")###grp_" + grp.name;
            if (ImGui::TreeNode(header.c_str())) {
                for (const auto& loc : grp.locations) {
                    if (hideChecked && loc.checked) continue;
                    if (!loc.checked && !loc.reachable && !showLocked) continue;

                    if (loc.checked) {
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.45f, 0.85f, 0.45f, 1.0f));
                        ImGui::Text("[x] %s", loc.name.c_str());
                        ImGui::PopStyleColor();
                    }
                    else if (!loc.reachable) {
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
                        ImGui::Text("[ ] %s (Locked)", loc.name.c_str());
                        ImGui::PopStyleColor();
                    }
                    else {
                        ImGui::Text("[ ] %s", loc.name.c_str());
                    }
                }
                ImGui::TreePop();
            }
        }
        ImGui::EndChild();

        if (ImGui::CollapsingHeader("Received Items", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::BeginChild("APTrackerItemsChild", ImVec2(0, 140), true);
            std::lock_guard<std::mutex> lock(g_apReceivedMutex);
            if (g_apReceivedItems.empty()) {
                ImGui::TextDisabled("No items received yet.");
            }
            else {
                for (const auto& kv : g_apReceivedItems) {
                    if (kv.second > 1)
                        ImGui::Text("%s  x%d", kv.first.c_str(), kv.second);
                    else
                        ImGui::Text("%s", kv.first.c_str());
                }
            }
            ImGui::EndChild();
        }
    }

    static void DrawCoopTab() {
        ImGui::Text("Status: %s", g_coopStatus.c_str());
        ImGui::Separator();

        ImGui::InputText("Player Name", g_coopPlayerName, sizeof(g_coopPlayerName));
        ImGui::Separator();

        ImGui::Text("Host a Game");
        if (ImGui::Button("Create Room")) {
            if (!g_SteamManager) g_SteamManager = new SteamCoopManager();
            g_SteamManager->HostRoom();
        }

        if (g_IsHost && g_CurrentLobbyID.IsValid()) {
            ImGui::SameLine();
            if (ImGui::Button("Copy ID")) {
                ImGui::SetClipboardText(std::to_string(g_CurrentLobbyID.ConvertToUint64()).c_str());
            }
            ImGui::Text("Your Room ID: %llu", g_CurrentLobbyID.ConvertToUint64());
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::Text("Join a Game");
        ImGui::InputText("Room ID", g_coopRoomIDInput, sizeof(g_coopRoomIDInput));
        if (ImGui::Button("Join Room")) {
            if (!g_SteamManager) g_SteamManager = new SteamCoopManager();
            if (g_coopRoomIDInput[0] != '\0') {
                uint64_t lobbyID = std::stoull(g_coopRoomIDInput);
                g_SteamManager->JoinRoom(lobbyID);
            }
        }

        ImGui::Separator();
        ImGui::Text("Players Connected:");
        ImGui::BeginChild("CoopPlayersChild", ImVec2(0, 80), true);
        {
            ISteamFriends* pFriends = GetBayoSteamFriends();
            if (g_CurrentLobbyID.IsValid()) {
                int numMembers = GetBayoMatchmaking()->GetNumLobbyMembers(g_CurrentLobbyID);
                for (int i = 0; i < numMembers; ++i) {
                    CSteamID member = GetBayoMatchmaking()->GetLobbyMemberByIndex(g_CurrentLobbyID, i);
                    const char* name = pFriends ? pFriends->GetFriendPersonaName(member) : nullptr;
                    ImGui::Text("- %s (SteamID: %llu)", name ? name : "Unknown", member.ConvertToUint64());
                }
            }
            else if (g_RemotePlayerID.IsValid()) {
                const char* name = pFriends ? pFriends->GetFriendPersonaName(g_RemotePlayerID) : nullptr;
                ImGui::Text("- %s (Remote Peer)", name ? name : "Unknown");
            }
            else {
                ImGui::TextDisabled("No active co-op connection.");
            }
        }
        ImGui::EndChild();

        ImGui::Separator();
        ImGui::Text("Co-op Chat:");
        ImGui::BeginChild("CoopChatChild", ImVec2(0, 120), true, ImGuiWindowFlags_HorizontalScrollbar);
        {
            for (const auto& msg : g_coopChatMessages) {
                ImGui::TextWrapped("%s", msg.c_str());
            }
            if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
                ImGui::SetScrollHereY(1.0f);
        }
        ImGui::EndChild();

        static char chatInput[128] = "";
        auto sendChatMsg = [&]() {
            if (chatInput[0] == '\0') return;
            CoopChatPacket pkt;
            pkt.type = PACKET_TYPE_CHAT;
            strncpy_s(pkt.sender, sizeof(pkt.sender), g_coopPlayerName, _TRUNCATE);
            strncpy_s(pkt.message, sizeof(pkt.message), chatInput, _TRUNCATE);

            SendP2PToAll(&pkt, sizeof(pkt), k_EP2PSendReliable);

            std::string selfMsg = std::string("[Co-op] ") + g_coopPlayerName + ": " + chatInput;
            g_coopChatMessages.push_back(selfMsg);
            chatInput[0] = '\0';
            };

        ImGui::SetNextItemWidth(-80.0f);
        bool entered = ImGui::InputText("##CoopChatInput", chatInput, sizeof(chatInput), ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::SameLine();
        if (ImGui::Button("Send##CoopChatSend")) sendChatMsg();
        if (entered) sendChatMsg();
    }

    void DrawTab() {
        if (ImGui::BeginTabBar("ArchipelagoSubTabs", ImGuiTabBarFlags_None)) {
            if (ImGui::BeginTabItem("Connect##APSub")) {
                ImGui::BeginChild("APConnectChild", ImVec2(0, 400), false);
                DrawConnectTab();
                ImGui::EndChild();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Console##APSub")) {
                ImGui::BeginChild("APConsoleTabChild", ImVec2(0, 400), false);
                DrawConsoleTab();
                ImGui::EndChild();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Tracker##APSub")) {
                ImGui::BeginChild("APTrackerTabChild", ImVec2(0, 400), false);
                DrawTrackerTab();
                ImGui::EndChild();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Hints##APSub")) {
                ImGui::BeginChild("APHintsTabChild", ImVec2(0, 400), false);
                DrawHintsTab();
                ImGui::EndChild();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Co-op##APSub")) {
                ImGui::BeginChild("APCoopTabChild", ImVec2(0, 400), false);
                DrawCoopTab();
                ImGui::EndChild();
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
    }

    static void KillAllBayonettaInstances() {
        HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (hSnapshot == INVALID_HANDLE_VALUE) return;
        PROCESSENTRY32W pe;
        pe.dwSize = sizeof(pe);
        if (Process32FirstW(hSnapshot, &pe)) {
            do {
                if (_wcsicmp(pe.szExeFile, L"Bayonetta.exe") == 0) {
                    HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pe.th32ProcessID);
                    if (hProcess) {
                        TerminateProcess(hProcess, 0);
                        CloseHandle(hProcess);
                    }
                }
            } while (Process32NextW(hSnapshot, &pe));
        }
        CloseHandle(hSnapshot);
    }

    void Init() {
        InitLogPath();
        SetUnhandledExceptionFilter(BayoHookCrashHandler);
        InitItemMaps();
        LoadSavedConnection();

        Log("[Steam] Hooking into Bayonetta's native Steamworks connection...");
    }

    void* g_aplogoTexture = nullptr;

    void DrawOverlay() {
        DrawNotifications();
        static bool g_showWatermark = true;
        static bool insertWasDown = false;
        bool insertDown = (GetAsyncKeyState(VK_INSERT) & 0x8000) != 0;
        if (insertDown && !insertWasDown) {
            g_showWatermark = !g_showWatermark;
        }
        insertWasDown = insertDown;
        if (g_showWatermark) {
            ImGuiIO& io = ImGui::GetIO();
            float padding = 15.0f;
            ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - padding, io.DisplaySize.y - padding), ImGuiCond_Always, ImVec2(1.0f, 1.0f));
            ImGui::SetNextWindowBgAlpha(0.0f);
            ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoInputs |
                ImGuiWindowFlags_AlwaysAutoResize;
            if (ImGui::Begin("APWatermarkWindow", NULL, flags)) {
                if (g_apLogoTexture != nullptr) {
                    float iconSize = 20.0f;
                    ImGui::Image((void*)g_apLogoTexture, ImVec2(iconSize, iconSize));
                    ImGui::SameLine(0, 8);
                }
                ImGui::Text("Bayonetta Archipelago v2.7.0");
            }
            ImGui::End();
        }
    }

    void Poll() {
        int ch = -1;
        if (g_apLastChapterStage != -1) ch = ChapterForStage(g_apLastChapterStage);
        ChestFinder::FlipPoll(ch);
        ChestFinder::ChestMapPoll(ch);

        bool wtNow = g_apConnected ? g_apWitchTimeLocked : false;
        if (wtNow != g_apLastWitchTimeLocked) {
            g_apLastWitchTimeLocked = wtNow;
            GameHook::UpdateHooks();
        }

        if (AREA_JUMP_ADDR != 0) {
            int32_t liveStage = 0;
            if (ReadMem(AREA_JUMP_ADDR, liveStage)) {
                if (IsRecognizedStage(liveStage)) {
                    static int32_t lastLoggedStage = -1;
                    if (liveStage != lastLoggedStage) {
                        int liveCh = ChapterForStage(liveStage);
                        int oldCh = ChapterForStage(lastLoggedStage);
                        if (liveCh >= 0 && liveCh != oldCh) {
                            uint8_t zeros[RANK_MAX_VERSE * RANK_VERSE_STRIDE] = { 0 };
                            WriteProcessMemory(GetCurrentProcess(), (LPVOID)RANK_TABLE_BASE, zeros, sizeof(zeros), nullptr);
                            Log("Wiped verse rank memory block for new chapter to prevent Alfheim ghosting.");
                        }
                        lastLoggedStage = liveStage;
                        char sb[96];
                        sprintf_s(sb, "[stage] 0x%X -> chapter %d", (unsigned)liveStage, liveCh);
                        Log(sb);
                    }
                    g_apLiveRecognizedStage = liveStage;
                    int ch2 = ChapterForStage(liveStage);
                    if (ch2 >= 0) {
                        g_apLastChapterStage = liveStage;
                        bool wasRequiem = (g_apActiveChapterStage == 0x5A1 || g_apActiveChapterStage == 0x5A2);
                        if (!(wasRequiem && ch2 != REQUIEM_CHAPTER))
                            g_apActiveChapterStage = liveStage;
                    }
                }
            }
        }

        AutoHintShop();
        if (g_apSendDeathPending.exchange(false)) SendDeathLink();
        if (g_apClient) g_apClient->poll();

        SteamAPI_RunCallbacks();

        ISteamNetworking* pNet = GetBayoNetworking();
        if (pNet) {
            uint32_t msgSize = 0;
            while (pNet->IsP2PPacketAvailable(&msgSize, 0)) {
                std::vector<uint8_t> buffer(msgSize);
                CSteamID remoteID;
                uint32_t bytesRead = 0;

                if (pNet->ReadP2PPacket(buffer.data(), msgSize, &bytesRead, &remoteID, 0)) {
                    if (bytesRead >= 1) {
                        uint8_t packetType = buffer[0];
                        if (packetType == PACKET_TYPE_SYNC && bytesRead == sizeof(PlayerSyncPacket)) {
                            PlayerSyncPacket* syncData = (PlayerSyncPacket*)buffer.data();

                            LocalPlayer* player2 = GameHook::GetPlayer2();
                            if (player2) {
                                player2->pos.x = syncData->x;
                                player2->pos.y = syncData->y;
                                player2->pos.z = syncData->z;
                                player2->rot.x = syncData->rX;
                                player2->rot.y = syncData->rY;
                                player2->rot.z = syncData->rZ;
                                player2->moveID = syncData->moveID;
                                player2->movePart = syncData->movePart;
                                player2->animFrame = syncData->animFrame;

                                *(int*)((uintptr_t)player2 + 0x120) = syncData->costumeId;
                            }
                        }
                        else if (packetType == PACKET_TYPE_CHAT && bytesRead == sizeof(CoopChatPacket)) {
                            CoopChatPacket* chatData = (CoopChatPacket*)buffer.data();
                            std::string chatMsg = std::string("[Co-op] ") + chatData->sender + ": " + chatData->message;
                            g_coopChatMessages.push_back(chatMsg);
                        }
                    }
                }
            }
        }

        // --- AUTOMATICALLY SPAWN/ACTIVATE PLAYER 2 UPON ENTERING A CHAPTER ---
        if (InChapterStage() && (g_RemotePlayerID.IsValid() || g_CurrentLobbyID.IsValid())) {
            LocalPlayer* player2 = GameHook::GetPlayer2();
            if (!player2) {
                GameHook::UpdateHooks();
            }
        }

        DrainItemQueue();
        DrainTrapQueue();

        uintptr_t playerBase = 0;
        bool isPlayerLoaded = ReadPlayerBase(playerBase) && playerBase != 0;
        bool inShop = (g_apLiveRecognizedStage == 0xF01 || g_apLiveRecognizedStage == 0xA10);

        if (!isPlayerLoaded && !inShop) {
            PollChapterBlock();
            CheckAutoUnlockRequiem();
            EvaluateGoal();
            return;
        }

        {
            int grace = g_apSyncGraceFrames.load();
            if (grace > 0) g_apSyncGraceFrames.store(grace - 1);
        }

        if (g_apDeathLinkPending.load()) {
            if (KillPlayer()) { g_apDeathLinkPending.store(false); g_apDeathSuppressFrames.store(600); Log("Applied incoming DeathLink - HP set to 0."); }
        }

        if (g_apDeathLinkKillActive.load()) {
            int32_t moveNow = 0;
            if (ReadMoveId(moveNow) && !IsDeathMove(moveNow)) {
                if (++g_apDeathLinkAliveFrames >= 60) {
                    g_apDeathLinkAliveFrames = 0;
                    g_apDeathLinkKillActive.store(false);
                    g_apVerseInit = false;
                }
            }
            else {
                g_apDeathLinkAliveFrames = 0;
            }
        }

        int32_t moveNow = 0;
        bool haveMove = ReadMoveId(moveNow);
        bool nowDeath = haveMove && IsDeathMove(moveNow);
        bool justDied = !g_apPrevMoveWasDeath && nowDeath;
        if (haveMove) {
            g_apPrevMoveWasDeath = nowDeath;
        }
        else {
            g_apPrevMoveWasDeath = false;
        }

        if (g_apDeathLinkEnabled.load() && g_apConnected) {
            int suppress = g_apDeathSuppressFrames.load();
            if (suppress > 0) g_apDeathSuppressFrames.store(suppress - 1);

            if (!g_apDeathLinkKillActive.load() && suppress == 0 && haveMove) {
                if (justDied) {
                    g_apSendDeathPending.store(true);
                    g_apDeathSuppressFrames.store(300);
                }
            }
        }

        ProcessTrapTimers();
        PollCurseTrap();
        PollPendingRankChecks();
        PollDamageLink();
        PollRingLink();
        EnforceMaxHP();
        PollWeaponPurchases();
        PollTechniqueLearns();
        PollAccessoryObtains();
        PollChapterCompletion();
        CheckAutoUnlockRequiem();
        PollVerseCompletion();
        EvaluateGoal();
        PollChests();
        PollTears();
        PollChapterBlock();

        if (g_apHeartAnimFrames > 0) {
            --g_apHeartAnimFrames;
        }
        else if (g_apLiveRecognizedStage == 0xF01) {
            RestoreHPWrite();
        }
        else if (g_apForceMaxHP.load()) {
            PatchOutHPWrite();
        }

        {
            uintptr_t hpBase = 0;
            int32_t heartFull = 0;
            if (ReadPlayerBase(hpBase) && ReadMem(0x5AA7500, heartFull)) {
                if (g_apPrevHeartFullSeen >= 0 && heartFull > g_apPrevHeartFullSeen) {
                    g_apHeartAnimFrames = 180;
                }
                g_apPrevHeartFullSeen = heartFull;
            }
            else {
                g_apPrevHeartFullSeen = -1;
            }
        }

        if (g_apGhostCleanFrames > 0) {
            --g_apGhostCleanFrames;
            int32_t ghostWitch = 0;
            if (ReadMem(0x5AA7500, ghostWitch) && ghostWitch > 0) {
                WriteMem(0x5AA7500, 0);
            }
            int32_t ghostMoon = 0;
            if (ReadMem(0x5AA7508, ghostMoon) && ghostMoon > 0) {
                WriteMem(0x5AA7508, 0);
            }
        }

        SendCoopSync();
    }

    void Shutdown(bool isProcessTerminating) {
        (void)isProcessTerminating;
        if (ChestFinder::g_armed.load()) ChestFinder::Stop();
        if (ChestFinder::g_veh) {
            RemoveVectoredExceptionHandler(ChestFinder::g_veh);
            ChestFinder::g_veh = nullptr;
        }

        if (g_apClient) {
            g_apConnected = false;
            g_apConnecting = false;
            delete g_apClient;
            g_apClient = nullptr;
        }

        if (g_SteamManager) {
            delete g_SteamManager;
            g_SteamManager = nullptr;
        }

        if (g_wsaInitialized) { WSACleanup(); g_wsaInitialized = false; }
        KillAllBayonettaInstances();
    }
} // namespace Archipelago
