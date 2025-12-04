#include "AbstractCalls.h"
using namespace AbstractGame;
using namespace AbstractTypes;
#include <windows.h>
#include "main.h"
#include "helperfunctions.h"
//helperfunctions.cpp



extern std::string LOG_FILE_NAME = "kkamel.log";
extern std::string LOG_FILE_NAME_METRICS = "kkamel_metrics.log";
extern std::string LOG_FILE_NAME_AUDIO = "kkamel_audio.log";
extern std::string LOG_FILE_NAME3 = "kkamel_load.log";

//logs the general base data
void Log(const std::string& msg) {
    std::ofstream f(LOG_FILE_NAME, std::ios::app);
    if (f) {
        auto t = std::time(nullptr);
        auto tm = *std::localtime(&t);
        f << "[" << std::put_time(&tm, "%Y-%m-%d %H:%M:%S") << "] " << msg << "\n";
    }
}


//logs metadata
void LogM(const std::string& msg) {
    std::ofstream f(LOG_FILE_NAME_METRICS, std::ios::app);
    if (f) {
        auto t = std::time(nullptr);
        auto tm = *std::localtime(&t);
        f << "[" << std::put_time(&tm, "%Y-%m-%d %H:%M:%S") << "] " << msg << "\n";
    }
}


//logs audio A
void LogA(const std::string& msg) {
    std::ofstream f(LOG_FILE_NAME_AUDIO, std::ios::app);
    if (f) {
        auto t = std::time(nullptr);
        auto tm = *std::localtime(&t);
        f << "[" << std::put_time(&tm, "%Y-%m-%d %H:%M:%S") << "] " << msg << "\n";
    }
}

/**
static void LogA(const std::string& message) {
    std::ofstream log("kkamel_audio2.log", std::ios_base::app);
    if (log.is_open()) {
        auto t = std::time(nullptr);
        auto tm = *std::localtime(&t);
        log << "[" << std::put_time(&tm, "%Y-%m-%d %H:%M:%S") << "] [ModMain] " << message << "\n";
        log.flush();
    }
} **/



// logs config loading
void LogConfig(const std::string& message) {
    auto now = std::chrono::system_clock::now();
    auto now_c = std::chrono::system_clock::to_time_t(now);
    std::ofstream log("kkamel_loader.log", std::ios_base::app);
    if (log.is_open()) {
        log << "[" << std::put_time(std::localtime(&now_c), "%Y-%m-%d %H:%M:%S") << "] [ConfigReader] " << message << "\n";
        log.flush();
    }
}


std::string FindLoRAFile(const std::string& root_path) {
    const std::string DEFAULT_LORA_NAME = "mod_lora.gguf";

    const std::string& custom_path = ConfigReader::g_Settings.LORA_FILE_PATH;
    const std::string& custom_name = ConfigReader::g_Settings.LORA_ALT_NAME;

    // --- 1. Check 1: Custom Path + Custom Name ---
    if (!custom_path.empty() && !custom_name.empty()) {
        std::string full_path = custom_path;
        if (full_path.back() != '\\' && full_path.back() != '/') {
            full_path += "\\"; // Ensure trailing slash if it's a directory
        }
        full_path += custom_name;
        if (DoesFileExist(full_path)) {
            Log("LoRA Resolver: Found custom path/name: " + full_path);
            return full_path;
        }
    }

    // --- 2. Check 2: Root Path + Custom Name ---
    if (!custom_name.empty()) {
        std::string full_path = root_path + custom_name;
        if (DoesFileExist(full_path)) {
            Log("LoRA Resolver: Found root/custom name: " + full_path);
            return full_path;
        }
    }

    // --- 3. Check 3: Root Path + Default Name (Final Fallback) ---
    std::string default_path = root_path + DEFAULT_LORA_NAME;
    if (DoesFileExist(default_path)) {
        Log("LoRA Resolver: Found default file: " + default_path);
        return default_path;
    }

    Log("LoRA Resolver: No LoRA file found matching configuration.");
    return ""; // Return empty string if no file is found
}


std::string WordWrap(const std::string& text, size_t limit) {
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

    if (currentWord.str().length() > 0) {
        if (currentLineLength + currentWord.str().length() > limit) {
            result << '\n';
        }
        result << currentWord.str();
    }
    return result.str();
}


extern "C" {

    // 1. Status Check
    __declspec(dllexport) bool API_IsModReady() {
        return g_isInitialized;
    }


    // 2. Busy Check
    __declspec(dllexport) bool API_IsBusy() {
        return (g_convo_state != ConvoState::IDLE || g_llm_state != InferenceState::IDLE);
    }

    // 3. ACTION: Start Conversation (Original, aber jetzt mit NPC Name Override)
    // C# usage: API_StartConversation(pedHandle, "Officer Miller", "I need info on the suspect.");
    __declspec(dllexport) void API_StartConversation(int pedHandle, const char* name_override, const char* instruction) {
        if (!g_isInitialized) return;
        if (!IsEntityValid(pedHandle)) return;

        // Reset alte Konversation
        if (g_convo_state != ConvoState::IDLE) {
            EndConversation();
        }

        Log("[API] External Trigger for AHandle " + std::to_string(pedHandle));

        // State setzen
        g_target_ped = pedHandle;
        g_convo_state = ConvoState::IN_CONVERSATION;
        g_chat_history.clear();

        // 1. NPC Name festlegen/überschreiben
        std::string finalName;
        if (name_override != nullptr && name_override[0] != '\0') {
            finalName = name_override;
        }
        else {
            // Generiere Name, wenn nicht überschrieben
            NpcPersona p = ConfigReader::GetPersona(g_target_ped);
            finalName = GenerateNpcName(p);
        }
        g_current_npc_name = finalName;
        g_persistent_npc_names[GetEntityModel(g_target_ped)] = finalName;

        // 2. NPC Physisch fixieren 
        StartNpcConversationTasks(g_target_ped, GetPlayerHandle());

        // 3. Prompt bauen
        std::string prompt = AssemblePrompt(g_target_ped, GetPlayerHandle(), g_chat_history);

        // 4. Instruktion injizieren
        if (instruction != nullptr && instruction[0] != '\0') {
            std::string instrStr = instruction;
            // WICHTIG: Die API-Instruktion muss am Ende des Prompts stehen, kurz vor <|assistant|>
            prompt += "\n[SYSTEM INSTRUCTION]: The player says/does: \"" + instrStr + "\". Respond immediately and naturally to this situation.\n<|assistant|>\n";
        }

        // 5. LLM Starten
        g_response_start_time = std::chrono::high_resolution_clock::now();
        g_llm_start_time = std::chrono::high_resolution_clock::now();

        g_llm_future = std::async(std::launch::async, GenerateLLMResponse, prompt);
        g_llm_state = InferenceState::RUNNING;
    }

    // 4. Stop Conversation
    __declspec(dllexport) void API_StopConversation() {
        EndConversation();
    }

    // 5. DATA RETRIEVAL: Get the text of the last response (Subtitle)
    __declspec(dllexport) bool API_GetLastResponse(char* buffer, int bufferSize) {
        if (g_renderText.empty()) return false;

        if (g_renderText.length() + 1 > static_cast<size_t>(bufferSize)) return false;

        // Nutze strcpy (aus <string.h>)
        strcpy(buffer, g_renderText.c_str());
        return true;
    }

    // 6. DATA RETRIEVAL: Get the calculated NPC Name (Current Target)
    __declspec(dllexport) bool API_GetNpcName(char* buffer, int bufferSize) {
        if (g_current_npc_name.empty()) return false;

        if (g_current_npc_name.length() + 1 > static_cast<size_t>(bufferSize)) return false;

        strcpy(buffer, g_current_npc_name.c_str());
        return true;
    }

    // 7. MEMORY: Clear the KV cache (C# "Clear Memory")
    __declspec(dllexport) void API_ClearCache() {
        if (g_ctx) {
            llama_memory_t mem = llama_get_memory(g_ctx);
            if (mem) {
                llama_memory_clear(mem, true);
                Log("[API] KV Cache (LLM working memory) cleared.");
                return;
            }
        }
        Log("[API] KV Cache: No context/memory to clear.");
    }

    // 8. LIFECYCLE: Completely unload LLM model (Frees VRAM/RAM)
    __declspec(dllexport) void API_DeloadLLM() {
        if (!g_model) {
            Log("[API] Deload LLM: Model not currently loaded.");
            return;
        }

        EndConversation();
        ShutdownLLM();
        g_isInitialized = false;

        Log("[API] Deload LLM: Model and context fully unloaded. VRAM freed.");
    }

    // 9. LIFECYCLE: Reload LLM model (Führt die komplexe Init-Logik aus)
    // Die Implementierung wird aus dem obigen Block verwendet.
    // NOTE: Die vollständige LLM- und Whisper-Initialisierungslogik aus ScriptMain muss hier stehen.
    __declspec(dllexport) bool API_LoadLLM() {
        if (g_isInitialized && g_model) {
            Log("[API] Load LLM: Already initialized, skipping load.");
            return true;
        }

        Log("[API] Load LLM: Starting full re-initialization sequence.");

        try {
            // 1. Pfadfindung für LLM
            std::string root = GetModRootPath();
            std::string modelPath;
            const auto& cust = ConfigReader::g_Settings.MODEL_PATH;
            const auto& alt = ConfigReader::g_Settings.MODEL_ALT_NAME;
            const auto def = "Phi3.gguf";

            if (!cust.empty() && DoesFileExist(cust)) modelPath = cust;
            else if (!alt.empty() && DoesFileExist(root + alt)) modelPath = root + alt;
            else if (DoesFileExist(root + def)) modelPath = root + def;

            if (modelPath.empty()) {
                Log("FATAL: API_LoadLLM failed: No LLM model found.");
                return false;
            }

            Log("Using LLM: " + modelPath);
            if (!InitializeLLM(modelPath.c_str())) {
                Log("FATAL: API_LoadLLM failed: InitializeLLM() failed.");
                return false;
            }

            // 2. Kontext-Erstellung
            enum ggml_type kv_type = GGML_TYPE_F32;
            llama_context_params ctx_params = llama_context_default_params();
            //ctx_params.n_ctx = static_cast<uint32_t>(ConfigReader::g_Settings.MaxHistoryTokens);
            ctx_params.n_ctx = static_cast<uint32_t>(ConfigReader::g_Settings.Max_Working_Input);
            ctx_params.n_batch = 1024;
            ctx_params.n_ubatch = 256;

            if (ConfigReader::g_Settings.USE_VRAM_PREFERED) {
                ctx_params.type_k = GGML_TYPE_F16;
                ctx_params.type_v = GGML_TYPE_F16;
            }

            g_ctx = llama_init_from_model(g_model, ctx_params);
            if (!g_ctx) {
                Log("FATAL: API_LoadLLM failed: llama_init_from_model failed.");
                return false;
            }


            if (ConfigReader::g_Settings.USE_VRAM_PREFERED) {
                ctx_params.type_k = GGML_TYPE_F16;
                ctx_params.type_v = GGML_TYPE_F16;
            }
            switch (ConfigReader::g_Settings.KV_Cache_Quantization_Type)
            {
            case 2: // ~2.56 bits-per-weight
                kv_type = GGML_TYPE_Q2_K;
                Log("KV Cache Quantization: Using Q2_K (~2.56 bits)");
                ctx_params.type_k = kv_type;
                ctx_params.type_v = kv_type;
                break;

            case 3: // ~3.43 bits-per-weight
                kv_type = GGML_TYPE_Q3_K;
                Log("KV Cache Quantization: Using Q3_K (~3.43 bits)");
                ctx_params.type_k = kv_type;
                ctx_params.type_v = kv_type;
                break;

            case 4: // ~4.5 bits-per-weight
                kv_type = GGML_TYPE_Q4_K;
                Log("KV Cache Quantization: Using Q4_K (~4.5 bits)");
                ctx_params.type_k = kv_type;
                ctx_params.type_v = kv_type;
                break;

            case 5: // ~5.5 bits-per-weight
                kv_type = GGML_TYPE_Q5_K;
                Log("KV Cache Quantization: Using Q5_K (~5.5 bits)");
                ctx_params.type_k = kv_type;
                ctx_params.type_v = kv_type;
                break;

            case 6: // ~6.56 bits-per-weight
                kv_type = GGML_TYPE_Q6_K;
                Log("KV Cache Quantization: Using Q6_K (~6.56 bits)");
                ctx_params.type_k = kv_type;
                ctx_params.type_v = kv_type;
                break;

            case 8: // 8.0 bits-per-weight
                kv_type = GGML_TYPE_Q8_0;
                Log("KV Cache Quantization: Using Q8_0 (8 bits)");
                ctx_params.type_k = kv_type;
                ctx_params.type_v = kv_type;
                break;

            default:
                // No explicit quantization or unknown value, keeps the default (F32/F16).
                Log("KV Cache Quantization: Using default float type (F32/F16).");
                break;
            }

            // 3. Optional Whisper (STT) Re-Initialisierung
            if (ConfigReader::g_Settings.StT_Enabled) {
                std::string sttPath;
                const auto& custSTT = ConfigReader::g_Settings.STT_MODEL_PATH;
                const auto& altSTT = ConfigReader::g_Settings.STT_MODEL_ALT_NAME;

                // Pfadfindung für STT-Modell
                if (!custSTT.empty() && DoesFileExist(custSTT)) sttPath = custSTT;
                else if (!altSTT.empty() && DoesFileExist(root + altSTT)) sttPath = root + altSTT;

                if (!sttPath.empty() && InitializeWhisper(sttPath.c_str()) && InitializeAudioCaptureDevice()) {
                    Log("Whisper re-initialized successfully.");
                }
                else {
                    Log("STT re-initialization failed or model missing.");
                    ConfigReader::g_Settings.StT_Enabled = false;
                }
            }

            g_isInitialized = true;
            Log("[API] Load LLM: Model and context loaded successfully.");
            return true;

        }
        catch (const std::exception& e) {
            Log("CRITICAL EXCEPTION in API_LoadLLM: " + std::string(e.what()));
            return false;
        }
        catch (...) {
            Log("CRITICAL UNKNOWN EXCEPTION in API_LoadLLM.");
            return false;
        }
    }

    // 10. DEBUG: Custom Log Message
    // C# usage: API_LogMessage("LSPDFR called TriggerSpeech.");
    __declspec(dllexport) void API_LogMessage(const char* message) {
        if (message != nullptr) {
            Log("[C# API] " + std::string(message));
        }
    }

    __declspec(dllexport) void API_SetNpcName(const char* newName) {
        if (newName != nullptr && newName[0] != '\0') {
            g_current_npc_name = newName;
            // Optional: Speichert den Namen auch als persistenten Namen für das aktuelle AHandle Model
            if (IsEntityValid(g_target_ped)) {
                uint32_t model = GetEntityModel(g_target_ped);
                g_persistent_npc_names[model] = newName;
            }
            Log("[API] Set NPC Name: Name forced to " + g_current_npc_name);
        }
    }

    std::vector<std::string> SplitString(const std::string& str, char delimiter) {
        std::vector<std::string> tokens;
        std::string token;
        std::istringstream tokenStream(str);
        while (std::getline(tokenStream, token, delimiter)) {
            // Trim whitespace from beginning and end
            token.erase(0, token.find_first_not_of(" \t\r\n"));
            token.erase(token.find_last_not_of(" \t\r\n") + 1);
            if (!token.empty()) {
                tokens.push_back(token);
            }
        }
        return tokens;
    }



    //11 clear all npc memories.
    __declspec(dllexport) void API_ClearAllNpcMemories() {
        EndConversation();
        UpdateNpcMemoryJanitor(true);
    }


    float GetFreeVRAM_MB() {
        IDXGIFactory4* pFactory = nullptr;
        float freeMB = 24000.0f; // Fallback: Viel Speicher annehmen, falls Check fehlschlägt

        if (SUCCEEDED(CreateDXGIFactory1(__uuidof(IDXGIFactory4), (void**)&pFactory))) {
            IDXGIAdapter* pAdapter = nullptr;
            if (SUCCEEDED(pFactory->EnumAdapters(0, &pAdapter))) {
                IDXGIAdapter3* pAdapter3 = nullptr;
                if (SUCCEEDED(pAdapter->QueryInterface(__uuidof(IDXGIAdapter3), (void**)&pAdapter3))) {
                    DXGI_QUERY_VIDEO_MEMORY_INFO memInfo;
                    if (SUCCEEDED(pAdapter3->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &memInfo))) {
                        // Budget = Gesamter VRAM, Usage = Verbrauchter VRAM
                        float budgetMB = static_cast<float>(memInfo.Budget) / 1024.f / 1024.f;
                        float usedMB = static_cast<float>(memInfo.CurrentUsage) / 1024.f / 1024.f;
                        freeMB = budgetMB - usedMB;
                    }
                    pAdapter3->Release();
                }
                pAdapter->Release();
            }
            pFactory->Release();
        }
        return freeMB;
    }

}



