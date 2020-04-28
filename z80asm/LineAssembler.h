#pragma once
#include "pch.h"

#pragma warning(disable:4820) // Don't need to know about padding bytes

class LineAssembler
{
public:
    LineAssembler();

    // Results of translating an ORG <adddress> line
    bool OriginStatement;
    addr_t OriginAddress;

    // Symbol definition SymbolName EQU <value>
    bool Definition;
    std::string SymbolName;
    addr_t SymbolValue; // TODO different type for symbol values?

    // Op-code
    bool OpCode;
    std::vector<std::string> OpCodeBytes; // List of strings because a forward reference e.g. ld a,UnknownLabel will be 3a xx xx

    void Translate(std::string line);
};

