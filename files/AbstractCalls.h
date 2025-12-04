#pragma once
#include "AbstractTypes.h"
#include <string>
#include <vector>

// Aliasse für Lesbarkeit
using AHandle = AbstractTypes::GameHandle;
using AVec3 = AbstractTypes::Vec3;

namespace AbstractGame {

    // --- SYSTEM & ZEIT ---
    // Ersetzt GetTickCount64 (Windows) -> Universelle Zeit in Millisekunden
    AbstractTypes::TimeMillis GetTimeMs();

    // Ersetzt SYSTEM::WAIT
    void SystemWait(int ms);

    // --- INPUT ---
    // Ersetzt GetAsyncKeyState / IsKeyJustPressed Logik
    bool IsKeyPressed(int keyID);

    // --- ENTITY BASICS ---
    AHandle GetPlayerHandle();
    bool IsEntityValid(AHandle entity);
    bool IsEntityDead(AHandle entity);

    // --- POSITION & MATH ---
    AVec3 GetEntityPosition(AHandle entity);
    float GetDistance(AVec3 a, AVec3 b);
    // Bequemlichkeits-Funktion für ModMain
    float GetDistanceBetweenEntities(AHandle e1, AHandle e2);

    // --- LOGIK CHECKS (Main Loop Guards) ---
    bool IsEntityInCombat(AHandle entity);
    bool IsEntitySwimming(AHandle entity);
    bool IsEntityJumping(AHandle entity);
    bool IsEntityFleeing(AHandle entity);
    bool IsEntityInVehicle(AHandle entity);
    bool IsEntityDriver(AHandle vehicle, AHandle entity); // Für SafeMode Check
    AHandle GetVehicleOfEntity(AHandle entity);

    // --- AI & TASKS (Aus ModHelpers.cpp) ---
    // Ersetzt AI::CLEAR_PED_TASKS_IMMEDIATELY
    void ClearTasks(AHandle entity);

    // Ersetzt AI::TASK_TURN_PED_TO_FACE_ENTITY
    void TaskLookAtEntity(AHandle entity, AHandle target, int durationMs);

    // Ersetzt AI::TASK_STAND_STILL
    void TaskStandStill(AHandle entity, int durationMs);

    // Ersetzt PED::SET_PED_CONFIG_FLAG(npc, 281, TRUE)
    // Wir nennen es abstrakt "HeadTracking", damit UE5 weiß, was gemeint ist
    void SetPedHeadTracking(AHandle entity, bool enabled);

    // Ersetzt PED::GET_CLOSEST_PED
    AHandle GetClosestPed(AVec3 center, float radius, AHandle ignoreEntity);

    // --- UI & KEYBOARD ---
    void ShowSubtitle(const std::string& text, int durationMs);
    void DrawText2D(const std::string& text, float x, float y, float scale, int r, int g, int b, int a);

    // Keyboard Abstraktion
    void OpenKeyboard(const std::string& title, const std::string& defaultText, int maxChars);
    int UpdateKeyboardStatus(); // 0=Running, 1=OK, 2=Cancel
    std::string GetKeyboardResult();
    // Gibt den Model-Hash zurück (uint32)
    AbstractTypes::ModelID GetEntityModel(AHandle entity);

    // Prüft, ob es ein Mensch ist (vs. Tier/Auto)
    bool IsPedHuman(AHandle entity);

    // Prüft das Geschlecht (true = Male, false = Female/Other)
    bool IsPedMale(AHandle entity);

    uint32_t GetCurrentWeatherType(); // Ersetzt GAMEPLAY::_GET_CURRENT_WEATHER_TYPE
    void GetGameTime(int& hour, int& minute); // Ersetzt TIME::GET_CLOCK...
    std::string GetZoneName(AVec3 pos); // Ersetzt ZONE::GET_NAME_OF_ZONE

    uint32 (AHandle target_ped) {
        return  GET_ENTITY_MODEL(target_ped);
    }

}