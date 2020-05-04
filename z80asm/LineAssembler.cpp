// Manages the line by line translation of Assembly Language into op-codes.
// Also handles labels, definitions and other non-asm directives

// System headers - switch warnings to L3
#pragma warning(push,3)
#pragma warning(disable:4365)
#include <iostream>
#include <regex>
// Warnings back on for my stuff
#pragma warning(pop)

#include "LineAssembler.h"

LineAssembler::LineAssembler(SymbolManager &sm) : SymbolMgr(sm)
{
    CurrentAddress = 0;
    ResetFlags();
}

void LineAssembler::ResetFlags()
{
    OriginStatement = false;
    OriginAddress = 0;

    OpCode = false;
}

void LineAssembler::CleanInput()
{
    // replace all whitespace characters with space
    Line = std::regex_replace(Line, std::regex("\\s"), " ");

    // Remove comments (including comments containing a ";", hence the "?"
    Line = std::regex_replace(Line, std::regex("(^.*?)(;.*$)"), "$1");

    // Convert to lower case
    std::transform(Line.begin(), Line.end(), Line.begin(),
        [](unsigned char c) { return static_cast<unsigned char>(std::tolower(c)); });

    // Extract a label, if present
    std::regex LabelRE("^ *([a-z][a-z0-9_]*):");
    std::smatch sm;
    // If we want multiple labels on a line, e.g. "foo: bar: quux: gronk: ld a,(hl)", just change this if to a while
    if (std::regex_search(Line, sm, LabelRE)) {
        SymbolMgr.AddLabel(sm[1].str(), CurrentAddress);
        Line = sm.suffix();
    }

    // Remove leading and trailing space, and compress enclosed spaces to one (e.g. "  ld  a,1  " -> "ld a,1"
    Line = std::regex_replace(Line, std::regex("^ +| +$|( ) +"), "$1");
}

void LineAssembler::Translate(std::string line)
{
    std::cout << "\nTranslate line from file: [" << line << "]\n";

    Line = line;
    ResetFlags();
    CleanInput();
    // What's left should be an opcode and its parameters, or just an opcode, e.g. "ld a,(ix + 2)", "nop", "rr a"

    std::regex Org("^org (.*)$");
    std::regex OneWord("^[a-z]+$");
    std::regex MultiPart("^([a-z]+) (.*)$");
    std::regex Equ("^([a-z][a-z0-9]*) equ (.*)$");
    std::smatch sm;

    if (Line.empty()) {
        std::cout << "Ignore blank line\n";
    }
    // Check for an ORG statement
    else if (std::regex_search(Line, sm, Org)) {
        std::cout << "ORG statement, parameter(s) << [" << sm[1].str() << "]\n";
    }
    // Check for an EQU statement
    else if (std::regex_search(Line, sm, Equ)) {
        std::cout << "Got definition [" << sm[1].str() << "] equ [" << sm[2].str() << "]\n";
    }
    // Check for a single word
    else if (std::regex_search(Line, OneWord)) {
        // In our first test program we haven't got any so we'll leave this blank for now
        std::cout << "Match single word (shouldn't have any yet)\n";
    }
    else if (std::regex_search(Line, sm, MultiPart)) {
        std::cout << "Got multipart statement: ";
        bool first = true;
        for (auto m : sm) {
            if (!first) {
                std::cout << "[" << m.str() << "] ";
            }
            first = false;
        }
        std::cout << std::endl;
    }
    else {
        std::cout << "Got no matches! [" << Line << "]\n";
    }
}
