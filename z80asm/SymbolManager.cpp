// System headers - switch warnings to L3
#pragma warning(push,3)
#pragma warning(disable:4365)
#include <algorithm>
#include <tuple>
#include <iostream>
// Warnings back on for my stuff
#pragma warning(pop)

#include "SymbolManager.h"

const int ValueUndefined = 0xdeadbeef;

// Get the label's value. The value is only valid if the label is present and defined.
bool SymbolManager::GetValue(std::string name, int& value)
{
    bool found = false;
    bool defined = false;
    value = ValueUndefined; // Let's not leave value completely undefined; this value should stick out if we decide to use an undefined value.
    auto it = std::find_if(Labels.begin(), Labels.end(), [&](const Label_t& label) {
        return label.Name == name;
        });
    if (it != Labels.end()) {
        found = true;
        defined = it->Defined;
        if (defined) {
            value = it->Value;
        }
    }
    return !(found && defined); // inverting for the convention 0==OK
}

// Add a defined symbol to the list. Returns false if the symbol is present and has a value. Returns true if the label wasn't already present or was present and undefined.
// Or should we follow the return value convention 0=OK?
bool SymbolManager::AddLabel(std::string name, int value)
{
    bool found = false;
    bool defined = false;
    auto it = std::find_if(Labels.begin(), Labels.end(), [&](const Label_t& label) {
        return label.Name == name;
        });
    if (it != Labels.end()) {
        found = true;
        defined = it->Defined;
    }
    // If not found, add the label and value to the list
    // typedef std::tuple<std::string, bool, int> Label_t;
    if (!found) {
        std::cout << "SymbolManager adding label [" << name << "] with value [" << value << "]\n";
        Labels.push_back({ name,true,value });
        return false;
    }
    else if (!defined) {
        // Found and not defined
        std::cout << "SymbolManager changing label [" << name << "] from undefined to value [" << value << "]\n";
        it->Defined = true;
        it->Value = value;
        return false;
    }
    else {
        // Found and defined - we don't support changing a label's value
        std::cout << "SymbolManager not changing label [" << name << "]; this is an error\n";
        return true;
    }
}

// Add an undefined symbol to the list. Returns false if the symbol is already present, whether or not it has a value.
bool SymbolManager::AddLabel(std::string name)
{
    bool found = false;
    bool defined = false;
    auto it = std::find_if(Labels.begin(), Labels.end(), [&](const Label_t& label) {
        return label.Name == name;
        });
    if (it != Labels.end()) {
        found = true;
        defined = it->Defined;
    }
    // If not found, add the label to the list as undefined. Otherwise return TRUE - we don't support duplicate labels or changing a label with a value to one without.
    if (found) {
        std::cout << "SymbolManager not adding label [" << name << "]; this is an error\n";
        return true;
    }
    else {
        std::cout << "SymbolManager adding undefined label [" << name << "]\n";
        Labels.push_back({ name,false,ValueUndefined });
        return false;
    }
}

std::vector<std::string> SymbolManager::GetLabelNames()
{
    std::vector<std::string> ret;
    for (auto label : Labels) {
        ret.push_back(label.Name);
    }
    return ret;
}
