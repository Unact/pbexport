#include "my_w32_h.h"
#include <time.h>
#include <iostream>

using namespace std;

DirException::DirException(std::string m) : mes(m) {};
DirException::~DirException() {};
const char* DirException::what() {
	return mes.c_str();
};

class FindHandle {
	HANDLE _h;
public:
	FindHandle(HANDLE init_h) : _h(init_h) {};
	~FindHandle() { if (_h != INVALID_HANDLE_VALUE) FindClose(_h); };
	HANDLE h() { return _h; };
};

std::string DescribeW32Error(DWORD err) {
	int buf_len = 1024;
	std::string res(buf_len, 0);

	if (! FormatMessage( FORMAT_MESSAGE_FROM_SYSTEM, 0, err, 
			GetUserDefaultLangID(), const_cast<char *>(res.data()), buf_len, 0) )
	{
		TCHAR buf_num[256];
		res = "Код ошибки = ";
		res.append( _ltoa(err, buf_num, 10) );
	}

	return res;
};

void ScanDirectory(std::string findMask, std::list<std::string> &voc, bool findDir) {
	WIN32_FIND_DATA fData;
	FindHandle pbh(FindFirstFile(findMask.c_str(), &fData) );
	bool go = true;

	if (pbh.h() == INVALID_HANDLE_VALUE) {
		DWORD errCode = GetLastError();
		
		if (errCode != ERROR_FILE_NOT_FOUND) {
			std::string findErr("Не удался поиск файлов: '");
			findErr.append(findMask).append("' ошибка: ").append(DescribeW32Error(errCode) );

			throw DirException(findErr);
		};

		go = false;
	};
	
	voc.clear();
	
	while(go) {
		if (	( ( findDir && (fData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) )
					|| ( !findDir && !(fData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) )
				)
				&& strcmp(fData.cFileName, ".") 
				&& strcmp(fData.cFileName, "..") ){
			std::string buf(fData.cFileName);
			voc.push_back(buf);
		};

		if (!FindNextFile(pbh.h(), &fData) ) {
			DWORD errCode = GetLastError();
		
			if (errCode != ERROR_NO_MORE_FILES) {
				std::string findErr("Не удался поиск файлов: '");
				findErr.append(findMask).append("' ошибка: ").append(DescribeW32Error(errCode) );

				throw DirException(findErr);
			};

			go = false;
		}

	}; // while
};

long GetModificationDate(string filePath){
	FILETIME ft, ft_local;
	SYSTEMTIME st;
	HANDLE f;
	long result=0;

	f=CreateFile(filePath.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (f == INVALID_HANDLE_VALUE) {
		return 0;
	};

	if ( GetFileTime(f, NULL, NULL, &ft) ) {	// Время последнего исправления
		FileTimeToLocalFileTime(&ft, &ft_local); //	т.к. mktime нужно местное время
		if ( FileTimeToSystemTime(&ft_local, &st) ) {
			time_t t;
			tm timeinfo = {0};

			timeinfo.tm_year = st.wYear - 1900;
			timeinfo.tm_mon = st.wMonth - 1;
			timeinfo.tm_mday = st.wDay;
			timeinfo.tm_hour = st.wHour;
			timeinfo.tm_min = st.wMinute;
			timeinfo.tm_sec = st.wSecond;

			t = mktime ( &timeinfo );
			result = (long) t; // Число секунд с 1970-01-01 00:00:00 
		}
	};

	CloseHandle(f);

	return result;
};

int SetModificationDate(string filePath, long date) {
	FILETIME ft;
	SYSTEMTIME st={0};
	HANDLE f;
	int result=0;
	time_t t;
	tm* timeinfo;

	t = date;
	timeinfo = gmtime(&t);

	st.wYear = timeinfo->tm_year + 1900;
	st.wMonth = timeinfo->tm_mon + 1;
	st.wDay = timeinfo->tm_mday;
	st.wHour = timeinfo->tm_hour;
	st.wMinute = timeinfo->tm_min;
	st.wSecond = timeinfo->tm_sec;

	if (! SystemTimeToFileTime(&st, &ft) ) {
		return GetLastError();
	};

	f=CreateFile(filePath.c_str(), FILE_WRITE_ATTRIBUTES, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (f == INVALID_HANDLE_VALUE) {
		return GetLastError();
	};

	if ( ! SetFileTime(f, NULL, NULL, &ft) ) {	// Время последнего исправления
		result = GetLastError();
	};

	CloseHandle(f);

	return result;
};

void BuildPath(std::string path) {
	if (GetFileAttributes(path.c_str() ) != INVALID_FILE_ATTRIBUTES) return;	// А вдруг всё уже есть

	long slashPos=0;
	string currPath;
	
	currPath.reserve(path.length() );

	//---- Выберем начальный путь, начиная с которого проверяем существование ветки

	if (path.length() > 1) {
		if (path.substr(1, 2) == ":\\") {
			// Локальный путь с буквой диска
			slashPos = 3;
		}
	};
	if (!slashPos) {
		if (path.substr(0, 2) == "\\\\") {
			// Сетевой путь
			slashPos = 2;
		};
	};
	// Иначе считаем путь относительным, и начинаем проверять с первого символа

	//---- Идём от корня, создавая недостающие куски
	while(currPath.length() < path.length() ) {
		slashPos = path.find_first_of('\\', slashPos+1);
		if (slashPos == string::npos) slashPos = path.length();
		currPath.assign(path, 0, slashPos);			// Лень заморачиваться с отладкой append

		long attr = GetFileAttributes(currPath.c_str() );
		if (attr == INVALID_FILE_ATTRIBUTES)
			CreateDirectory(currPath.c_str(), NULL);	// Пофиг ошибки. Если не удалось создать, выясним это на этапе открытия файла
	};	
};

std::string CatalogFromPath(std::string path) {
	long slashPos = path.find_last_of('\\');

	if (slashPos == string::npos) 
		return string();
	else
		return path.substr(0, slashPos);	// Ибо первая позиция в строке: slashPos==0
};

std::string FilenameFromPath(std::string path){
	long slashPos = path.find_last_of('\\');

	if (slashPos == string::npos) 
		return path;
	else
		return path.substr(slashPos+1, path.length() - (slashPos+1));	// Ибо первая позиция в строке: slashPos==0
};

int CopyDirectory_pbl(std::string src, std::string dst) {
	int res = 0;
	std::list<std::string> dirList;
	ScanDirectory(src + "\\*.pbl", dirList);

	if (dirList.size() > 0) BuildPath(dst);

	for( std::list<std::string>::iterator i = dirList.begin(); i != dirList.end(); ++i) {
		std::string srcPath=src +"\\"+ *i;
		std::string dstPath=dst + "\\"+ *i;
		if (!CopyFile(srcPath.c_str(), dstPath.c_str(), FALSE) ) {
			int err = GetLastError();
			cout << "Не удалось скопировать '" << srcPath << "' -> '" << dstPath << "' : " << DescribeW32Error(err) << endl;
			res = 1;
		}
	};

	return res;
};