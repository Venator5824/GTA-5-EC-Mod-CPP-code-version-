#define _CRT_SECURE_NO_WARNINGS
#define NOMINMAX

// 1. ONLY INCLUDE main.h
#include "main.h"
#include <string.h>

// ------------------------------------------------------------
// GLOBAL STATE (DEFINITIONS for LLM_Inference.cpp)
// (This is where the 'extern' variables from main.h are born)
// ------------------------------------------------------------
std::string LOG_FILE_NAME3 = "kkamel_load.log";

// LLM Globals
llama_model* g_model = nullptr;
llama_context* g_ctx = nullptr;
InferenceState g_llm_state = InferenceState::IDLE;
std::future<std::string> g_llm_future;
std::string g_llm_response = "";
std::chrono::high_resolution_clock::time_point g_response_start_time;

// STT Globals
struct whisper_context* g_whisper_ctx = nullptr;
std::vector<float> g_audio_buffer;
ma_device g_capture_device;
bool g_is_recording = false;
std::future<std::string> g_stt_future;

// --- MEMORY MONITORING ---
static size_t g_memoryAllocations = 0;
static size_t g_memoryFrees = 0;


static void LogA(const std::string& message) {
    std::ofstream log("kkamel_audio2.log", std::ios_base::app);
    if (log.is_open()) {
        auto t = std::time(nullptr);
        auto tm = *std::localtime(&t);
        log << "[" << std::put_time(&tm, "%Y-%m-%d %H:%M:%S") << "] [ModMain] " << message << "\n";
        log.flush();
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
static std::map<Hash, std::string> g_WeatherMap = {
    {669657108, "BLIZZARD"}, {916995460, "CLEAR"}, {1840358669, "CLEARING"},
    {821931868, "CLOUDS"}, {-1750463879, "EXTRASUNNY"}, {-1368164796, "FOGGY"},
    {-921030142, "HALLOWEEN"}, {-1148613331, "OVERCAST"}, {1420204096, "RAIN"},
    {282916021, "SMOG"}, {603685163, "SNOWLIGHT"}, {-1233681761, "THUNDER"},
    {-1429616491, "XMAS"}
};

// --- FUNKTIONS-IMPLEMENTIERUNGEN ---
std::string GetCurrentWeatherState() {
    Hash currentHash = GAMEPLAY::_GET_CURRENT_WEATHER_TYPE();
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
    int hours = TIME::GET_CLOCK_HOURS();
    int minutes = TIME::GET_CLOCK_MINUTES();
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

std::string AssemblePrompt(Ped targetPed, Ped playerPed, const std::vector<std::string>& chatHistory) {
    std::stringstream promptStream;
    NpcPersona target = ConfigReader::GetPersona(targetPed);
    NpcPersona player = ConfigReader::GetPersona(playerPed);
    // NEUE ZEILE: Verwendet den global gespeicherten Namen
    std::string npcName = g_current_npc_name;

    // --- DIESEN NEUEN BLOCK EINFÜGEN ---
    std::string playerName = "Stranger";  // Standard
    std::string char_rel = "unknown";    // Spezifische Charakter-Beziehung
    std::string group_rel = "unknown";   // Allgemeine Gruppen-Beziehung

    // --- NEUE LOGIK (ALLES AUF EINMAL) ---
    // 1. Hole CHARAKTER-Beziehung (z.B. Trevor -> Franklin)
    if (!target.inGameName.empty() && !player.inGameName.empty()) {
        std::string temp_rel = ConfigReader::GetRelationship(target.inGameName, player.inGameName);

        // Prüfen, ob ein Eintrag gefunden wurde (alles außer "unknown" oder "neutral")
        if (temp_rel.find("unknown") == std::string::npos && temp_rel.find("neutral") == std::string::npos) {
            char_rel = temp_rel;
            playerName = player.inGameName; // Wir kennen den Spieler jetzt beim Namen
            LogLLM("AssemblePrompt: Found specific CHARACTER relationship: " + char_rel);
        }
    }

    // 2. Hole GRUPPEN-Beziehung (IMMER, sofern vorhanden, z.B. LSPD -> Families)
    if (!target.subGroup.empty() && !player.subGroup.empty()) {
        std::string temp_rel = ConfigReader::GetRelationship(target.subGroup, player.subGroup);

        if (temp_rel.find("unknown") == std::string::npos && temp_rel.find("neutral") == std::string::npos) {
            group_rel = temp_rel;
            LogLLM("AssemblePrompt: Found GROUP relationship: " + group_rel);
        }
    }

    // 3. Fallback: Spieler ist ein Hauptcharakter, aber keine spezifische Beziehung gefunden
    if (playerName == "Stranger" && player.type == "PLAYER") {
        playerName = player.inGameName; // Stellt sicher, dass Franklin immer "Franklin" ist
    }
    // --- ENDE NEUE LOGIK ---

    promptStream << "<|system|>\n";
    promptStream << ConfigReader::g_ContentGuidelines << "\n\n";
    promptStream << "You,  are one participant in an 2 Person Conversation, player and you. You only answer, the other person (player) asks.";
    promptStream << "YOUR CHARACTER:\n";
    promptStream << "- Name: " << npcName << " (Do NOT use this name in your response)\n";
    promptStream << "- Gender: " << target.gender << "\n";
    promptStream << "- Role: " << target.type << " / " << target.subGroup << "\n";
    promptStream << "- Traits: " << target.behaviorTraits << "\n";
    Vector3 pos = ENTITY::GET_ENTITY_COORDS(targetPed, true);
    std::string zone = ZONE::GET_NAME_OF_ZONE(pos.x, pos.y, pos.z);
    promptStream << "\nSCENARIO:\n";
    promptStream << "- Location: " << zone << "\n";
    promptStream << "- Time: " << GetCurrentTimeState() << "\n";
    promptStream << "- Interacting with: " << playerName << " (Role: " << player.type << " / " << player.subGroup << ")\n";
    promptStream << "- Character Relationship: " << char_rel << "\n";
    promptStream << "- Group Relationship: " << group_rel << "\n";
    promptStream << "\nINSTRUCTIONS:\n";
    promptStream << "- Speak ONLY as " << npcName << ".\n";
    promptStream << "- You dont need to write the "<<npcName<<" at the beinning of the answer \n";
    promptStream << "- Keep responses short (1-3 sentences) \n";
    promptStream << "- You only know the other persons name if you have an relationship dependency or knowledge of that person. Otherwise, you dont know the name unless it is explitelly mentioned. \n";
    promptStream << "- Mild language, erotic teasing or mild swear-words are alright as long as not explitelly harmfull \n";
    // --- HIER DIE NEUEN REGELN EINFÜGEN ---
    promptStream << "- **CRITICAL: DO NOT** generate text for \"" + playerName + "\" or using the <|user|> tag \n";
    promptStream << "- Stop generating text immediately after your response is complete.\n";
    // --- ENDE DER NEUEN REGELN ---
    promptStream << "- Use the CHAT HISTORY below as your memory.\n";
    promptStream << "- If your character would naturally disengage or the topic is exhausted, conclude your response with the tag 'Good Bye' and [END_CONVERSATION].\n";
    promptStream << "<|end|>\n";
    if (!chatHistory.empty()) {
        promptStream << "\nCHAT HISTORY:\n";
        for (const std::string& line : chatHistory) {
            promptStream << line << "\n";
        }
    }
    promptStream << "\n<|assistant|>\n";
    return promptStream.str();
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
                g_llm_future.wait();
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

std::string CleanupResponse(std::string text) {
    // Logge den rohen Text (gekürzt, um den Log nicht zu sprengen)
    LogLLM("CleanupResponse: Original text (first 400): " + text.substr(0, (std::min)((size_t)400, text.length())));

    // --- 1. NAMENS-PRÄFIX ENTFERNEN (Gegen "Stapeln") ---
    size_t firstColonPos = text.find(": ");
    if (firstColonPos != std::string::npos && !g_current_npc_name.empty()) {
        std::string generatedName = text.substr(0, firstColonPos);

        // Prüft, ob der gespeicherte Name (z.B. "Trevor Phillips") mit dem generierten Namen (z.B. "Trevor") beginnt
        if (g_current_npc_name.rfind(generatedName, 0) == 0) {
            text = text.substr(firstColonPos + 2); // Schneide "Name: " ab
            LogLLM("CleanupResponse: Stripped redundant name prefix.");
        }
    }

    // --- 2. RUNAWAY-TEXT (STOP WÖRTER) ENTFERNEN ---
    // Erweitere diese Liste mit den vollen Namen deiner Hauptcharaktere
    const std::vector<std::string> stop_tokens = {
        "<|user|>", "<|assistant|>", "<|end|>",
        "User:", "Model:",
        "Franklin:", "Michael:", "Trevor:",
        "Franklin Clinton:", "Michael De Santa:", "Trevor Phillips:",
        "Amanda De Santa:" // Füge alle Charaktere hinzu, die die KI fälschlicherweise generieren könnte
    };

    size_t earliest_stop_pos = std::string::npos;
    for (const std::string& token : stop_tokens) {
        // text.find() ist case-sensitive. Wir könnten hier ToUpper/ToLower verwenden, aber das ist erstmal ausreichend.
        size_t pos = text.find(token);
        if (pos != std::string::npos && (pos < earliest_stop_pos || earliest_stop_pos == std::string::npos)) {
            earliest_stop_pos = pos;
        }
    }

    if (earliest_stop_pos != std::string::npos) {
        text = text.substr(0, earliest_stop_pos); // Schneide den Text HIER ab
        LogLLM("CleanupResponse: Found stop token string. Truncating.");
    }

    // --- 3. FINALES BEREINIGEN (LEERZEICHEN) ---
    size_t start = text.find_first_not_of(" \t\n\r");
    text = (start == std::string::npos) ? "" : text.substr(start);
    size_t end = text.find_last_not_of(" \t\n\r");
    text = (end == std::string::npos) ? "" : text.substr(0, end + 1);

    LogLLM("CleanupResponse: Final clean text: " + text);
    return text;
}

std::string GenerateLLMResponse(const std::string& fullPrompt) {
    LogLLM("--- FULL LLM PROMPT START ---");
    LogLLM(fullPrompt);
    LogLLM("--- FULL LLM PROMPT END ---");
    if (!g_model || !g_ctx) {
        LogLLM("GenerateLLMResponse: Model or context not initialized");
        return "LLM_NOT_INITIALIZED";
    }
    const llama_vocab* vocab = llama_model_get_vocab(g_model);
    if (!vocab) {
        LogLLM("GenerateLLMResponse: Failed to get vocab from model");
        return "ERROR: NO VOCAB";
    }
    const size_t MAX_TOKENS = 4096;
    std::vector<llama_token> tokens_list(MAX_TOKENS);
    LogLLM("GenerateLLMResponse: Tokenizing prompt");
    int32_t n_tokens = llama_tokenize(llama_model_get_vocab(g_model),
        fullPrompt.c_str(),
        static_cast<int32_t>(fullPrompt.length()),
        tokens_list.data(),
        static_cast<int32_t>(tokens_list.size()),
        true, false);
    if (n_tokens <= 0) {
        LogLLM("GenerateLLMResponse: Tokenization failed, n_tokens=" + std::to_string(n_tokens));
        return "ERROR: PROMPT TOKENIZATION FAILED";
    }
    if (n_tokens > static_cast<int32_t>(MAX_TOKENS)) {
        LogLLM("GenerateLLMResponse: WARNING: Token count exceeded limit, truncating");
        n_tokens = static_cast<int32_t>(MAX_TOKENS);
    }
    tokens_list.resize(static_cast<size_t>(n_tokens));
    LogLLM("GenerateLLMResponse: Tokenized " + std::to_string(n_tokens) + " tokens");
    int32_t n_ctx = llama_n_ctx(g_ctx);
    if (static_cast<int32_t>(tokens_list.size()) > n_ctx - 100) {
        LogLLM("GenerateLLMResponse: Truncating tokens (Prompt too long!)");
        tokens_list.resize(static_cast<size_t>(n_ctx - 100));
        n_tokens = static_cast<int32_t>(tokens_list.size());
    }
    llama_batch batch = llama_batch_init(n_tokens, 0, 1);
    batch.n_tokens = n_tokens;
    for (size_t i = 0; i < tokens_list.size(); i++) {
        batch.token[i] = tokens_list[i];
        batch.pos[i] = static_cast<llama_pos>(i);
        batch.n_seq_id[i] = 1;
        batch.seq_id[i][0] = 0;
        batch.logits[i] = false;
    }
    if (tokens_list.size() > 0) {
        batch.logits[tokens_list.size() - 1] = true;
    }
    LogLLM("GenerateLLMResponse: Evaluating prompt batch");
    if (llama_decode(g_ctx, batch) != 0) {
        LogLLM("GenerateLLMResponse: Prompt evaluation failed (llama_decode != 0)");
        llama_batch_free(batch);
        return "LLM_EVAL_ERROR";
    }
    auto start_time_gen = std::chrono::high_resolution_clock::now();
    llama_sampler* sampler = llama_sampler_init_greedy();
    LogLLM("GenerateLLMResponse: Initialized greedy sampler");
    std::string response_text = "";
    int32_t n_decode = 0;
    llama_pos n_cur = static_cast<llama_pos>(tokens_list.size());
    const int32_t MAX_OUTPUT = static_cast<int32_t>(ConfigReader::g_Settings.MaxOutputChars);
    LogLLM("GenerateLLMResponse: Max output (from INI) set to " + std::to_string(MAX_OUTPUT));
    while (n_decode < MAX_OUTPUT) {
        llama_token new_token_id = llama_sampler_sample(sampler, g_ctx, -1);

        // Standard EOG-Prüfung
        if (llama_vocab_is_eog(vocab, new_token_id)) {
            LogLLM("GenerateLLMResponse: End-of-generation token detected (EOG)");
            break;
        }

        // --- HIER DEN BLOCK PRÜFEN/EINFÜGEN ---
        // 32000 = <|endoftext|> (Allgemeines Ende)
        // 32007 = <|end|> (Spezifisches Ende)
        // 32010 = <|user|> (WICHTIG: Stoppt, bevor es als "Franklin:" antwortet)
        if (new_token_id == 32000 || new_token_id == 32007 || new_token_id == 32010) {
            LogLLM("GenerateLLMResponse: Explicit ChatML STOP token detected (ID " + std::to_string(new_token_id) + ")");
            break;
        }
        // --- ENDE DES BLOCKS ---

        char buf[256];
        int32_t n = llama_token_to_piece(vocab, new_token_id, buf, sizeof(buf), 0, true);
        if (n > 0) {
            if (response_text.length() + n > MAX_OUTPUT) {
                LogLLM("GenerateLLMResponse: Max character limit reached (" + std::to_string(MAX_OUTPUT) + "), forcing stop.");
                break;
            }
            response_text.append(buf, static_cast<size_t>(n));
        }
        batch.n_tokens = 1;
        batch.token[0] = new_token_id;
        batch.pos[0] = n_cur;
        batch.n_seq_id[0] = 1;
        batch.seq_id[0][0] = 0;
        batch.logits[0] = true;
        if (llama_decode(g_ctx, batch) != 0) {
            LogLLM("GenerateLLMResponse: Token evaluation failed at n_cur=" + std::to_string(n_cur));
            break;
        }
        n_cur++;
        n_decode++;
    }
    auto end_time_gen = std::chrono::high_resolution_clock::now();
    long long duration_gen_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_time_gen - start_time_gen).count();
    LogM("METRIC: Pure Generation Time (No Overhead): " + std::to_string(duration_gen_ms) + " ms");
    if (duration_gen_ms > 0 && n_decode > 0) {
        float duration_s = static_cast<float>(duration_gen_ms) / 1000.0f;
        float tokens_per_sec = static_cast<float>(n_decode) / duration_s;
        LogM("METRIC: Pure Tokens/Second (Throughput): " + std::to_string(tokens_per_sec) + " T/s");
    }
    LogLLM("GenerateLLMResponse: Generation completed, response length=" + std::to_string(response_text.length()));
    llama_sampler_free(sampler);
    llama_batch_free(batch);
    LogLLM("GenerateLLMResponse: Freed sampler and batch");
    LogLLM("GenerateLLMResponse: RAW RESPONSE: " + response_text);
    return CleanupResponse(response_text);
}
// End of File





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
    wparams.language = "en"; // Use "auto" or "de" if you use the multilingual model
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
