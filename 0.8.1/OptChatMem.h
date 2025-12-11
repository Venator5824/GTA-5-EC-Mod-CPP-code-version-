#pragma once
#include <vector>
#include <string>
#include <future>
#include <mutex>
#include "AbstractTypes.h"

// Define ChatID if not strictly imported from ConversationSystem.h
// This ensures compatibility with the new ID system.
typedef uint64_t ChatID;

struct OptimizationProfile {
    int level = 0; // 0=Off, 1=Light, 2=Aggressive, 3=Auto
    int tokensPerSecondLimit = 10;
    bool isActive = false;
};

class ChatOptimizer {
public:
    // Returns true if an optimization task was successfully started
    // UPDATE: Changed 'int' to 'ChatID' to match your new system
    static bool CheckAndOptimize(
        ChatID chatID,
        std::vector<std::string>& history,
        const std::string& npcName,
        const std::string& playerName
    );

    // Returns TRUE if the history was modified (summarized).
    // ModMain must check this bool and then save the history back to ConvoManager.
    static bool ApplyPendingOptimizations(std::vector<std::string>& history);

    // UPDATE: Changed 'int' to 'ChatID'
    static void SetConversationProfile(ChatID chatID, int level);

private:
    static std::string BackgroundSummarizerTask(
        std::vector<std::string> linesToSummarize,
        std::string npcName,
        std::string playerName,
        int throttleSpeed
    );

    static float GetAvailableVRAM_MB();
};