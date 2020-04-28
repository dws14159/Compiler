#pragma once

#include <vector>
#include <string>

class CommandLineProcessor
{
private:
	std::vector<std::string> args;

public:
	CommandLineProcessor(std::vector<std::string> args);
	void ProcessArgs();

	bool ShowHelp;
	bool InteractiveMode;
	bool InvalidArgs;
	bool ProcessFiles;
	std::string InputFile;
	std::string OutputFile;
};

