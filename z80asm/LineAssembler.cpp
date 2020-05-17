// Manages the line by line translation of Assembly Language into op-codes.
// Also handles labels, definitions and other non-asm directives

// System headers - switch warnings to L3
#pragma warning(push,3)
#pragma warning(disable:4365)
#include <iostream>
#include <regex>
#include <vector>
#include <functional>
// Warnings back on for my stuff
#pragma warning(pop)

#include "LineAssembler.h"

// Base class (interface) for the parser objects in the lookup table
// We'll move these out into their own files later.
class IAssemble
{
protected:
    bool IsConst(std::string str, reg8_t* val);
    bool IsReg(std::string str, reg8_t* regMask);
    bool IsLabel8(std::string str, std::string *labelVal);
    bool IsIndex(std::string str);

public:
    std::vector<std::string> ByteList;
    bool TranslationOK;
    std::string TranslateFailReason;

public:
    virtual ~IAssemble() {};
    virtual void ProcessMatches(std::smatch& sm) = 0;
    void dummyReport(std::string foo, std::smatch& sm)
    {
        std::cout << "Class " << foo << " invoked for matching strings: ";
        for (auto s : sm) {
            std::cout << "[" << s.str() << "] ";
        }
        std::cout << std::endl;
    }
};

bool IAssemble::IsConst(std::string str, reg8_t* val)
{
    // A more complex regex with more results in the smatch[] might make this lot easier
    reg8_t result = 0;

    // Check for digits-only: decimal integer
    bool isDecimal = true;
    std::cout << "IsConst check if [" << str << "] is a decimal int\n";
    for (char c : str) {
        if (c >= '0' && c <= '9') {
            result = static_cast<reg8_t>(result * 10 + c - '0');
        }
        else {
            isDecimal = false;
            break;
        }
    }
    if (isDecimal) {
        *val = result;
        return true;
    }

    // Check for $+hex digits: hex integer
    bool isHex = true;
    std::cout << "IsConst check if [" << str << "] is a hex int\n";
    bool first = true;
    for (char c : str) {
        if (first) {
            first = false;
            if (c != '$') {
                isHex = false;
                break;
            }
        }
        else {
            if (c >= '0' && c <= '9') {
                result = static_cast<reg8_t>(result * 16 + c - '0');
            }
            else if (c >= 'a' && c <= 'f') {
                result = static_cast<reg8_t>(result * 16 + 10 + c - 'a');
            }
            else {
                isHex = false;
                break;
            }
        }
    }
    if (isHex) {
        *val = result;
        return true;
    }

    return false;
}

bool IAssemble::IsLabel8(std::string str, std::string* labelVal)
{
    // Check if str is a label with an 8-bit value; if it is then set labelVal to the label.
    // If it's a forward reference set labelVal="XX" and bung the label name into SymbolManager with a size and "not defined".
    // If it's not a label just return false.
    // Does pulling the label value out of SymbolManager violate SRP?

    bool ret = false;

    return ret;
}

bool IAssemble::IsIndex(std::string str)
{
    // check for (ix / iy + n) not forgetting spaces, ixy + -n ixy + -n ixy + -n ixy + -n, consider labels too
    bool ret = false;

    return ret;
}

bool IAssemble::IsReg(std::string str, reg8_t* regMask)
{
    std::string regNames[] = { "b", "c", "d", "e", "h", "l", "(hl)", "a" };
    reg8_t i = 0;
    for (auto rn : regNames) {
        if (str == rn) {
            *regMask = i;
            return true;
        }
        i++;
    }
    return false;
}

class AsmLD : public IAssemble
{
public:
    void ProcessMatches(std::smatch& sm)
    {
        dummyReport("AsmLD",sm);
    }
};

class AsmOrg : public IAssemble
{
public:
    void ProcessMatches(std::smatch& sm)
    {
        dummyReport("AsmOrg",sm);
    }
};

class AsmEqu : public IAssemble
{
public:
    void ProcessMatches(std::smatch& sm) {
        dummyReport("AsmEqu", sm);
    }
};

class AsmALU : public IAssemble
{
    /*
+-------------+----+---+------+------------+---------------------+----------------------+
|AND r        | 4  | 1 |***P00|A0+rb       |Logical AND          |A=A&s                 |
|AND N        | 7  | 2 |      |E6 XX       |                     |                      |
|AND (HL)     | 7  | 1 |      |A6          |                     |                      |
|AND (IX+N)   | 19 | 3 |      |DD A6 XX    |                     |                      |
|AND (IY+N)   | 19 | 3 |      |FD A6 XX    |                     |                      |
+-------------+----+---+------+------------+---------------------+----------------------+
|OR r         | 4  | 1 |***P00|B0+rb       |Logical inclusive OR |A=Avs                 |
|OR N         | 7  | 2 |      |F6 XX       |                     |                      |
|OR (HL)      | 7  | 1 |      |B6          |                     |                      |
|OR (IX+N)    | 19 | 3 |      |DD B6 XX    |                     |                      |
|OR (IY+N)    | 19 | 3 |      |FD B6 XX    |                     |                      |
+-------------+----+---+------+------------+---------------------+----------------------+
|XOR r        | 4  | 1 |***P00|A8+rb       |Logical Exclusive OR |A=Axs                 |
|XOR N        | 7  | 2 |      |EE XX       |                     |                      |
|XOR (HL)     | 7  | 1 |      |AE          |                     |                      |
|XOR (IX+N)   | 19 | 3 |      |DD AE XX    |                     |                      |
|XOR (IY+N)   | 19 | 3 |      |FD AE XX    |                     |                      |
+-------------+----+---+------+------------+---------------------+----------------------+
    */
public:
    void ProcessMatches(std::smatch& sm) {
        dummyReport("AsmALU", sm);
        reg8_t op = 0x00;

        TranslationOK = true;

        // Opcode is dependent on parameters: reg set, constant or IX/IY expression
        reg8_t regMask = 0, constVal = 0;
        std::string labelVal = "";

        if (IsReg(sm[2].str(), &regMask)) {
            switch (sm[1].str()[0]) {
            case 'a': op = static_cast<reg8_t>(0xa0 | regMask); break;
            case 'o': op = static_cast<reg8_t>(0xb0 | regMask); break;
            case 'x': op = static_cast<reg8_t>(0xa8 | regMask); break;
            }
            ByteList.push_back(ByteToString(op));
        }
        else if (IsConst(sm[2], &constVal) || IsLabel8(sm[2], &labelVal)) {
            switch (sm[1].str()[0]) {
            case 'a': op = 0xe6; break;
            case 'o': op = 0xf6; break;
            case 'x': op = 0xee; break;
            }
            ByteList.push_back(ByteToString(op));
            if (labelVal == "") // labelVal is a string because it will be a number or XX
                ByteList.push_back(ByteToString(constVal));
            else
                ByteList.push_back(labelVal);
        }
        else if (IsIndex(sm[2])) {
        }
        else {
            TranslationOK = false;
            TranslateFailReason = "AsmALU unknown argument " + sm[2].str();
        }
    }
};

// Structure for mapping the regex to the lambda
typedef struct
{
    std::string RegEx;
    std::function<IAssemble* ()> GetAsm;
} Lookup_t;

static std::vector<Lookup_t> LookupTable =
{
    {"^([a-z][a-z0-9]*) equ (.*)$", []() {return new AsmEqu(); }}
    ,{"^org (.*)$", []() {return new AsmOrg(); }}
    ,{"^(and|or|xor) ([abcdehl]|\\(hl\\)|[0-9]+)$", []() {return new AsmALU(); }} // we'll add (hl) and ((IX|IY)+N) later
    // ,{"", []() {return new Asm(); }}
    // ,{"", []() {return new Asm(); }}
    // ,{"", []() {return new Asm(); }}
    // ,{"", []() {return new Asm(); }}
};


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
    std::cout << "After cleanup: [" << Line << "]\n";
    // What's left should be an opcode and its parameters, or just an opcode, e.g. "ld a,(ix + 2)", "nop", "rr a"

    //std::regex Org("^org (.*)$");
    //std::regex OneWord("^[a-z]+$");
    //std::regex MultiPart("^([a-z]+) (.*)$");
    //std::regex Equ("^([a-z][a-z0-9]*) equ (.*)$");
    //std::smatch sm;

    if (Line.empty()) {
        std::cout << "Ignore blank line\n";
    }
    else {
        std::smatch sm;
        auto x = std::find_if(LookupTable.begin(), LookupTable.end(), [&](Lookup_t& row) {return std::regex_search(Line, sm, std::regex(row.RegEx)); });
        if (x != LookupTable.end()) {
            IAssemble* ia = x->GetAsm();
            ia->ProcessMatches(sm); // This will result in a bunch of bytes (or XX's) that represents the assembled line - caller will process that
            if (ia->TranslationOK) {
                std::cout << "Translated bytes: " << StringVectorToString(ia->ByteList, " ") << std::endl;
            }
            else {
                std::cout << "Translation failed, reason: " << ia->TranslateFailReason << std::endl;
            }
            // In the case of XX's we'll need to signal "undefined (1-byte|2-byte) reference at offset X. Possible formats are: n, nn, nX, nnX, nXX, nnXX, nnXn
            // Of course if the symbol is known (by sm) then we can include the value directly as nN, nnN, nNN, nnNN, nnNn and there won't be an undefined reference anywhere
            // Also need to make sure the label size matches the context. Do we allow an 8-bit label to be used in a 16 bit context? Don't see why not. But the reverse won't work.
        }
        else {
            std::cout << "Got no matches! [" << Line << "]\n";
        }
    }

    //// Check for an ORG statement
    //else if (std::regex_search(Line, sm, Org)) {
    //    std::cout << "ORG statement, parameter(s) << [" << sm[1].str() << "]\n";
    //}
    //// Check for an EQU statement
    //else if (std::regex_search(Line, sm, Equ)) {
    //    std::cout << "Got definition [" << sm[1].str() << "] equ [" << sm[2].str() << "]\n";
    //}
    //// Check for a single word
    //else if (std::regex_search(Line, OneWord)) {
    //    // In our first test program we haven't got any so we'll leave this blank for now
    //    std::cout << "Match single word (shouldn't have any yet)\n";
    //}
    //else if (std::regex_search(Line, sm, MultiPart)) {
    //    std::cout << "Got multipart statement: ";
    //    bool first = true;
    //    for (auto m : sm) {
    //        if (!first) {
    //            std::cout << "[" << m.str() << "] ";
    //        }
    //        first = false;
    //    }
    //    std::cout << std::endl;
    //}
    //else {
    //    std::cout << "Got no matches! [" << Line << "]\n";
    //}
}
