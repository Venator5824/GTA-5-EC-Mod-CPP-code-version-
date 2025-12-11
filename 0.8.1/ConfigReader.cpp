#include "AbstractCalls.h"
using namespace AbstractGame;
using namespace AbstractTypes;

#define _CRT_SECURE_NO_WARNINGS
#include "ConfigReader.h"
#include "main.h"




// const std::string LOG_FILE_NAME4 = ConfigReader::g_Settings.LOG4_NAME;

// --- LOGGING ---


// --- INITIALIZATION OF STATIC CACHES AND PATHS ---
const char* SETTINGS_INI_PATH = ".\\GTA_LLM_Settings.ini";
const char* RELATIONSHIPS_INI_PATH = ".\\GTA_LLM_Relationships.ini";
const char* PERSONAS_INI_PATH = ".\\GTA_LLM_Personas.ini";
std::map<std::string, VoiceConfig> ConfigReader::g_VoiceMap;
std::map<std::string, KnowledgeSection> ConfigReader::g_KnowledgeDB;
// Initialization of static members
ModSettings ConfigReader::g_Settings;
std::map<uint32_t, NpcPersona> ConfigReader::g_PersonaCache;
std::map<std::string, NpcPersona> ConfigReader::g_DefaultTypeCache;
std::map<std::string, std::string> ConfigReader::g_RelationshipMatrix;
std::map<std::string, std::string> ConfigReader::g_ZoneContextCache;
std::map<std::string, std::string> ConfigReader::g_OrgContextCache;
std::string ConfigReader::g_GlobalContextStyle;
std::string ConfigReader::g_GlobalContextTimeEra;
std::string ConfigReader::g_GlobalContextLocation;
std::string ConfigReader::g_ContentGuidelines;
//std::vector<std::string> ConfigReader::g_chat_history;


// --- HELPER FUNCTION IMPLEMENTATIONS ---
std::string ConfigReader::GetValueFromINI(const char* iniPath, const std::string& section, const std::string& key, const std::string& defaultValue) {
    char temp[2048];
    uint32_t result = GetPrivateProfileStringA(section.c_str(), key.c_str(), defaultValue.c_str(), temp, sizeof(temp), iniPath);
    std::string value = (result == 0) ? defaultValue : std::string(temp);
    size_t commentPos = value.find(';');
    if (commentPos != std::string::npos) {
        value = value.substr(0, commentPos);
    }
    value.erase(0, value.find_first_not_of(" \t\r\n"));
    value.erase(value.find_last_not_of(" \t\r\n") + 1);
    return value;
}

std::vector<std::string> ConfigReader::SplitString(const std::string& str, char delimiter) {
    LogConfig("SplitString called with string: " + str + ", delimiter: " + delimiter);
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream tokenStream(str);
    while (std::getline(tokenStream, token, delimiter)) {
        token.erase(0, token.find_first_not_of(" \t"));
        token.erase(token.find_last_not_of(" \t") + 1);
        if (!token.empty()) {
            tokens.push_back(token);
        }
    }
    LogConfig("SplitString returned " + std::to_string(tokens.size()) + " tokens");
    return tokens;
}

int ConfigReader::KeyNameToVK(const std::string& keyName) {
    LogConfig("KeyNameToVK called with keyName: " + keyName);
    std::string upperKey = keyName;
    std::transform(upperKey.begin(), upperKey.end(), upperKey.begin(), ::toupper);
    if (upperKey.length() == 1 && upperKey[0] >= 'A' && upperKey[0] <= 'Z') {
        int vk_code = upperKey[0];
        LogConfig("KeyNameToVK mapped " + keyName + " to VK_" + upperKey + " (" + std::to_string(vk_code) + ")");
        return vk_code;
    }
    if (upperKey.length() == 1 && upperKey[0] >= '0' && upperKey[0] <= '9') {
        int vk_code = upperKey[0];
        LogConfig("KeyNameToVK mapped " + keyName + " to VK_" + upperKey + " (" + std::to_string(vk_code) + ")");
        return vk_code;
    }
    if (upperKey == "ESC" || upperKey == "VK_ESCAPE") {
        LogConfig("KeyNameToVK mapped " + keyName + " to VK_ESCAPE");
        return VK_ESCAPE;
    }
    if (upperKey == "F1" || upperKey == "VK_F1") {
        LogConfig("KeyNameToVK mapped " + keyName + " to VK_F1");
        return VK_F1;
    }
    if (upperKey == "F2" || upperKey == "VK_F2") {
        LogConfig("KeyNameToVK mapped " + keyName + " to VK_F2");
        return VK_F2;
    }
    if (upperKey == "F3" || upperKey == "VK_F3") {
        LogConfig("KeyNameToVK mapped " + keyName + " to VK_F3");
        return VK_F3;
    }
    if (upperKey == "F4" || upperKey == "VK_F4") {
        LogConfig("KeyNameToVK mapped " + keyName + " to VK_F4");
        return VK_F4;
    }
    if (upperKey == "F5" || upperKey == "VK_F5") {
        LogConfig("KeyNameToVK mapped " + keyName + " to VK_F5");
        return VK_F5;
    }
    if (upperKey == "F6" || upperKey == "VK_F6") {
        LogConfig("KeyNameToVK mapped " + keyName + " to VK_F6");
        return VK_F6;
    }
    if (upperKey == "F7" || upperKey == "VK_F7") {
        LogConfig("KeyNameToVK mapped " + keyName + " to VK_F7");
        return VK_F7;
    }
    if (upperKey == "F8" || upperKey == "VK_F8") {
        LogConfig("KeyNameToVK mapped " + keyName + " to VK_F8");
        return VK_F8;
    }
    if (upperKey == "F9" || upperKey == "VK_F9") {
        LogConfig("KeyNameToVK mapped " + keyName + " to VK_F9");
        return VK_F9;
    }
    if (upperKey == "F10" || upperKey == "VK_F10") {
        LogConfig("KeyNameToVK mapped " + keyName + " to VK_F10");
        return VK_F10;
    }
    if (upperKey == "F11" || upperKey == "VK_F11") {
        LogConfig("KeyNameToVK mapped " + keyName + " to VK_F11");
        return VK_F11;
    }
    if (upperKey == "F12" || upperKey == "VK_F12") {
        LogConfig("KeyNameToVK mapped " + keyName + " to VK_F12");
        return VK_F12;
    }
    LogConfig("KeyNameToVK FAILED. Unmapped key: " + keyName + ". Returning 0.");
    return 0;
}

uint32_t ConfigReader::GetHashFromHex(const std::string& hexString) {
    if (hexString.empty() || (hexString.rfind("0x", 0) != 0 && hexString.rfind("0X", 0) != 0)) {
        LogConfig("GetHashFromHex returned 0 (invalid hex string format)");
        return 0;
    }
    try {
        unsigned long hash = std::stoul(hexString, nullptr, 16);
        return static_cast<uint32_t>(hash);
    }
    catch (const std::exception& e) {
        LogConfig("GetHashFromHex caught exception: " + std::string(e.what()) + ", returning 0");
        return 0;
    }
}

void ConfigReader::LoadINISectionToCache(const char* iniPath, const std::string& section, std::map<std::string, std::string>& cache) {
    LogConfig("LoadINISectionToCache called for INI: " + std::string(iniPath) + ", section: " + section);
    const int bufferSize = 32767;
    std::vector<char> buffer(bufferSize);
    DWORD bytesRead = GetPrivateProfileSectionA(section.c_str(), buffer.data(), bufferSize, iniPath);
    if (bytesRead == 0 || bytesRead >= bufferSize - 2) {
        LogConfig("LoadINISectionToCache: No data read or buffer too small for section " + section);
        return;
    }
    int entries = 0;
    for (char* pKey = buffer.data(); *pKey; ) {
        std::string keyValue(pKey);
        size_t separatorPos = keyValue.find('=');
        if (separatorPos == std::string::npos) {
            separatorPos = keyValue.find(':');
        }
        if (separatorPos != std::string::npos) {
            std::string key = keyValue.substr(0, separatorPos);
            std::string value = keyValue.substr(separatorPos + 1);
            key.erase(0, key.find_first_not_of(" \t"));
            key.erase(key.find_last_not_of(" \t") + 1);
            value.erase(0, value.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of(" \t") + 1);
            if (!key.empty() && !value.empty()) {
                cache[key] = value;
                entries++;
            }
        }
        pKey += (keyValue.length() + 1);
    }
    LogConfig("LoadINISectionToCache: Loaded " + std::to_string(entries) + " entries for section " + section);
}

// --- CORE LOADING FUNCTIONS ---
void ConfigReader::LoadWorldContextDatabase() {
    LogConfig("LoadWorldContextDatabase started");
    g_GlobalContextStyle = GetValueFromINI(SETTINGS_INI_PATH, "GLOBAL_CONTEXT", "STYLE");
    g_GlobalContextTimeEra = GetValueFromINI(SETTINGS_INI_PATH, "GLOBAL_CONTEXT", "TIME_ERA");
    g_GlobalContextLocation = GetValueFromINI(SETTINGS_INI_PATH, "GLOBAL_CONTEXT", "LOCATION");
    LoadINISectionToCache(SETTINGS_INI_PATH, "KEY_ORGANIZATIONS", g_ZoneContextCache);
    LoadINISectionToCache(SETTINGS_INI_PATH, "CITY_CONTEXT", g_ZoneContextCache);
    LoadINISectionToCache(SETTINGS_INI_PATH, "MEDIA_AND_CULTURE", g_ZoneContextCache);
    LoadINISectionToCache(SETTINGS_INI_PATH, "POLITICS", g_ZoneContextCache);
    LoadINISectionToCache(SETTINGS_INI_PATH, "ECONOMY_AND_BRANDS", g_ZoneContextCache);
    LoadINISectionToCache(SETTINGS_INI_PATH, "ENTERTAINMENT", g_ZoneContextCache);
    LoadINISectionToCache(SETTINGS_INI_PATH, "HEALTH_AND_ISSUES", g_ZoneContextCache);
    LoadINISectionToCache(SETTINGS_INI_PATH, "COMPANIES", g_ZoneContextCache);
    LoadINISectionToCache(SETTINGS_INI_PATH, "GANGS", g_ZoneContextCache);
    LoadINISectionToCache(SETTINGS_INI_PATH, "CELEBRITIES", g_ZoneContextCache);
    LoadINISectionToCache(SETTINGS_INI_PATH, "NORTH_YANKTON", g_ZoneContextCache);
    LoadINISectionToCache(SETTINGS_INI_PATH, "REGIONS_LIST", g_ZoneContextCache);
    LogConfig("LoadWorldContextDatabase completed");
}

void ConfigReader::LoadRelationshipDatabase() {
    LogConfig("LoadRelationshipDatabase started");
    const int bufferSize = 65536;
    std::vector<char> sectionNamesBuffer(bufferSize);
    DWORD bytesRead = GetPrivateProfileSectionNamesA(sectionNamesBuffer.data(), bufferSize, RELATIONSHIPS_INI_PATH);
    if (bytesRead == 0 || bytesRead >= bufferSize - 2) {
        LogConfig("LoadRelationshipDatabase: No sections found or buffer too small for " + std::string(RELATIONSHIPS_INI_PATH));
        return;
    }
    int sectionCount = 0;
    for (char* pSection = sectionNamesBuffer.data(); *pSection; ) {
        std::string sectionName(pSection);
        if (sectionName == "RELATIONSHIPS" || sectionName == "TYPES" || sectionName == "GENDERS" ||
            sectionName == "GANG_SUBGROUPS" || sectionName == "LAW_SUBGROUPS" ||
            sectionName == "PRIVATE_SUBGROUPS" || sectionName == "BUSINESS_SUBGROUPS") {
            pSection += (sectionName.length() + 1);
            continue;
        }
        std::map<std::string, std::string> sectionCache;
        LoadINISectionToCache(RELATIONSHIPS_INI_PATH, sectionName, sectionCache);
        for (const auto& pair : sectionCache) {
            std::string key = sectionName + ":" + pair.first;
            g_RelationshipMatrix[key] = pair.second;
        }
        sectionCount++;
        pSection += (sectionName.length() + 1);
    }
    // LogConfig("LoadRelationshipDatabase: Loaded " + std::to_string(sectionCount) + " character/group sections");
}

std::string ConfigReader::GetSetting(const std::string& section, const std::string& key) {
    std::string value = GetValueFromINI(SETTINGS_INI_PATH, section, key);
    return value;}

void ConfigReader::LoadPersonaDatabase() {
    LogConfig("LoadPersonaDatabase started");
    const int bufferSize = 65536;
    std::vector<char> sectionNamesBuffer(bufferSize);
    DWORD bytesRead = GetPrivateProfileSectionNamesA(sectionNamesBuffer.data(), bufferSize, PERSONAS_INI_PATH);
    if (bytesRead == 0 || bytesRead >= bufferSize - 2) {
        LogConfig("LoadPersonaDatabase: No sections found or buffer too small in " + std::string(PERSONAS_INI_PATH));
        return;
    }
    int personaCount = 0;
    for (char* pSection = sectionNamesBuffer.data(); *pSection; ) {
        std::string sectionName(pSection);
        NpcPersona persona;
        persona.modelName = sectionName;
        persona.modelHash = GetHashFromHex(GetValueFromINI(PERSONAS_INI_PATH, sectionName, "Hash"));
        persona.isHuman = (GetValueFromINI(PERSONAS_INI_PATH, sectionName, "IsHuman") == "1");
        persona.inGameName = GetValueFromINI(PERSONAS_INI_PATH, sectionName, "InGameName");
        persona.type = GetValueFromINI(PERSONAS_INI_PATH, sectionName, "Type");
        persona.relationshipGroup = GetValueFromINI(PERSONAS_INI_PATH, sectionName, "Relationship");
        persona.subGroup = GetValueFromINI(PERSONAS_INI_PATH, sectionName, "SubGroup");
        persona.gender = GetValueFromINI(PERSONAS_INI_PATH, sectionName, "Gender");
        persona.behaviorTraits = GetValueFromINI(PERSONAS_INI_PATH, sectionName, "Behavior");
        if (sectionName.rfind("DEFAULT_", 0) == 0) {
            g_DefaultTypeCache[persona.type] = persona;
        }
        else if (persona.modelHash != 0) {
            g_PersonaCache[persona.modelHash] = persona;
        }
        personaCount++;
        pSection += (sectionName.length() + 1);
    }
    LogConfig("LoadPersonaDatabase: Loaded " + std::to_string(personaCount) + " personas");
}

// --- CORE PUBLIC FUNCTIONS ---
void ConfigReader::LoadAllConfigs() {
    LogConfig("LoadAllConfigs started");
    try {
        g_Settings.Enabled = (GetValueFromINI(SETTINGS_INI_PATH, "SETTINGS", "Enabled", "1") == "1");
        g_Settings.ActivationKey = KeyNameToVK(GetValueFromINI(SETTINGS_INI_PATH, "SETTINGS", "ACTIVATION_KEY", "T"));
        g_Settings.ActivationDurationMs = std::stoi(GetValueFromINI(SETTINGS_INI_PATH, "SETTINGS", "ACTIVATION_DURATION", "1000"));
        g_Settings.StopKey_Primary = KeyNameToVK(GetValueFromINI(SETTINGS_INI_PATH, "SETTINGS", "STOP_KEY", "U")); // Simplified splitter logic
        g_Settings.StopDurationMs = std::stoi(GetValueFromINI(SETTINGS_INI_PATH, "SETTINGS", "STOP_DURATION", "3000"));
        g_Settings.MaxConversationRadius = std::stof(GetValueFromINI(SETTINGS_INI_PATH, "SETTINGS", "MAX_CONVERSATION_RADIUS", "3.0"));

        g_Settings.MaxOutputChars = std::stoi(GetValueFromINI(SETTINGS_INI_PATH, "SETTINGS", "MAX_OUTPUT_CHARS", "512"));
        g_Settings.MaxInputChars = std::stoi(GetValueFromINI(SETTINGS_INI_PATH, "SETTINGS", "MAX_INPUT_CHARS", "786"));
        g_Settings.MaxHistoryTokens = std::stoi(GetValueFromINI(SETTINGS_INI_PATH, "SETTINGS", "MAX_REMEMBER_HISTORY", "1024"));
        g_Settings.MaxChatHistoryLines = std::stoi(GetValueFromINI(SETTINGS_INI_PATH, "SETTINGS", "MAX_PROMPT_MEMORY_HALFED", "16"));
        g_Settings.MinResponseDelayMs = std::stoi(GetValueFromINI(SETTINGS_INI_PATH, "SETTINGS", "MIN_RESPONSE_DELAY_MS", "750"));

        g_Settings.USE_VRAM_PREFERED = (GetValueFromINI(SETTINGS_INI_PATH, "SETTINGS", "USE_VRAM_PREFERED", "1") == "1");
        g_Settings.USE_GPU_LAYERS = std::stoi(GetValueFromINI(SETTINGS_INI_PATH, "SETTINGS", "USE_GPU_LAYERS", "33"));

        // Models & Logging
        g_Settings.MODEL_PATH = GetValueFromINI(SETTINGS_INI_PATH, "SETTINGS", "MODEL_PATH", "");
        g_Settings.MODEL_ALT_NAME = GetValueFromINI(SETTINGS_INI_PATH, "SETTINGS", "MODEL_ALT_NAME", "Phi3.gguf");
        g_Settings.LOG_NAME = GetValueFromINI(SETTINGS_INI_PATH, "SETTINGS", "LOG_NAME", "kkamel.log");
        g_Settings.DEBUG_LEVEL = std::stoi(GetValueFromINI(SETTINGS_INI_PATH, "SETTINGS", "DEBUG_LEVEL", "0"));

        // STT / TTS
        g_Settings.StT_Enabled = (GetValueFromINI(SETTINGS_INI_PATH, "SETTINGS", "SPEECH_TO_TEXT", "0") == "1");
        g_Settings.StTRB_Activation_Key = KeyNameToVK(GetValueFromINI(SETTINGS_INI_PATH, "SETTINGS", "SPEECH_TO_TEXT_RECORDING_BUTTON", "L"));
        g_Settings.STT_MODEL_PATH = GetValueFromINI(SETTINGS_INI_PATH, "SETTINGS", "STT_MODEL_PATH", "");
        g_Settings.STT_MODEL_ALT_NAME = GetValueFromINI(SETTINGS_INI_PATH, "SETTINGS", "STT_MODEL_ALT_NAME", "ggml-base.en.bin");

        g_Settings.TtS_Enabled = (GetValueFromINI(SETTINGS_INI_PATH, "SETTINGS", "TEXT_TO_SPEECH", "0") == "1");
        g_Settings.TTS_MODEL_PATH = GetValueFromINI(SETTINGS_INI_PATH, "SETTINGS", "TTS__MODEL_PATH", "");
        g_Settings.TTS_MODEL_ALT_NAME = GetValueFromINI(SETTINGS_INI_PATH, "SETTINGS", "TTS_MODEL_ALT_NAME", "");

        // 2. MEMORY & OPTIMIZATION SETTINGS
        g_Settings.DeletionTimer = std::stoi(GetValueFromINI(SETTINGS_INI_PATH, "SETTINGS", "DELETION_TIMER", "120"));
        g_Settings.MaxAllowedChatHistory = std::stoi(GetValueFromINI(SETTINGS_INI_PATH, "SETTINGS", "MAX_ALLOWED_CHAT_HISTORY", "1"));
        g_Settings.DeletionTimerClearFull = std::stoi(GetValueFromINI(SETTINGS_INI_PATH, "SETTINGS", "DELETION_TIMER_CLEAR_FULL", "160"));

        g_Settings.TrySummarizeChat = std::stoi(GetValueFromINI(SETTINGS_INI_PATH, "SETTINGS", "TRY_SUMMARIZE_CHAT", "1"));
        g_Settings.MIN_PCSREMEMBER_SIZE = std::stoi(GetValueFromINI(SETTINGS_INI_PATH, "SETTINGS", "MIN_PCSREMEMBER_SIZE", "5"));
        g_Settings.MAX_PCSREMEMBER_SIZE = std::stoi(GetValueFromINI(SETTINGS_INI_PATH, "SETTINGS", "MAX_PCSREMEMBER_SIZE", "256"));
        g_Settings.Level_Optimization_Chat_Going = std::stoi(GetValueFromINI(SETTINGS_INI_PATH, "SETTINGS", "Level_Optimization_Chat_Going", "0"));

        // 3. ADDITIONAL SETTINGS (ADVANCED & LORA)
        try { g_Settings.Max_Working_Input = std::stoi(GetValueFromINI(SETTINGS_INI_PATH, "ADDITIONAL_SETTINGS", "MAX_INPUT_SIZE", "4096")); }
        catch (...) {}
        try { g_Settings.n_batch = std::stoi(GetValueFromINI(SETTINGS_INI_PATH, "ADDITIONAL_SETTINGS", "n_batch", "512")); }
        catch (...) {}
        try { g_Settings.n_ubatch = std::stoi(GetValueFromINI(SETTINGS_INI_PATH, "ADDITIONAL_SETTINGS", "n_ubatch", "256")); }
        catch (...) {}
        try { g_Settings.KV_Cache_Quantization_Type = std::stoi(GetValueFromINI(SETTINGS_INI_PATH, "ADDITIONAL_SETTINGS", "kv_cache_model_quantization_type", "0")); }
        catch (...) {}

        // Sampling Params (Floats need try/catch)
        try { g_Settings.temp = std::stof(GetValueFromINI(SETTINGS_INI_PATH, "ADDITIONAL_SETTINGS", "temp", "0.65")); }
        catch (...) {}
        try { g_Settings.top_k = std::stof(GetValueFromINI(SETTINGS_INI_PATH, "ADDITIONAL_SETTINGS", "top_k", "40")); }
        catch (...) {}
        try { g_Settings.top_p = std::stof(GetValueFromINI(SETTINGS_INI_PATH, "ADDITIONAL_SETTINGS", "top_p", "0.95")); }
        catch (...) {}
        try { g_Settings.min_p = std::stof(GetValueFromINI(SETTINGS_INI_PATH, "ADDITIONAL_SETTINGS", "min_p", "0.05")); }
        catch (...) {}
        try { g_Settings.repeat_penalty = std::stof(GetValueFromINI(SETTINGS_INI_PATH, "ADDITIONAL_SETTINGS", "repeat_penatly", "1.1")); }
        catch (...) {} // Matched typo 'penatly'
        try { g_Settings.freq_penalty = std::stof(GetValueFromINI(SETTINGS_INI_PATH, "ADDITIONAL_SETTINGS", "freq_penalty", "0.0")); }
        catch (...) {}
        try { g_Settings.presence_penalty = std::stof(GetValueFromINI(SETTINGS_INI_PATH, "ADDITIONAL_SETTINGS", "presence_penalty", "0.0")); }
        catch (...) {}

        // LoRA
        std::string loraEn = GetValueFromINI(SETTINGS_INI_PATH, "ADDITIONAL_SETTINGS", "lora_enabled", "0");
        g_Settings.Lora_Enabled = (loraEn == "1");
        g_Settings.LORA_ALT_NAME = GetValueFromINI(SETTINGS_INI_PATH, "ADDITIONAL_SETTINGS", "LORA_ALT_NAME", "mod_lora.gguf");
        g_Settings.LORA_FILE_PATH = GetValueFromINI(SETTINGS_INI_PATH, "ADDITIONAL_SETTINGS", "LORA_FILE_PATH", "");
        try { g_Settings.LORA_SCALE = std::stof(GetValueFromINI(SETTINGS_INI_PATH, "ADDITIONAL_SETTINGS", "LORA_SCALE", "1.0")); }
        catch (...) {}

        g_Settings.StopStrings = GetValueFromINI(SETTINGS_INI_PATH, "SETTINGS", "STOP_TOKENS", ""); // From [SETTINGS]
        g_ContentGuidelines = GetValueFromINI(SETTINGS_INI_PATH, "CONTENT_GUIDELINES", "PROMPT_INJECTION", "You are a helpful assistant.");

        // 4. LOAD DATABASES
        LoadWorldContextDatabase();
        LoadRelationshipDatabase();
        LoadPersonaDatabase();
        LoadKnowledgeDatabase();

        if (g_Settings.TtS_Enabled) {
            LoadVoiceDatabase();
        }

        LogConfig("LoadAllConfigs completed");
    }
    catch (const std::exception& e) {
        LogConfig("Exception in LoadAllConfigs: " + std::string(e.what()));
    }
}

NpcPersona ConfigReader::GetPersona(AHandle ped) {
    if (!AbstractGame::IsEntityValid(ped)) return NpcPersona();
    Hash entityHash = AbstractGame::GetEntityModel(ped);
    LogConfig("GetPersona: Entity model hash=" + std::to_string(entityHash));
    auto it = g_PersonaCache.find(entityHash);
    if (it != g_PersonaCache.end()) {
        LogConfig("Persona found in .ini");
        return it->second;
    }
    NpcPersona p;
    p.modelHash = entityHash;
    p.modelName = "UNKNOWN_MODEL";
    p.isHuman = AbstractGame::IsPedHuman(ped);
    p.inGameName = "";
    p.type = "Stranger";
    p.relationshipGroup = "Ambient";
    p.subGroup = "None";
    p.behaviorTraits = "Unknown";
    if (p.isHuman) {
        p.gender = AbstractGame::IsPedMale(ped) ? "Male" : "Female";
    }
    else {
        p.gender = "Neutral";
        p.type = "ANIMAL";
    }
    LogConfig("Persona loaded successfully");
    g_PersonaCache[entityHash] = p;
    return p;
}

std::string ConfigReader::GetRelationship(const std::string& npcSubGroup, const std::string& playerSubGroup) {
    LogConfig("GetRelationship called for npcSubGroup: " + npcSubGroup + ", playerSubGroup: " + playerSubGroup);
    if (npcSubGroup.empty() || playerSubGroup.empty()) {
        LogConfig("GetRelationship: SubGroup is empty, fallback to neutral");
        return "neutral, stranger";
    }
    std::string key = npcSubGroup + ":" + playerSubGroup;
    auto it = g_RelationshipMatrix.find(key);
    if (it != g_RelationshipMatrix.end()) {
        LogConfig("GetRelationship: Found direct relationship: " + it->second);
        return it->second;
    }
    key = playerSubGroup + ":" + npcSubGroup;
    it = g_RelationshipMatrix.find(key);
    if (it != g_RelationshipMatrix.end()) {
        LogConfig("GetRelationship: Found reverse relationship: " + it->second);
        return it->second;
    }
    if (npcSubGroup == "Ambient" || playerSubGroup == "Ambient") {
        LogConfig("GetRelationship: Fallback to neutral, stranger for Ambient group");
        return "neutral, stranger";
    }
    if ((npcSubGroup == "Law" || npcSubGroup == "LSPD" || npcSubGroup == "FIB") &&
        (playerSubGroup == "Gang" || playerSubGroup == "Families" || playerSubGroup == "Ballas")) {
        LogConfig("GetRelationship: Fallback to adversary, distrusts, pursues for Law vs Gang");
        return "adversary, distrusts, pursues";
    }
    if ((npcSubGroup == "Gang" || npcSubGroup == "Families" || playerSubGroup == "Ballas") &&
        (playerSubGroup == "Law" || playerSubGroup == "LSPD" || playerSubGroup == "FIB")) {
        LogConfig("GetRelationship: Fallback to adversary, distrusts, pursued for Gang vs Law");
        return "adversary, distrusts, pursued";
    }
    LogConfig("GetRelationship: Fallback to neutral, unknown");
    return "neutral, unknown";
}

std::string ConfigReader::GetZoneContext(const std::string& zoneName) {
    LogConfig("GetZoneContext called for zoneName: " + zoneName);
    auto it = g_ZoneContextCache.find(zoneName);
    if (it != g_ZoneContextCache.end()) {
        LogConfig("GetZoneContext: Found context: " + it->second);
        return it->second;
    }
    LogConfig("GetZoneContext: No context found, returning default");
    return "Location: Unknown";
}

std::string ConfigReader::GetOrgContext(const std::string& orgName) {
    LogConfig("GetOrgContext called for orgName: " + orgName);
    auto it = g_OrgContextCache.find(orgName);
    if (it != g_OrgContextCache.end()) {
        LogConfig("GetOrgContext: Found context: " + it->second);
        return it->second;
    }
    LogConfig("GetOrgContext: No context found, returning empty string");
    return "";
}
void ConfigReader::LoadVoiceDatabase() {
    LogConfig("LoadVoiceDatabase started");
    // Ensure this path matches your file structure
    const char* vPath = ".\\EC_Voices_list_01.ini";

    const int bufferSize = 65536;
    std::vector<char> sectionNamesBuffer(bufferSize);
    // Note: Windows API uses DWORD, but we cast to uint32_t for safety
    uint32_t bytesRead = GetPrivateProfileSectionNamesA(sectionNamesBuffer.data(), bufferSize, vPath);

    if (bytesRead == 0 || bytesRead >= bufferSize - 2) {
        LogConfig("LoadVoiceDatabase: No sections found in " + std::string(vPath));
        return;
    }

    for (char* pSection = sectionNamesBuffer.data(); *pSection; ) {
        std::string sectionName(pSection);
        if (sectionName.empty()) {
            pSection += 1;
            continue;
        }

        VoiceConfig vc;
        vc.gender = GetValueFromINI(vPath, sectionName, "gender", "");
        vc.age = GetValueFromINI(vPath, sectionName, "age", "");
        vc.voice = GetValueFromINI(vPath, sectionName, "voice", "");
        vc.special = GetValueFromINI(vPath, sectionName, "special", "");

        ConfigReader::g_VoiceMap[sectionName] = vc;
        pSection += (sectionName.length() + 1);
    }
    LogConfig("LoadVoiceDatabase completed. Loaded " + std::to_string(ConfigReader::g_VoiceMap.size()) + " voices.");
}

void ConfigReader::LoadKnowledgeDatabase() {
    g_KnowledgeDB.clear();
    std::ifstream iniFile(SETTINGS_INI_PATH);
    if (!iniFile.is_open()) return;

    std::string line;
    KnowledgeSection currentSection;

    while (std::getline(iniFile, line)) {
        // Trim whitespace
        line.erase(0, line.find_first_not_of(" \t\r\n"));
        line.erase(line.find_last_not_of(" \t\r\n") + 1);

        if (line.empty() || line[0] == ';') continue;

        // New Section Start [SECTION]
        if (line[0] == '[' && line.back() == ']') {
            // Save previous section if valid
            if (!currentSection.sectionName.empty()) {
                g_KnowledgeDB[currentSection.sectionName] = currentSection;
            }
            // Start new section
            currentSection = KnowledgeSection();
            currentSection.sectionName = line;
            continue;
        }

        // Parse Keys and Values
        if (!currentSection.sectionName.empty()) {
            size_t delimiterPos = line.find('=');
            if (delimiterPos == std::string::npos) continue;

            std::string key = line.substr(0, delimiterPos);
            std::string value = line.substr(delimiterPos + 1);

            // Trim Key and Value
            key.erase(0, key.find_first_not_of(" \t"));
            key.erase(key.find_last_not_of(" \t") + 1);
            value.erase(0, value.find_first_not_of(" \t"));
            // (We usually don't trim the end of value to preserve formatting, but basic trim is safe)

            if (key == "isalwaysloaded") {
                currentSection.isAlwaysLoaded = (value == "1");
            }
            else if (key == "loadeverysectionwhenloading") {
                currentSection.loadEntireSectionOnMatch = (value == "1");
            }
            else if (key == "keywordstoload") {
                // Split comma-separated keywords
                std::stringstream ss(value);
                std::string keyword;
                while (std::getline(ss, keyword, ',')) {
                    // Trim keyword
                    keyword.erase(0, keyword.find_first_not_of(" \t"));
                    keyword.erase(keyword.find_last_not_of(" \t") + 1);
                    if (!keyword.empty()) {
                        // NormalizeString must be visible here!
                        currentSection.keywords.push_back(NormalizeString(keyword));
                    }
                }
            }
            else {
                // Regular content content
                currentSection.content += line + "\n";
                currentSection.keyValues[key] = value;
                if (!key.empty()) {
                    currentSection.keywords.push_back(NormalizeString(key));
                }
            }
        }
    }
    // Save the very last section found
    if (!currentSection.sectionName.empty()) {
        g_KnowledgeDB[currentSection.sectionName] = currentSection;
    }
}

// 3. NormalizeString (Global Helper)
std::string NormalizeString(const std::string& input) {
    std::string output = "";
    output.reserve(input.length());
    for (char c : input) {
        char lower_c = std::tolower(static_cast<unsigned char>(c));
        if (std::isalnum(static_cast<unsigned char>(lower_c))) {
            output += lower_c;
        }
    }
    return output;
}

// End of File
