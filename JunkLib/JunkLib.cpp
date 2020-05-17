// JunkLib.cpp : Defines the functions for the static library.
// This library is a bunch of random stuff that will eventually get refactored or deleted.

// System headers - switch warnings to L3
#pragma warning(push,3)
#pragma warning(disable:4365)
#include <iostream>
#include <vector>
#include <windows.h>
// Warnings back on for my stuff
#pragma warning(pop)

#include "junklib.h"

// TODO: This is an example of a library function
void fnJunkLib()
{
}

std::string ByteToString(reg8_t byte)
{
    std::string ret = "";
    std::string hexChars = "0123456789abcdef";

    ret += hexChars[static_cast<reg8_t>((byte >> 4) & 0x0f)];
    ret += hexChars[static_cast<reg8_t>(byte & 0x0f)];
    return ret;
}

// vec=vector of strings; sep=separator character or string
std::string StringVectorToString(std::vector<std::string> vec, std::string sep)
{
    bool first = true;
    std::string ret;
    for (auto s : vec) {
        if (first) {
            first = false;
            ret = s;
        }
        else {
            ret += sep + s;
        }
    }
    return ret;
}

void DisplayTypedefSizes()
{
    typedef unsigned int  uint;
    typedef unsigned __int16  uint16;

    std::cout << "Size of uint is " << sizeof(uint) << std::endl; // 4
    std::cout << "Size of uint16 is " << sizeof(uint16) << std::endl; // 2
}

std::string GetErrnoString(errno_t err)
{
    LPSTR messageBuffer = nullptr;
    size_t size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL, static_cast<DWORD>(err), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&messageBuffer, 0, NULL);
    std::string message(messageBuffer, size);
    LocalFree(messageBuffer);
    return message;
}

//void StripTrailingSpace(char* t)
//{
//    int i;
//    for (i = 0; t[i]; i++)
//    {
//        if (isspace(t[i]) && t[i] != ' ')
//            t[i] = ' ';
//    }
//    i--;
//    while (isspace(t[i]) && i > 0)
//    {
//        t[i] = 0;
//        i--;
//    }
//}
