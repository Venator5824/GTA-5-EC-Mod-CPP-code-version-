#pragma once
#include "AbstractTypes.h"

// Declarations for the NPC logic in ModHelpers.cpp
void StartNpcConversationTasks(AHandle npc, AHandle player);
void UpdateNpcConversationTasks(AHandle npc, AHandle player);
void UpdateNpcMemoryJanitor(bool force_clear_all);
