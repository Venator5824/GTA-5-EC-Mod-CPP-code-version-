#pragma once  // <--- THIS IS CRITICAL
#include <cstdint>
//FileEnums.h
enum class ConvoState {
    IDLE,
    IN_CONVERSATION
};

enum class InputState {
    IDLE,
    WAITING_FOR_INPUT,
    RECORDING,
    TRANSCRIBING
};

enum class InferenceState {
    IDLE,
    RUNNING,
    COMPLETE
};

//EOF