#pragma once

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers

#include <string>

// Type definitions
typedef unsigned __int16 addr_t; // Address type
typedef unsigned __int16 reg16_t; // 16-bit register
typedef unsigned __int8  reg8_t; // 8-bit register

void DisplayTypedefSizes();
std::string GetErrnoString(errno_t err);
//void StripTrailingSpace(char*);

