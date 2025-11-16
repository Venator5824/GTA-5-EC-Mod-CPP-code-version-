#pragma once
#ifndef MAIN_H
#define MAIN_H

// ------------------------------------------------------------
// SECTION 1: C++ & WINDOWS LIBRARIES (IN CORRECT ORDER)
// ------------------------------------------------------------
#define _CRT_SECURE_NO_WARNINGS
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

// Windows & DX (must be before miniaudio)
#include <Windows.h>
#include <psapi.h>
#include <dxgi1_4.h>
#pragma comment(lib, "dxgi.lib")

// ------------------------------------------------------------
// SECTION 2: EXTERNAL LIBRARIES (ScriptHook, AI, Audio)
// ------------------------------------------------------------
#pragma comment(lib, "ScriptHookV.lib")
#include "types.h"      // ScriptHookV types
#include "natives.h"    // ScriptHookV natives
#include "enums.h"      // ScriptHookV enums
#include "nativeCaller.h"
#include "llama.h"
#include "llama-sampling.h"
#include "whisper.h"
#include "miniaudio.h"  // **MUST BE LAST**

// ------------------------------------------------------------
// SECTION 3: PROJECT HEADERS + NPC PERSONA STRUCT
// ------------------------------------------------------------
// <<< FIX >>>  NpcPersona moved here – visible to every file that includes main.h
struct NpcPersona {
    Hash hash = 0;
    bool isHuman = true;
    std::string modelName = "DEFAULT_UNKNOWN_PED";
    std::string inGameName = "";
    std::string type = "DEFAULT";
    std::string relationshipGroup = "Ambient";
    std::string subGroup = "";
    std::string gender = "M";               // default male
    std::string behaviorTraits = "generic, neutral";
};

#include "ConfigReader.h"   // now knows NpcPersona
#include "ModHelpers.h"
#include "LLM_Inference.h"

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
extern Ped g_target_ped;
extern std::vector<std::string> g_chat_history;
extern std::string g_current_npc_name;
extern std::string LOG_FILE_NAME;
extern std::string LOG_FILE_NAME_METRICS;
extern std::string LOG_FILE_NAME_AUDIO;

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
std::string GenerateLLMResponse(const std::string& fullPrompt);
std::string AssemblePrompt(Ped targetPed, Ped playerPed, const std::vector<std::string>& chatHistory);
std::string CleanupResponse(std::string text);
std::string GenerateNpcName(const NpcPersona& persona);
void LogLLM(const std::string& message);
void LogMemoryStats();
bool InitializeWhisper(const char* model_path);
bool InitializeAudioCaptureDevice();
void StartAudioRecording();
void StopAudioRecording();
std::string TranscribeAudio(std::vector<float> pcm_data);
void ShutdownWhisper();
void ShutdownAudioDevice();
std::string WordWrap(const std::string& text, size_t limit);

// ModHelpers.cpp
void ApplyNpcTasks(Ped npc, Ped player);

#endif // MAIN_H
