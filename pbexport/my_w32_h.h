#pragma once

#include <string>
#include <list>
#include <Windows.h>

class DirException : public std::exception {
	std::string mes;
public:
	DirException(std::string m);
	const char* what();	// !!!
	~DirException();
};

long GetModificationDate(std::string filePath);
int SetModificationDate(std::string filePath, long date);
std::string DescribeW32Error(DWORD err);
void ScanDirectory(std::string findMask, std::list<std::string> &voc, bool findDir=false);
int CopyDirectory_pbl(std::string src, std::string dst);

void BuildPath(std::string path);
std::string CatalogFromPath(std::string path);
std::string FilenameFromPath(std::string path);