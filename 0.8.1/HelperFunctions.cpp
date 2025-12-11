#include "AbstractCalls.h"
using namespace AbstractGame;
using namespace AbstractTypes;

#pragma comment(lib, "dxgi.lib")
#include "helperfunctions.h"
#include <String.h>
#include <time.h>
#include "ConfigReader.h"
#include "ConversationSystem.h"
#include "main.h"
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

    __declspec(dllexport) void API_StartConversation(int pedHandle, const char* name_override, const char* instruction) {
        if (!g_isInitialized) return;
        if (!IsEntityValid(pedHandle)) return;

        // Clean up previous if distinct
        if (g_target_ped != 0 && g_target_ped != pedHandle) {
            EndConversation();
        }

        g_target_ped = pedHandle;

        // 1. Initiate via Manager
        ChatID chatID = ConvoManager::InitiateConversation(GetPlayerHandle(), pedHandle);
        g_current_chat_ID = chatID; // Update the global ID used by ModMain logic

        Log("[API] Started Chat ID: " + std::to_string(chatID));

        // 2. Handle Name
        std::string finalName;
        if (name_override && name_override[0] != '\0') finalName = name_override;
        else {
            NpcPersona p = ConfigReader::GetPersona(pedHandle);
            finalName = GenerateNpcName(p);
        }
        g_current_npc_name = finalName;

        g_convo_state = ConvoState::IN_CONVERSATION;

        // 3. Init Tasks
        StartNpcConversationTasks(g_target_ped, GetPlayerHandle());

        // 4. Handle Instruction / System Prompt
        std::vector<std::string> history = ConvoManager::GetChatHistory(chatID);
        std::string prompt = AssemblePrompt(g_target_ped, GetPlayerHandle(), history);

        if (instruction != nullptr && instruction[0] != '\0') {
            std::string instrStr = instruction;
            prompt += "\n[SYSTEM INSTRUCTION]: " + instrStr + "\n<|assistant|>\n";
        }

        g_response_start_time = std::chrono::high_resolution_clock::now();
        g_llm_start_time = std::chrono::high_resolution_clock::now();

        g_llm_future = std::async(std::launch::async, GenerateLLMResponse, prompt);
        g_llm_state = InferenceState::RUNNING;
    }


    __declspec(dllexport) void API_StopConversation() {
        EndConversation();
    }


    __declspec(dllexport) bool API_GetLastResponse(char* buffer, int bufferSize) {
        if (g_renderText.empty()) return false;

        if (g_renderText.length() + 1 > static_cast<size_t>(bufferSize)) return false;


        strcpy(buffer, g_renderText.c_str());
        return true;
    }


    __declspec(dllexport) bool API_GetNpcName(char* buffer, int bufferSize) {
        if (g_current_npc_name.empty()) return false;

        if (g_current_npc_name.length() + 1 > static_cast<size_t>(bufferSize)) return false;

        strcpy(buffer, g_current_npc_name.c_str());
        return true;
    }


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

    __declspec(dllexport) bool API_LoadLLM() {
        if (g_isInitialized && g_model) {
            Log("[API] Load LLM: Already initialized, skipping load.");
            return true;
        }

        Log("[API] Load LLM: Starting full re-initialization sequence.");

        try {
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


            enum ggml_type kv_type = GGML_TYPE_F32;
            llama_context_params ctx_params = llama_context_default_params();
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
            case 2: 
                kv_type = GGML_TYPE_Q2_K;
                Log("KV Cache Quantization: Using Q2_K (~2.56 bits)");
                ctx_params.type_k = kv_type;
                ctx_params.type_v = kv_type;
                break;

            case 3: 
                kv_type = GGML_TYPE_Q3_K;
                Log("KV Cache Quantization: Using Q3_K (~3.43 bits)");
                ctx_params.type_k = kv_type;
                ctx_params.type_v = kv_type;
                break;

            case 4: 
                kv_type = GGML_TYPE_Q4_K;
                Log("KV Cache Quantization: Using Q4_K (~4.5 bits)");
                ctx_params.type_k = kv_type;
                ctx_params.type_v = kv_type;
                break;

            case 5: 
                kv_type = GGML_TYPE_Q5_K;
                Log("KV Cache Quantization: Using Q5_K (~5.5 bits)");
                ctx_params.type_k = kv_type;
                ctx_params.type_v = kv_type;
                break;

            case 6: 
                kv_type = GGML_TYPE_Q6_K;
                Log("KV Cache Quantization: Using Q6_K (~6.56 bits)");
                ctx_params.type_k = kv_type;
                ctx_params.type_v = kv_type;
                break;

            case 8: 
                kv_type = GGML_TYPE_Q8_0;
                Log("KV Cache Quantization: Using Q8_0 (8 bits)");
                ctx_params.type_k = kv_type;
                ctx_params.type_v = kv_type;
                break;

            default:
                Log("KV Cache Quantization: Using default float type (F32/F16).");
                break;
            }

            if (ConfigReader::g_Settings.StT_Enabled) {
                std::string sttPath;
                const auto& custSTT = ConfigReader::g_Settings.STT_MODEL_PATH;
                const auto& altSTT = ConfigReader::g_Settings.STT_MODEL_ALT_NAME;

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

    __declspec(dllexport) void API_LogMessage(const char* message) {
        if (message != nullptr) {
            Log("[C# API] " + std::string(message));
        }
    }

    __declspec(dllexport) void API_SetNpcName(const char* newName) {
        if (newName != nullptr && newName[0] != '\0') {
            g_current_npc_name = newName;
            // Removed: g_persistent_npc_names update (Handled internally now)
            Log("[API] Set NPC Name: Name forced to " + g_current_npc_name);
        }
    }

    std::vector<std::string> SplitString(const std::string& str, char delimiter) {
        std::vector<std::string> tokens;
        std::string token;
        std::istringstream tokenStream(str);
        while (std::getline(tokenStream, token, delimiter)) {
           token.erase(0, token.find_first_not_of(" \t\r\n"));
            token.erase(token.find_last_not_of(" \t\r\n") + 1);
            if (!token.empty()) {
                tokens.push_back(token);
            }
        }
        return tokens;
    }

    __declspec(dllexport) void API_ClearAllNpcMemories() {
        EndConversation();
        ConvoManager::RunMaintenance();
    }

    __declspec(dllexport) float OutputFreeVRAM_MB() {
        return GetFreeVRAM_MB();
    }

    __declspec(dllexport) int API_Convo_Initiate(int initiatorHandle, int targetHandle, int forceID) {
        return (int)ConvoManager::InitiateConversation(initiatorHandle, targetHandle, forceID);
    }

    __declspec(dllexport) int API_Convo_GetActiveID(int entityHandle) {
        return (int)ConvoManager::GetActiveChatID(entityHandle);
    }

    __declspec(dllexport) void API_Convo_SetContext(int chatID, const char* location, const char* weather) {
        if (chatID <= 0) return;

       std::string locStr = (location) ? location : "Unknown";
        std::string weaStr = (weather) ? weather : "Unknown";

        ConvoManager::SetChatContext(chatID, locStr, weaStr);
    }
    
    __declspec(dllexport) void API_Convo_AddMessage(int chatID, const char* sender, const char* message) {
        if (chatID <= 0 || !message) return;

        std::string senderStr = (sender) ? sender : "System";
        std::string msgStr = message;

        ConvoManager::AddMessageToChat(chatID, senderStr, msgStr);
    }

    __declspec(dllexport) void API_Convo_Close(int chatID) {
        ConvoManager::CloseConversation(chatID);
    }

    __declspec(dllexport) int API_Convo_GetHistoryCount(int chatID) {
        auto history = ConvoManager::GetChatHistory(chatID);
        return (int)history.size();
    }

    __declspec(dllexport) bool API_Convo_GetHistoryLine(int chatID, int index, char* buffer, int bufferSize) {
        auto history = ConvoManager::GetChatHistory(chatID);

        if (index < 0 || index >= history.size()) return false;

        const std::string& line = history[index];
        if (line.length() + 1 > (size_t)bufferSize) return false;

        strcpy(buffer, line.c_str());
        return true;
    }
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
