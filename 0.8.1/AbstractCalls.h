// abstractcalls.h
#pragma once
#include "AbstractTypes.h"
#include <string>
#include <vector>

// CRITICAL: Include natives.h here if you have it, otherwise forward declare
// If you don't include it, .cpp files using natives will fail.
#include "nativesheader.h"

namespace AbstractGame {
    // System
    AbstractTypes::TimeMillis GetTimeMs();
    void SystemWait(int ms);

    // Input
    bool IsKeyPressed(int keyID);

    // Entity
    AHandle GetPlayerHandle();
    bool IsEntityValid(AHandle entity);
    bool IsEntityDead(AHandle entity);
    AVec3 GetEntityPosition(AHandle entity);
    float GetDistanceBetweenEntities(AHandle e1, AHandle e2);

    // Logic
    bool IsEntityInCombat(AHandle entity);
    bool IsEntitySwimming(AHandle entity);
    bool IsEntityJumping(AHandle entity);
    bool IsEntityFleeing(AHandle entity);
    bool IsEntityInVehicle(AHandle entity);
    bool IsEntityDriver(AHandle vehicle, AHandle entity);
    AHandle GetVehicleOfEntity(AHandle entity);

    // AI
    void ClearTasks(AHandle entity);
    void TaskLookAtEntity(AHandle entity, AHandle target, int durationMs);
    void TaskStandStill(AHandle entity, int durationMs);
    void SetPedHeadTracking(AHandle entity, bool enabled);

    // World
    AHandle GetClosestPed(AVec3 center, float radius, AHandle ignoreEntity);
    AbstractTypes::ModelID GetEntityModel(AHandle entity);
    bool IsPedHuman(AHandle entity);
    bool IsPedMale(AHandle entity);

    // Environment
    uint32_t GetCurrentWeatherType();
    void GetGameTime(int& hour, int& minute);
    std::string GetZoneName(AVec3 pos);

    // UI
    void ShowSubtitle(const std::string& text, int durationMs);
    void DrawText2D(const std::string& text, float x, float y, float scale, int r, int g, int b, int a);

    // Keyboard
    void OpenKeyboard(const std::string& title, const std::string& defaultText, int maxChars);
    int UpdateKeyboardStatus();
    std::string GetKeyboardResult();
    void SetPedConfigFlag(AHandle entity, int flagId, bool value);
    bool IsEntityLivingEntity(AHandle tempTarget);
    void SetPedConfigFlag(AHandle entity, int flagId, bool value);
    bool IsGamePausedOrLoading();
}

//EOF