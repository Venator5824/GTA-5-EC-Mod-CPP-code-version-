
#pragma once
#ifndef LLM_INFERENCE_H
#define LLM_INFERENCE_H
#include "ConfigReader.h"
// LLM_Inference.h v1.0.18
// FIX: Header-Reihenfolge korrigiert (behebt "AHandle" und "NpcPersona" nicht definiert)
// FIX: Signatur für AssemblePrompt aktualisiert (akzeptiert jetzt History)
// FIX: CleanupResponse und GenerateNpcName hinzugefügt
/**
#pragma once

#include "main.h" // Stellt AHandle, uint32_t etc. bereit
#include "types.h"
#include "ConfigReader.h" // WICHTIG: Stellt NpcPersona bereit, BEVOR es verwendet wird
#include "llama.h"
#include <string>
#include <vector>
#include <future>
#include <chrono>


#include "whisper.h"
#include "miniaudio.h"

// Definiert den Status der KI-Berechnung
enum class InferenceState { IDLE, RUNNING, COMPLETE };

// --- GLOBALE VARIABLEN (Deklarationen) ---
extern llama_model* g_model;
extern llama_context* g_ctx;
extern InferenceState g_llm_state;
extern std::future<std::string> g_llm_future;
extern std::string g_llm_response;
extern std::chrono::high_resolution_clock::time_point g_response_start_time;
extern std::vector<std::string> g_chat_history;
extern std::string g_current_npc_name;

// --- Hauptfunktionen ---
void LogLLM(const std::string& message);
bool InitializeLLM(const char* model_path);
void ShutdownLLM();
std::string GenerateLLMResponse(const std::string& fullPrompt);

// --- Prompt-Logik ---
// NEUE SIGNATUR v1.0.18
std::string AssemblePrompt(AHandle targetPed, AHandle playerPed, const std::vector<std::string>& chatHistory);
std::string GenerateNpcName(const NpcPersona& persona);
std::string CleanupResponse(std::string text);

std::string GetCurrentWeatherState();
std::string GetCurrentTimeState();

bool InitializeWhisper(const char* model_path);
bool InitializeAudioCaptureDevice();
void StartAudioRecording();
void StopAudioRecording();
std::string TranscribeAudio(std::vector<float> pcm_data);
void ShutdownWhisper();
void ShutdownAudioDevice();

// --- Extern STT Globals (Definiert in LLM_Inference.cpp) ---
// (Diese Zeilen sagen ModMain.cpp, dass diese Variablen existieren)
extern struct whisper_context* g_whisper_ctx;
extern std::vector<float> g_audio_buffer;
extern ma_device g_capture_device;
extern bool g_is_recording;
extern std::future<std::string> g_stt_future;
//End of File

*/

#endif

//EOF




