#pragma once

#include <string>
#include <exception>
#include <map>
#include <Windows.h>
#include "pborca.h"

class LibEntity {
public:
	std::string entName;			// Имя объекта
	std::string entCode;			// Исходный код
	std::string entComment;			// Комментарий
	PBORCA_TYPE entType;			// Тип объекта

	std::string entCode_backup;		// Понадобится при неудачной попытке загрузки новых исходников
	std::string entComment_backup;	//

	int compileRes;					// 0, если удачно загружен в библиотеку

	std::string libPath;			// Путь к библиотеке, в которой содержится объект
	long libDate;					// Дата модификации исходника в библиотеке (UNIX-формат)
	long libDateOffset;				// Смещение 4-байтовой даты исходника в pbl-файле

	std::string srcPath;			// Путь к файлу с исходным кодом
	long fileDate;					// Дата модификации файла с исходным кодом (UNIX-формат)

	LibEntity() : compileRes(0), libDate(0), fileDate(0), entType(PBORCA_BINARY), libDateOffset(0) {}; // Считаю, что PBORCA_BINARY -- нам не нужная сущность, пусть будет индикатором пустоты
};

// Исключение "что-то произошло на этапе работы с библиотеками PB"
class PBException: public std::exception {
		std::string mes;
public:
	PBException(std::string m);
	const char* what();
	~PBException();
};

// Чтобы открытая сессия всегда закрывалась
class PBSessionHandle {
	HPBORCA _h;
public:
	PBSessionHandle(HPBORCA init_h) : _h(init_h) {};
	~PBSessionHandle() { if (_h) PBORCA_SessionClose(_h); };
	HPBORCA h() { return _h; };
};

std::string DescribePBError(long errCode);	// Описание ошибки с кодом

void ScanDirectory_pbl(std::string dir, std::map<std::string, LibEntity> &voc);	// Ругаются в std::cout
void ScanDirectory_src(std::string dir, std::map<std::string, LibEntity> &voc);	//

std::string GetLibName(std::string libPath);
PBORCA_TYPE GetPBTypeByName(std::string& entName);	// Может кинуть PBException
std::string GetExtByPBType(PBORCA_TYPE t);
std::string GetPBError(HPBORCA s);