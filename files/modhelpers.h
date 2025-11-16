// ModHelpers.h v1.0.15
// (Bereinigt, enthält nur noch ApplyNpcTasks)
#pragma once
#include "main.h" 
#include "types.h"
#include <string>
#include "natives.h"


/**
 * @brief Forces the NPC to stop current tasks and face the player for conversation.
 */
void StartNpcConversationTasks(Ped npc, Ped player);

void UpdateNpcConversationTasks(Ped npc, Ped player);
