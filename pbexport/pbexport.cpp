// pbexport.cpp: определяет точку входа для консольного приложения.
//

#include "stdafx.h"

#include "Args.h"

enum {RES_OK=0, RES_BADARGUMENTS=1, RES_CANNOTLOADORCALIBRARY=2, RES_ERROR=3, RES_EXCEPTION=4};

#include <Windows.h>
#include <iostream>
#include <fstream>
#include <string>
#include <list>
#include <map>
#include <vector>
#include <clocale>

#include <exception>
#include <iterator>

#include "my_w32_h.h"
#include "my_pb_h.h"

#include <time.h>

using namespace std;

const int MAX_PBCODE_BUFFER = 1024*1024*2;

int DoImport(Args& args);
int DoExport(Args& args);

void CALLBACK PBCompErr( PBORCA_COMPERR* err, LPVOID uData);	// Для вызова PBORCA_CompileEntryImportList

int main (int argc, char* argv[]){
	setlocale(LC_CTYPE, "Russian");	// Кодовая страница по умолчанию

	Args args(argc, argv);

	if (args.help) {
		cout << "Экспорт:" << endl <<
			"  --pbl2src" << endl <<
			"  --pbl-<каталог с библиотеками>" << endl <<
			"  --src-<каталог для исходников>" << endl <<
			"  [--full]" << endl <<
			endl <<
			"Импорт:" << endl <<
			"  --src2pbl" << endl <<
			"  --pbl-<каталог с библиотеками>" << endl <<
			"  --src-<каталог c исходниками>" << endl <<
			"  --backup-<каталог для резервной копии библиотек>" << endl <<
			"  --appl-<имя приложения>" << endl <<
			"  [--recomp-<сколько раз перекомпилировать объекты, не скомпилированные сразу>]" << endl <<
			"  [--full]" << std::endl;
		return RES_OK;
	};

	if ( (args.pbl2src && args.src2pbl)  || (!args.pbl2src && !args.src2pbl) ) {
		cout << "Должен быть определён один и только один ключ из пары {--src2pbl, --pbl2src}" << endl;
		return RES_BADARGUMENTS;
	};

	if (args.pbl.empty() ) {
		cout << "Не задан каталог с файлами PBL" << endl;
		return RES_BADARGUMENTS;
	};	

	if (args.src.empty() ) {
		cout << "Не задан каталог с исходными файлами" << endl;
		return RES_BADARGUMENTS;
	};

	if (args.src2pbl && args.appl.empty() ) {
		cout << "Необходимо задать имя приложения" << endl;
		return RES_BADARGUMENTS;
	};

	if (args.src2pbl && args.backup.empty() ) {
		cout << "Необходимо указать каталог для резервного копирования" << endl;
		return RES_BADARGUMENTS;
	};

	HINSTANCE orcalib, hInst;
	hInst = GetModuleHandle(NULL);

	orcalib = LoadLibrary("pborc90.dll" );
	if (orcalib == NULL) {
		int errCode = GetLastError();
		cout << "Не удалось загрузить библиотеку pborc90.dll" << endl;
		
		return RES_CANNOTLOADORCALIBRARY;
	};

	int res = 0;

	try {
		if (args.pbl2src) res = DoExport(args);
		if (args.src2pbl) res = DoImport(args);		
	}
	catch (PBException e) {
		cout << "Неприятность: " << e.what() << endl;
		return RES_EXCEPTION;
	}
	catch (exception e) {
		cout << "Неизвестная неприятность: " << e.what() << endl;
		return RES_EXCEPTION;
	}

	if (res)
		return RES_ERROR;
	else
		return RES_OK;
};

// -----------------------------------

void CALLBACK PBCompErr( PBORCA_COMPERR* err, LPVOID uData) {
	int* pCompRes = reinterpret_cast<int*> (uData);
	*pCompRes = 1;

	cout << "Ошибка компилирования: ";
	if (err->iLevel) cout << " уровень:" << err->iLevel;
	if (err->iLineNumber) cout << " строка:" << err->iLineNumber;
	if (err->iColumnNumber) cout << " столбец:" << err->iColumnNumber;
	if (err->lpszMessageNumber != NULL) cout << err->lpszMessageNumber << " ";
	if (err->lpszMessageText != NULL) cout << err->lpszMessageText;
	cout << endl;
}

int DoExport(Args& args){
	int res = 0;

	map<string, LibEntity> voc;

	ScanDirectory_pbl(args.pbl, voc);
	BuildPath(args.src);
	ScanDirectory_src(args.src, voc);

	// Отберём тех, кого нужно экспортировать:
	list<map<string, LibEntity>::iterator> needsExport; // Список указателей на элементы voc, которые нужно экспортировать

	if (args.full) { 
		// отберём всех, упомянутых в библиотеках
		for (map<string, LibEntity>::iterator i = voc.begin(); i != voc.end(); ++i )
			if ( ! i->second.libPath.empty() ) needsExport.push_back(i);
	} else {
		// отберём всех, для которых дата в библиотеке больше даты файла в каталоге исходников (или такого файла нет)
		for (map<string, LibEntity>::iterator i = voc.begin(); i != voc.end(); ++i )
			if ( i->second.libDate > i->second.fileDate ) needsExport.push_back(i); // Если файла нет -- из логики построения (voc) его .fileDate должно быть 0
	};

	// Всем, у кого нет исходного файла, нужно сформировать путь к нему
	for ( list<map<string, LibEntity>::iterator>::iterator ii = needsExport.begin(); ii != needsExport.end(); ++ii )
		if ( (*ii)->second.srcPath.empty() ) {
			(*ii)->second.srcPath = args.src + "\\" + GetLibName((*ii)->second.libPath) + "\\" + (*ii)->second.entName + GetExtByPBType((*ii)->second.entType) ;
		};

	// И экспортируем
	cout << "------------- Экспорт: --------------" << endl;
	for ( list<map<string, LibEntity>::iterator>::iterator ii = needsExport.begin(); ii != needsExport.end(); ++ii )
		cout << (*ii)->second.srcPath << endl;
	cout << "------------------------------------" << endl;

	// Получим исходники из PB
	PBSessionHandle s(PBORCA_SessionOpen() );
	if (!s.h() ) {		
		throw PBException("Не удалось открыть сессию PBORCA" );
	};

	string codeBuf(MAX_PBCODE_BUFFER, 0);	// Там может быть много -- не хочу создавать буфер в стеке
	PBORCA_ENTRYINFO commentBuf;
	ofstream f;
	long err;
	for ( list<map<string, LibEntity>::iterator>::iterator ii = needsExport.begin(); ii != needsExport.end(); ++ii ) {
		BuildPath( CatalogFromPath((*ii)->second.srcPath) );

		//ofstream f((*ii)->second.srcPath, std::ios_base::binary | std::ios_base::out);	// см. ios_base::failure,  f.failbit
		f.open((*ii)->second.srcPath, std::ios_base::binary | std::ios_base::out);
		if (f.fail() ) {
			cout << "Не удалось открыть на запись файл '" << (*ii)->second.srcPath << "'" << endl;
			res = 1;
			continue;
		}

		// Заголовок
		f.write("$PBExportHeader$", 16);
		f.write((*ii)->second.entName.data(), (*ii)->second.entName.length() );
		f.write(GetExtByPBType((*ii)->second.entType).data(), 4);
		f.write("\x0D\x0A", 2);

		// Комментарий
		if (err = PBORCA_LibraryEntryInformation(s.h(), 
			const_cast<char*>((*ii)->second.libPath.c_str() ), 
			const_cast<char*>((*ii)->second.entName.c_str() ),
			(*ii)->second.entType, &commentBuf ) ) {
			cout << "Не удалось получить информацию о '" << (*ii)->second.libPath << "'." << (*ii)->second.entName << " :" << DescribePBError(err);
			res = 1;
		} else {
			if (commentBuf.szComments[0] ) {	// Комментарий есть
				f.write("$PBExportComments$", 18);
				f.write(commentBuf.szComments, strlen(commentBuf.szComments) );
				f.write("\x0D\x0A", 2);
			};
		};

		// Код
		if (err = PBORCA_LibraryEntryExport(s.h(), const_cast<char*>((*ii)->second.libPath.c_str() ), 
			const_cast<char*>((*ii)->second.entName.c_str() ),
			(*ii)->second.entType,
			const_cast<char*>(codeBuf.data() ), MAX_PBCODE_BUFFER) ) {
				cout << "Не удалось получить информацию о '" << (*ii)->second.libPath << "'." << (*ii)->second.entName << " :" << DescribePBError(err);
				res = 1;
		} 
		else
			f.write(codeBuf.data(), strlen(codeBuf.data()) );

		f.close();

		SetModificationDate((*ii)->second.srcPath, (*ii)->second.libDate );
	};

	if (res)
		cout << "*** Экспорт в целом неудачен" << endl;
	else
		cout << "Экспорт завершён" << endl;

	return res;
};


int DoImport(Args& args){
	int res = 0;

	map<string, LibEntity> voc;

	ScanDirectory_pbl(args.pbl, voc);
	ScanDirectory_src(args.src, voc);

	// Отберём тех, кого нужно импортировать
	list<map<string, LibEntity>::iterator> needsImport; // Список указателей на элементы voc, которые нужно импортировать

	if (args.full) { 
		// отберём всех, упомянутых в каталоге исходников
		for (map<string, LibEntity>::iterator i = voc.begin(); i != voc.end(); ++i )
			if ( ! i->second.srcPath.empty() ) needsImport.push_back(i);
	} else {
		// отберём всех, для которых дата в библиотеке меньше даты файла в каталоге исходников (или такого объекта нет в библиотеке)
		for (map<string, LibEntity>::iterator i = voc.begin(); i != voc.end(); ++i )
			if ( i->second.libDate < i->second.fileDate ) needsImport.push_back(i); // Если объекта нет -- из логики построения (voc) его .libDate должно быть 0
	};

	cout << "------------- Импорт: --------------" << endl;
	for ( list<map<string, LibEntity>::iterator>::iterator ii = needsImport.begin(); ii != needsImport.end(); ++ii )
		cout << (*ii)->second.srcPath << endl;
	cout << "------------------------------------" << endl;

	// Укажем библиотеку для новых файлов
	for ( list<map<string, LibEntity>::iterator>::iterator ii = needsImport.begin(); ii != needsImport.end(); ++ii )
		if( (*ii)->second.libPath.empty() ) {
			string libName=FilenameFromPath(CatalogFromPath( (*ii)->second.srcPath ) );
			(*ii)->second.libPath = args.pbl + "\\" + libName + ".pbl";
		};

	if (needsImport.size() > 0 ) {
		// Сделаем резервную копию библиотек
		if (CopyDirectory_pbl(args.pbl, args.backup) ) {
			cout << "Не удалось создать резервную копию библиотек. Процедура импорта остановлена. Изменений в библиотеках нет" << endl;
			return 1;
		};

		// Поднимем исходники в память
		for ( list<map<string, LibEntity>::iterator>::iterator ii = needsImport.begin(); ii != needsImport.end(); ++ii ) {
			ifstream f( (*ii)->second.srcPath, std::ios_base::binary | std::ios_base::in );	// Чтобы не заморачиваться с закрытием при использовании continue в цикле

			if (f.fail() ) {
				cout << "Не удалось открыть на чтение файл '" << (*ii)->second.srcPath << "'" << endl;
				res = 1;
				continue;
			};

			// Поднимем в буфер
			string buf(
				(std::istreambuf_iterator<char>(f)), 
					std::istreambuf_iterator<char>()
			);			

			// В файле первой строкой должно идти имя объекта и файла
			string headTest("$PBExportHeader$");
			headTest.append((*ii)->second.entName).append(GetExtByPBType((*ii)->second.entType ) );
			if (headTest != buf.substr(0, headTest.length() ) ) {
				cout << "Начало файла '" << (*ii)->second.srcPath << "' не совпадает с ожидаемым" << endl;
				res = 1;
				continue;
			};

			long codeStart = buf.find_first_of(13, 0);	// CR
			if (codeStart == string::npos ) {
				cout << "Файл '" << (*ii)->second.srcPath << "' : не найден конец заголовка" << endl;
				res = 1;
				continue;
			};
			codeStart += 2;	// Считаем, что строки в исходнике разделены CRLF

			// Возможно, второй строкой в файле будет комментарий
			if (buf.substr(codeStart, 18) == "$PBExportComments$" ) {
				long commentEnd = buf.find_first_of(13, codeStart);	// CR
				if (commentEnd == string::npos ) {
					cout << "Файл '" << (*ii)->second.srcPath << "' : не найден конец комментария" << endl;
					res = 1;
					continue;
				};

				(*ii)->second.entComment.assign(buf.substr(codeStart+18, commentEnd - (codeStart+18) ) ); // 18 -- длина заголовка комментария

				codeStart = commentEnd + 2;	// Считаем, что строки в исходнике разделены CRLF
			};

			(*ii)->second.entCode.assign(buf.substr(codeStart) );

			f.close();
		};

		// Если не удалось прочитать исходники -- прекращаем работу
		if (res) {
			cout << "Не удалось прочитать исходный код из файлов. Процедура импорта остановлена. Изменений в библиотеках нет." << endl;
			return 1;
		};

		// Сформируем список библиотек для сессии PBORCA
		list<string> libList;
		ScanDirectory(args.pbl + "\\*.pbl", libList, false);
		vector<char*> libListPtrs;
		libListPtrs.reserve(libList.size() );

		for( list<string>::iterator i = libList.begin(); i != libList.end(); ++i) {
			*i = args.pbl + "\\" + *i;	// Добавим пути
			libListPtrs.push_back(const_cast<char*>(i->c_str() ) );
		};

		// Инициализируем сессию PBORCA
		PBSessionHandle s(PBORCA_SessionOpen() );
		if (!s.h() ) 
			throw PBException("Не удалось открыть сессию PBORCA" );

		// Список библиотек
		if (PBORCA_SessionSetLibraryList(s.h(), libListPtrs.data(), libListPtrs.size() ) ) 
			throw PBException(string("Не удалось установить список библиотек для сессии. ") + GetPBError(s.h() ) );

		// Имя приложения
		{
			map<string, LibEntity>::iterator pAppl;
			if ( voc.end() == (pAppl = voc.find(args.appl) ) )
				throw PBException(string("Не удалось найти библиотеку с приложением '").append(args.appl).append("'") );

			if ( PBORCA_SessionSetCurrentAppl(s.h(), const_cast<char*>(pAppl->second.libPath.c_str() ), const_cast<char*>(args.appl.c_str() ) ) )
				throw PBException(string("Не удалось указать приложение сессии: ") + GetPBError(s.h() ) );
		};

		// ----Запускаем импорт
		for ( list<map<string, LibEntity>::iterator>::iterator ii = needsImport.begin(); ii != needsImport.end(); ++ii ) {
			if (PBORCA_CompileEntryImport( s.h(),
						const_cast<char*>( (*ii)->second.libPath.c_str() ),
						const_cast<char*>( (*ii)->second.entName.c_str() ),
						(*ii)->second.entType,
						const_cast<char*>( (*ii)->second.entComment.c_str() ),
						const_cast<char*>( (*ii)->second.entCode.c_str() ),
						(*ii)->second.entCode.length() + 1,
						PBCompErr,
						&( (*ii)->second.compileRes)  
						) 
				)
			{
				cout << GetPBError(s.h() ) << endl;	
				res = 1;
			};
		};

		// Перекомпилируем объекты с ошибками нужное число раз
		if (res)
			for( int recomp = 1; recomp<=args.recompCount && res; recomp++ ) {
				cout << "----Перекомпиляция, попытка " << recomp << " ----" << endl;
				res = 0;
				for ( list<map<string, LibEntity>::iterator>::iterator ii = needsImport.begin(); ii != needsImport.end(); ++ii )
					if ((*ii)->second.compileRes ) {
						(*ii)->second.compileRes = 0;
						if (PBORCA_CompileEntryRegenerate(s.h(),
							const_cast<char*> ((*ii)->second.libPath.c_str() ),
							const_cast<char*> ((*ii)->second.entName. c_str() ),
							(*ii)->second.entType,
							PBCompErr, 
							&( (*ii)->second.compileRes ) 
							) 
						) {
							cout << GetPBError(s.h() ) << endl;
							res = 1;
						};							
					};
			}; //for(recomp...)

		// Обновим даты исходников в библиотеках
		map<string, LibEntity> voc_res; // После импорта потроха библиотек могли измениться
		ScanDirectory_pbl(args.pbl, voc_res);

		for ( list<map<string, LibEntity>::iterator>::iterator ii = needsImport.begin(); ii != needsImport.end(); ++ii )
			if (voc_res[(*ii)->first].libDateOffset == 0) {
				cout << "Дата модификации не исправлена: не удалось найти в библиотеках сущность '" << (*ii)->first << "'" << endl;
				res = 1;
			}
			else
			{
				ofstream f((*ii)->second.libPath, std::ios_base::binary | std::ios_base::in);
				if (f.fail() ) {
					cout << "Дата модификации '" << (*ii)->first << "' не исправлена: не удалось открыть на запись файл '" << (*ii)->second.srcPath << "'" << endl;
					res = 1;
					continue;
				};

				f.seekp(voc_res[(*ii)->first].libDateOffset );
				f.write(reinterpret_cast<char*>(&((*ii)->second.fileDate) ), 4); // Пишем в файл дату -- она long
				f.close();
			}; // if( libDateOffset == 0 )			

		if (res)
			cout << "*** Импорт в целом неудачен" << endl;
		else
			cout << "Импорт завершён" << endl;
	}; // needsImport.size() > 0

	return res;
};
