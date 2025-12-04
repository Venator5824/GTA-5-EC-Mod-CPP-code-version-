#pragma once
#include <vector>
#include <string>
#include <future>
#include <mutex>
#include "AbstractTypes.h"

// Einstellungen für eine spezifische Optimierungs-Session
struct OptimizationProfile {
    int level = 0; // 0=Off, 1=Light, 2=Aggressive, 3=Auto
    int tokensPerSecondLimit = 10; // Drosselung (Throttling)
    bool isActive = false;
};

class ChatOptimizer {
public:
    // Hauptfunktion: Wird von GameLogic.cpp aufgerufen
    // Gibt true zurück, wenn eine Optimierung gestartet wurde
    static bool CheckAndOptimize(
        int conversationID,
        std::vector<std::string>& history,
        const std::string& npcName,
        const std::string& playerName
    );

    // Prüft, ob ein Hintergrund-Job fertig ist und wendet ihn an
    static void ApplyPendingOptimizations(std::vector<std::string>& history);

    // Manuelles Tuning für Entwickler (API)
    static void SetConversationProfile(int conversationID, int level);

private:
    // Der eigentliche Worker (läuft im Thread)
    static std::string BackgroundSummarizerTask(
        std::vector<std::string> linesToSummarize,
        std::string npcName,
        std::string playerName,
        int throttleSpeed
    );

    // VRAM Check Helper
    static float GetAvailableVRAM_MB();
};