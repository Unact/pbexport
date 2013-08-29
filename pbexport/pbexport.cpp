// pbexport.cpp: ���������� ����� ����� ��� ����������� ����������.
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

void CALLBACK PBCompErr( PBORCA_COMPERR* err, LPVOID uData);	// ��� ������ PBORCA_CompileEntryImportList

int main (int argc, char* argv[]){
	setlocale(LC_CTYPE, "Russian");	// ������� �������� �� ���������

	Args args(argc, argv);

	if (args.help) {
		cout << "�������:" << endl <<
			"  --pbl2src" << endl <<
			"  --pbl-<������� � ������������>" << endl <<
			"  --src-<������� ��� ����������>" << endl <<
			"  [--full]" << endl <<
			endl <<
			"������:" << endl <<
			"  --src2pbl" << endl <<
			"  --pbl-<������� � ������������>" << endl <<
			"  --src-<������� c �����������>" << endl <<
			"  --backup-<������� ��� ��������� ����� ���������>" << endl <<
			"  --appl-<��� ����������>" << endl <<
			"  [--recomp-<������� ��� ����������������� �������, �� ���������������� �����>]" << endl <<
			"  [--full]" << std::endl;
		return RES_OK;
	};

	if ( (args.pbl2src && args.src2pbl)  || (!args.pbl2src && !args.src2pbl) ) {
		cout << "������ ���� �������� ���� � ������ ���� ���� �� ���� {--src2pbl, --pbl2src}" << endl;
		return RES_BADARGUMENTS;
	};

	if (args.pbl.empty() ) {
		cout << "�� ����� ������� � ������� PBL" << endl;
		return RES_BADARGUMENTS;
	};	

	if (args.src.empty() ) {
		cout << "�� ����� ������� � ��������� �������" << endl;
		return RES_BADARGUMENTS;
	};

	if (args.src2pbl && args.appl.empty() ) {
		cout << "���������� ������ ��� ����������" << endl;
		return RES_BADARGUMENTS;
	};

	if (args.src2pbl && args.backup.empty() ) {
		cout << "���������� ������� ������� ��� ���������� �����������" << endl;
		return RES_BADARGUMENTS;
	};

	HINSTANCE orcalib, hInst;
	hInst = GetModuleHandle(NULL);

	orcalib = LoadLibrary("pborc90.dll" );
	if (orcalib == NULL) {
		int errCode = GetLastError();
		cout << "�� ������� ��������� ���������� pborc90.dll" << endl;
		
		return RES_CANNOTLOADORCALIBRARY;
	};

	int res = 0;

	try {
		if (args.pbl2src) res = DoExport(args);
		if (args.src2pbl) res = DoImport(args);		
	}
	catch (PBException e) {
		cout << "������������: " << e.what() << endl;
		return RES_EXCEPTION;
	}
	catch (exception e) {
		cout << "����������� ������������: " << e.what() << endl;
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

	cout << "������ ��������������: ";
	if (err->iLevel) cout << " �������:" << err->iLevel;
	if (err->iLineNumber) cout << " ������:" << err->iLineNumber;
	if (err->iColumnNumber) cout << " �������:" << err->iColumnNumber;
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

	// ������ ���, ���� ����� ��������������:
	list<map<string, LibEntity>::iterator> needsExport; // ������ ���������� �� �������� voc, ������� ����� ��������������

	if (args.full) { 
		// ������ ����, ���������� � �����������
		for (map<string, LibEntity>::iterator i = voc.begin(); i != voc.end(); ++i )
			if ( ! i->second.libPath.empty() ) needsExport.push_back(i);
	} else {
		// ������ ����, ��� ������� ���� � ���������� ������ ���� ����� � �������� ���������� (��� ������ ����� ���)
		for (map<string, LibEntity>::iterator i = voc.begin(); i != voc.end(); ++i )
			if ( i->second.libDate > i->second.fileDate ) needsExport.push_back(i); // ���� ����� ��� -- �� ������ ���������� (voc) ��� .fileDate ������ ���� 0
	};

	// ����, � ���� ��� ��������� �����, ����� ������������ ���� � ����
	for ( list<map<string, LibEntity>::iterator>::iterator ii = needsExport.begin(); ii != needsExport.end(); ++ii )
		if ( (*ii)->second.srcPath.empty() ) {
			(*ii)->second.srcPath = args.src + "\\" + GetLibName((*ii)->second.libPath) + "\\" + (*ii)->second.entName + GetExtByPBType((*ii)->second.entType) ;
		};

	// � ������������
	cout << "------------- �������: --------------" << endl;
	for ( list<map<string, LibEntity>::iterator>::iterator ii = needsExport.begin(); ii != needsExport.end(); ++ii )
		cout << (*ii)->second.srcPath << endl;
	cout << "------------------------------------" << endl;

	// ������� ��������� �� PB
	PBSessionHandle s(PBORCA_SessionOpen() );
	if (!s.h() ) {		
		throw PBException("�� ������� ������� ������ PBORCA" );
	};

	string codeBuf(MAX_PBCODE_BUFFER, 0);	// ��� ����� ���� ����� -- �� ���� ��������� ����� � �����
	PBORCA_ENTRYINFO commentBuf;
	ofstream f;
	long err;
	for ( list<map<string, LibEntity>::iterator>::iterator ii = needsExport.begin(); ii != needsExport.end(); ++ii ) {
		BuildPath( CatalogFromPath((*ii)->second.srcPath) );

		//ofstream f((*ii)->second.srcPath, std::ios_base::binary | std::ios_base::out);	// ��. ios_base::failure,  f.failbit
		f.open((*ii)->second.srcPath, std::ios_base::binary | std::ios_base::out);
		if (f.fail() ) {
			cout << "�� ������� ������� �� ������ ���� '" << (*ii)->second.srcPath << "'" << endl;
			res = 1;
			continue;
		}

		// ���������
		f.write("$PBExportHeader$", 16);
		f.write((*ii)->second.entName.data(), (*ii)->second.entName.length() );
		f.write(GetExtByPBType((*ii)->second.entType).data(), 4);
		f.write("\x0D\x0A", 2);

		// �����������
		if (err = PBORCA_LibraryEntryInformation(s.h(), 
			const_cast<char*>((*ii)->second.libPath.c_str() ), 
			const_cast<char*>((*ii)->second.entName.c_str() ),
			(*ii)->second.entType, &commentBuf ) ) {
			cout << "�� ������� �������� ���������� � '" << (*ii)->second.libPath << "'." << (*ii)->second.entName << " :" << DescribePBError(err);
			res = 1;
		} else {
			if (commentBuf.szComments[0] ) {	// ����������� ����
				f.write("$PBExportComments$", 18);
				f.write(commentBuf.szComments, strlen(commentBuf.szComments) );
				f.write("\x0D\x0A", 2);
			};
		};

		// ���
		if (err = PBORCA_LibraryEntryExport(s.h(), const_cast<char*>((*ii)->second.libPath.c_str() ), 
			const_cast<char*>((*ii)->second.entName.c_str() ),
			(*ii)->second.entType,
			const_cast<char*>(codeBuf.data() ), MAX_PBCODE_BUFFER) ) {
				cout << "�� ������� �������� ���������� � '" << (*ii)->second.libPath << "'." << (*ii)->second.entName << " :" << DescribePBError(err);
				res = 1;
		} 
		else
			f.write(codeBuf.data(), strlen(codeBuf.data()) );

		f.close();

		SetModificationDate((*ii)->second.srcPath, (*ii)->second.libDate );
	};

	if (res)
		cout << "*** ������� � ����� ��������" << endl;
	else
		cout << "������� ��������" << endl;

	return res;
};


int DoImport(Args& args){
	int res = 0;

	map<string, LibEntity> voc;

	ScanDirectory_pbl(args.pbl, voc);
	ScanDirectory_src(args.src, voc);

	// ������ ���, ���� ����� �������������
	list<map<string, LibEntity>::iterator> needsImport; // ������ ���������� �� �������� voc, ������� ����� �������������

	if (args.full) { 
		// ������ ����, ���������� � �������� ����������
		for (map<string, LibEntity>::iterator i = voc.begin(); i != voc.end(); ++i )
			if ( ! i->second.srcPath.empty() ) needsImport.push_back(i);
	} else {
		// ������ ����, ��� ������� ���� � ���������� ������ ���� ����� � �������� ���������� (��� ������ ������� ��� � ����������)
		for (map<string, LibEntity>::iterator i = voc.begin(); i != voc.end(); ++i )
			if ( i->second.libDate < i->second.fileDate ) needsImport.push_back(i); // ���� ������� ��� -- �� ������ ���������� (voc) ��� .libDate ������ ���� 0
	};

	cout << "------------- ������: --------------" << endl;
	for ( list<map<string, LibEntity>::iterator>::iterator ii = needsImport.begin(); ii != needsImport.end(); ++ii )
		cout << (*ii)->second.srcPath << endl;
	cout << "------------------------------------" << endl;

	// ������ ���������� ��� ����� ������
	for ( list<map<string, LibEntity>::iterator>::iterator ii = needsImport.begin(); ii != needsImport.end(); ++ii )
		if( (*ii)->second.libPath.empty() ) {
			string libName=FilenameFromPath(CatalogFromPath( (*ii)->second.srcPath ) );
			(*ii)->second.libPath = args.pbl + "\\" + libName + ".pbl";
		};

	if (needsImport.size() > 0 ) {
		// ������� ��������� ����� ���������
		if (CopyDirectory_pbl(args.pbl, args.backup) ) {
			cout << "�� ������� ������� ��������� ����� ���������. ��������� ������� �����������. ��������� � ����������� ���" << endl;
			return 1;
		};

		// �������� ��������� � ������
		for ( list<map<string, LibEntity>::iterator>::iterator ii = needsImport.begin(); ii != needsImport.end(); ++ii ) {
			ifstream f( (*ii)->second.srcPath, std::ios_base::binary | std::ios_base::in );	// ����� �� �������������� � ��������� ��� ������������� continue � �����

			if (f.fail() ) {
				cout << "�� ������� ������� �� ������ ���� '" << (*ii)->second.srcPath << "'" << endl;
				res = 1;
				continue;
			};

			// �������� � �����
			string buf(
				(std::istreambuf_iterator<char>(f)), 
					std::istreambuf_iterator<char>()
			);			

			// � ����� ������ ������� ������ ���� ��� ������� � �����
			string headTest("$PBExportHeader$");
			headTest.append((*ii)->second.entName).append(GetExtByPBType((*ii)->second.entType ) );
			if (headTest != buf.substr(0, headTest.length() ) ) {
				cout << "������ ����� '" << (*ii)->second.srcPath << "' �� ��������� � ���������" << endl;
				res = 1;
				continue;
			};

			long codeStart = buf.find_first_of(13, 0);	// CR
			if (codeStart == string::npos ) {
				cout << "���� '" << (*ii)->second.srcPath << "' : �� ������ ����� ���������" << endl;
				res = 1;
				continue;
			};
			codeStart += 2;	// �������, ��� ������ � ��������� ��������� CRLF

			// ��������, ������ ������� � ����� ����� �����������
			if (buf.substr(codeStart, 18) == "$PBExportComments$" ) {
				long commentEnd = buf.find_first_of(13, codeStart);	// CR
				if (commentEnd == string::npos ) {
					cout << "���� '" << (*ii)->second.srcPath << "' : �� ������ ����� �����������" << endl;
					res = 1;
					continue;
				};

				(*ii)->second.entComment.assign(buf.substr(codeStart+18, commentEnd - (codeStart+18) ) ); // 18 -- ����� ��������� �����������

				codeStart = commentEnd + 2;	// �������, ��� ������ � ��������� ��������� CRLF
			};

			(*ii)->second.entCode.assign(buf.substr(codeStart) );

			f.close();
		};

		// ���� �� ������� ��������� ��������� -- ���������� ������
		if (res) {
			cout << "�� ������� ��������� �������� ��� �� ������. ��������� ������� �����������. ��������� � ����������� ���." << endl;
			return 1;
		};

		// ���������� ������ ��������� ��� ������ PBORCA
		list<string> libList;
		ScanDirectory(args.pbl + "\\*.pbl", libList, false);
		vector<char*> libListPtrs;
		libListPtrs.reserve(libList.size() );

		for( list<string>::iterator i = libList.begin(); i != libList.end(); ++i) {
			*i = args.pbl + "\\" + *i;	// ������� ����
			libListPtrs.push_back(const_cast<char*>(i->c_str() ) );
		};

		// �������������� ������ PBORCA
		PBSessionHandle s(PBORCA_SessionOpen() );
		if (!s.h() ) 
			throw PBException("�� ������� ������� ������ PBORCA" );

		// ������ ���������
		if (PBORCA_SessionSetLibraryList(s.h(), libListPtrs.data(), libListPtrs.size() ) ) 
			throw PBException(string("�� ������� ���������� ������ ��������� ��� ������. ") + GetPBError(s.h() ) );

		// ��� ����������
		{
			map<string, LibEntity>::iterator pAppl;
			if ( voc.end() == (pAppl = voc.find(args.appl) ) )
				throw PBException(string("�� ������� ����� ���������� � ����������� '").append(args.appl).append("'") );

			if ( PBORCA_SessionSetCurrentAppl(s.h(), const_cast<char*>(pAppl->second.libPath.c_str() ), const_cast<char*>(args.appl.c_str() ) ) )
				throw PBException(string("�� ������� ������� ���������� ������: ") + GetPBError(s.h() ) );
		};

		// ----��������� ������
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

		// ��������������� ������� � �������� ������ ����� ���
		if (res)
			for( int recomp = 1; recomp<=args.recompCount && res; recomp++ ) {
				cout << "----��������������, ������� " << recomp << " ----" << endl;
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

		// ������� ���� ���������� � �����������
		map<string, LibEntity> voc_res; // ����� ������� ������� ��������� ����� ����������
		ScanDirectory_pbl(args.pbl, voc_res);

		for ( list<map<string, LibEntity>::iterator>::iterator ii = needsImport.begin(); ii != needsImport.end(); ++ii )
			if (voc_res[(*ii)->first].libDateOffset == 0) {
				cout << "���� ����������� �� ����������: �� ������� ����� � ����������� �������� '" << (*ii)->first << "'" << endl;
				res = 1;
			}
			else
			{
				ofstream f((*ii)->second.libPath, std::ios_base::binary | std::ios_base::in);
				if (f.fail() ) {
					cout << "���� ����������� '" << (*ii)->first << "' �� ����������: �� ������� ������� �� ������ ���� '" << (*ii)->second.srcPath << "'" << endl;
					res = 1;
					continue;
				};

				f.seekp(voc_res[(*ii)->first].libDateOffset );
				f.write(reinterpret_cast<char*>(&((*ii)->second.fileDate) ), 4); // ����� � ���� ���� -- ��� long
				f.close();
			}; // if( libDateOffset == 0 )			

		if (res)
			cout << "*** ������ � ����� ��������" << endl;
		else
			cout << "������ ��������" << endl;
	}; // needsImport.size() > 0

	return res;
};
