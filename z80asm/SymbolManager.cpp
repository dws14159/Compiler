#include "SymbolManager.h"
#include <algorithm>
#include <tuple>

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
        Labels.push_back({ name,true,value });
        return false;
    }
    else if (!defined) {
        // Found and not defined
        it->Defined = true;
        it->Value = value;
        return false;
    }
    else {
        // Found and defined - we don't support changing a label's value
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
        return true; 
    }
    else {
        Labels.push_back({ name,false,ValueUndefined });
        return false;
    }
}
