#pragma once

// System headers - switch warnings to L3
#pragma warning(push,3)
#pragma warning(disable:4365)
#include <vector>
#include <string>
// Warnings back on for my stuff
#pragma warning(pop)
#pragma warning(disable:4820) // don't care about padding byte warnings

// Label type. If the bool is false then the number in the int is not valid
typedef struct
{ 
    std::string Name; 
    bool Defined; 
    int Value; 
} Label_t;

class SymbolManager
{
private:
    std::vector<Label_t> Labels;

public:
    // Get the label's value. The value is only valid if the label is present and defined.
    bool GetValue(std::string name, int& value);
    // Add a defined symbol to the list. Returns false if the symbol is present and has a value. Returns true for forward references.
    bool AddLabel(std::string name, int value);
    // Add an undefined symbol to the list. Returns false if the symbol is already present, whether or not it has a value.
    bool AddLabel(std::string name);
    // Return a list of label names
    std::vector<std::string> GetLabelNames();
};
