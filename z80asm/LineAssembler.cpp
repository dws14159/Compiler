// Manages the line by line translation of Assembly Language into op-codes.
// Also handles labels, definitions and other non-asm directives

#include "LineAssembler.h"
#include <iostream>

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
