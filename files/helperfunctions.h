# pragma once
// helperfunctions.h
#include "main.h"


// config reader is kkameloader.log

/**
static void LogM(const std::string& msg);
*/

void LogA(const std::string& msg);



void Log(const std::string& msg);

void LogConfig(const std::string& message);



std::string FindLoRAFile(const std::string& root_path);

std::string WordWrap(const std::string& text, size_t limit = 50);




