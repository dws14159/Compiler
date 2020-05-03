#pragma once

// System headers - switch warnings to L3
#pragma warning(push,3)
#include <vector>
// Warnings back on for my stuff
#pragma warning(pop)
#pragma warning(disable:4820) // don't care about padding byte warnings

#include <junklib.h>
#include "SymbolManager.h"
#pragma warning(push)
#pragma warning(disable:4626) // assignment operator implicitly defined as deleted
#pragma warning(disable:5027) // move assignment operator implicitly defined as deleted

class LineAssembler
{
private:
    SymbolManager& sm;

public:
    LineAssembler(SymbolManager& _sm);

    // Results of translating an ORG <address> line
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

#pragma warning(pop)

