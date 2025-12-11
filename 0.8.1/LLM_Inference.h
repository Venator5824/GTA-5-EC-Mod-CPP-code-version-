#pragma once
// LLM INFERENCE.h
#include <string>
#include <vector>
#include <future>
#include <chrono>
#include "AbstractTypes.h"
#include "ConfigReader.h" // Needed for NpcPersona
#include "FileEnums.h"



// Global Variables (Externs)
extern InferenceState g_llm_state;
extern std::future<std::string> g_llm_future;
extern std::string g_llm_response;
extern std::chrono::high_resolution_clock::time_point g_response_start_time;
extern std::chrono::high_resolution_clock::time_point g_llm_start_time;

// This was missing and causing massive errors in HelperFunctions:

extern float g_current_tps;

// Functions
bool InitializeLLM(const char* model_path);
void ShutdownLLM();
std::string GenerateLLMResponse(const std::string& fullPrompt);
std::string AssemblePrompt(AHandle targetPed, AHandle playerPed, const std::vector<std::string>& chatHistory);
std::string CleanupResponse(std::string text);
std::string PerformChatSummarization(const std::string& npcName, const std::vector<std::string>& history);
std::string GenerateNpcName(const NpcPersona& persona);
void LogLLM(const std::string& message);
void LogMemoryStats();
void UpdateTPS(int tokensGenerated, double elapsedSeconds);

// Audio Functions
bool InitializeWhisper(const char* model_path);
bool InitializeAudioCaptureDevice();
void StartAudioRecording();
void StopAudioRecording();
std::string TranscribeAudio(std::vector<float> pcm_data);
void ShutdownWhisper();
void ShutdownAudioDevice();

// Audio Globals
extern bool g_is_recording;
extern std::future<std::string> g_stt_future;
extern std::vector<float> g_audio_buffer;

//EOF