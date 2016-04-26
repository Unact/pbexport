#include <vector>
#include <fstream>
#include <algorithm>
#include <iostream>

#include "my_pb_h.h"
#include "my_w32_h.h"

using namespace std;

const int MAX_ERR_BUF_LEN = 32766;

PBException::PBException(std::string m) : mes(m) {};
PBException::~PBException() {};
const char* PBException::what() {
	return mes.c_str();
};

//------------------ Местные объявления

/*
+--------------------------------------------------------------+
I Entry Chunk (Variable Length)                                I
+-----------+------------+-------------------------------------+
I Pos.      I Type       I Information                         I
+-----------+------------+-------------------------------------+
I   1 - 4   I Char(4)    I 'ENT*'                              I
I   5 - 8   I Char(4)    I PBL version? (0400/0500/0600)       I
I   9 - 12  I Long       I Offset of first data block          I
I  13 - 16  I Long       I Objectsize (Net size of data)       I
I  17 - 20  I Long       I Unix datetime                       I
I  21 - 22  I Integer    I Length of Comment                   I
I  23 - 24  I Integer    I Length of Objectname                I
I  25 - xx  I String     I Objectname                          I
+-----------+------------+-------------------------------------+
*/

typedef struct {
	char EntryHead[4];
	char PBVersion[4];
	long DataOffset;
	long ObjectSize;
	long UnixDatetime;
	short CommentLen;
	short EntryNameLen;
	char EntryName[65536];
} ENTRY_HEADER;																// Нужен ScanEntryDates

void ScanEntryDates(string libPath, map<string, LibEntity> &voc);			// Этот ужас нужен, так как средствами API не получается получить дату модификации исходника

void WINAPI DirCallback(PPBORCA_DIRENTRY libItem, LPVOID uData);			// Нужен в ScanDirectory_pbl



//----------------------------------

std::string GetPBError(HPBORCA s) {
	char buf[MAX_ERR_BUF_LEN];

	string res("Ошибка PBORCA: ");
	PBORCA_SessionGetError(s, buf, MAX_ERR_BUF_LEN);
		
	return res.append(buf);
};

void ScanDirectory_pbl(std::string pbdir, std::map<std::string, LibEntity> &voc) {
	// Список файлов
	string pbMask(pbdir);
	pbMask.append("\\*.pbl");

	list<string> libList;
	try{
		ScanDirectory(pbMask, libList, false);
	}
	catch (DirException e) {
		cout << e.what() << endl;
		return;
	};

	// Добавим путь
	string libPath;

	// Теперь переберём библиотеки и наковыряем из них интересного
	for (list<string>::iterator i = libList.begin(); i != libList.end(); i++ ) {
		libPath = pbdir;
		libPath.append("\\").append(*i);

		ScanEntryDates(libPath, voc);
	};
};

void WINAPI DirCallback(PPBORCA_DIRENTRY libItem, LPVOID uData)
{

};

void ScanEntryDates(string libPath, map<string, LibEntity> &voc) {
	char entBegin[]="ENT*0600";
	long entBeginLen=8;
	long entDate;
	short entNameLen;
	string entName;
	
	// Поднимем файл библиотеки в память
	vector<char> buf;
	{
		ifstream f(libPath, std::ifstream::binary);
		f.seekg (0, f.end);
		buf.resize(f.tellg(), 0 );
		f.seekg (0, f.beg);

		copy(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>(), buf.begin() );
		f.close();
	}

	int i=0;
	ENTRY_HEADER *h;

	for (vector<char>::iterator p = buf.begin(); p != buf.end(); p++ ) {
		if (*p == entBegin[i]) 
			i ++;
		else {
			i=0;
			continue;
		};

		if (i == entBeginLen) {									// Нашли сущность
			h = (ENTRY_HEADER*) &( *(p-entBeginLen+1) );

			// Получим имя
			entNameLen = h->EntryNameLen;

			entName.resize(entNameLen-1, 0);
			copy(h->EntryName, h->EntryName+(entNameLen-1), entName.begin() );

			// Ищем только исходники
			int dotPos = entName.find_last_of('.');
			
			if (entName.substr(dotPos, 3) == ".sr" ) {
				// Получим дату
				entDate = h->UnixDatetime;

				string pureEntName(entName.substr(0, dotPos) );

				voc[pureEntName].libDate = entDate;
				voc[pureEntName].libDateOffset = (p-entBeginLen+1) - buf.begin() + 16;
				voc[pureEntName].libPath = libPath;
				voc[pureEntName].entName = pureEntName;
				voc[pureEntName].entType = GetPBTypeByName(entName);
			}

			i = 0;
		}
	} //for

};

void ScanDirectory_src(std::string dir, std::map<std::string, LibEntity> &voc) {
	list<string> dirList;
	string subdir;
	list<string> subdirList;

	try{
		ScanDirectory(dir + "\\*.*", dirList, true);
	}
	catch(DirException e) {
		cout << e.what() << endl;
		return;
	};
	
	// Список каталогов
	for (list<string>::iterator d=dirList.begin(); d!=dirList.end(); d++ ) {
		subdir = dir + "\\" + *d;
		subdirList.clear();
		try {
			ScanDirectory(subdir + "\\*.sr*", subdirList, false);
		}
		catch (DirException e) {
			cout << e.what() << endl;
			subdirList.clear();
		};

		// Список файлов в каталогах, соответствующих библиотекам
		for(list<string>::iterator f=subdirList.begin(); f!=subdirList.end(); f++ ) {
			transform(f->begin(), f->end(), f->begin(), tolower);	// Имя файла -- в нижний регистр

			int dotPos = f->find_last_of('.');
			string pureEntName(f->substr(0, dotPos) );

			voc[pureEntName].entName = pureEntName;
			voc[pureEntName].srcPath = subdir + "\\" + *f;
			voc[pureEntName].fileDate = GetModificationDate(voc[pureEntName].srcPath );
			voc[pureEntName].entType = GetPBTypeByName(*f);

		}; // for (subdirList)
	}; // for (dirList)
	
};

PBORCA_TYPE GetPBTypeByName(string& entName) {
	int dotPos = entName.find_last_of('.');

	if (dotPos >= 0) {
		string entExt(entName.substr(dotPos, entName.length() - dotPos));
		
		if (entExt==".srd") return PBORCA_DATAWINDOW;	// .srd  
		if (entExt==".srw") return PBORCA_WINDOW;		// .srw     
		if (entExt==".sru") return PBORCA_USEROBJECT;	// .sru
		if (entExt==".srf") return PBORCA_FUNCTION;		// .srf
		if (entExt==".srs") return PBORCA_STRUCTURE;	// .srs  
		if (entExt==".srm") return PBORCA_MENU;			// .srm 
		if (entExt==".sra") return PBORCA_APPLICATION;	// .sra
		if (entExt==".srq") return PBORCA_QUERY;		// .srq
		if (entExt==".srp") return PBORCA_PIPELINE;		// .srp
		if (entExt==".srj") return PBORCA_PROJECT;		// .srj
        if (entExt==".srx") return PBORCA_PROXYOBJECT;	// .srx
	};

	string buf("Не удалось определить тип для объекта с именем ");
	buf.append(entName);
	throw PBException(buf);

	return PBORCA_BINARY; // Недостижимо
};

string GetExtByPBType(PBORCA_TYPE t) {
	switch(t) {
		case PBORCA_APPLICATION:
			return ".sra";
		case PBORCA_DATAWINDOW:
			return ".srd"; 
    	case PBORCA_FUNCTION:
			return ".srf";  
    	case PBORCA_MENU:
			return ".srm";
    	case PBORCA_QUERY:
			return ".srq";
    	case PBORCA_STRUCTURE:
			return ".srs";     
    	case PBORCA_USEROBJECT:
			return ".sru";
    	case PBORCA_WINDOW:
			return ".srw";
    	case PBORCA_PIPELINE:
			return ".srp";
		case PBORCA_PROJECT:
			return ".srj";
        case PBORCA_PROXYOBJECT:
			return ".srx";
		default:
			return ".unknown";
	};
};

std::string GetLibName(std::string libPath) {
	int slashPos = libPath.find_last_of('\\');
	int dotPos = libPath.find_last_of('.');

	if (slashPos == string::npos ) slashPos = -1;
	if (dotPos <= slashPos+1 ) throw PBException(string("Не удалось определить имя библиотеки для файла '") + libPath + "'");

	return libPath.substr(slashPos+1, dotPos - slashPos - 1);
};

string DescribePBError(long errCode) {
	char buf[MAX_ERR_BUF_LEN];

	if (! FormatMessage( FORMAT_MESSAGE_FROM_SYSTEM, 0, errCode, 
			GetUserDefaultLangID(), buf, MAX_ERR_BUF_LEN, 0) )
	{
		TCHAR buf_num[256];
		strcpy(buf, "Ошибка. Код ошибки = " );
		strncat(buf, ltoa(errCode, buf_num, 10), MAX_ERR_BUF_LEN );
	}

	return buf; // std::string(buf)
};