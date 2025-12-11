#pragma once
#include <string>

// Logging
void Log(const std::string& msg);
void LogM(const std::string& msg);
void LogA(const std::string& msg);
void LogConfig(const std::string& message);

// Utilities
std::string FindLoRAFile(const std::string& root_path);
std::string WordWrap(const std::string& text, size_t limit = 50);
std::string NormalizeString(const std::string& input); // Was missing
float GetFreeVRAM_MB(); // Was missing