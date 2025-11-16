// ModMain.cpp – v1.0.21 (Central Header Fix + Modular NPC Task Fix)
// ------------------------------------------------------------
// 1. PREPROCESSOR & LIBRARIES
// ------------------------------------------------------------
#define _CRT_SECURE_NO_WARNINGS
#pragma comment(lib, "ScriptHookV.lib")
#pragma comment(lib, "dxgi.lib")

// 2. INCLUDES
// ------------------------------------------------------------
#include "main.h"                  // ONLY INCLUDE main.h
#define MINIAUDIO_IMPLEMENTATION   // Miniaudio implementation (must be in one .cpp)
#include "miniaudio.h"

// ------------------------------------------------------------
// GLOBAL STATE DEFINITIONS (from main.h extern declarations)
// ------------------------------------------------------------
static bool g_isInitialized = false;

// Rendering
static std::string g_renderText;
static DWORD g_renderEndTime = 0;
int checker_loaded = 1;

// Conversation / Input / LLM states
ConvoState g_convo_state = ConvoState::IDLE;
InputState g_input_state = InputState::IDLE;
//InferenceState g_llm_state = InferenceState::IDLE;


// Target NPC
Ped g_target_ped = 0;

// Chat history & logging
std::vector<std::string> g_chat_history;
std::string LOG_FILE_NAME = "kkamel.log";
std::string LOG_FILE_NAME_METRICS = "kkamel_metrics.log";
std::string LOG_FILE_NAME_AUDIO = "kkamel_audio.log";

// Persistent NPC names (by model hash)
static std::map<Hash, std::string> g_persistent_npc_names;
std::string g_current_npc_name;

// Timing for timeouts
static ULONGLONG g_stt_start_time = 0;
static std::chrono::high_resolution_clock::time_point g_llm_start_time;

// ------------------------------------------------------------
// LOGGING HELPERS
// ------------------------------------------------------------
void Log(const std::string& msg) {
    std::ofstream f(LOG_FILE_NAME, std::ios::app);
    if (f) {
        auto t = std::time(nullptr);
        auto tm = *std::localtime(&t);
        f << "[" << std::put_time(&tm, "%Y-%m-%d %H:%M:%S") << "] " << msg << "\n";
    }
}

static void LogM(const std::string& msg) {
    std::ofstream f(LOG_FILE_NAME_METRICS, std::ios::app);
    if (f) {
        auto t = std::time(nullptr);
        auto tm = *std::localtime(&t);
        f << "[" << std::put_time(&tm, "%Y-%m-%d %H:%M:%S") << "] " << msg << "\n";
    }
}

static void LogA(const std::string& msg) {
    std::ofstream f(LOG_FILE_NAME_AUDIO, std::ios::app);
    if (f) {
        auto t = std::time(nullptr);
        auto tm = *std::localtime(&t);
        f << "[" << std::put_time(&tm, "%Y-%m-%d %H:%M:%S") << "] " << msg << "\n";
    }
}

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
    Log("EndConversation()");
    if (g_is_recording) { StopAudioRecording(); LogA("forced stop"); }
    if (ENTITY::DOES_ENTITY_EXIST(g_target_ped))
        AI::CLEAR_PED_TASKS(g_target_ped);
    g_target_ped = 0;
    g_convo_state = ConvoState::IDLE;
    g_input_state = InputState::IDLE;
    g_llm_state = InferenceState::IDLE;
    g_chat_history.clear();
    g_current_npc_name.clear();
    if (g_ctx) {
        llama_memory_t mem = llama_get_memory(g_ctx);
        if (mem) llama_memory_clear(mem, true);
    }
    g_renderText.clear();
    GAMEPLAY::UPDATE_ONSCREEN_KEYBOARD(); // close any lingering keyboard
    // Invalidate running futures to prevent lingering threads
    if (g_llm_state == InferenceState::RUNNING && g_llm_future.valid()) {
        g_llm_future = std::future<std::string>();
        g_llm_state = InferenceState::IDLE;
    }
    if (g_input_state == InputState::TRANSCRIBING && g_stt_future.valid()) {
        g_stt_future = std::future<std::string>();
        g_input_state = InputState::IDLE;
    }
}

// ------------------------------------------------------------
// SAFETY CHECK (no mission, no combat, etc.)
// ------------------------------------------------------------
bool IsGameInSafeMode() {
    if (!g_isInitialized || !ConfigReader::g_Settings.Enabled) return false;
    Ped p = PLAYER::PLAYER_PED_ID();
    if (GAMEPLAY::GET_MISSION_FLAG()) return false;
    if (PED::IS_PED_IN_COMBAT(p, PLAYER::PLAYER_ID())) return false;
    if (PED::IS_PED_SWIMMING(p) || PED::IS_PED_JUMPING(p)) return false;
    if (PED::IS_PED_IN_ANY_VEHICLE(p, FALSE)) {
        Vehicle v = PED::GET_VEHICLE_PED_IS_IN(p, FALSE);
        if (VEHICLE::GET_PED_IN_VEHICLE_SEAT(v, -1) == p) return false;
    }
    if (g_llm_state != InferenceState::IDLE || g_input_state != InputState::IDLE || g_convo_state != ConvoState::IDLE) return false;
    return true;
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

            // 1. CONFIG
            try { ConfigReader::LoadAllConfigs(); Log("Config OK"); }
            catch (const std::exception& e) {
                Log("FATAL CONFIG: " + std::string(e.what()));
                UI::_SET_TEXT_ENTRY((char*)"STRING");
                UI::_ADD_TEXT_COMPONENT_STRING((char*)"CONFIG ERROR – see log");
                UI::_DRAW_SUBTITLE_TIMED(10000, true);
                TERMINATE(); return;
            }
            LogSystemMetrics("Baseline");

            // 2. LLM (Phi-3) – robust fallback chain
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
            llama_context_params ctx_params = llama_context_default_params();
            ctx_params.n_ctx = static_cast<uint32_t>(ConfigReader::g_Settings.MaxHistoryTokens);
            ctx_params.n_batch = 1024;
            ctx_params.n_ubatch = 256;
            if (ConfigReader::g_Settings.USE_VRAM_PREFERED) {
                ctx_params.type_k = GGML_TYPE_F16;
                ctx_params.type_v = GGML_TYPE_F16;
            }
            g_ctx = llama_init_from_model(g_model, ctx_params);
            if (!g_ctx) { Log("FATAL: llama_init_from_model"); TERMINATE(); return; }

            // 3. OPTIONAL WHISPER (STT)
            if (ConfigReader::g_Settings.StT_Enabled) {
                std::string sttPath;
                const auto& custSTT = ConfigReader::g_Settings.STT_MODEL_PATH;
                const auto& altSTT = ConfigReader::g_Settings.STT_MODEL_ALT_NAME;
                if (!custSTT.empty() && DoesFileExist(custSTT)) sttPath = custSTT;
                else if (!altSTT.empty() && DoesFileExist(root + altSTT)) sttPath = root + altSTT;
                if (!sttPath.empty() && InitializeWhisper(sttPath.c_str()) && InitializeAudioCaptureDevice())
                    Log("Whisper + mic ready");
                else {
                    Log("STT disabled – model or mic missing");
                    ConfigReader::g_Settings.StT_Enabled = false;
                }
            }
            else Log("STT disabled in config");

            auto t1 = std::chrono::high_resolution_clock::now();
            LogM("INIT TIME: " + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count()) + " ms");
            LogSystemMetrics("Post-LLM");
            g_isInitialized = true;
            UI::_SET_TEXT_ENTRY((char*)"STRING");
            UI::_ADD_TEXT_COMPONENT_STRING((char*)"GTA_EC Loaded");
            UI::_DRAW_SUBTITLE_TIMED(5000, true);
        }

        // ----------------------------------------------------
        // MAIN GAME LOOP
        // ----------------------------------------------------
        while (true) {
            // ----- 1. SUBTITLE RENDERING -----
            if (!g_renderText.empty() && GetTickCount64() < g_renderEndTime) {
                UI::SET_TEXT_FONT(0);
                UI::SET_TEXT_SCALE(0.0f, 0.5f);
                UI::SET_TEXT_COLOUR(255, 255, 255, 255);
                UI::SET_TEXT_CENTRE(TRUE);
                UI::SET_TEXT_DROPSHADOW(0, 0, 0, 0, 255);
                UI::SET_TEXT_OUTLINE();
                UI::_SET_TEXT_ENTRY((char*)"STRING");
                UI::_ADD_TEXT_COMPONENT_STRING((char*)g_renderText.c_str());
                UI::_DRAW_TEXT(0.5f, 0.85f);
            }

            Ped playerPed = PLAYER::PLAYER_PED_ID();
            Vector3 playerPos = ENTITY::GET_ENTITY_COORDS(playerPed, true);

            // ----- 2. CONVERSATION GUARD (distance / death / ESC) - MODIFIZIERT -----
            if (g_convo_state == ConvoState::IN_CONVERSATION) {
                if (!ENTITY::DOES_ENTITY_EXIST(g_target_ped) ||
                    PED::IS_PED_DEAD_OR_DYING(g_target_ped, true)) {
                    Log("Target dead/invalid → end");
                    EndConversation();
                }
                else {
                    float dist = GAMEPLAY::GET_DISTANCE_BETWEEN_COORDS(
                        playerPos.x, playerPos.y, playerPos.z,
                        ENTITY::GET_ENTITY_COORDS(g_target_ped, true).x,
                        ENTITY::GET_ENTITY_COORDS(g_target_ped, true).y,
                        ENTITY::GET_ENTITY_COORDS(g_target_ped, true).z, TRUE);
                    if (dist > ConfigReader::g_Settings.MaxConversationRadius * 1.5f) {
                        Log("Too far → end");
                        EndConversation();
                    }
                }
                // *** HIER IST DER MODULARE FIX ***
                // Rufe die "leichte" Update-Funktion aus ModHelpers in jedem Frame auf
                if (g_convo_state == ConvoState::IN_CONVERSATION) {
                    UpdateNpcConversationTasks(g_target_ped, playerPed);
                }
                // ESC / U key (ABBRUCH IMMER MÖGLICH, wenn in Konversation)
                if (IsKeyJustPressed(ConfigReader::g_Settings.StopKey_Primary) ||
                    IsKeyJustPressed(ConfigReader::g_Settings.StopKey_Secondary)) {
                    // Optionale PTT-Konfliktvermeidung: Ignoriere Secondary, wenn es PTT ist und gerade gedrückt gehalten wird
                    if (ConfigReader::g_Settings.StopKey_Secondary == ConfigReader::g_Settings.StTRB_Activation_Key &&
                        GetAsyncKeyState(ConfigReader::g_Settings.StTRB_Activation_Key) & 0x8000) {
                        // Ignoriere, da PTT aktiv (nur für Secondary)
                    }
                    else {
                        Log("StopKey pressed → end");
                        EndConversation();
                    }
                }
            }

            // ----- 3. LLM INFERENCE COMPLETE (ERWEITERT) -----
            if (g_llm_state == InferenceState::RUNNING) {
                auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::high_resolution_clock::now() - g_llm_start_time).count();
                if (elapsed > 30) { // 30-Sekunden-Timeout
                    Log("LLM Timeout → discard");
                    if (g_llm_future.valid()) g_llm_future = std::future<std::string>();
                    g_llm_response = "LLM_TIMEOUT";
                    g_llm_state = InferenceState::COMPLETE; // Erzwinge Übergang zu Abschnitt 8
                }
                else if (g_llm_future.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
                    // enforce minimum delay
                    auto elapsed_delay = std::chrono::high_resolution_clock::now() - g_response_start_time;
                    long long ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed_delay).count();
                    if (ms < ConfigReader::g_Settings.MinResponseDelayMs) {
                        WAIT(static_cast<DWORD>(ConfigReader::g_Settings.MinResponseDelayMs - ms));
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
                int kb = GAMEPLAY::UPDATE_ONSCREEN_KEYBOARD();
                if (kb == 1) { // submitted
                    std::string txt = GAMEPLAY::GET_ONSCREEN_KEYBOARD_RESULT();
                    if (!txt.empty()) {
                        std::string name = GenerateNpcName(ConfigReader::GetPersona(playerPed));
                        g_chat_history.push_back(name + ": " + txt);
                        while (g_chat_history.size() > ConfigReader::g_Settings.MaxChatHistoryLines)
                            g_chat_history.erase(g_chat_history.begin());
                        if (g_ctx) { llama_memory_t m = llama_get_memory(g_ctx); if (m) llama_memory_clear(m, true); }
                        std::string prompt = AssemblePrompt(g_target_ped, playerPed, g_chat_history);
                        LogSystemMetrics("Pre-Inference (KB)");
                        g_response_start_time = std::chrono::high_resolution_clock::now();
                        g_llm_start_time = std::chrono::high_resolution_clock::now(); // Added for LLM timeout
                        g_llm_future = std::async(std::launch::async, GenerateLLMResponse, prompt);
                        g_llm_state = InferenceState::RUNNING;
                        g_input_state = InputState::IDLE;
                    }
                    else { // empty → reopen
                        GAMEPLAY::DISPLAY_ONSCREEN_KEYBOARD(1, (char*)"FMMC_KEY_TIP", (char*)"", (char*)"", (char*)"", (char*)"", (char*)"",
                            ConfigReader::g_Settings.MaxInputChars);
                    }
                }
                else if (kb == 2 || kb == 3) { // cancelled
                    Log("Keyboard cancelled");
                    EndConversation();
                }
            }

            // ----- 5. STT RECORDING (PTT press) -----
            if (g_input_state == InputState::RECORDING) {
                if (ConfigReader::g_Settings.StTRB_Activation_Key != 0 &&
                    GetAsyncKeyState(ConfigReader::g_Settings.StTRB_Activation_Key) == 0) {
                    LogA("PTT released → stop");
                    StopAudioRecording();
                    g_stt_start_time = GetTickCount64(); // Added for STT timeout
                    g_input_state = InputState::TRANSCRIBING;
                    g_stt_future = std::async(std::launch::async, TranscribeAudio, g_audio_buffer);
                    UI::_SET_TEXT_ENTRY((char*)"STRING");
                    UI::_ADD_TEXT_COMPONENT_STRING((char*)"Transcribing…");
                    UI::_DRAW_SUBTITLE_TIMED(5000, true);
                }
            }

            // ----- 6. STT TRANSCRIPTION DONE (MODIFIZIERT) -----
            if (g_input_state == InputState::TRANSCRIBING) {
                if (g_stt_future.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
                    std::string txt;
                    try {
                        txt = g_stt_future.get();
                    }
                    catch (const std::exception& e) {
                        LogA("STT future exception: " + std::string(e.what()));
                        txt = "";
                    }
                    g_stt_future = std::future<std::string>();
                    if (!txt.empty() && txt.length() > 2) {
                        std::string name = GenerateNpcName(ConfigReader::GetPersona(playerPed));
                        g_chat_history.push_back(name + ": " + txt);
                        while (g_chat_history.size() > ConfigReader::g_Settings.MaxChatHistoryLines)
                            g_chat_history.erase(g_chat_history.begin());
                        if (g_ctx) { llama_memory_t m = llama_get_memory(g_ctx); if (m) llama_memory_clear(m, true); }
                        std::string prompt = AssemblePrompt(g_target_ped, playerPed, g_chat_history);
                        LogSystemMetrics("Pre-Inference (STT)");
                        g_response_start_time = std::chrono::high_resolution_clock::now();
                        g_llm_start_time = std::chrono::high_resolution_clock::now(); // Added for LLM timeout
                        g_llm_future = std::async(std::launch::async, GenerateLLMResponse, prompt);
                        g_llm_state = InferenceState::RUNNING;
                    }
                    else {
                        LogA("STT empty → discard");
                    }
                    g_input_state = InputState::IDLE;
                }
                else if (GetTickCount64() > g_stt_start_time + 10000) { // 10-Sekunden-Timeout
                    LogA("STT Timeout → discard");
                    if (g_stt_future.valid()) {
                        g_stt_future = std::future<std::string>(); // Invalidieren
                    }
                    UI::_SET_TEXT_ENTRY((char*)"STRING");
                    UI::_ADD_TEXT_COMPONENT_STRING((char*)"Transcription timeout or error.");
                    UI::_DRAW_SUBTITLE_TIMED(3000, true);
                    g_input_state = InputState::IDLE;
                }
            }

            // ----- 7. START CONVERSATION (T-Key) -----
            if (g_convo_state == ConvoState::IDLE && g_input_state == InputState::IDLE && g_llm_state == InferenceState::IDLE) {
                if (IsGameInSafeMode()) {
                    // ---- 7a. T-Key → find closest ped ----
                    if (IsKeyJustPressed(ConfigReader::g_Settings.ActivationKey)) {
                        Ped tempTarget = 0;
                        Vector3 centre = ENTITY::GET_ENTITY_COORDS(playerPed, TRUE);
                        BOOL found = PED::GET_CLOSEST_PED(
                            centre.x, centre.y, centre.z,
                            ConfigReader::g_Settings.MaxConversationRadius,
                            TRUE, FALSE, &tempTarget, TRUE, TRUE, 0);
                        if (found && ENTITY::IS_ENTITY_A_PED(tempTarget) && tempTarget != playerPed) {
                            if (PED::IS_PED_IN_ANY_VEHICLE(tempTarget, FALSE) ||
                                PED::IS_PED_IN_COMBAT(tempTarget, playerPed) ||
                                PED::IS_PED_DEAD_OR_DYING(tempTarget, TRUE) ||
                                PED::IS_PED_FLEEING(tempTarget)) {
                                UI::_SET_TEXT_ENTRY((char*)"STRING");
                                UI::_ADD_TEXT_COMPONENT_STRING((char*)"NPC busy");
                                UI::_DRAW_SUBTITLE_TIMED(2000, true);
                            }
                            else {
                                // ---- conversation begins ----
                                Log("Conversation START → ped " + std::to_string(tempTarget));
                                g_target_ped = tempTarget;
                                // name caching
                                Hash model = ENTITY::GET_ENTITY_MODEL(g_target_ped);
                                if (g_persistent_npc_names.count(model))
                                    g_current_npc_name = g_persistent_npc_names[model];
                                else {
                                    NpcPersona p = ConfigReader::GetPersona(g_target_ped);
                                    g_current_npc_name = GenerateNpcName(p);
                                    g_persistent_npc_names[model] = g_current_npc_name;
                                }
                                g_convo_state = ConvoState::IN_CONVERSATION;
                                g_chat_history.clear();
                                // *** HIER ÄNDERN ***
                                // Rufe die "schwere" Funktion nur EINMAL auf
                                StartNpcConversationTasks(g_target_ped, playerPed);
                                // ---- input mode ----
                                if (ConfigReader::g_Settings.StT_Enabled && g_whisper_ctx) {
                                    g_input_state = InputState::IDLE; // wait for PTT
                                    UI::_SET_TEXT_ENTRY((char*)"STRING");
                                    UI::_ADD_TEXT_COMPONENT_STRING((char*)"Hold [PTT] to speak…");
                                    UI::_DRAW_SUBTITLE_TIMED(5000, true);
                                    Log("STT mode – awaiting PTT");
                                }
                                else {
                                    GAMEPLAY::DISPLAY_ONSCREEN_KEYBOARD(
                                        1, (char*)"FMMC_KEY_TIP", (char*)"", (char*)"", (char*)"", (char*)"", (char*)"",
                                        ConfigReader::g_Settings.MaxInputChars);
                                    g_input_state = InputState::WAITING_FOR_INPUT;
                                    Log("Keyboard opened");
                                }
                            }
                        }
                    }
                }
            }

            // ----- 7b. PTT START (WÄHREND KONVERSATION) -----
            if (g_convo_state == ConvoState::IN_CONVERSATION &&
                g_input_state == InputState::IDLE &&
                g_llm_state == InferenceState::IDLE &&
                ConfigReader::g_Settings.StT_Enabled &&
                ConfigReader::g_Settings.StTRB_Activation_Key != 0 &&
                IsKeyJustPressed(ConfigReader::g_Settings.StTRB_Activation_Key))
            {
                LogA("PTT pressed → start recording");
                StartAudioRecording();
                g_input_state = InputState::RECORDING;
                UI::_SET_TEXT_ENTRY((char*)"STRING");
                UI::_ADD_TEXT_COMPONENT_STRING((char*)"RECORDING… (release to stop)");
                UI::_DRAW_SUBTITLE_TIMED(10000, true);
            }

            // ----- 8. LLM RESPONSE READY → DISPLAY & PREPARE NEXT INPUT -----
            if (g_llm_state == InferenceState::COMPLETE) {
                if (g_convo_state != ConvoState::IN_CONVERSATION) { g_llm_state = InferenceState::IDLE; continue; }
                std::string clean = CleanupResponse(g_llm_response);
                // Handle timeout/error cases in display
                if (g_llm_response == "LLM_TIMEOUT" || g_llm_response == "LLM_ERROR") {
                    clean = "Response error or timeout.";
                }
                // 1. Save the raw, cleaned response to the chat history
                g_chat_history.push_back(g_current_npc_name + ": " + clean);
                while (g_chat_history.size() > ConfigReader::g_Settings.MaxChatHistoryLines)
                    g_chat_history.erase(g_chat_history.begin());
                // 2. Use WordWrap to break the long string into multiple lines (e.g., max 50 chars per line)
                std::string wrappedText = WordWrap(clean, 50);
                // 3. Set the render text using the wrapped string.
                g_renderText = g_current_npc_name + ": " + wrappedText;
                g_renderEndTime = GetTickCount64() + 10000; // 10 s
                Log("RENDER: " + g_renderText);
                // *** HIER ENTFERNEN ***
                // ApplyNpcTasks(g_target_ped, playerPed); // <--- ENTFERNT! Sektion 2 macht das jetzt.
                // ---- decide next input mode ----
                if (ConfigReader::g_Settings.StT_Enabled) {
                    g_input_state = InputState::IDLE;
                    Log("LLM done → STT idle");
                }
                else {
                    GAMEPLAY::DISPLAY_ONSCREEN_KEYBOARD(
                        1, (char*)"FMMC_KEY_TIP", (char*)"", (char*)"", (char*)"", (char*)"", (char*)"",
                        ConfigReader::g_Settings.MaxInputChars);
                    g_input_state = InputState::WAITING_FOR_INPUT;
                    Log("LLM done → keyboard reopened");
                }
                g_llm_state = InferenceState::IDLE;
            }

            WAIT(0);
        } // end while(true)
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
BOOL APIENTRY DllMain(HMODULE hMod, DWORD reason, LPVOID) {
    switch (reason) {
    case DLL_PROCESS_ATTACH:
    { std::ofstream f(LOG_FILE_NAME, std::ios::trunc); }
    Log("DLL attached");
    scriptRegister(hMod, ScriptMain);
    break;
    case DLL_PROCESS_DETACH:
        Log("DLL detach – shutdown");
        if (g_isInitialized) ShutdownLLM();
        scriptUnregister(hMod);
        break;
    }
    return TRUE;
}

// ------------------------------------------------------------
// WORD WRAP UTILITY
// ------------------------------------------------------------
std::string WordWrap(const std::string& text, size_t limit = 50) {
    if (text.empty()) return "";
    std::stringstream result;
    std::stringstream currentWord;
    size_t currentLineLength = 0;
    for (char c : text) {
        if (std::isspace(c)) {
            // Wenn das Wort zu lang für die aktuelle Zeile ist, füge einen Umbruch ein
            if (currentLineLength + currentWord.str().length() > limit) {
                result << '\n';
                currentLineLength = 0;
            }
            // Füge das Wort und ein Leerzeichen hinzu
            result << currentWord.str() << ' ';
            currentLineLength += currentWord.str().length() + 1;
            currentWord.str("");
        }
        else {
            currentWord << c;
        }
    }
    // Füge das letzte Wort hinzu
    if (currentWord.str().length() > 0) {
        if (currentLineLength + currentWord.str().length() > limit) {
            result << '\n';
        }
        result << currentWord.str();
    }
    return result.str();
}

//EOF
