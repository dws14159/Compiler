#include "pch.h"
#include "CommandLineProcessor.h"

CommandLineProcessor::CommandLineProcessor(std::vector<std::string> args)
{
	this->args = args;
    ShowHelp = false;
    InteractiveMode = false;
    InvalidArgs = false;
    ProcessFiles = false;
}

void CommandLineProcessor::ProcessArgs()
{
    switch (args.size())
    {
    case 2:
        if (args[1] == "-h")
            ShowHelp = true;
        else if (args[1] == "-i")
            InteractiveMode = true;
        else
            InvalidArgs = true;
        break;

    case 4:
        if (args[1] == "-o") // z80asm -o outfile infile
        {
            InputFile = args[3];
            OutputFile = args[2];
            ProcessFiles = true;
        }
        else if (args[2] == "-o") // z80asm infile -o outfile
        {
            InputFile = args[1];
            OutputFile = args[3];
            ProcessFiles = true;
        }
        else
            InvalidArgs = true;

    default:
        ShowHelp = true;
        break;
    }
}
