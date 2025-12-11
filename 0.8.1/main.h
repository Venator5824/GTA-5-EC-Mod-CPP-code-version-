#pragma once
#ifndef MAIN_H
#define MAIN_H

#define _CRT_SECURE_NO_WARNINGS
#define _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN

// 1. STANDARD LIBS
#include <string>
#include <vector>
#include <future>
#include <chrono>
#include <map>
#include <fstream>
#include <mutex>
#include <set>
#include <sstream>  
#include <algorithm>
#include <iostream> 
#include <iomanip> 

// 2. CRITICAL TYPES
#include "AbstractTypes.h"
#define NOMINMAX
#include <Windows.h>
#include <dxgi1_4.h>
#pragma comment(lib, "dxgi.lib")

// 3. EXTERNAL
#include "llama.h"
#include "llama-sampling.h"
#include "whisper.h"
#include "FileEnums.h" // <--- This provides the Enum definitions.

// ------------------------------------------------------------
// 4. MODULES (The Source of Truth)
// ------------------------------------------------------------
#include "ConfigReader.h"       
#include "AbstractCalls.h"      
#include "EntityRegistry.h"    
#include "ConversationSystem.h" 
#include "LLM_Inference.h"      
#include "ModHelpers.h"
#include "helperfunctions.h"
#include "OptChatMem.h"


using namespace AbstractGame;


// NPC MEMORY
struct NpcMemory {
    std::string summary;
    std::string lastTopic;
};


//NPC SESSION
struct NpcSession {
    int pedHandle;
    std::string assignedName;
    bool isUniqueCharacter;
    std::vector<std::string> chatHistory;
    std::chrono::steady_clock::time_point lastInteractionTime;
};

// ------------------------------------------------------------
// 5. GLOBAL VARIABLES (Externs only)
// ------------------------------------------------------------



// State
extern ConvoState g_convo_state;
extern InputState g_input_state;
extern InferenceState g_llm_state;
extern bool g_isInitialized;
extern std::mutex g_session_mutex;
extern std::map<std::string, NpcMemory> g_PersistentNpcMemory;
extern std::map<uint32_t, std::string> g_persistent_npc_names;
// Data
extern AHandle g_target_ped;
extern std::string g_current_npc_name;
extern std::vector<std::string> g_chat_history;
extern std::string g_renderText;

// Files
extern std::string LOG_FILE_NAME;
extern std::string LOG_FILE_NAME_METRICS;
extern std::string LOG_FILE_NAME_AUDIO;
extern std::string LOG_FILE_NAME3;

// LLM
extern llama_model* g_model;
extern llama_context* g_ctx;
extern std::future<std::string> g_llm_future;
extern std::string g_llm_response;
extern std::chrono::high_resolution_clock::time_point g_response_start_time;
extern std::chrono::high_resolution_clock::time_point g_llm_start_time;
extern struct llama_adapter_lora* g_lora_adapter;
extern float g_current_tps;

// Audio
#include "miniaudio.h"
extern struct whisper_context* g_whisper_ctx;
extern std::vector<float> g_audio_buffer;
extern ma_device g_capture_device;
extern bool g_is_recording;
extern std::future<std::string> g_stt_future;

// ------------------------------------------------------------
// 6. FUNCTION PROTOTYPES (Exports for ModMain)
// ------------------------------------------------------------

// Logging
void Log(const std::string& msg);
void LogM(const std::string& msg);
void LogA(const std::string& msg);
void LogSystemMetrics(const std::string& ctx);

// Lifecycle
extern "C" __declspec(dllexport) void ScriptMain();
void TERMINATE();
void EndConversation();
bool IsGameInSafeMode();
std::string GetModRootPath();
bool DoesFileExist(const std::string& p);
bool IsKeyJustPressed(int vk);
extern ChatID g_current_chat_ID;

#endif