// Junk.cpp : This file contains the 'main' function. Program execution begins and ends there.
// This is a junk test program that will eventually be deleted.

// System headers - switch warnings off
#pragma warning(push,3)
#pragma warning(disable:4365)
#include <iostream>
// Warnings back on for my stuff
#pragma warning(pop)

#include "junklib.h"

int main()
{
    std::cout << "Hello World!\n";
    DisplayTypedefSizes();
}
