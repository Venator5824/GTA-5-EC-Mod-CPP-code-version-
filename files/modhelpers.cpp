// ModHelpers.cpp v1.0.14
// (Bereinigt, enthält nur noch ApplyNpcTasks)
#define _CRT_SECURE_NO_WARNINGS

#include "ModHelpers.h"
#include "natives.h"
#include "types.h"
#include "main.h" 
#include <fstream> 
#include <chrono> 
#include <iomanip> 
#include <ctime>
#include <string> 

/**
 * @brief Schreibt eine Nachricht in die Log-Datei mit Zeitstempel.
 */
void LogHelpers(const std::string& message) {
    auto now = std::chrono::system_clock::now();
    auto now_c = std::chrono::system_clock::to_time_t(now);
    std::ofstream log("kkamel.log", std::ios_base::app);
    if (log.is_open()) {
        log << "[" << std::put_time(std::localtime(&now_c), "%Y-%m-%d %H:%M:%S") << "] [ModHelpers] " << message << "\n";
        log.flush();
    }
}

void ApplyNpcTasks(Ped npc, Ped player) {
    LogHelpers("ApplyNpcTasks called for npc=" + std::to_string(npc) + ", player=" + std::to_string(player));
    if (!ENTITY::DOES_ENTITY_EXIST(npc)) {
        LogHelpers("ApplyNpcTasks: NPC does not exist");
        return;
    }

    LogHelpers("ApplyNpcTasks: Clearing NPC tasks");
    AI::CLEAR_PED_TASKS_IMMEDIATELY(npc);

    // Warte kurz, damit der Ped die Task-Löschung verarbeiten kann
    SYSTEM::WAIT(50);

    LogHelpers("ApplyNpcTasks: Setting NPC to face player");
    AI::TASK_TURN_PED_TO_FACE_ENTITY(npc, player, -1);

    SYSTEM::WAIT(50);

    LogHelpers("ApplyNpcTasks: Setting NPC to stand still");
    AI::TASK_STAND_STILL(npc, -1);

    LogHelpers("ApplyNpcTasks: Setting PED config flag 281 (Allows turning head)");
    PED::SET_PED_CONFIG_FLAG(npc, 281, TRUE);

    LogHelpers("ApplyNpcTasks completed");
}
