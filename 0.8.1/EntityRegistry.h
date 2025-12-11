#pragma once
#include "AbstractTypes.h"
#include <string>
#include <vector>

// The "Soul" of an NPC. Stored in RAM.
struct EntityData {
    AbstractTypes::PersistID id = 0;
    AbstractTypes::GameHandle handle = 0; // 0 if currently despawned
    bool isRegistered = false;

    // --- PERSISTENCE ---
    // TRUE = Hero (Saved forever). FALSE = Extra (Deleted on despawn).
    bool isPersistent = false;

    // --- BASE IDENTITY ---
    std::string defaultName;
    std::string defaultGender;
    uint32_t modelHash = 0;

    // --- SANDBOX OVERRIDES ---
    std::string overrideName;
    std::string overrideGender;
    std::string overrideGroup;

    // --- DYNAMIC CONTENT ---
    std::string dynamicGoal;
    std::string customKnowledge;
};

class EntityRegistry {
public:
    // Global Setting: Should we force random NPCs to remember things? (Default: false)
    static bool s_KeepGenericMemory;

    // --- LIFECYCLE ---
    static AbstractTypes::PersistID RegisterNPC(AbstractTypes::GameHandle handle);
    static void OnEntityRemoved(AbstractTypes::GameHandle handle);

    // --- GETTERS ---
    static EntityData GetData(AbstractTypes::PersistID id);
    static AbstractTypes::PersistID GetIDFromHandle(AbstractTypes::GameHandle handle);
    static bool HasEntity(AbstractTypes::PersistID id);

    // --- SETTERS ---
    static void SetIdentity(AbstractTypes::PersistID id, const std::string& name, const std::string& gender = "");
    static void SetGoal(AbstractTypes::PersistID id, const std::string& goal);
    static void SetCustomKnowledge(AbstractTypes::PersistID id, const std::string& knowledge);
    static void AppendMemory(AbstractTypes::PersistID id, const std::string& fact);
};