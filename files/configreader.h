#pragma once
#include "main.h"          // <<< FIX >>> only include main.h (NpcPersona is there)
#include "types.h"
#include <string>
#include <map>
#include <vector>

// ------------------------------------------------------------
// SETTINGS STRUCT
// ------------------------------------------------------------
struct ModSettings {
    bool Enabled = false;
    int ActivationKey = 0x54;
    int ActivationDurationMs = 1000;
    int StopKey_Primary = 0x1B;
    int StopKey_Secondary = 0;
    int StopDurationMs = 3000;
    int MaxInputChars = 128;
    float MaxConversationRadius = 3.0f;
    int MaxOutputChars = 70;
    int MinResponseDelayMs = 1500;
    int MaxHistoryTokens = 2048;
    int DeletionTimer = 120;
    int MaxNpcGetModel = 1;
    int MaxChatHistoryLines = 10;
    int USE_GPU_LAYERS = 0;
    bool USE_VRAM_PREFERED = false;
    int StT_Enabled = 0;
    int TtS_Enabled = 0;
    std::string MODEL_PATH = "";
    std::string MODEL_ALT_NAME = "";
    int StTRB_Activation_Key = 0;
    int DEBUG_LEVEL = 0;
    std::string LOG_NAME = "kkamel.log";
    std::string LOG2_NAME = "kkamel_metrics.log";
    std::string LOG3_NAME = "kkamel_timers.log";
    std::string LOG4_NAME = "kkamel_load.log";
    std::string STT_MODEL_PATH = "";
    std::string STT_MODEL_ALT_NAME = "";
    std::string TTS_MODEL_PATH = "";
    std::string TTS_MODEL_ALT_NAME = "";
};

extern ModSettings g_ModSettings;

// ------------------------------------------------------------
// CONFIG READER CLASS
// ------------------------------------------------------------
class ConfigReader {
public:
    // RAM caches
    static ModSettings g_Settings;
    static std::map<Hash, NpcPersona> g_PersonaCache;
    static std::map<std::string, NpcPersona> g_DefaultTypeCache;
    static std::map<std::string, std::string> g_RelationshipMatrix;
    static std::map<std::string, std::string> g_ZoneContextCache;
    static std::map<std::string, std::string> g_OrgContextCache;
    static std::string g_GlobalContextStyle;
    static std::string g_GlobalContextTimeEra;
    static std::string g_GlobalContextLocation;
    static std::vector<std::string> g_chat_history;

    // Public API
    static void LoadAllConfigs();
    static NpcPersona GetPersona(Ped npc);
    static std::string GetRelationship(const std::string& npcSubGroup, const std::string& playerSubGroup);
    static std::string GetZoneContext(const std::string& zoneName);
    static std::string GetOrgContext(const std::string& orgName);
    static std::string GetSetting(const std::string& section, const std::string& key);
    static std::string g_ContentGuidelines;

private:
    static std::string GetValueFromINI(const char* iniPath, const std::string& section,
        const std::string& key, const std::string& defaultValue = "");
    static void LoadINISectionToCache(const char* iniPath, const std::string& section,
        std::map<std::string, std::string>& cache);
    static void LoadPersonaDatabase();
    static void LoadRelationshipDatabase();
    static void LoadWorldContextDatabase();
    static Hash GetHashFromHex(const std::string& hexString);
    static int KeyNameToVK(const std::string& keyName);
    static std::vector<std::string> SplitString(const std::string& str, char delimiter);
};
