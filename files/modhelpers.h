// ModHelpers.h v1.0.14
// (Bereinigt, enthält nur noch ApplyNpcTasks)
#pragma once

#define _CRT_SECURE_NO_WARNGINS
#include "main.h" 
#include "types.h"
#include <string>
// #include "natives.h" (Refactored)


/**
 * @brief Forces the NPC to stop current tasks and face the player for conversation.
 */
void StartNpcConversationTasks(AHandle npc, AHandle player);

void UpdateNpcConversationTasks(AHandle npc, AHandle player);




