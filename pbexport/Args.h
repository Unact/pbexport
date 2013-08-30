#pragma once

#include <string>

// Разборщик аргументов командной строки
class Args {
public:
	Args(int argc, char* argv[]);
	std::string pbl;
	std::string src;
	std::string appl;
	std::string backup;
	bool full;
	bool help;
	bool pbl2src;
	bool src2pbl;
	int recompCount;
};
