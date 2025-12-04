#pragma once
#ifndef MAIN_H
#define MAIN_H

#define _CRT_SECURE_NO_WARNINGS
#define _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS
// ------------------------------------------------------------
// SECTION 1: C++ & WINDOWS LIBRARIES (IN CORRECT ORDER)
// ------------------------------------------------------------
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN

#include <string>
#include <vector>
#include <future>
#include <chrono>
#include <map>
#include <fstream>
#include <stdexcept>
#include <iomanip>
#include <ctime>
#include <algorithm>
#include <random>
#include <sstream>
#include <cctype>
#include <iostream>
#include <set>

// Windows & DX (must be before miniaudio)
#include <Windows.h>
#include <psapi.h>
#include <dxgi1_4.h>
#pragma comment(lib, "dxgi.lib")

// ------------------------------------------------------------
// SECTION 2: EXTERNAL LIBRARIES (ScriptHook, AI, Audio)
// ------------------------------------------------------------

// ScriptHookV types
// ScriptHookV enums

#include "llama.h"
#include "llama-sampling.h"
#include "whisper.h"
#include "miniaudio.h"  // **MUST BE LAST**

   // now knows NpcPersona
#include "ModHelpers.h"
#include "LLM_Inference.h"
#include "ConfigReader.h"
#include "SharedData.h"
#include "helperfunctions.h"

#include "AbstractTypes.h"
#include "AbstractCalls.h"
using namespace AbstractGame;

// ------------------------------------------------------------
// SECTION 3: PROJECT HEADERS + NPC PERSONA STRUCT
// ------------------------------------------------------------
// 
struct VoiceConfig {
std::string gender;
std::string age;
std::string voice;
std::string special;
};
// <<< FIX >>>  NpcPersona moved here – visible to every file that includes main.h
struct NpcPersona {
    uint32_t uint32_t = 0;
    bool isHuman = true;
    std::string modelName = "DEFAULT_UNKNOWN_PED";
    std::string inGameName = "";
    std::string type = "DEFAULT";
    std::string relationshipGroup = "Ambient";
    std::string subGroup = "";
    std::string gender = "M";               // default male
    std::string behaviorTraits = "generic, neutral";
    std::string assignedVoiceId;
};

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
    uint32_t Max_Working_Input = 4096;
    int Allow_EX_Script = 0; 
    int KV_Cache_Quantization_Type = -1;
    int n_batch = 512;
    int n_ubatch = 256;
    float temp = 0.65;
    float top_k;
    float top_p;
    float min_p;
    float float_p;
    float repeat_penalty;
    float freq_penalty;
    float presence_penalty;
    int Level_Optimization_Chat_Going = 0;

    //LoRA
    int Lora_Enabled = 0;
    std::string LORA_ALT_NAME;
    std::string LORA_FILE_PATH;
    float LORA_SCALE;

    //Memory Logic
    int DeletionTimer = 120;
    int MaxAllowedChatHistory = 1;
    int DeletionTimerClearFull = 160;
    int TrySummarizeChat = 0;

};


struct NpcSession {
    int pedHandle;
    std::string assignedName;
    std::vector<std::string> chatHistory;
    std::chrono::steady_clock::time_point lastInteractionTime;
    bool isUniqueCharacter;
};


extern ModSettings g_ModSettings;

extern struct llama_adapter_lora* g_lora_adapter;

struct KnowledgeSection {
    std::string sectionName;
    std::string content; // Holds the full, raw content of the section
    std::map<std::string, std::string> keyValues; // Holds individual lines (e.g., "Hospital" -> "Mount Zonah...")
    std::vector<std::string> keywords;
    bool isAlwaysLoaded = false;
    bool loadEntireSectionOnMatch = true; // Your new setting
};

class ConfigReader {
public:
    // RAM caches
    static ModSettings g_Settings;
    static std::map<uint32_t, NpcPersona> g_PersonaCache;
    static std::map<std::string, NpcPersona> g_DefaultTypeCache;
    static std::map<std::string, std::string> g_RelationshipMatrix;
    static std::map<std::string, std::string> g_ZoneContextCache;
    static std::map<std::string, std::string> g_OrgContextCache;
    static std::string g_GlobalContextStyle;
    static std::string g_GlobalContextTimeEra;
    static std::string g_GlobalContextLocation;
    static std::vector<std::string> g_chat_history;
    static std::map<std::string, VoiceConfig> g_VoiceMap;
    static std::map<std::string, KnowledgeSection> g_KnowledgeDB; // The new database

    // Public API
    static void LoadAllConfigs();
    static NpcPersona GetPersona(AHandle npc);
    static std::string GetRelationship(const std::string& npcSubGroup, const std::string& playerSubGroup);
    static std::string GetZoneContext(const std::string& zoneName);
    static std::string GetOrgContext(const std::string& orgName);
    static std::string GetSetting(const std::string& section, const std::string& key);
    static std::string g_ContentGuidelines;
    static void LoadKnowledgeDatabase(); 

private:
    static std::string GetValueFromINI(const char* iniPath, const std::string& section,
        const std::string& key, const std::string& defaultValue = "");
    static void LoadINISectionToCache(const char* iniPath, const std::string& section,
        std::map<std::string, std::string>& cache);
    static void LoadPersonaDatabase();
    static void LoadRelationshipDatabase();
    static void LoadVoiceDatabase();
    static void LoadWorldContextDatabase();
    static uint32_t GetHashFromHex(const std::string& hexString);
    static int KeyNameToVK(const std::string& keyName);
    static std::vector<std::string> SplitString(const std::string& str, char delimiter);
};


//EOF

// ------------------------------------------------------------
// SECTION 4: GLOBAL ENUMS
// ------------------------------------------------------------
enum class ConvoState { IDLE, IN_CONVERSATION };
enum class InputState { IDLE, WAITING_FOR_INPUT, RECORDING, TRANSCRIBING };
enum class InferenceState { IDLE, RUNNING, COMPLETE };

// ------------------------------------------------------------
// SECTION 5: GLOBAL VARIABLE DECLARATIONS (extern)
// ------------------------------------------------------------
extern ConvoState g_convo_state;
extern InputState g_input_state;
extern AHandle g_target_ped;
extern std::vector<std::string> g_chat_history;
extern std::string g_current_npc_name;
extern std::string LOG_FILE_NAME;
extern std::string LOG_FILE_NAME_METRICS;
extern std::string LOG_FILE_NAME_AUDIO;
extern std::mutex g_session_mutex;

// LLM_Inference globals
extern llama_model* g_model;
extern llama_context* g_ctx;
extern InferenceState g_llm_state;
extern std::future<std::string> g_llm_future;
extern std::string g_llm_response;
extern std::chrono::high_resolution_clock::time_point g_response_start_time;
extern struct whisper_context* g_whisper_ctx;
extern std::vector<float> g_audio_buffer;
extern ma_device g_capture_device;
extern bool g_is_recording;
extern std::future<std::string> g_stt_future;
extern std::string LOG_FILE_NAME3;

// ------------------------------------------------------------
// SECTION 6: FUNCTION PROTOTYPES
// ------------------------------------------------------------
// ModMain.cpp
void Log(const std::string& msg);
void LogM(const std::string& msg);
void LogA(const std::string& msg);
void LogSystemMetrics(const std::string& ctx);
bool IsKeyJustPressed(int vk);
std::string GetModRootPath();
bool DoesFileExist(const std::string& p);
void EndConversation();
bool IsGameInSafeMode();
extern "C" __declspec(dllexport) void ScriptMain();   // <<< FIX >>> correct linkage

void TERMINATE();

// LLM_Inference.cpp
bool InitializeLLM(const char* model_path);
void ShutdownLLM();

//summairze history post conversation
std::string SummarizeChatHistory(const std::vector<std::string>& history, std::string npcName, std::string playerName);

std::string GenerateLLMResponse(const std::string& fullPrompt);
std::string AssemblePrompt(AHandle targetPed, AHandle playerPed, const std::vector<std::string>& chatHistory);
std::string CleanupResponse(std::string text);
std::string GenerateNpcName(const NpcPersona& persona);
void LogLLM(const std::string& message);
void LogMemoryStats();
bool InitializeWhisper(const char* model_path);
bool InitializeAudioCaptureDevice();
void StartAudioRecording();
void StopAudioRecording();
std::string NormalizeString(const std::string& input);
std::string TranscribeAudio(std::vector<float> pcm_data);
void ShutdownWhisper();
void ShutdownAudioDevice();
std::string WordWrap(const std::string& text, size_t limit);



// NPC MEMORY
struct NpcMemory {
    std::vector<std::string> chatHistory;
    std::chrono::steady_clock::time_point lastInteractionTime;
};


// ModHelpers.cpp
void ApplyNpcTasks(AHandle npc, AHandle player);

void UpdateNpcMemoryJanitor(bool force_clear_all);

extern C;

#endif 




