#include "AbstractCalls.h"
using namespace AbstractGame;
using namespace AbstractTypes;

#include <mutex>
#include <shared_mutex>
#include <unordered_map>
#include "EntityRegistry.h"
#include "main.h"

using namespace AbstractTypes;

// --- GLOBAL STORAGE ---
// We keep these static so they are private to this file
static std::unordered_map<PersistID, EntityData> g_registry;
static std::unordered_map<GameHandle, PersistID> g_handleToID;

// Thread Safety
static std::shared_mutex g_registryMutex;
static std::mutex g_idGenMutex;
static PersistID g_nextGenericID = 0x001001;

// --- CONFIGURATION ---
bool EntityRegistry::s_KeepGenericMemory = false;

// ---------------------------------------------------------------------
// LIFECYCLE
// ---------------------------------------------------------------------

PersistID EntityRegistry::RegisterNPC(GameHandle handle) {
    if (handle == 0) return 0;

    // 1. Fast Read Check
    {
        std::shared_lock<std::shared_mutex> lock(g_registryMutex);
        if (g_handleToID.count(handle)) return g_handleToID[handle];
    }

    // 2. Write Lock
    std::unique_lock<std::shared_mutex> lock(g_registryMutex);

    // Double check inside lock to prevent race conditions
    if (g_handleToID.count(handle)) return g_handleToID[handle];

    // 3. Create New Identity
    // We use ConfigReader to get the base "Persona" from the .ini files
    NpcPersona persona = ConfigReader::GetPersona((AHandle)handle);

    PersistID newID;
    bool isPersistent = false;

    // --- LOGIC: HERO VS EXTRA ---
    if (!persona.inGameName.empty()) {
        // CASE A: HERO (Has specific name in INI, e.g., "Trevor")
        // Use a hash of the name so the ID is always the same across game sessions
        size_t nameHash = std::hash<std::string>{}(persona.inGameName);
        newID = (PersistID)nameHash;
        isPersistent = true;
    }
    else {
        // CASE B: EXTRA (Random Ped)
        // Generate a new sequential ID
        {
            std::lock_guard<std::mutex> idLock(g_idGenMutex);
            newID = g_nextGenericID++;
        }
        isPersistent = s_KeepGenericMemory;
    }

    // --- FILL DATA ---
    EntityData data;
    data.id = newID;
    data.handle = handle;
    data.isRegistered = true;
    data.isPersistent = isPersistent;

    if (!persona.inGameName.empty()) data.defaultName = persona.inGameName;
    else data.defaultName = persona.modelName; // Fallback to model name if no specific name

    data.defaultGender = persona.gender;
    data.modelHash = persona.modelHash; // Fixed member name

    // Save to Maps
    g_registry[newID] = data;
    g_handleToID[handle] = newID;

    return newID;
}

void EntityRegistry::OnEntityRemoved(GameHandle handle) {
    std::unique_lock<std::shared_mutex> lock(g_registryMutex);

    if (g_handleToID.count(handle)) {
        PersistID id = g_handleToID[handle];

        // Break physical link
        g_handleToID.erase(handle);

        // Handle Persistence
        if (g_registry.count(id)) {
            if (g_registry[id].isPersistent) {
                // Hero: Mark as "Despawned" (handle = 0) but keep data
                g_registry[id].handle = 0;
            }
            else {
                // Extra: Forget forever
                g_registry.erase(id);
            }
        }
    }
}

// ---------------------------------------------------------------------
// GETTERS
// ---------------------------------------------------------------------

EntityData EntityRegistry::GetData(PersistID id) {
    std::shared_lock<std::shared_mutex> lock(g_registryMutex);
    if (g_registry.count(id)) return g_registry[id];
    return EntityData(); // Return empty struct if not found
}

PersistID EntityRegistry::GetIDFromHandle(GameHandle handle) {
    std::shared_lock<std::shared_mutex> lock(g_registryMutex);
    if (g_handleToID.count(handle)) return g_handleToID[handle];
    return 0;
}

bool EntityRegistry::HasEntity(PersistID id) {
    std::shared_lock<std::shared_mutex> lock(g_registryMutex);
    return g_registry.count(id) > 0;
}

// ---------------------------------------------------------------------
// SETTERS
// ---------------------------------------------------------------------

void EntityRegistry::SetIdentity(PersistID id, const std::string& name, const std::string& gender) {
    std::unique_lock<std::shared_mutex> lock(g_registryMutex);
    if (g_registry.count(id)) {
        g_registry[id].overrideName = name;
        if (!gender.empty()) g_registry[id].overrideGender = gender;
    }
}

void EntityRegistry::SetGoal(PersistID id, const std::string& goal) {
    std::unique_lock<std::shared_mutex> lock(g_registryMutex);
    if (g_registry.count(id)) {
        g_registry[id].dynamicGoal = goal;
    }
}

void EntityRegistry::SetCustomKnowledge(PersistID id, const std::string& knowledge) {
    std::unique_lock<std::shared_mutex> lock(g_registryMutex);
    if (g_registry.count(id)) {
        g_registry[id].customKnowledge = knowledge;
    }
}

void EntityRegistry::AppendMemory(PersistID id, const std::string& fact) {
    std::unique_lock<std::shared_mutex> lock(g_registryMutex);
    if (g_registry.count(id)) {
        if (!g_registry[id].customKnowledge.empty()) {
            g_registry[id].customKnowledge += "\n";
        }
        g_registry[id].customKnowledge += "[MEMORY]: " + fact;
    }
}
