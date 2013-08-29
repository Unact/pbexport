#pragma once

#include <string>
#include <exception>
#include <map>
#include <Windows.h>
#include "pborca.h"

class LibEntity {
public:
	std::string entName;			// ��� �������
	std::string entCode;			// �������� ���
	std::string entComment;			// �����������
	PBORCA_TYPE entType;			// ��� �������

	std::string entCode_backup;		// ����������� ��� ��������� ������� �������� ����� ����������
	std::string entComment_backup;	//

	int compileRes;					// 0, ���� ������ �������� � ����������

	std::string libPath;			// ���� � ����������, � ������� ���������� ������
	long libDate;					// ���� ����������� ��������� � ���������� (UNIX-������)
	long libDateOffset;				// �������� 4-�������� ���� ��������� � pbl-�����

	std::string srcPath;			// ���� � ����� � �������� �����
	long fileDate;					// ���� ����������� ����� � �������� ����� (UNIX-������)

	LibEntity() : compileRes(0), libDate(0), fileDate(0), entType(PBORCA_BINARY), libDateOffset(0) {}; // ������, ��� PBORCA_BINARY -- ��� �� ������ ��������, ����� ����� ����������� �������
};

// ���������� "���-�� ��������� �� ����� ������ � ������������ PB"
class PBException: public std::exception {
		std::string mes;
public:
	PBException(std::string m);
	const char* what();
	~PBException();
};

// ����� �������� ������ ������ �����������
class PBSessionHandle {
	HPBORCA _h;
public:
	PBSessionHandle(HPBORCA init_h) : _h(init_h) {};
	~PBSessionHandle() { if (_h) PBORCA_SessionClose(_h); };
	HPBORCA h() { return _h; };
};

std::string DescribePBError(long errCode);	// �������� ������ � �����

void ScanDirectory_pbl(std::string dir, std::map<std::string, LibEntity> &voc);	// �������� � std::cout
void ScanDirectory_src(std::string dir, std::map<std::string, LibEntity> &voc);	//

std::string GetLibName(std::string libPath);
PBORCA_TYPE GetPBTypeByName(std::string& entName);	// ����� ������ PBException
std::string GetExtByPBType(PBORCA_TYPE t);
std::string GetPBError(HPBORCA s);