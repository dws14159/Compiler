#pragma once

// System headers - switch warnings off
#pragma warning(push,3)
#pragma warning(disable:4365)
#include <vector>
#include <string>
// Warnings back on for my stuff
#pragma warning(pop)


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

