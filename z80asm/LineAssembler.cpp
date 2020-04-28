// Manages the line by line translation of Assembly Language into op-codes.
// Also handles labels, definitions and other non-asm directives

// System headers - switch warnings off
#pragma warning(push,3)
#pragma warning(disable:4365)
#include <iostream>
// Warnings back on for my stuff
#pragma warning(pop)

#include "LineAssembler.h"

LineAssembler::LineAssembler()
{
    OriginStatement = false;
    OriginAddress = 0;

    Definition = false;
    SymbolValue = 0;

    OpCode = false;
}

void LineAssembler::Translate(std::string line)
{
    std::cout << "Translate line from file into assembler bytes\n";
}
