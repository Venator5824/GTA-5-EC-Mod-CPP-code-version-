#include "OptChatMem.h"
#include "main.h" // Zugriff auf g_model, ConfigReader
#include "AbstractCalls.h" // Für Time, Logging
#include <dxgi.h> // VRAM Check
#include <thread>
#include <chrono>
#include <sstream>

// Globale Verwaltung
static std::future<std::string> g_optimizationFuture;
static bool g_isOptimizing = false;
static int g_linesBeingSummarized = 0; // Wie viele Zeilen werden gerade verarbeitet?
static std::map<int, OptimizationProfile> g_profiles;

using namespace AbstractGame;

// ---------------------------------------------------------
// 1. VRAM CHECKER (DirectX)
// ---------------------------------------------------------
float ChatOptimizer::GetAvailableVRAM_MB() {
    // Einfache Abfrage über DXGI
    IDXGIFactory* pFactory = nullptr;
    if (FAILED(CreateDXGIFactory(__uuidof(IDXGIFactory), (void**)&pFactory))) return 0.0f;

    IDXGIAdapter* pAdapter = nullptr;
    if (FAILED(pFactory->EnumAdapters(0, &pAdapter))) { pFactory->Release(); return 0.0f; }

    DXGI_ADAPTER_DESC desc;
    pAdapter->GetDesc(&desc);

    // Das ist grob geschätzt, da Windows VRAM dynamisch verwaltet.
    // DedicatedVideoMemory ist der feste VRAM.
    float vram = (float)(desc.DedicatedVideoMemory / 1024 / 1024);

    pAdapter->Release();
    pFactory->Release();
    return vram;
}

// ---------------------------------------------------------
// 2. MAIN CHECK LOGIC
// ---------------------------------------------------------
bool ChatOptimizer::CheckAndOptimize(int convID, std::vector<std::string>& history, const std::string& npcName, const std::string& playerName) {

    // A. Läuft schon was? Dann raus hier.
    if (g_isOptimizing) return false;

    // B. Profile laden (Default oder Custom)
    int level = ConfigReader::g_Settings.Level_Optimization_Chat_Going;
    if (g_profiles.count(convID)) {
        level = g_profiles[convID].level;
    }
    if (level == 0) return false;

    // C. Trigger-Bedingungen prüfen
    size_t historySize = history.size();
    int triggerLineCount = 10; // Default: Ab 10 Zeilen

    if (level == 2) triggerLineCount = 6; // Aggressive
    if (level == 3) {
        // AUTO MODE: VRAM Check
        // Wenn wir weniger als 2GB VRAM frei haben, aggressiver werden
        // (Pseudocode Logik, da wir "Freien" Speicher schwer exakt messen können, nutzen wir Chat Länge als Proxy)
        if (historySize > 15) triggerLineCount = 10;
        else return false;
    }

    if (historySize <= triggerLineCount) return false;

    // D. Vorbereitung: Wir schneiden die alten Zeilen aus (ohne System Prompt am Anfang)
    // Wir nehmen an: Index 0 ist SystemPrompt, Index 1 ist ggf. alte Summary.
    // Wir fassen Zeile 1 bis (Größe - 4) zusammen. Die letzten 4 Zeilen bleiben frisch.

    int startIdx = 1;
    int endIdx = (int)historySize - 4; // Behalte die letzten 4 Interaktionen frisch

    if (endIdx <= startIdx) return false; // Nicht genug Material

    std::vector<std::string> chunkToSummarize;
    for (int i = startIdx; i < endIdx; i++) {
        chunkToSummarize.push_back(history[i]);
    }

    g_linesBeingSummarized = (endIdx - startIdx);
    g_isOptimizing = true;

    // E. Start Async Worker
    int throttle = (level == 1) ? 20 : 100; // Level 1 = Langsam (Hintergrund), Level 2 = Schnell

    g_optimizationFuture = std::async(std::launch::async,
        &ChatOptimizer::BackgroundSummarizerTask,
        chunkToSummarize, npcName, playerName, throttle
    );

    Log("ChatOptimizer: Background task started for " + std::to_string(g_linesBeingSummarized) + " lines.");
    return true;
}

// ---------------------------------------------------------
// 3. BACKGROUND WORKER (Der Ghost Writer)
// ---------------------------------------------------------
#include "OptChatMem.h"
#include "main.h" 
#include "AbstractCalls.h" 
#include "llama.h" 
#include "llama-sampling.h" // Für Sampling-Funktionen
#include <thread>
#include <chrono>
#include <sstream>
#include <dxgi.h> 

using namespace AbstractGame;

// ---------------------------------------------------------
// 3. BACKGROUND WORKER (Der Ghost Writer) - FINAL IMPLEMENTIERUNG
// ---------------------------------------------------------
#include "OptChatMem.h"
#include "main.h" 
#include "AbstractCalls.h" 
#include "llama.h" 
#include "llama-sampling.h" // Für Sampling-Funktionen
#include <thread>
#include <chrono>
#include <sstream>
#include <dxgi.h> 

using namespace AbstractGame;

// ---------------------------------------------------------
// 3. BACKGROUND WORKER (Der Ghost Writer) - FINAL IMPLEMENTIERUNG
// ---------------------------------------------------------
std::string ChatOptimizer::BackgroundSummarizerTask(std::vector<std::string> lines, std::string npcName, std::string playerName, int throttleSpeed) {

    // --- 0. Initialisierung & Safety Checks ---
    if (!g_model) return "ERROR: Model not loaded";

    const llama_vocab* vocab = llama_model_get_vocab(g_model);
    if (!vocab) return "ERROR: Vocab missing";
    int32_t n_vocab = llama_n_vocab(vocab);

    // 1. PROMPT BUILDING (Unverändert)
    std::stringstream ss;
    ss << "<|system|>\n";
    ss << "Task: Compress the following dialogue log into a concise narrative memory.\n";
    ss << "Format: '[Summary: " << npcName << " and " << playerName << " discussed X...]'\n";
    ss << "<|end|>\n<|user|>\n";
    for (const auto& line : lines) { ss << line << "\n"; }
    ss << "<|end|>\n<|assistant|>\n";
    std::string prompt = ss.str();

    // 2. TEMPORÄREN KONTEXT ERSTELLEN (Der Schlüssel zur Parallelität)
    llama_context_params ctx_params = llama_context_default_params();
    ctx_params.n_ctx = 1024;
    ctx_params.n_batch = 512;
    // GPU-Layers setzen, falls VRAM bevorzugt wird (aus der globalen Config)
    if (ConfigReader::g_Settings.USE_VRAM_PREFERED) {
        ctx_params.n_gpu_layers = -1;
    }

    llama_context* ctx_sum = llama_new_context_with_model(g_model, ctx_params);
    if (!ctx_sum) return "ERROR: Context creation failed";

    // 3. TOKENISIERUNG
    std::vector<llama_token> tokens_list(ctx_params.n_ctx);
    int32_t n_tokens = llama_tokenize(vocab, prompt.c_str(), (int32_t)prompt.length(), tokens_list.data(), (int32_t)tokens_list.size(), true, false);
    if (n_tokens <= 0) { llama_free(ctx_sum); return "TOKENIZATION_FAILED"; }
    tokens_list.resize(n_tokens);

    // 4. GENERATION LOOP (Mit Throttling)
    llama_batch batch = llama_batch_init(1024, 0, 1);
    std::string summaryResult = "";

    // A. Prompt Evaluierung (Context-Prefill)
    batch.n_tokens = n_tokens;
    for (int i = 0; i < batch.n_tokens; i++) {
        batch.token[i] = tokens_list[i];
        batch.pos[i] = i;
        batch.n_seq_id[i] = 1;
        batch.seq_id[i][0] = 0;
        batch.logits[i] = (i == batch.n_tokens - 1); // Logits nur für das letzte Token
    }
    if (llama_decode(ctx_sum, batch) != 0) {
        llama_batch_free(batch);
        llama_free(ctx_sum);
        return "ERROR: Context decode failed";
    }

    int32_t n_cur = n_tokens;
    int32_t n_decode = 0;

    // B. Drosselzeit berechnen (Millisekunden pro Token)
    int sleepMs = (throttleSpeed > 0) ? (1000 / throttleSpeed) : 0;

    // C. Generierungsschleife
    for (int i = 0; n_decode < 100; i++) { // Max 100 Tokens für die Summary

        // --- 1. THROTTLING (WICHTIG!) ---
        if (sleepMs > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(sleepMs));
        }

        // 2. SAMPLING (Wiederverwendung deiner Logik)
        float* logits = llama_get_logits_ith(ctx_sum, batch.n_tokens - 1);
        llama_token new_token_id = llama_sample_token_greedy(ctx_sum, NULL); // Oder deine Manuelle Sample-Funktion

        // 3. STOPP-CHECK
        if (llama_token_is_eog(g_model, new_token_id)) break;
        // Spezifische Stop-Tokens müssten hier geprüft werden, z.B. "[SUMMARY_END]"

        // 4. CONVERT & APPEND
        char buf[256] = { 0 };
        int n = llama_token_to_piece(vocab, new_token_id, buf, 256, 0, true);
        if (n > 0) summaryResult += std::string(buf, n);

        // 5. NÄCHSTER DECODE (Cleanup der Batch und Hinzufügen des neuen Tokens)
        llama_batch_clear(&batch);
        llama_batch_add(&batch, new_token_id, n_cur, { 0 }, true);

        if (llama_decode(ctx_sum, batch) != 0) break;

        n_cur++;
        n_decode++;
    }

    // --- 5. AUFRÄUMEN ---
    llama_batch_free(batch);
    llama_free(ctx_sum);

    return summaryResult;
}

// ---------------------------------------------------------
// 1. VRAM CHECKER (DirectX) - Korrigierter Rückgabewert
// ---------------------------------------------------------
float ChatOptimizer::GetAvailableVRAM_MB() {
    // Diese Funktion bleibt als Platzhalter, wird aber in der ModMain umgangen.
    // Sie wird nicht aktiv zur Entscheidungsfindung im Code genutzt.
    // [UNVERÄNDERT]
    IDXGIFactory* pFactory = nullptr;
    if (FAILED(CreateDXGIFactory(__uuidof(IDXGIFactory), (void**)&pFactory))) return 0.0f;    // ...
    return 0.0f;
}

// ---------------------------------------------------------
// 2. MAIN CHECK LOGIC - ANPASSUNG (Level 3 VRAM Entscheidung)
// ---------------------------------------------------------
bool ChatOptimizer::CheckAndOptimize(int convID, std::vector<std::string>& history, const std::string& npcName, const std::string& playerName) {
    // ...
    int level = ConfigReader::g_Settings.Level_Optimization_Chat_Going;
    // ...
    if (level == 3) {
        // HIER MUSS DER VRAM CHECK STATTFINDEN, DEN DU IN MODMAIN MACHST
        // Da ModMain den VRAM checkt und das Level VORHER setzt, müssen wir hier nicht mehr prüfen.
        // Wir verlassen uns auf das gesetzte Level in ModMain.
    }
    // ...
    // [REST UNVERÄNDERT]
}







//_______________
// ---------------------------------------------------------
// 1. VRAM CHECKER (DirectX) - Korrigierter Rückgabewert
// ---------------------------------------------------------
float ChatOptimizer::GetAvailableVRAM_MB() {
    // Diese Funktion bleibt als Platzhalter, wird aber in der ModMain umgangen.
    // Sie wird nicht aktiv zur Entscheidungsfindung im Code genutzt.
    // [UNVERÄNDERT]
    IDXGIFactory* pFactory = nullptr;
    if (FAILED(CreateDXGIFactory(__uuidof(IDXGIFactory), (void**)&pFactory))) return 0.0f;
    // ...
    return 0.0f;
}

// ---------------------------------------------------------
// 2. MAIN CHECK LOGIC - ANPASSUNG (Level 3 VRAM Entscheidung)
// ---------------------------------------------------------
bool ChatOptimizer::CheckAndOptimize(int convID, std::vector<std::string>& history, const std::string& npcName, const std::string& playerName) {
    // ...
    int level = ConfigReader::g_Settings.Level_Optimization_Chat_Going;
    // ...
    if (level == 3) {
        // HIER MUSS DER VRAM CHECK STATTFINDEN, DEN DU IN MODMAIN MACHST
        // Da ModMain den VRAM checkt und das Level VORHER setzt, müssen wir hier nicht mehr prüfen.
        // Wir verlassen uns auf das gesetzte Level in ModMain.
    }
    // ...
    // [REST UNVERÄNDERT]
}

// ---------------------------------------------------------
// 4. APPLY RESULT (Integration)
// ---------------------------------------------------------
void ChatOptimizer::ApplyPendingOptimizations(std::vector<std::string>& history) {
    if (!g_isOptimizing) return;

    // Check if finished (non-blocking)
    if (g_optimizationFuture.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {

        std::string summary = g_optimizationFuture.get();
        g_isOptimizing = false;

        if (summary.empty()) return; // Fail

        Log("ChatOptimizer: Applying summary. Replacing " + std::to_string(g_linesBeingSummarized) + " lines.");

        // MODIFY HISTORY
        // 1. System Prompt behalten (Index 0)
        // 2. Alte Zeilen (Index 1 bis X) löschen
        // 3. Neue Summary an Index 1 einfügen

        int removeCount = g_linesBeingSummarized;
        // Safety Check: Hat sich die History verändert während wir gerechnet haben?
        // Da wir nur "alte" Zeilen (Index 1..) bearbeiten und neue hinten angehängt werden,
        // sollte Index 1 immer noch der start der alten Conversation sein.

        if (history.size() > removeCount + 1) {
            auto startIt = history.begin() + 1;
            auto endIt = startIt + removeCount;

            // Lösche die alten Zeilen
            history.erase(startIt, endIt);

            // Füge Summary ein
            history.insert(history.begin() + 1, "<|system|>\n" + summary + "\n<|end|>");
        }
    }
}

void ChatOptimizer::SetConversationProfile(int conversationID, int level) {
    g_profiles[conversationID].level = level;
    g_profiles[conversationID].isActive = true;
}