// z80asm.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

// System headers - switch warnings to L3
#pragma warning(push,3)
#pragma warning(disable:4365)
#include <iostream>
#include <fstream>
#include <sstream>
// Warnings back on for my stuff
#pragma warning(pop)

#include "LineAssembler.h"
#include "CommandLineProcessor.h"
#include "SymbolManager.h"

using std::endl;
using std::cout;
using std::cin;

// TODO: Want a function to tokenise a line --> we're going to regular expressions

void ShowHelp(std::string ProgramName)
{
    cout << "Assembles an input file containing z80 assembly code and outputs an object file\n\n";
    cout << ProgramName << " [-i] [-h] [inputfile] [-o outputfile]\n\n";
    cout << "-i             Interactive mode: takes a single assembly line and displays the resulting assembled bytes\n";
    cout << "-h             Help: displays this text\n";
    cout << "inputfile      The file to assemble\n";
    cout << "-o outputfile  The output file\n\n";
    cout << "-i and -h take no other arguments. 'inputfile' and '-o outputfile' must be specified together or not at all\n";
}

void InteractiveMode()
{
    cout << "\nEntering interactive mode. Press Ctrl-C to exit the program\n\n";
    SymbolManager sm;
    LineAssembler la(sm);
    for (;;)
    {
        cout << "Enter line to assemble (Ctrl-C to quit) : ";
        std::string Input;
        cin >> Input; // interesting: leading and trailing spaces both stripped
        cout << "You entered [" << Input << "]\n";
//        la.Translate(Input); // but what about zero byte lines such as directives and labels?
//        if (la.OriginStatement)
//            cout << "Origin stmt, new address is " << la.OriginAddress << endl;
//        else if (la.Definition)
//            cout << "Symbol definition " << la.SymbolName << "=" << la.SymbolValue << endl;
//        else
//        {
//            // A label could be on its own on a line or with an instruction
//            if (la.NewLabel)
//                cout << "New label " << la.LabelName << " at address " << la.LabelAddress << endl;
//            if (la.NewInstruction)
//            {
//                std::string dummy = "0x01 0x02 0x03";
//                cout << "New instruction bytes (dummy): " << dummy << endl;
//            }
//        }
//        cout << "Input [" << Input << "] --> [" << bytes << "}\n";
    }
}

// Assemble inFilename and write the object code to outFilename
void Assemble(std::string inFilename, std::string outFilename)
{
    cout << "Assemble file [" << inFilename << "], output into [" << outFilename << "]\n";
    std::ifstream inFS(inFilename);
    if (!inFS.is_open())
    {
        cout << "Unable to open input file [" << inFilename << "], reason: " << GetErrnoString(errno);
        return;
    }

    std::ofstream outFS(outFilename);
    if (!outFS.is_open())
    {
        cout << "Unable to open output file [" << outFilename << "], reason: " << GetErrnoString(errno);
        inFS.close();
        return;
    }

    SymbolManager sm;
    LineAssembler la(sm); // Translates a line of assembly into the relevant bytes (also used by InteractiveMode)

    // I was planning a class called Assembler which would co-ordinate the inFS, outFS and LineAssembler.
    // But I can't pass streams into other objects - the constructors to do that are explicitly defined as deleted.
    // So there will need to be another solution.

    std::stringstream outBlock;
    std::string inLine;
    bool inAddrBlock = false;
    while (std::getline(inFS, inLine)) {
        la.Translate(inLine);
        // File structure is going to be: [address { byte+ }\n]+ SymbolTable {\n[name:(value|UNDEFINED)\n]+}\n
        // Under the new design I'm still going to need here:
        // - ORG statements for building the file structure - a new ORG will close the current address block and start a new one
        // - not labels, we'll get those from SymbolManager after this loop
        // - anything else?
        if (la.OriginStatement) {
            if (inAddrBlock) {
                cout << "Close address block\n";
                inAddrBlock = false; // so do I need this flag at all? Only if something's going in between the address blocks
            }
            else {
                cout << "Start address block at " << la.OriginAddress << endl;
                inAddrBlock = true;
            }
        }

        // Did we get an ORG statement?
        //if (la.OriginStatement) {
        //    cout << "Origin statement; new address $" << std::hex << la.OriginAddress << endl;
        //}
        // Labels count as definitions too; LineAssembler will have added either to the SymbolManager
        // - but should LineAssembler manage the SymbolManager? This file (z80asm.cpp) needs to manage the application flow but
        // -- delegate the details; if LineAssembler found a label should it 
        //if (la.Definition) {
        //    cout << "Definition: " << la.SymbolName << "= $" << std::hex << la.SymbolValue << endl;
        //}
        //if (la.OpCode) {
        //    cout << "OpCode: " << StringVectorToString(la.OpCodeBytes, " ") << endl;
        //}
    }

    std::cout << "Finished processing input file. Symbol table:\n";
    auto symTab = sm.GetLabelNames();
    for (auto s : symTab) {
        int value = 0;
        if (!sm.GetValue(s, value)) {
            cout << s << "=" << value << endl;
        }
        else {
            cout << s << "=UNDEFINED\n"; // this is ok because the RHS will be a number not another label
        }
    }
    // until EOF:
    // read line from inFS
    // assemble the line
    // write the assembled bytes to outFS
    // EOF: close both files
    // But what about resolving references? That needs to be done before we write the output file.
    //std::string inLine;
    //while (std::getline(inFS, inLine))
    //{
    //    cout << inLine << endl;
    //}

    inFS.close();
    outFS.close();
}


int main(int argc,char *argv[])
{
    std::vector<std::string> args(argv, argv + argc);
    CommandLineProcessor clp(args);
    clp.ProcessArgs();
    if (clp.InvalidArgs)
        cout << "Invalid input arguments\n\n";

    if (clp.InvalidArgs || clp.ShowHelp)
    {
        ShowHelp(args[0]);
        return 0;
    }

    if (clp.InteractiveMode)
    {
        InteractiveMode();
        return 0;
    }

    if (clp.ProcessFiles)
    {
        Assemble(clp.InputFile, clp.OutputFile);
        return 0;
    }
}
