// z80asm.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "pch.h"
#include "LineAssembler.h"
#include "CommandLineProcessor.h"

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
    LineAssembler la;
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

    // instead of below:
    LineAssembler la; // Translates a line of assembly into the relevant bytes (also used by InteractiveMode)

    // I was planning a class called Assembler which would co-ordinate the inFS, outFS and LineAssembler.
    // But I can't pass streams into other objects - the constructors to do that are explicitly defined as deleted.
    // So there will need to be another solution.



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
