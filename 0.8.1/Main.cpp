#include "AbstractCalls.h"
#include "AbstractTypes.h"
using namespace AbstractGame;
using namespace AbstractTypes;
// ModMain.cpp – v1.0.22 (Fix: Clean Error entfernt)
// ------------------------------------------------------------
// 1. PREPROCESSOR & LIBRARIES
// ------------------------------------------------------------
#define _CRT_SECURE_NO_WARNINGS
#define NOMINMAX
#define _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS
#include <Windows.h>
#include <Psapi.h>
#pragma comment(lib, "ScriptHookV.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "User32.lib")
#pragma comment(lib, "Advapi32.lib")
#pragma comment(lib, "Psapi.lib")
#include <String.h>
#include "main.h"
#include "EntityRegistry.h"
#include "ConversationSystem.h"
#include "SharedData.h"
#include "LLM_Inference.h"
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
#undef max
#undef min

// 2. INCLUDES
// ------------------------------------------------------------


#define MINIAUDIO_IMPLEMENTATION


ChatID g_current_chat_ID = 0;

// ------------------------------------------------------------
// GLOBAL STATE DEFINITIONS 
// ------------------------------------------------------------
bool g_isInitialized = false;


VoiceBridge* bridge = nullptr;
//std::mutex g_session_mutex;

std::string g_renderText;
uint32_t g_renderEndTime = 0;
int checker_loaded = 1;
HWND g_gameHWND = NULL;      
bool g_wasFullscreen = false; 


ConvoState g_convo_state = ConvoState::IDLE;
InputState g_input_state = InputState::IDLE;



AHandle g_target_ped = 0;


// std::vector<std::string> g_chat_history;



// std::map<uint32_t, std::string> g_persistent_npc_names;
std::string g_current_npc_name;

std::vector<std::future<void>> g_backgroundTasks;

ULONGLONG g_stt_start_time = 0;
std::chrono::high_resolution_clock::time_point g_llm_start_time;
//std::map<int, NpcSession> g_ActiveSessions;
//std::map<std::string, NpcMemory> g_PersistentNpcMemory;

// ------------------------------------------------------------
// LOGGING HELPERS
// ------------------------------------------------------------

// ------------------------------------------------------------
// SYSTEM METRICS (RAM / VRAM)
// ------------------------------------------------------------
void LogSystemMetrics(const std::string& ctx) {
    LogM("--- BENCHMARK [" + ctx + "] ---");
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
        float ramMB = static_cast<float>(pmc.WorkingSetSize) / 1024.f / 1024.f;
        LogM("RAM: " + std::to_string(ramMB) + " MB");
    }
    IDXGIFactory4* pFactory = nullptr;
    if (SUCCEEDED(CreateDXGIFactory1(__uuidof(IDXGIFactory4), (void**)&pFactory))) {
        IDXGIAdapter* pAdapter = nullptr;
        if (SUCCEEDED(pFactory->EnumAdapters(0, &pAdapter))) {
            IDXGIAdapter3* pAdapter3 = nullptr;
            if (SUCCEEDED(pAdapter->QueryInterface(__uuidof(IDXGIAdapter3), (void**)&pAdapter3))) {
                DXGI_QUERY_VIDEO_MEMORY_INFO memInfo;
                if (SUCCEEDED(pAdapter3->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &memInfo))) {
                    float usedMB = static_cast<float>(memInfo.CurrentUsage) / 1024.f / 1024.f;
                    float budgetMB = static_cast<float>(memInfo.Budget) / 1024.f / 1024.f;
                    LogM("VRAM: " + std::to_string(usedMB) + " / " + std::to_string(budgetMB) + " MB");
                }
                pAdapter3->Release();
            }
            pAdapter->Release();
        }
        pFactory->Release();
    }
    LogM("--- END BENCHMARK ---");
}


std::string GetOrAssignNpcVoiceId(AHandle targetPed) {
    // 1. Persona abrufen (dies holt auch den Cache-Eintrag)
    NpcPersona persona = ConfigReader::GetPersona(targetPed);

    // 2. Prüfen, ob im Cache bereits eine ID zugewiesen wurde
    // ACHTUNG: Da GetPersona eine Kopie zurückgibt, müssen wir direkt in den Cache schauen!
    uint32_t modelHash = GetEntityModel(targetPed);
    if (ConfigReader::g_PersonaCache.count(modelHash)) {
        if (!ConfigReader::g_PersonaCache[modelHash].assignedVoiceId.empty()) {
            return ConfigReader::g_PersonaCache[modelHash].assignedVoiceId;
        }
    }

    // 3. Neue ID finden (Matching)
    std::string targetGender = persona.gender; // "Male" oder "Female" (Achte auf Groß/Kleinschreibung in deiner INI!)
    // Konvertiere ggf. zu "m" oder "f" passend zur INI
    std::string searchGender = (targetGender == "Male") ? "m" : "f";

    std::vector<std::string> candidates;
    for (const auto& pair : ConfigReader::g_VoiceMap) {
        if (pair.second.gender == searchGender) {
            candidates.push_back(pair.first);
        }
    }

    std::string finalId = "1"; // Fallback
    if (!candidates.empty()) {
        int idx = rand() % candidates.size();
        finalId = candidates[idx];
    }

    // 4. Speichern für die Zukunft
    ConfigReader::g_PersonaCache[modelHash].assignedVoiceId = finalId;
    Log("Assigned Voice ID " + finalId + " to NPC " + std::to_string(modelHash));

    return finalId;
}

// ------------------------------------------------------------
// KEY INPUT HELPERS
// ------------------------------------------------------------
static bool g_keyStates[256] = { false };
bool IsKeyJustPressed(int vk) {
    if (vk <= 0 || vk >= 256) return false;
    bool pressed = (GetAsyncKeyState(vk) & 0x8000) != 0;
    bool just = pressed && !g_keyStates[vk];
    g_keyStates[vk] = pressed;
    return just;
}

// ------------------------------------------------------------
// PATH HELPERS
// ------------------------------------------------------------
std::string GetModRootPath() {
    char buf[MAX_PATH];
    GetModuleFileNameA(NULL, buf, MAX_PATH);
    std::string exe = buf;
    return exe.substr(0, exe.find_last_of("\\/")) + "\\";
}

bool DoesFileExist(const std::string& p) {
    std::ifstream f(p.c_str());
    return f.good();
}

// ------------------------------------------------------------
// CONVERSATION CLEAN-UP
// ------------------------------------------------------------

void EndConversation() {
    Log("EndConversation() called.");

    // 1. Hardware & Game Cleanup
    if (g_is_recording) {
        StopAudioRecording();
        LogA("Forced audio stop.");
    }
    if (AbstractGame::IsEntityValid(g_target_ped)) {
        AbstractGame::ClearTasks(g_target_ped);
    }

    // 2. Logic Cleanup & Launch Parallel Summary
    if (g_current_chat_ID != 0) {
        // A. Capture Data Snapshot (Copy history NOW so it's safe)
        ChatID savedID = g_current_chat_ID;
        std::vector<std::string> historySnapshot = ConvoManager::GetChatHistory(savedID);
        std::string savedName = g_current_npc_name;

        // B. Archive the chat immediately
        ConvoManager::CloseConversation(savedID);
        Log("PERSISTENCE: Chat " + std::to_string(savedID) + " closed. Launching background summary.");

        // C. Launch Secretary in Background (Parallel)
        if (ConfigReader::g_Settings.TrySummarizeChat && historySnapshot.size() > 4) {
            g_backgroundTasks.push_back(std::async(std::launch::async, [savedID, historySnapshot, savedName]() {

                // This runs on another thread. It takes 2-5 seconds.
                // It uses the FUNCTION we just defined above.
                std::string summary = PerformChatSummarization(savedName, historySnapshot);

                // When done, send result to Manager to update the archive
                if (!summary.empty() && summary.find("LLM_ERROR") == std::string::npos) {
                    ConvoManager::SetConversationSummary(savedID, summary);
                }
                }));
        }
    }

    // 3. Reset Globals
    g_target_ped = 0;
    g_current_chat_ID = 0;
    g_current_npc_name.clear();
    g_convo_state = ConvoState::IDLE;
    g_input_state = InputState::IDLE;
    g_llm_state = InferenceState::IDLE;
    g_renderText.clear();

    // 4. Reset Futures
    if (g_llm_future.valid()) g_llm_future = std::future<std::string>();
    if (g_stt_future.valid()) g_stt_future = std::future<std::string>();
}


// ------------------------------------------------------------
// SAFETY CHECK (no mission, no combat, etc.)
// ------------------------------------------------------------
bool IsGameInSafeMode() {
    if (!g_isInitialized || !ConfigReader::g_Settings.Enabled) return false;
    AHandle p = GetPlayerHandle();
    if (AbstractGame::IsGamePausedOrLoading()) return false;
    if (IsEntityInCombat(p)) return false;
    if (IsEntitySwimming(p) || IsEntityJumping(p)) return false;
    if (IsEntityInVehicle(p)) {
        AHandle v = GetVehicleOfEntity(p);
        if (IsEntityDriver(v, p)) return false;
    }
    if (g_llm_state != InferenceState::IDLE || g_input_state != InputState::IDLE || g_convo_state != ConvoState::IDLE) return false;
    return true;
}



HWND FindGameWindowHandle() {
    const char* possibleTitles[] = {
        "Grand Theft Auto V",
        "Enhanced Conversations",
        "Rockstar Games"
    };

    for (const char* title : possibleTitles) {
        HWND hwnd = FindWindowA(NULL, title);
        if (hwnd != NULL) {
            Log("Found game window handle with title: " + std::string(title));
            return hwnd;
        }
    }
    Log("FATAL: Could not find game window handle.");
    return NULL;
}

void HandleFullscreenYield() {
    if (g_gameHWND == NULL) return;

    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    RECT windowRect;
    GetWindowRect(g_gameHWND, &windowRect);

    bool isFullscreen = (windowRect.left == 0 &&
        windowRect.top == 0 &&
        windowRect.right == screenWidth &&
        windowRect.bottom == screenHeight);

    if (isFullscreen == g_wasFullscreen) {
        g_wasFullscreen = isFullscreen;
        return;
    }

    ULONGLONG yieldMs = 500;

    if (g_llm_state == InferenceState::RUNNING || g_convo_state == ConvoState::IN_CONVERSATION) {
        LogM("GPU Yield: LLM is BUSY. Blocking thread for " + std::to_string(yieldMs) + "ms for safety.");
        AbstractGame::SystemWait(static_cast<int>(yieldMs));
    }
    else {
        LogM("GPU Yield: IDLE state. Pausing script for " + std::to_string(yieldMs) + "ms.");
        AbstractGame::SystemWait(static_cast<int>(yieldMs));
    }

    g_wasFullscreen = isFullscreen;
}

// ------------------------------------------------------------
// MAIN SCRIPT
// ------------------------------------------------------------
extern "C" __declspec(dllexport) void ScriptMain() {
    try {
        // ----------------------------------------------------
        // ONE-TIME INITIALISATION
        // ----------------------------------------------------
        if (!g_isInitialized) {
            Log("ScriptMain: Initialising…");
            auto t0 = std::chrono::high_resolution_clock::now();
            g_gameHWND = FindGameWindowHandle();
            if (g_gameHWND == NULL) {
                Log("FATAL: Cannot find window handle. Mod may crash on fullscreen change.");
            }
            // 1. CONFIG
            try { ConfigReader::LoadAllConfigs(); Log("Config OK"); }
            catch (const std::exception& e) {
                Log("FATAL CONFIG: " + std::string(e.what()));
                // UI::SET_TEXT... replaced((char*)"STRING");
                // UI::ADD_TEXT... replaced((char*)"CONFIG ERROR – see log");
                AbstractGame::ShowSubtitle("Config Error", 10000);
                TERMINATE(); return;
            }
            LogSystemMetrics("Baseline");

            // **NEU: Bridge initialisieren**
            bridge = new VoiceBridge(true);
            if (bridge && bridge->IsConnected()) {
                Log("Shared Memory Bridge initialized (Host Mode)");
            }
            else {
                Log("ERROR: Failed to init Shared Memory Bridge");
            }

            // 2. LLM (Phi-3)
            std::string root = GetModRootPath();
            std::string modelPath;
            const auto& cust = ConfigReader::g_Settings.MODEL_PATH;
            const auto& alt = ConfigReader::g_Settings.MODEL_ALT_NAME;
            const auto def = "Phi3.gguf";
            if (!cust.empty() && DoesFileExist(cust)) modelPath = cust;
            else if (!alt.empty() && DoesFileExist(root + alt)) modelPath = root + alt;
            else if (DoesFileExist(root + def)) modelPath = root + def;
            if (modelPath.empty()) {
                Log("FATAL: No LLM model found");
                TERMINATE(); return;
            }
            Log("Using LLM: " + modelPath);
            if (!InitializeLLM(modelPath.c_str())) {
                Log("FATAL: InitializeLLM() failed");
                TERMINATE(); return;
            }

            enum ggml_type kv_type = GGML_TYPE_F32;
            llama_context_params ctx_params = llama_context_default_params();
            //ctx_params.n_ctx = static_cast<uint32_t>(ConfigReader::g_Settings.MaxHistoryTokens);
            ctx_params.n_ctx = static_cast<uint32_t>(ConfigReader::g_Settings.Max_Working_Input);
            ctx_params.n_batch = 1024;
            ctx_params.n_ubatch = 256;
            if (!ConfigReader::g_Settings.USE_VRAM_PREFERED) {
                kv_type = GGML_TYPE_F16;
            }
            switch (ConfigReader::g_Settings.KV_Cache_Quantization_Type)
            {
            case 2: // ~2.56 bits-per-weight
                kv_type = GGML_TYPE_Q2_K;
                Log("KV Cache Quantization: Using Q2_K (~2.56 bits)");
                break;

            case 3: // ~3.43 bits-per-weight
                kv_type = GGML_TYPE_Q3_K;
                Log("KV Cache Quantization: Using Q3_K (~3.43 bits)");
                break;

            case 4: // ~4.5 bits-per-weight
                kv_type = GGML_TYPE_Q4_K;
                Log("KV Cache Quantization: Using Q4_K (~4.5 bits)");
                break;

            case 5: // ~5.5 bits-per-weight
                kv_type = GGML_TYPE_Q5_K;
                Log("KV Cache Quantization: Using Q5_K (~5.5 bits)");
                break;

            case 6: // ~6.56 bits-per-weight
                kv_type = GGML_TYPE_Q6_K;
                Log("KV Cache Quantization: Using Q6_K (~6.56 bits)");
                break;

            case 8: // 8.0 bits-per-weight
                kv_type = GGML_TYPE_Q8_0;
                Log("KV Cache Quantization: Using Q8_0 (8 bits)");
                break;

            default:
                // No explicit quantization or unknown value, keeps the default (F32/F16).
                Log("KV Cache Quantization: Using default float type (F32/F16).");
                break;
            }
            ctx_params.type_k = kv_type;
            ctx_params.type_v = kv_type;

            g_ctx = llama_init_from_model(g_model, ctx_params);
            if (g_ctx == nullptr) {
                Log("FATAL: llama_init_from_model failed. Cannot proceed with LLM context.");
                // Handle error (e.g., return false or terminate)
                return;
            }

            // ------------------------------------------------------------
             // LORA ADAPTER LOADING
             // ------------------------------------------------------------
            if (ConfigReader::g_Settings.Lora_Enabled) {
                std::string root_path = GetModRootPath();
                // Ensure FindLoRAFile is defined in LLM_Inference.h, otherwise use manual path:
                // std::string lora_file_path = root_path + ConfigReader::g_Settings.LORA_ALT_NAME;
                std::string lora_file_path = FindLoRAFile(root_path);

                if (!lora_file_path.empty()) {
                    float loraScale = ConfigReader::g_Settings.LORA_SCALE;
                    Log("LoRA: Attempting to load adapter: " + lora_file_path);

                    // 1. Load the adapter file
                    g_lora_adapter = llama_adapter_lora_init(g_model, lora_file_path.c_str());

                    if (g_lora_adapter != nullptr) {
                        // 2. Apply the adapter
                        if (llama_set_adapter_lora(g_ctx, g_lora_adapter, loraScale) == 0) {
                            Log("LoRA: Adapter loaded and applied successfully with scale " + std::to_string(loraScale));
                        }
                        else {
                            Log("LoRA: ERROR: Failed to apply adapter. Reverting.");
                            llama_adapter_lora_free(g_lora_adapter);
                            g_lora_adapter = nullptr;
                        }
                    }
                    else {
                        Log("LoRA: ERROR: Failed to load adapter file.");
                    }
                }
            }

            // ------------------------------------------------------------
            // 3. WHISPER (STT) INITIALIZATION
            // ------------------------------------------------------------
            if (ConfigReader::g_Settings.StT_Enabled) {
                std::string sttPath;
                const auto& custSTT = ConfigReader::g_Settings.STT_MODEL_PATH;
                const auto& altSTT = ConfigReader::g_Settings.STT_MODEL_ALT_NAME;

                // Re-ensure root is valid if needed (it persists from Block 1)
                if (root.empty()) root = GetModRootPath();

                if (!custSTT.empty() && DoesFileExist(custSTT)) sttPath = custSTT;
                else if (!altSTT.empty() && DoesFileExist(root + altSTT)) sttPath = root + altSTT;

                if (!sttPath.empty() && InitializeWhisper(sttPath.c_str()) && InitializeAudioCaptureDevice()) {
                    Log("Whisper + mic ready");
                }
                else {
                    Log("STT disabled – model or mic missing");
                    ConfigReader::g_Settings.StT_Enabled = false;
                }
            }
            else {
                Log("STT disabled in config");
            }

            // ------------------------------------------------------------
            // 4. TTS (TEXT TO SPEECH) CHECK
            // ------------------------------------------------------------
            if (ConfigReader::g_Settings.TtS_Enabled) {
                Log("TTS: Feature enabled in INI. Checking models...");

                // Just path checking here, bridge handles the rest
                if (root.empty()) root = GetModRootPath();

                // Debug log to confirm path detection
                if (!ConfigReader::g_Settings.TTS_MODEL_PATH.empty() && DoesFileExist(ConfigReader::g_Settings.TTS_MODEL_PATH)) {
                    Log("TTS: Custom model found.");
                }
            }
            else {
                Log("TTS: Disabled in INI.");
            }

            // ------------------------------------------------------------
            // FINALIZE INIT
            // ------------------------------------------------------------
            auto t1 = std::chrono::high_resolution_clock::now();
            LogM("INIT TIME: " + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count()) + " ms");
            LogSystemMetrics("Post-LLM");

            g_isInitialized = true;

            // Show "Loaded" message on screen
            AbstractGame::ShowSubtitle("Enhanced Conversations Loaded", 5000);
        }

        // ----------------------------------------------------
        // MAIN GAME LOOP
        // ----------------------------------------------------
        while (true) {


            static uint32_t last_janitor_run = 0;
            if (GetTimeMs() > last_janitor_run + 1000) {
                ConvoManager::RunMaintenance();
                last_janitor_run = GetTimeMs();
            }

            HandleFullscreenYield();
            // ----- 1. SUBTITLE RENDERING -----
            if (!g_renderText.empty() && GetTimeMs() < g_renderEndTime) {
                std::stringstream ss(g_renderText);
                std::string line;
                float y_pos = 0.765f; // Starting Y position for the first line
                float line_height = 0.035f; // Adjust this for line spacing

                while (std::getline(ss, line, '\n')) {
                    UI::SET_TEXT_FONT(0);
                    UI::SET_TEXT_SCALE(0.0f, 0.5f);
                    UI::SET_TEXT_COLOUR(255, 255, 255, 255);
                    UI::SET_TEXT_CENTRE(true);
                    UI::SET_TEXT_DROPSHADOW(0, 0, 0, 0, 255);
                    UI::SET_TEXT_OUTLINE();
                    // UI::SET_TEXT... replaced((char*)"STRING");
                    // UI::ADD_TEXT... replaced((char*)line.c_str());
                    UI::_DRAW_TEXT(0.5f, y_pos);

                    y_pos += line_height; // Move down for the next line
                }
            }

            AHandle playerPed = GetPlayerHandle();
            AVec3 playerPos = GetEntityPosition(playerPed);

            // ----- 2. CONVERSATION GUARD -----
            // --- 2. CONVERSATION GUARD & OPTIMIZER ---
            // ----- 2. CONVERSATION GUARD & OPTIMIZER -----
            if (g_convo_state == ConvoState::IN_CONVERSATION) {

                // ==========================================
                // A. CHAT OPTIMIZER LOGIC (Updated for Manager)
                // ==========================================
                if (g_current_chat_ID != 0) {
                    // 1. Get a copy of the history from the Manager
                    std::vector<std::string> currentHistory = ConvoManager::GetChatHistory(g_current_chat_ID);

                    // 2. Apply any pending optimizations (Async result check)
                    if (ChatOptimizer::ApplyPendingOptimizations(currentHistory)) {
                        // If changed, push the optimized history back to the Manager
                        ConvoManager::ReplaceHistory(g_current_chat_ID, currentHistory);
                    }

                    // 3. Trigger new optimization check (Interval)
                    static uint32_t last_opt_check = 0;
                    if (AbstractGame::GetTimeMs() > last_opt_check + 5000) {
                        last_opt_check = AbstractGame::GetTimeMs();

                        // The Optimizer class now handles VRAM checks internally
                        ChatOptimizer::CheckAndOptimize(
                            g_current_chat_ID,
                            currentHistory,
                            g_current_npc_name,
                            "Player"
                        );
                    }
                }

                // ==========================================
                // B. STATUS CHECKS (Safety)
                // ==========================================
                if (!AbstractGame::IsEntityValid(g_target_ped) || AbstractGame::IsEntityDead(g_target_ped)) {
                    Log("Target dead/invalid -> end");
                    EndConversation();
                }
                else {
                    float dist = AbstractGame::GetDistanceBetweenEntities(playerPed, g_target_ped);
                    if (dist > ConfigReader::g_Settings.MaxConversationRadius * 1.5f) {
                        Log("Too far -> end");
                        EndConversation();
                    }
                }

                // Renew Tasks (Keep NPC looking at you)
                UpdateNpcConversationTasks(g_target_ped, playerPed);

                // ==========================================
                // C. STOP KEYS
                // ==========================================
                if (IsKeyJustPressed(ConfigReader::g_Settings.StopKey_Primary) ||
                    IsKeyJustPressed(ConfigReader::g_Settings.StopKey_Secondary)) {

                    // PTT Exception Check
                    bool isPtt = (ConfigReader::g_Settings.StopKey_Secondary == ConfigReader::g_Settings.StTRB_Activation_Key);

                    if (isPtt && IsKeyPressed(ConfigReader::g_Settings.StTRB_Activation_Key)) {
                        // Do nothing, user is holding PTT
                    }
                    else {
                        Log("StopKey pressed -> end");
                        EndConversation();
                    }
                }
            }

            // ----- 3. LLM TIMEOUT CHECK -----
            if (g_llm_state == InferenceState::RUNNING) {
                auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::high_resolution_clock::now() - g_llm_start_time).count();
                if (elapsed > 30) {
                    Log("LLM Timeout ? discard");
                    if (g_llm_future.valid()) g_llm_future = std::future<std::string>();
                    g_llm_response = "LLM_TIMEOUT";
                    g_llm_state = InferenceState::COMPLETE;
                }
                else if (g_llm_future.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
                    auto elapsed_delay = std::chrono::high_resolution_clock::now() - g_response_start_time;
                    long long ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed_delay).count();
                    if (ms < ConfigReader::g_Settings.MinResponseDelayMs) {
                        AbstractGame::SystemWait(static_cast<uint32_t>(ConfigReader::g_Settings.MinResponseDelayMs - ms));
                    }
                    try { g_llm_response = g_llm_future.get(); }
                    catch (const std::exception& e) {
                        Log("LLM future exception: " + std::string(e.what()));
                        g_llm_response = "LLM_ERROR";
                    }
                    g_llm_future = std::future<std::string>();
                    g_llm_state = InferenceState::COMPLETE;
                }
            }

            // ----- 4. KEYBOARD INPUT -----
            if (g_input_state == InputState::WAITING_FOR_INPUT) {
                int kb = UpdateKeyboardStatus();

                if (kb == 1) { // User pressed ENTER
                    std::string txt = GetKeyboardResult();
                    if (!txt.empty()) {

                        // --- NEW INTEGRATION START ---
                        // 1. Get the Safe ID
                        ChatID activeID = ConvoManager::GetActiveChatID(playerPed);

                        if (activeID != 0) {
                            // 2. Log User Message to System
                            ConvoManager::AddMessageToChat(activeID, "Player", txt);

                            // 3. Update Context (Location, Time, etc.)
                            // We use AbstractCalls to get game state safely
                            std::string zone = GetZoneName(playerPos);
                            // You might need to add GetWeather() to AbstractCalls later
                            ConvoManager::SetChatContext(activeID, zone, "Clear");

                            // 4. Fetch History & Prompt
                            // We pull the CLEAN history from the manager (summaries included)
                            std::vector<std::string> history = ConvoManager::GetChatHistory(activeID);

                            // 5. Run LLM
                            // AssemblePrompt should now accept the vector we just fetched
                            std::string prompt = AssemblePrompt(g_target_ped, playerPed, history);

                            LogSystemMetrics("Pre-Inference (KB)");
                            g_response_start_time = std::chrono::high_resolution_clock::now();
                            g_llm_start_time = std::chrono::high_resolution_clock::now();

                            // Launch Async Generation
                            g_llm_future = std::async(std::launch::async, GenerateLLMResponse, prompt);
                            g_llm_state = InferenceState::RUNNING;
                            g_input_state = InputState::IDLE;
                        }
                        else {
                            Log("ERROR: Input received but no Active Chat ID found!");
                            EndConversation();
                        }
                        // --- NEW INTEGRATION END ---
                    }
                    else {
                        // Re-open keyboard if empty
                        OpenKeyboard("Talk", "", 100);
                    }
                }
                else if (kb == 2 || kb == 3) { // User Cancelled
                    Log("Keyboard cancelled");
                    EndConversation();
                }
            }

            // ----- 5. STT RECORDING -----
            if (g_input_state == InputState::RECORDING) {
                if (ConfigReader::g_Settings.StTRB_Activation_Key != 0 &&
                    GetAsyncKeyState(ConfigReader::g_Settings.StTRB_Activation_Key) == 0) {
                    LogA("PTT released ? stop");
                    StopAudioRecording();
                    g_stt_start_time = GetTimeMs();
                    g_input_state = InputState::TRANSCRIBING;
                    g_stt_future = std::async(std::launch::async, TranscribeAudio, g_audio_buffer);
                    // UI::SET_TEXT... replaced((char*)"STRING");
                    // UI::ADD_TEXT... replaced((char*)"Transcribing…");
                    AbstractGame::ShowSubtitle("", 5000);
                }
            }

            // ----- 6. STT TRANSCRIPTION DONE -----
            if (g_input_state == InputState::TRANSCRIBING) {
                // Check if the thread is finished
                if (g_stt_future.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
                    std::string txt = g_stt_future.get();

                    // Reset future
                    g_stt_future = std::future<std::string>();

                    if (!txt.empty() && txt.length() > 2) {
                        // --- NEW INTEGRATION (The Fix) ---
                        if (g_current_chat_ID != 0) {
                            // 1. Send text to the Manager (instead of g_chat_history)
                            ConvoManager::AddMessageToChat(g_current_chat_ID, "Player", txt);

                            // 2. Get the clean history from the Manager
                            std::vector<std::string> history = ConvoManager::GetChatHistory(g_current_chat_ID);

                            // 3. Build Prompt using the Manager's history
                            std::string prompt = AssemblePrompt(g_target_ped, playerPed, history);

                            // 4. Launch LLM
                            LogSystemMetrics("Pre-Inference (STT)");
                            g_response_start_time = std::chrono::high_resolution_clock::now();
                            g_llm_start_time = std::chrono::high_resolution_clock::now();
                            g_llm_future = std::async(std::launch::async, GenerateLLMResponse, prompt);
                            g_llm_state = InferenceState::RUNNING;
                        }
                    }
                    else {
                        Log("STT empty -> discard");
                    }
                    g_input_state = InputState::IDLE;
                }
                // Timeout check
                else if (AbstractGame::GetTimeMs() > g_stt_start_time + 10000) {
                    Log("STT Timeout -> discard");
                    if (g_stt_future.valid()) g_stt_future = std::future<std::string>();
                    AbstractGame::ShowSubtitle("Transcription Timeout", 3000);
                    g_input_state = InputState::IDLE;
                }
            }

            // ----- 7. START CONVERSATION TRIGGER -----
            if (g_convo_state == ConvoState::IDLE && g_input_state == InputState::IDLE && g_llm_state == InferenceState::IDLE) {
                if (IsGameInSafeMode()) {
                    if (IsKeyJustPressed(ConfigReader::g_Settings.ActivationKey)) {

                        // 1. Find Target
                        AHandle tempTarget = 0;
                        AVec3 centre = AbstractGame::GetEntityPosition(playerPed);
                        tempTarget = AbstractGame::GetClosestPed(centre, ConfigReader::g_Settings.MaxConversationRadius, playerPed);

                        bool found = AbstractGame::IsEntityValid(tempTarget);

                        // 2. Validate Target
                        if (found && AbstractGame::IsEntityLivingEntity(tempTarget) && tempTarget != playerPed) {

                            // Check Busy States
                            if (AbstractGame::IsEntityInVehicle(tempTarget) ||
                                AbstractGame::IsEntityInCombat(tempTarget) ||
                                AbstractGame::IsEntityDead(tempTarget) ||
                                AbstractGame::IsEntityFleeing(tempTarget)) {

                                // UI::SET_TEXT... replaced((char*)"STRING");
                                // UI::ADD_TEXT... replaced((char*)"NPC busy");
                                AbstractGame::ShowSubtitle("NPC is busy.", 2000);
                            }
                            else {
                                Log("Conversation START -> AHandle " + std::to_string(tempTarget));
                                g_target_ped = tempTarget;

                                // --- NEW SYSTEM START ---
                                // A. Create Session via Manager (Returns unique ID)
                                g_current_chat_ID = ConvoManager::InitiateConversation(playerPed, g_target_ped);

                                // B. Cache Name for UI (Manager handles the real memory)
                                NpcPersona p = ConfigReader::GetPersona(g_target_ped);
                                g_current_npc_name = !p.inGameName.empty() ? p.inGameName : GenerateNpcName(p);

                                Log("Started Chat ID: " + std::to_string(g_current_chat_ID));

                                // C. Set State
                                g_convo_state = ConvoState::IN_CONVERSATION;

                                // D. Init Tasks (Call your Modular Helper function)
                                StartNpcConversationTasks(g_target_ped, playerPed);

                                // E. Prompt User (Input Mode)
                                if (ConfigReader::g_Settings.StT_Enabled) {
                                    // UI::SET_TEXT... replaced((char*)"STRING");
                                    // UI::ADD_TEXT... replaced((char*)"Hold [PTT] to speak…");
                                    AbstractGame::ShowSubtitle("Hold PTT to speak...", 3000);
                                    g_input_state = InputState::IDLE;
                                }
                                else {
                                    AbstractGame::OpenKeyboard("FMMC_KEY_TIP", "", ConfigReader::g_Settings.MaxInputChars);
                                    g_input_state = InputState::WAITING_FOR_INPUT;
                                    Log("Keyboard opened");
                                }
                            }
                        }
                    }
                }
            }

            if (g_convo_state == ConvoState::IN_CONVERSATION &&
                g_input_state == InputState::IDLE &&
                g_llm_state == InferenceState::IDLE &&
                ConfigReader::g_Settings.StT_Enabled &&
                ConfigReader::g_Settings.StTRB_Activation_Key != 0 &&
                IsKeyJustPressed(ConfigReader::g_Settings.StTRB_Activation_Key))
            {
                LogA("PTT pressed ? start recording");
                StartAudioRecording();
                g_input_state = InputState::RECORDING;
                // UI::SET_TEXT... replaced((char*)"STRING");
                // UI::ADD_TEXT... replaced((char*)"RECORDING… (release to stop)");
                AbstractGame::ShowSubtitle("", 10000);
            }


            // ----- 8. LLM RESPONSE READY (UPDATED) -----
            if (g_llm_state == InferenceState::COMPLETE) {
                // Safety check: Are we still talking?
                if (g_convo_state != ConvoState::IN_CONVERSATION) {
                    g_llm_state = InferenceState::IDLE;
                    continue;
                }

                // 1. Clean the text
                std::string clean = CleanupResponse(g_llm_response);

                // 2. Check for "Goodbye" triggers
                bool endConvo = false;
                if (clean.find("<END_CONVERSATION>") != std::string::npos ||
                    clean.find("[END_CONVERSATION]") != std::string::npos ||
                    clean.find("<|endofchat|>") != std::string::npos ||
                    clean.find("Bye") != std::string::npos ||
                    clean.find("Good Bye") != std::string::npos) {
                    endConvo = true;
                }

                // Handle Errors
                if (g_llm_response == "LLM_TIMEOUT" || g_llm_response == "LLM_ERROR") {
                    clean = "Response error.";
                    endConvo = true;
                }

                // --- 3. SAVE TO MEMORY (The New Way) ---
                ChatID activeID = ConvoManager::GetActiveChatID(playerPed);
                if (activeID != 0) {
                    // This single line replaces the old push_back and while-loop resize logic
                    ConvoManager::AddMessageToChat(activeID, g_current_npc_name, clean);
                }

                // --- 4. RENDER (Optimized) ---
                // We use the helper to split the string once, saving CPU cycles
                std::string wrapped = WordWrap(clean, 50);
                g_renderText = wrapped;
                g_renderEndTime = GetTimeMs() + 10000;
                Log("RENDER: " + wrapped);

                // --- 5. TTS (Audio) ---
                if (ConfigReader::g_Settings.TtS_Enabled) {
                    std::string voiceId = GetOrAssignNpcVoiceId(g_target_ped);
                    if (bridge && bridge->IsConnected()) {
                        bridge->Send(clean, voiceId);
                    }
                }

                // --- 6. DECIDE NEXT STEP ---
                if (endConvo) {
                    Log("LLM requested end. Closing session.");
                    EndConversation(); // Calls ConvoManager::CloseConversation internally now
                }
                else {
                    // Reset input state for the next turn
                    if (ConfigReader::g_Settings.StT_Enabled) {
                        g_input_state = InputState::IDLE;
                        Log("LLM done -> STT idle");
                    }
                    else {
                        // Re-open keyboard
                        AbstractGame::OpenKeyboard("FMMC_KEY_TIP", "", ConfigReader::g_Settings.MaxInputChars);                        g_input_state = InputState::WAITING_FOR_INPUT;
                    }
                }

                // Done
                g_llm_state = InferenceState::IDLE;
            }

            AbstractGame::SystemWait(0);
        }
    }
    catch (const std::exception& e) {
        Log("SCRIPT EXCEPTION: " + std::string(e.what()));
        TERMINATE();
    }
    catch (...) {
        Log("UNKNOWN EXCEPTION");
        TERMINATE();
    }
}

// ------------------------------------------------------------
// DLL ENTRY
// ------------------------------------------------------------
bool APIENTRY DllMain(HMODULE hMod, uint32_t reason, LPVOID) {
    switch (reason) {
    case DLL_PROCESS_ATTACH:
    { std::ofstream f(LOG_FILE_NAME, std::ios::trunc); }
    Log("DLL attached");
    scriptRegister(hMod, ScriptMain);
    break;
    case DLL_PROCESS_DETACH:
        Log("DLL detach – shutdown");
        if (g_isInitialized) ShutdownLLM();

        // Clean up bridge
        if (bridge) {
            delete bridge;
            bridge = nullptr;
        }

        scriptUnregister(hMod);
        break;
    }
    return true;
}
// ------------------------------------------------------------
// WORD WRAP UTILITY
// ------------------------------------------------------------

// -------------------------------------------------------------------------
// NEW: Summarize History (The "Secretary" Function)
// -------------------------------------------------------------------------
// -------------------------------------------------------------------------
// THE SECRETARY: Summarizes the full conversation logic
// -------------------------------------------------------------------------
std::string PerformChatSummarization(const std::string& npcName, const std::vector<std::string>& history) {
    if (!g_model || !g_ctx) return "";

    std::string playerName = "Player";

    // 1. Build the specific prompt (Your original logic)
    std::stringstream prompt;
    prompt << "<|system|>\n";
    prompt << "You are a secretary writing a memo. Summarize the key facts from the following conversation between '" << npcName << "' and '" << playerName << "'.\n";
    prompt << "RULES:\n";
    prompt << "- Output only the memo. Do not be conversational.\n";
    prompt << "- Focus on agreements, questions, important names, locations, or numbers mentioned.\n";
    prompt << "- Keep the summary between " << ConfigReader::g_Settings.MIN_PCSREMEMBER_SIZE << " and " << ConfigReader::g_Settings.MAX_PCSREMEMBER_SIZE << " characters.\n";
    prompt << "- Example format: 'Discussed weather (hot). " << npcName << " dislikes heat. Player mentioned liking ice cream.'\n";
    prompt << "<|end|>\n";

    // 2. Add the full history
    prompt << "<|user|>\nCONVERSATION LOG:\n";
    for (const auto& line : history) {
        prompt << line << "\n";
    }
    prompt << "<|end|>\n<|assistant|>\n";

    // 3. Run inference (Returns the string)
    return GenerateLLMResponse(prompt.str());
}//EOF


/**
void UpdateNpcMemoryJanitor(bool force_clear_all = false) {

    // Lock the mutex because this function modifies the session map,
    // and the background summarizer might be trying to access it too.
    std::lock_guard<std::mutex> lock(g_session_mutex);

    if (g_ActiveSessions.empty()) return;

    if (force_clear_all) {
        g_ActiveSessions.clear();
        Log("PERSISTENCE: All sessions cleared by force.");
        return;
    }

    // --- READING VALUES FROM YOUR .INI SETTINGS ---
    auto now = std::chrono::steady_clock::now();
    int timeoutSec = ConfigReader::g_Settings.DeletionTimer;           // USES DeletionTimer
    int absTimeoutSec = ConfigReader::g_Settings.DeletionTimerClearFull; // USES DeletionTimerClearFull
    int maxHistory = ConfigReader::g_Settings.MaxAllowedChatHistory;   // USES MaxAllowedChatHistory
    // -----------------------------------------------

    // 1. Check for Timeouts
    if (timeoutSec >= 0 || absTimeoutSec >= 0) {
        for (auto it = g_ActiveSessions.begin(); it != g_ActiveSessions.end(); ) {
            bool isProtected = it->second.isUniqueCharacter;
            long long elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - it->second.lastInteractionTime).count();
            bool shouldDelete = false;

            if (timeoutSec >= 0 && !isProtected && elapsed > timeoutSec) {
                shouldDelete = true;
            }
            if (absTimeoutSec >= 0 && elapsed > absTimeoutSec) {
                shouldDelete = true;
            }

            if (shouldDelete) {
                Log("PERSISTENCE: Janitor forgetting " + it->second.assignedName + " (ID: " + std::to_string(it->first) + ") - Timeout.");
                it = g_ActiveSessions.erase(it);
            }
            else {
                ++it;
            }
        }
    }

    // 2. Check for Capacity (if you have more memories than allowed)
    if (maxHistory > 0 && g_ActiveSessions.size() > (size_t)maxHistory) {
        int oldestID = -1;
        auto oldestTime = (std::chrono::steady_clock::time_point::max)();

        for (const auto& pair : g_ActiveSessions) {
            if (pair.second.isUniqueCharacter) continue; // Don't delete unique characters for capacity reasons

            if (pair.second.lastInteractionTime < oldestTime) {
                oldestTime = pair.second.lastInteractionTime;
                oldestID = pair.first;
            }
        }

        if (oldestID != -1) {
            Log("PERSISTENCE: Janitor forgetting ID " + std::to_string(oldestID) + " due to MaxAllowedChatHistory limit.");
            g_ActiveSessions.erase(oldestID);
        }
    }
}

void CleanupSessions(bool forceClearAll) {
    if (g_ActiveSessions.empty()) return;

    if (forceClearAll) {
        g_ActiveSessions.clear();
        Log("PERSISTENCE: All sessions cleared forced.");
        return;
    }

    auto now = std::chrono::steady_clock::now();
    int timeoutSec = ConfigReader::g_Settings.DeletionTimer;
    int absTimeoutSec = ConfigReader::g_Settings.DeletionTimerClearFull;
    int maxHistory = ConfigReader::g_Settings.MaxAllowedChatHistory;

    // 1. Check for Timeouts (DELETION_TIMER)
    if (timeoutSec >= 0 || absTimeoutSec >= 0) {
        for (auto it = g_ActiveSessions.begin(); it != g_ActiveSessions.end(); ) {

            // Don't auto-delete Unique Characters (e.g. Amanda) unless AbsTimeout hits
            bool isProtected = it->second.isUniqueCharacter;

            long long elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - it->second.lastInteractionTime).count();

            bool shouldDelete = false;

            // Rule 1: Normal Deletion Timer (only for non-unique)
            if (timeoutSec >= 0 && !isProtected && elapsed > timeoutSec) {
                shouldDelete = true;
            }

            // Rule 2: Absolute Clear Timer (kills everything, even Amanda, if you leave PC for 3 hours)
            if (absTimeoutSec >= 0 && elapsed > absTimeoutSec) {
                shouldDelete = true;
            }

            if (shouldDelete) {
                Log("PERSISTENCE: Forgetting " + it->second.assignedName + " (ID: " + std::to_string(it->first) + ") - Timeout.");
                it = g_ActiveSessions.erase(it);
            }
            else {
                ++it;
            }
        }
    }

    // 2. Check for Capacity (MAX_ALLOWED_CHAT_HISTORY)
    // Only runs if we have too many saved conversations.
    if (maxHistory >= 0 && g_ActiveSessions.size() > (size_t)maxHistory) {
        // Find the oldest interaction to delete
        int oldestID = -1;
        auto oldestTime = (std::chrono::steady_clock::time_point::max)();

        for (const auto& pair : g_ActiveSessions) {
            // Optional: You might want to protect "Unique" chars from capacity deletion too
            // if (pair.second.isUniqueCharacter) continue; 

            if (pair.second.lastInteractionTime < oldestTime) {
                oldestTime = pair.second.lastInteractionTime;
                oldestID = pair.first;
            }
        }

        if (oldestID != -1) {
            Log("PERSISTENCE: Forgetting ID " + std::to_string(oldestID) + " due to MaxHistory Limit.");
            g_ActiveSessions.erase(oldestID);
        }
    }
}

NpcSession* GetOrCreateSession(AHandle AHandle) {
    int id = static_cast<int>(AHandle); // Use AHandle Handle as ID

    // 1. Check if session exists
    if (g_ActiveSessions.count(id)) {
        g_ActiveSessions[id].lastInteractionTime = std::chrono::steady_clock::now();
        return &g_ActiveSessions[id];
    }

    // 2. Create New Session
    NpcSession newSession;
    newSession.pedHandle = id;
    newSession.lastInteractionTime = std::chrono::steady_clock::now();
    newSession.chatHistory.clear();

    // 3. Name Logic (The important part!)
    NpcPersona persona = ConfigReader::GetPersona(AHandle);

    if (!persona.inGameName.empty()) {
        // predefined name (Amanda, Trevor)
        newSession.assignedName = persona.inGameName;
        newSession.isUniqueCharacter = true;
    }
    else {
        // Generate random name (Officer Kaley)
        newSession.assignedName = GenerateNpcName(persona);
        newSession.isUniqueCharacter = false;
    }

    // Store it
    g_ActiveSessions[id] = newSession;
    Log("PERSISTENCE: Created new session for " + newSession.assignedName + " (ID: " + std::to_string(id) + ")");

    // Trigger cleanup (in case we exceeded max capacity)
    CleanupSessions(false);

    return &g_ActiveSessions[id];
}



void AsyncSummarizeAndSaveTask(int pedHandle, std::string npcName, std::vector<std::string> historyCopy) {
    Log("BACKGROUND_TASK: Starting summary for " + npcName + " (ID: " + std::to_string(pedHandle) + ")");

    // This can take 1-5 seconds, but the game is NOT frozen.
    std::string summary = PerformChatSummarization(npcName, historyCopy);

    // After the work is done, LOCK the session map to prevent crashes.
    std::lock_guard<std::mutex> lock(g_session_mutex);

    if (!g_isInitialized) {
        Log("BACKGROUND_TASK: Mod was shut down during summarization. Discarding result.");
        return; // Exit if the mod was unloaded
    }

    if (g_ActiveSessions.count(pedHandle)) {
        if (!summary.empty() && summary.length() > ConfigReader::g_Settings.MIN_PCSREMEMBER_SIZE) {
            // Success! Replace long history with short summary.
            g_ActiveSessions[pedHandle].chatHistory.clear();
            g_ActiveSessions[pedHandle].chatHistory.push_back("<|system|>\nSummary of last conversation:\n" + summary + "\n<|end|>\n");
            Log("BACKGROUND_TASK: Summary saved successfully for " + npcName);
        }
        else {
            // Fallback: save the raw (trimmed) history if summary fails.
            Log("BACKGROUND_TASK: Summary failed. Saving trimmed raw history instead.");
            g_ActiveSessions[pedHandle].chatHistory = historyCopy;
        }
        g_ActiveSessions[pedHandle].lastInteractionTime = std::chrono::steady_clock::now();
    }
} **/

