#include "AbstractCalls.h"
using namespace AbstractGame;
using namespace AbstractTypes;
#define _CRT_SECURE_NO_WARNINGS
#define NOMINMAX

// 1. ONLY INCLUDE main.h
#include "main.h"
#include <string.h>
#include <algorithm> // Für std::min, std::max

ModSettings g_ModSettings;
// ------------------------------------------------------------
// GLOBAL STATE (DEFINITIONS for LLM_Inference.cpp)
// (This is where the 'extern' variables from main.h are born)
// ------------------------------------------------------------


// LLM Globals
llama_model* g_model = nullptr;
llama_context* g_ctx = nullptr;
InferenceState g_llm_state = InferenceState::IDLE;
std::future<std::string> g_llm_future;
std::string g_llm_response = "";
std::chrono::high_resolution_clock::time_point g_response_start_time;
static int32_t g_repeat_last_n = 512;

// In LLM_Inference.cpp
struct llama_adapter_lora* g_lora_adapter = nullptr;


// STT Globals
struct whisper_context* g_whisper_ctx = nullptr;
std::vector<float> g_audio_buffer;
ma_device g_capture_device;
bool g_is_recording = false;
std::future<std::string> g_stt_future;

// --- MEMORY MONITORING ---
static size_t g_memoryAllocations = 0;
static size_t g_memoryFrees = 0;


//logs audio A
static void LogA(const std::string& msg) {
    std::ofstream f("kkamel_audio2.log", std::ios::app);
    if (f) {
        auto t = std::time(nullptr);
        auto tm = *std::localtime(&t);
        f << "[" << std::put_time(&tm, "%Y-%m-%d %H:%M:%S") << "] " << msg << "\n";
    }
}


void LogMemoryStats() {
    if (g_memoryAllocations % 100 == 0) {
        LogLLM("Memory Stats - Allocations: " + std::to_string(g_memoryAllocations) +
            ", Frees: " + std::to_string(g_memoryFrees) +
            ", Net: " + std::to_string(g_memoryAllocations - g_memoryFrees));
    }
}

// --- LOGGING ---
static void LogM(const std::string& message) {
    std::ofstream log(LOG_FILE_NAME3, std::ios_base::app);
    if (log.is_open()) {
        auto t = std::time(nullptr);
        auto tm = *std::localtime(&t);
        log << "[" << std::put_time(&tm, "%Y-%m-%d %H:%M:%S") << "] [ModMain] " << message << "\n";
        log.flush();
    }
}

static void LlamaLogCallback(ggml_log_level level, const char* text, void* user_data) {
    std::string logText = text;
    if (!logText.empty() && logText.back() == '\n') {
        logText.pop_back();
    }
    LogLLM("LLAMA_INTERNAL: " + logText);
}

void LogLLM(const std::string& message) {
    auto now = std::chrono::system_clock::now();
    auto now_c = std::chrono::system_clock::to_time_t(now);
    std::ofstream log("kkamel.log", std::ios_base::app);
    if (log.is_open()) {
        log << "[" << std::put_time(std::localtime(&now_c), "%Y-%m-%d %H:%M:%S") << "] [LLM_Inference] " << message << "\n";
        log.flush();
    }
}

// --- NAMENSLISTEN (WIE IN v1.0.8) ---
const std::vector<std::string> MALE_FIRST_NAMES = {
    "James", "John", "Robert", "Michael", "William", "David", "Richard", "Joseph", "Thomas", "Charles",
    "Chris", "Daniel", "Matthew", "Anthony", "Mark", "Don", "Paul", "Steve", "Andy", "Ken",
    "Luis", "Carlos", "Juan", "Miguel", "Jose", "Hector", "Javier", "Ricardo", "Manny", "Chico"
};
const std::vector<std::string> FEMALE_FIRST_NAMES = {
    "Mary", "Patricia", "Jennifer", "Linda", "Elizabeth", "Barbara", "Susan", "Jessica", "Sarah", "Karen",
    "Nancy", "Lisa", "Betty", "Margaret", "Sandra", "Ashley", "Kim", "Emily", "Donna", "Michelle",
    "Maria", "Carmen", "Rosa", "Isabella", "Sofia", "Camila", "Valeria", "Lucia", "Ximena"
};
const std::vector<std::string> LAST_NAMES = {
    "Smith", "Johnson", "Williams", "Brown", "Jones", "Garcia", "Miller", "Davis", "Rodriguez", "Martinez",
    "Hernandez", "Lopez", "Gonzalez", "Wilson", "Anderson", "Thomas", "Taylor", "Moore", "Jackson", "Martin",
    "Lee", "Perez", "Thompson", "White", "Harris", "Sanchez", "Clark", "Ramirez", "Lewis", "Robinson"
};

// --- WETTER-KARTE ---
static std::map<uint32_t, std::string> g_WeatherMap = {
    {669657108, "BLIZZARD"}, {916995460, "CLEAR"}, {1840358669, "CLEARING"},
    {821931868, "CLOUDS"}, {-1750463879, "EXTRASUNNY"}, {-1368164796, "FOGGY"},
    {-921030142, "HALLOWEEN"}, {-1148613331, "OVERCAST"}, {1420204096, "RAIN"},
    {282916021, "SMOG"}, {603685163, "SNOWLIGHT"}, {-1233681761, "THUNDER"},
    {-1429616491, "XMAS"}
};

// --- FUNKTIONS-IMPLEMENTIERUNGEN ---
std::string GetCurrentWeatherState() {
    uint32_t currentHash = AbstractGame::GetCurrentWeatherType();
    std::string weatherStr = "UNKNOWN";
    int signedHash = static_cast<int>(currentHash);
    for (const auto& pair : g_WeatherMap) {
        if (pair.first == signedHash) {
            weatherStr = pair.second;
            break;
        }
    }
    return "Current Weather: " + weatherStr + ".";
}

std::string GetCurrentTimeState() {
    int hours, minutes;
    AbstractGame::GetGameTime(hours, minutes);
    std::string timeOfDay = "Day";
    if (hours >= 20 || hours < 6) timeOfDay = "Night";
    else if (hours >= 6 && hours < 10) timeOfDay = "Morning";
    else if (hours >= 17 && hours < 20) timeOfDay = "Evening";
    std::stringstream ss;
    ss << hours << ":" << (minutes < 10 ? "0" : "") << minutes << " (" << timeOfDay << ")";
    return ss.str();
}

std::string GenerateNpcName(const NpcPersona& persona) {
    if (!persona.inGameName.empty()) {
        return persona.inGameName;
    }
    int rand_idx = 0;
    if (persona.relationshipGroup == "Gang") {
        if (persona.gender == "M" && !MALE_FIRST_NAMES.empty()) {
            rand_idx = rand() % MALE_FIRST_NAMES.size();
            return MALE_FIRST_NAMES[rand_idx];
        }
        else if (persona.gender == "F" && !FEMALE_FIRST_NAMES.empty()) {
            rand_idx = rand() % FEMALE_FIRST_NAMES.size();
            return FEMALE_FIRST_NAMES[rand_idx];
        }
    }
    if (persona.relationshipGroup == "Law" && !LAST_NAMES.empty()) {
        rand_idx = rand() % LAST_NAMES.size();
        return "Officer " + LAST_NAMES[rand_idx];
    }
    if (persona.gender == "M" && !MALE_FIRST_NAMES.empty() && !LAST_NAMES.empty()) {
        int first_idx = rand() % MALE_FIRST_NAMES.size();
        int last_idx = rand() % LAST_NAMES.size();
        return MALE_FIRST_NAMES[first_idx] + " " + LAST_NAMES[last_idx];
    }
    else if (persona.gender == "F" && !FEMALE_FIRST_NAMES.empty() && !LAST_NAMES.empty()) {
        int first_idx = rand() % FEMALE_FIRST_NAMES.size();
        int last_idx = rand() % LAST_NAMES.size();
        return FEMALE_FIRST_NAMES[first_idx] + " " + LAST_NAMES[last_idx];
    }
    return persona.modelName;
}

void UpdateRepeatPenaltyFromConfig() {

    int estimated_tokens = ConfigReader::g_Settings.MaxHistoryTokens / 2;
    estimated_tokens = std::max(128, std::min(1024, estimated_tokens));

    g_repeat_last_n = static_cast<int32_t>(estimated_tokens);

    LogLLM("UpdateRepeatPenaltyFromConfig: repeat_last_n set to " + std::to_string(g_repeat_last_n) +
        " (based on MaxHistoryTokens = " + std::to_string(ConfigReader::g_Settings.MaxHistoryTokens) + ")");
}


std::string AssemblePrompt(AHandle targetPed, AHandle playerPed, const std::vector<std::string>& chatHistory) {
    // Safety check: Ensure the model is loaded before we try to use it.
    if (!g_model || !g_ctx) {
        LogLLM("AssemblePrompt FATAL: g_model or g_ctx is null. Cannot build prompt.");
        return ""; // Return an empty string to indicate failure.
    }
    const llama_vocab* vocab = llama_model_get_vocab(g_model);
    if (!vocab) {
        LogLLM("AssemblePrompt FATAL: Could not get vocab from model.");
        return "";
    }

    // =================================================================
    // STEP 1: Build the Base Prompt & Dynamically Injected Context
    // =================================================================
    std::stringstream basePromptStream;
    NpcPersona target = ConfigReader::GetPersona(targetPed);
    NpcPersona player = ConfigReader::GetPersona(playerPed);
    std::string npcName = g_current_npc_name;

    // --- Part A: Core Personality & Scenario ---
    std::string playerName = "Stranger";
    std::string char_rel = "unknown";
    std::string group_rel = "unknown";
    if (!target.inGameName.empty() && !player.inGameName.empty()) {
        std::string temp_rel = ConfigReader::GetRelationship(target.inGameName, player.inGameName);
        if (temp_rel.find("unknown") == std::string::npos && temp_rel.find("neutral") == std::string::npos) {
            char_rel = temp_rel;
            playerName = player.inGameName;
        }
    }
    if (!target.subGroup.empty() && !player.subGroup.empty()) {
        std::string temp_rel = ConfigReader::GetRelationship(target.subGroup, player.subGroup);
        if (temp_rel.find("unknown") == std::string::npos && temp_rel.find("neutral") == std::string::npos) {
            group_rel = temp_rel;
        }
    }
    if (playerName == "Stranger" && player.type == "PLAYER") {
        playerName = player.inGameName;
    }

    basePromptStream << "<|system|>\n";
    // NOTE: g_ContentGuidelines is now loaded via isalwaysloaded, so we remove it from here.
    basePromptStream << "You, are one participant in an 2 Person Conversation, player and you. You only answer, the other person (player) asks.";
    basePromptStream << "YOUR CHARACTER:\n";
    basePromptStream << "- Name: " << npcName << " \n";
    basePromptStream << "- Gender: " << target.gender << "\n";
    basePromptStream << "- Role: " << target.type << " / " << target.subGroup << "\n";
    basePromptStream << "- Traits: " << target.behaviorTraits << "\n";
    basePromptStream << "\nSCENARIO:\n";
    // NOTE: Physical location is now handled by the dynamic injector.
    basePromptStream << "- Time: " << GetCurrentTimeState() << "\n";
    basePromptStream << "- Interacting with: " << playerName << " (Role: " << player.type << " / " << player.subGroup << ")\n";
    basePromptStream << "- Character Relationship: " << char_rel << "\n";
    basePromptStream << "- Group Relationship: " << group_rel << "\n";
    basePromptStream << "\nINSTRUCTIONS:\n";
    basePromptStream << "- Speak ONLY as " << npcName << ".\n";
    basePromptStream << "- You dont need to write the " << npcName << " at the beginning of the answer \n";
    basePromptStream << "- Keep responses short (1-3 sentences) \n";
    basePromptStream << "- **CRITICAL: DO NOT** generate text for \"" + playerName + "\" or using the <|user|> tag \n";
    basePromptStream << "- If your character would naturally disengage or the topic is exhausted, conclude your response with the tag 'Good Bye' and [END_CONVERSATION].\n";
    basePromptStream << "Never Say that you are an fictional character, an AI, phi3, or similar. Never say you are in a fictional world.";


    // --- Part B: The Dynamic Context Injection Logic ---
    std::stringstream injectedContext;
    std::set<std::string> alreadyInjectedSections;

    // 1. Inject "always loaded" sections from our new database
    for (const auto& pair : ConfigReader::g_KnowledgeDB) {
        if (pair.second.isAlwaysLoaded) {
            injectedContext << pair.second.content;
            alreadyInjectedSections.insert(pair.first);
        }
    }

    // 2. Scan the last player message for keywords
    std::string lastPlayerMsg = "";
    for (auto it = chatHistory.rbegin(); it != chatHistory.rend(); ++it) {
        if (it->find("<|user|>") != std::string::npos) { lastPlayerMsg = *it; break; }
    }
    std::string normalizedPlayerInput = NormalizeString(lastPlayerMsg);

    if (!normalizedPlayerInput.empty()) {
        for (const auto& pair : ConfigReader::g_KnowledgeDB) {
            const auto& section = pair.second;
            if (section.isAlwaysLoaded || alreadyInjectedSections.count(pair.first)) continue;

            for (const std::string& keyword : section.keywords) {
                if (normalizedPlayerInput.find(keyword) != std::string::npos) {
                    if (section.loadEntireSectionOnMatch) {
                        injectedContext << section.content;
                    }
                    else {
                        for (const auto& kvPair : section.keyValues) {
                            if (NormalizeString(kvPair.first) == keyword) {
                                injectedContext << kvPair.first << " = " << kvPair.second << "\n";
                                break;
                            }
                        }
                    }
                    alreadyInjectedSections.insert(pair.first);
                    goto next_section_label;
                }
            }
        next_section_label:;
        }
    }

    // 3. Inject current location context (from the special [CITY_CONTEXT] section)
    //AVec3 pos = GetEntityPosition(targetPed);
    AVec3 centre = GetEntityPosition(playerPed);
    std::string zoneName = AbstractGame::GetZoneName(pos);
    std::string zoneContext = ConfigReader::GetZoneContext(zoneName);
    if (!zoneContext.empty()) {
        injectedContext << zoneName << " = " << zoneContext << "\n";
    }

    // --- Part C: Combine Base Prompt and Injected Context ---
    std::string finalInjectedText = injectedContext.str();
    if (!finalInjectedText.empty()) {
        basePromptStream << "\n[ADDITIONAL CONTEXT]:\n" << finalInjectedText;
    }
    basePromptStream << "<|end|>\n";
    std::string static_prompt = basePromptStream.str();


    // =================================================================
    // STEP 2: Calculate Budgets (VRAM Management)
    // =================================================================
    const int32_t n_ctx = llama_n_ctx(g_ctx);
    const int32_t response_buffer = 256;

    std::vector<llama_token> static_tokens(n_ctx);
    int32_t static_token_count = llama_tokenize(vocab, static_prompt.c_str(), static_prompt.length(), static_tokens.data(), static_tokens.size(), false, false);

    int32_t history_token_budget = n_ctx - static_token_count - response_buffer;
    history_token_budget = std::min(history_token_budget, static_cast<int32_t>(ConfigReader::g_Settings.MaxHistoryTokens));

    LogLLM("Context Budgeting: Total (n_ctx): " + std::to_string(n_ctx) + ", Static Cost: " + std::to_string(static_token_count) + ", Final History Budget: " + std::to_string(history_token_budget));

    // =================================================================
    // STEP 3: Fill the History Budget (from Newest to Oldest)
    // =================================================================
    std::vector<std::string> selected_history;
    int32_t history_tokens_used = 0;

    if (history_token_budget > 0 && !chatHistory.empty()) {
        std::vector<llama_token> msg_tokens(n_ctx);
        for (auto it = chatHistory.rbegin(); it != chatHistory.rend(); ++it) {
            const std::string& message = *it;
            int32_t msg_token_count = llama_tokenize(vocab, message.c_str(), message.length(), msg_tokens.data(), msg_tokens.size(), false, false);

            if (history_tokens_used + msg_token_count <= history_token_budget) {
                selected_history.insert(selected_history.begin(), message);
                history_tokens_used += msg_token_count;
            }
            else {
                LogLLM("Context Budgeting: History budget full. Truncating older messages.");
                break;
            }
        }
    }

    LogLLM("Context Budgeting: Final History: " + std::to_string(selected_history.size()) + " entries (" + std::to_string(history_tokens_used) + " tokens)");

    // =================================================================
    // STEP 4: Assemble the Final, Perfectly Sized Prompt
    // =================================================================
    std::stringstream finalPromptStream;
    finalPromptStream << static_prompt;

    if (!selected_history.empty()) {
        finalPromptStream << "\nCHAT HISTORY:\n";
        for (const std::string& line : selected_history) {
            finalPromptStream << line << "\n";
        }
    }

    finalPromptStream << "\n<|assistant|>\n";
    return finalPromptStream.str();
}

bool InitializeLLM(const char* model_path) {
    LogLLM("InitializeLLM called with model_path: " + std::string(model_path));
    if (g_model != nullptr) {
        LogLLM("InitializeLLM: Model already initialized, returning true");
        return true;
    }
    srand(static_cast<unsigned int>(std::time(nullptr)));
    LogLLM("InitializeLLM: Random seed initialized.");
    llama_log_set(LlamaLogCallback, nullptr);
    LogLLM("InitializeLLM: Initializing backend");
    llama_backend_init();
    llama_model_params model_params = llama_model_default_params();
    int requested_layers = ConfigReader::g_Settings.USE_GPU_LAYERS;
    if (requested_layers < -1 || requested_layers > 33) {
        model_params.n_gpu_layers = 0;
    }
    else {
        model_params.n_gpu_layers = requested_layers;
    }
    if (model_params.n_gpu_layers == 0) {
        LogLLM("InitializeLLM: Forced CPU mode: n_gpu_layers set to 0");
    }
    else if (model_params.n_gpu_layers == -1) {
        LogLLM("InitializeLLM: FULL GPU MODE: n_gpu_layers set to -1");
    }
    else {
        LogLLM("InitializeLLM: GPU mode: n_gpu_layers set to " + std::to_string(model_params.n_gpu_layers));
    }
    LogLLM("InitializeLLM: Loading model from " + std::string(model_path));
    SetEnvironmentVariableA("GGML_VK_FORCE_HEAP_INDEX", "0");
    SetEnvironmentVariableA("GGML_VK_MAX_MEMORY_MB", "14000");
    SetEnvironmentVariableA("GGML_VK_FORCE_HOST_MEMORY", "1");
    SetEnvironmentVariableA("GGML_VULKAN_MEMORY_DEBUG", "1");
    g_model = llama_model_load_from_file(model_path, model_params);
    if (g_model == nullptr) {
        LogLLM("InitializeLLM: Failed to load model");
        llama_backend_free();
        return false;
    }
    LogLLM("InitializeLLM: Model loaded. Context will be created by ModMain.");
    g_memoryAllocations++;
    LogMemoryStats();
    return true;
}

void ShutdownLLM() {
    LogLLM("ShutdownLLM called");
    if (g_llm_state == InferenceState::RUNNING) {
        if (g_llm_future.valid()) {
            try {
                g_llm_future.SystemSystemSystemSystemWait();
                g_llm_future.get();
            }
            catch (...) {
                LogLLM("ShutdownLLM: Ignored exception during future cleanup");
            }
        }
    }
    if (g_ctx != nullptr) {
        LogLLM("ShutdownLLM: Freeing context");
        llama_free(g_ctx);
        g_ctx = nullptr;
        g_memoryFrees++;
    }
    if (g_model != nullptr) {
        LogLLM("ShutdownLLM: Freeing model");
        llama_model_free(g_model);
        g_model = nullptr;
        g_memoryFrees++;
    }
    LogLLM("ShutdownLLM: Freeing backend");
    llama_backend_free();
    g_chat_history.clear();
    g_chat_history.shrink_to_fit();
    LogLLM("ShutdownLLM completed");
    LogMemoryStats();
}

// In LLM_Inference.cpp

std::string CleanupResponse(std::string text) {
    // Logging Helper (optional, aber nützlich)
    LogLLM("CleanupResponse: Original text (first 400): " + text.substr(0, (std::min)((size_t)400, text.length())));

    // ------------------------------------------------------------
    // 1. DYNAMISCHE STOP-TOKEN LISTE ERSTELLEN
    // ------------------------------------------------------------

    // Hole die konfigurierte, Komma-getrennte Liste (aus der INI)
    std::vector<std::string> stop_tokens = ConfigReader::SplitString(
        ConfigReader::g_Settings.StopStrings,
        ','
    );

    // Füge harte interne LLM- und Chat-Tags hinzu (die der Nutzer nicht ändern sollte)
    stop_tokens.push_back("<|endoftext|>");
    stop_tokens.push_back("<|end|>");
    stop_tokens.push_back("\n<|user|>");
    stop_tokens.push_back("<|assistant|>");

    // Füge dynamisch den NPC-Namen als Stop-Regel hinzu (falls er am Anfang steht)
    if (!g_current_npc_name.empty()) {
        stop_tokens.push_back(g_current_npc_name + ":");
    }

    // ------------------------------------------------------------
    // 2. TEXT NACH ERSTEM STOP-TOKEN ABSCHNEIDEN (PRUNING)
    // ------------------------------------------------------------

    size_t earliest_stop_pos = std::string::npos;
    for (const std::string& token : stop_tokens) {
        size_t pos = text.find(token);
        if (pos != std::string::npos) {
            // Finde die früheste Position
            if (pos < earliest_stop_pos || earliest_stop_pos == std::string::npos) {
                earliest_stop_pos = pos;
            }
        }
    }

    if (earliest_stop_pos != std::string::npos) {
        text = text.substr(0, earliest_stop_pos);
        LogLLM("CleanupResponse: Truncated at final stop token string.");
    }

    // ------------------------------------------------------------
    // 3. FINALE BEREINIGUNG (Prefix und Whitespace)
    // ------------------------------------------------------------

    // [Optionaler Schritt: Entferne Namenspräfix, falls das LLM ihn generiert hat]
    size_t firstColonPos = text.find(": ");
    if (firstColonPos != std::string::npos && !g_current_npc_name.empty()) {
        std::string generatedPrefix = text.substr(0, firstColonPos);
        if (g_current_npc_name.rfind(generatedPrefix, 0) == 0 || generatedPrefix.length() < 15) {
            text = text.substr(firstColonPos + 2);
            LogLLM("CleanupResponse: Stripped redundant name prefix.");
        }
    }


    // Trimmt Whitespace von Anfang und Ende (Standard C++ Logik)
    size_t start = text.find_first_not_of(" \t\n\r");
    text = (start == std::string::npos) ? "" : text.substr(start);
    size_t end = text.find_last_not_of(" \t\n\r");
    text = (end == std::string::npos) ? "" : text.substr(0, end + 1);

    LogLLM("CleanupResponse: Final clean text: " + text);
    return text;
}

// Ensure this is visible (put at top of file if needed, or leave here if main.h covers it)
extern ConvoState g_convo_state;
// Ensure this is visible (put at top of file if needed)

struct TokenProb {
    int id;
    float val; // Logit or Probability
};


llama_token ManualSample(float* logits, int32_t n_vocab, const std::vector<llama_token>& history,
    float temp, float top_p, int top_k, float min_p, float rep_penalty)
{
    std::vector<TokenProb> candidates(n_vocab);
    for (int i = 0; i < n_vocab; ++i) candidates[i] = { i, logits[i] };

    // Repetition Penalty
    if (rep_penalty > 1.0f && !history.empty()) {
        for (const auto& h : history) {
            if (h < n_vocab) candidates[h].val = (candidates[h].val > 0) ? candidates[h].val / rep_penalty : candidates[h].val * rep_penalty;
        }
    }

    // Temperature
    float max_logit = -1e9;
    for (const auto& c : candidates) if (c.val > max_logit) max_logit = c.val;
    float sum = 0.0f;
    for (int i = 0; i < n_vocab; ++i) {
        float p = exp((candidates[i].val - max_logit) / temp);
        candidates[i].val = p;
        sum += p;
    }
    for (int i = 0; i < n_vocab; ++i) candidates[i].val /= sum;

    // Sort
    std::sort(candidates.begin(), candidates.end(), [](const TokenProb& a, const TokenProb& b) { return a.val > b.val; });

    // Filters (Min-P, Top-K, Top-P)
    size_t keep = candidates.size();
    if (min_p > 0.0f) {
        float thr = candidates[0].val * min_p;
        for (size_t i = 0; i < keep; ++i) if (candidates[i].val < thr) { keep = i; break; }
    }
    if (top_k > 0 && top_k < (int)keep) keep = top_k;

    float cum = 0.0f;
    for (size_t i = 0; i < keep; ++i) {
        cum += candidates[i].val;
        if (cum >= top_p) { keep = i + 1; break; }
    }

    // Select
    float r = ((float)rand() / RAND_MAX) * cum;
    float acc = 0.0f;
    for (size_t i = 0; i < keep; ++i) {
        acc += candidates[i].val;
        if (r <= acc) return candidates[i].id;
    }
    return candidates[0].id;
}

std::string GenerateLLMResponse(const std::string& fullPrompt) {
    LogLLM(">>> GenerateLLMResponse (MANUAL CPU SAMPLER)");

    // 1. Init
    auto start = std::chrono::high_resolution_clock::now();
    std::string response_text = "";
    int32_t n_decode = 0;
    int32_t MAX_OUTPUT = ConfigReader::g_Settings.MaxOutputChars;

    if (!g_model || !g_ctx) return "LLM_NOT_INITIALIZED";
    const llama_vocab* vocab = llama_model_get_vocab(g_model);
    int32_t n_vocab = llama_n_vocab(vocab);

    // 2. Tokenize
    std::vector<llama_token> tokens_list(4096);
    int32_t n_tokens = llama_tokenize(vocab, fullPrompt.c_str(), (int32_t)fullPrompt.length(), tokens_list.data(), (int32_t)tokens_list.size(), true, false);
    if (n_tokens <= 0) return "TOKENIZATION_FAILED";
    tokens_list.resize(n_tokens);

    // 3. Truncate
    int32_t n_ctx = llama_n_ctx(g_ctx);
    if (n_tokens > n_ctx - 100) {
        tokens_list.resize(n_ctx - 100);
        n_tokens = (int32_t)tokens_list.size();
    }

    // 4. SPLIT DECODE (Keep this!)
    llama_batch batch = llama_batch_init(n_tokens, 0, 1);
    int32_t n_past = 0;

    // A. Context
    if (n_tokens > 1) {
        batch.n_tokens = n_tokens - 1;
        for (int i = 0; i < batch.n_tokens; i++) {
            batch.token[i] = tokens_list[i];
            batch.pos[i] = i;
            batch.n_seq_id[i] = 1;
            batch.seq_id[i][0] = 0;
            batch.logits[i] = false;
        }
        if (llama_decode(g_ctx, batch) != 0) { llama_batch_free(batch); return "CTX_FAIL"; }
        n_past = batch.n_tokens;
    }

    // B. Last Token
    batch.n_tokens = 1;
    batch.token[0] = tokens_list[n_tokens - 1];
    batch.pos[0] = n_past;
    batch.n_seq_id[0] = 1;
    batch.seq_id[0][0] = 0;
    batch.logits[0] = true;

    if (llama_decode(g_ctx, batch) != 0) { llama_batch_free(batch); return "LAST_FAIL"; }

    int32_t n_cur = n_tokens;

    // 5. Stop Tokens
    std::vector<llama_token> stop_tokens;

    // A. Liste aus der INI laden (Dein neues Feature!)
    std::vector<std::string> stop_strings = ConfigReader::SplitString(
        ConfigReader::g_Settings.StopStrings,
        ','
    );

    // B. Sicherheits-Tokens hinzufügen (Die müssen immer funktionieren)
    stop_strings.push_back("<|user|>");
    stop_strings.push_back("\n<|user|>");
    stop_strings.push_back("<|assistant|>");
    stop_strings.push_back("<|end|>");
    stop_strings.push_back("<|endoftext|>");
    stop_strings.push_back("[END_CONVERSATION]");

    // C. Auch den aktuellen NPC-Namen als Stop nutzen (verhindert Selbstgespräche)
    if (!g_current_npc_name.empty()) {
        stop_strings.push_back(g_current_npc_name + ":");
    }

    // D. Alle Strings in Tokens umwandeln
    for (const std::string& s : stop_strings) {
        std::vector<llama_token> buf(256);
        // add_bos = false, special = true (wichtig für Control Tokens)
        int n = llama_tokenize(vocab, s.c_str(), (int)s.length(), buf.data(), (int)buf.size(), false, true);

        if (n > 0) {
            // Wir fügen alle Tokens des Stop-Wortes zur Liste hinzu
            stop_tokens.insert(stop_tokens.end(), buf.begin(), buf.begin() + n);
        }
    }

    // 6. SETTINGS (Here you connect your ConfigReader!)
    float temp = ConfigReader::g_Settings.temp;
    float top_p = ConfigReader::g_Settings.float_p;
    int   top_k = ConfigReader::g_Settings.top_k;
    float min_p = ConfigReader::g_Settings.min_p;
    float penalty = ConfigReader::g_Settings.repeat_penalty; 

    // 7. GENERATION LOOP (SAFE)
    try {
        while (n_decode < MAX_OUTPUT) {
            if (g_convo_state == ConvoState::IDLE) break;

            // A. GET LOGITS (Safe Access)
            float* logits = llama_get_logits_ith(g_ctx, 0);
            if (!logits) logits = llama_get_logits_ith(g_ctx, -1);

            if (!logits) {
                LogLLM("FATAL: Logits missing.");
                break;
            }

            // B. MANUAL SAMPLE (No Crash Risk!)
            llama_token id = ManualSample(logits, n_vocab, tokens_list, temp, top_p, top_k, min_p, penalty);

            tokens_list.push_back(id); // Update History

            // C. STOP
            if (llama_vocab_is_eog(vocab, id)) break;
            bool stop = false;
            for (auto t : stop_tokens) if (t == id) stop = true;
            if (stop) break;

            // D. DECODE
            char buf[256] = { 0 };
            int n = llama_token_to_piece(vocab, id, buf, 256, 0, true);
            if (n > 0) {
                std::string piece(buf, n);
                if (piece.find("<|") != std::string::npos) break;
                response_text += piece;
            }

            // E. NEXT BATCH
            batch.n_tokens = 1;
            batch.token[0] = id;
            batch.pos[0] = n_cur;
            batch.logits[0] = true;

            if (llama_decode(g_ctx, batch) != 0) break;

            n_cur++;
            n_decode++;
        }
    }
    catch (...) {
        llama_batch_free(batch);
        return "Error: Exception";
    }

    llama_batch_free(batch);
    auto end = std::chrono::high_resolution_clock::now();
    LogLLM("Gen time: " + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count()) + "ms");

    return CleanupResponse(response_text);
}

// miniaudio callback
void audio_capture_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount)
{
    if (g_is_recording && pInput != NULL) {
        const float* pInputF32 = (const float*)pInput;
        // Add the new audio samples to our global buffer
        g_audio_buffer.insert(g_audio_buffer.end(), pInputF32, pInputF32 + frameCount);
    }
}

// 1. Initialize Whisper Model
bool InitializeWhisper(const char* model_path) {
    LogA("InitializeWhisper: Attempting to load model at: " + std::string(model_path));
    struct whisper_context_params cparams = whisper_context_default_params();
    g_whisper_ctx = whisper_init_from_file_with_params(model_path, cparams);

    if (g_whisper_ctx == nullptr) {
        LogA("InitializeWhisper: FAILED to initialize whisper context.");
        return false;
    }
    LogA("InitializeWhisper: Model loaded successfully.");
    return true;
}

// 2. Initialize Microphone
bool InitializeAudioCaptureDevice() {
    ma_device_config deviceConfig;
    deviceConfig = ma_device_config_init(ma_device_type_capture);
    deviceConfig.capture.format = ma_format_f32; // Whisper needs f32
    deviceConfig.capture.channels = 1;             // Mono
    deviceConfig.sampleRate = WHISPER_SAMPLE_RATE; // 16000
    deviceConfig.dataCallback = audio_capture_callback;
    deviceConfig.pUserData = NULL;

    if (ma_device_init(NULL, &deviceConfig, &g_capture_device) != MA_SUCCESS) {
        LogA("InitializeAudioCaptureDevice: FAILED to initialize audio capture device.");
        return false;
    }
    LogA("InitializeAudioCaptureDevice: Audio device initialized (16kHz, 1-ch, f32).");
    return true;
}

// 3. Start Recording
void StartAudioRecording() {
    LogA("StartAudioRecording: Starting audio stream...");
    g_audio_buffer.clear();
    g_is_recording = true;
    if (ma_device_start(&g_capture_device) != MA_SUCCESS) {
        LogA("StartAudioRecording: FAILED to start audio device.");
        g_is_recording = false;
    }
}

// 4. Stop Recording
void StopAudioRecording() {
    LogA("StopAudioRecording: Stopping audio stream.");
    g_is_recording = false;
    ma_device_stop(&g_capture_device);
    LogA("StopAudioRecording: Audio buffer size: " + std::to_string(g_audio_buffer.size()));
}

// 5. Transcribe (This runs in the async thread)
std::string TranscribeAudio(std::vector<float> pcm_data) {
    LogA("TranscribeAudio: Thread started. Processing " + std::to_string(pcm_data.size()) + " audio samples.");
    if (g_whisper_ctx == nullptr) {
        LogA("TranscribeAudio: FAILED - Whisper context is null.");
        return "";
    }
    if (pcm_data.empty()) {
        LogA("TranscribeAudio: FAILED - Audio buffer is empty.");
        return "";
    }

    whisper_full_params wparams = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
    wparams.print_progress = false;
    wparams.print_realtime = false;
    wparams.print_special = false;
    wparams.print_timestamps = false;
    wparams.language = "auto"; // Use "auto" or "de" if you use the multilingual model
    wparams.n_threads = 4;   // Keep it low to not lag the game

    if (whisper_full(g_whisper_ctx, wparams, pcm_data.data(), pcm_data.size()) != 0) {
        LogA("TranscribeAudio: FAILED - whisper_full failed to process audio.");
        return "";
    }

    int n_segments = whisper_full_n_segments(g_whisper_ctx);
    std::string transcribed_text = "";
    for (int i = 0; i < n_segments; ++i) {
        const char* text = whisper_full_get_segment_text(g_whisper_ctx, i);
        transcribed_text += text;
    }

    LogA("TranscribeAudio: Transcription complete: " + transcribed_text);
    return transcribed_text;

}

// 6. Shutdown STT
void ShutdownWhisper() {
    if (g_whisper_ctx) {
        whisper_free(g_whisper_ctx);
        g_whisper_ctx = nullptr;
        LogA("ShutdownWhisper: Whisper context freed.");
    }
}

// 7. Shutdown Audio
void ShutdownAudioDevice() {
    ma_device_uninit(&g_capture_device);
    LogA("ShutdownAudioDevice: Audio device uninitialized.");
}





