/**@file dirdiff.cpp -- compare and diff folder.
 * @author Hiroshi Kuno <http://code.google.com/p/win32cmdx/>
 */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <mbstring.h>
#include <time.h>
#include <io.h>
#include <process.h>

#include <map>
#include <vector>
#include <string>

#include "mydef.h"
using namespace std;

//------------------------------------------------------------------------
// 汎用関数群 - inline関数が多いので、分割コンパイルせずincludeで取り込む.
//........................................................................
#include "mylib\errfunc.cpp"
#include "mylib\strfunc.cpp"
#include "mylib\dirfunc.cpp"

//------------------------------------------------------------------------
// 型、定数、グローバル変数の定義
//........................................................................
/** ISO 8601形式に整形するstrftime()の書式 */
#define ISO8601FMT	"%Y-%m-%dT%H:%M:%S"

//........................................................................
//!@name option settings
//@{
/** -s: ignore same file date */
bool gIgnoreSameFileDate = false;

/** -r: ignore right only file */
bool gIgnoreRightOnlyFile = false;

/** -l: ignore left only file */
bool gIgnoreLeftOnlyFile = false;

/** -d: diff for file */
bool gDiff = false;

/** -t,-T: time format */
const char* gTmFmt = ISO8601FMT;
//@}

//........................................................................
//!@name messages
//@{
/** short help-message */
const char* gUsage  = "usage :dirdiff [-h?srlutTd] DIR1 [DIR2] [WILD]\n";

/** detail help-message for options and version */
const char* gUsage2 =
	"  version 1.6 (r57)\n"
	"  -h -?  this help\n"
	"  -s     ignore same file date\n"
	"  -r     ignore right only file\n"
	"  -l     ignore left  only file\n"
	"  -u     ignore unique file(same as -r -l)\n"
	"  -t     use locale time format\n"
	"  -T     use ISO 8601 time format(default)\n"
	"  -d     diff for file\n"
	"  DIR1   compare folder\n"
	"  DIR2   compare folder(default is current-folder)\n"
	"  WILD   file match pattern(default is '*')\n"
	;
//@}

//------------------------------------------------------------------------
/** ファイル一覧を作成する */
void MakeFileList(vector<_finddata_t>& vec, const char* dir, const char* wild)
{
	FindFile find;
	for (find.Open(dir, wild); find; find.Next()) {
		// フォルダは収集対象から除外する.
		if (find.IsFolder())
			continue;

		// vector の伸張を高速化する.
		if (vec.capacity() == vec.size())
			vec.reserve(vec.size() + 1024);

		// vectorに検索ファイルを登録する.
		vec.push_back(find);
	}
}

/** 存在するフォルダであることを保証する. もし問題があれば終了する. */
void ValidateFolder(const char* dir)
{
	DWORD attr = ::GetFileAttributes(dir);
	if (attr == -1) {
		print_win32error(dir);
		error_abort();
	}
	if ((attr & FILE_ATTRIBUTE_DIRECTORY) == 0) {
		error_abort("not a folder", dir);
	}
}

//------------------------------------------------------------------------
/** 左右のファイル情報 */
class Entry {
public:
	Entry()
		: Left(NULL), Right(NULL) { }
	const _finddata_t* Left;
	const _finddata_t* Right;

	int print(FILE* fp) const;
};

/** 表示.
 * @param fp	出力先
 * @retval -1	ignored
 * @retval 0	left only / right only
 * @retval =	same
 * @retval <	right is newer
 * @retval >	left is newer
 */
int Entry::print(FILE* fp) const
{
	time_t l = Left  ? Left->time_write  : 0;
	time_t r = Right ? Right->time_write : 0;
	char lbuf[100];
	char rbuf[100];
	int mark = 0;
	if (Left && Right) {
		if (l == r) {
			if (gIgnoreSameFileDate) return -1;
			mark = '=';
		}
		else if (l < r)
			mark = '<';
		else
			mark = '>';
		strftime(lbuf, sizeof(lbuf), gTmFmt, localtime(&l));
		strftime(rbuf, sizeof(rbuf), gTmFmt, localtime(&r));
		fprintf(fp, "[ %s ] %c [ %s ] %s\n", lbuf, mark, rbuf, Left->name);
		return mark;
	}
	else if (Left) {
		if (gIgnoreLeftOnlyFile) return -1;
		size_t n = strftime(lbuf, sizeof(lbuf), gTmFmt, localtime(&l));
		fprintf(fp, "[ %s ]     %*s   %s\n", lbuf, n, "", Left->name);
	}
	else if (Right) {
		if (gIgnoreRightOnlyFile) return -1;
		size_t n = strftime(rbuf, sizeof(rbuf), gTmFmt, localtime(&r));
		fprintf(fp, "  %*s     [ %s ] %s\n", n, "", rbuf, Right->name);
	}
	return mark;
}

//------------------------------------------------------------------------
/** フォルダ比較を実行する */
void Compare(const char* dir1, const char* dir2, const char* wild)
{
	// dir1, dir2 が有効なフォルダか調べる.
	ValidateFolder(dir1);
	ValidateFolder(dir2);

	printf("folder compare [ %s ] <-> [ %s ] with \"%s\"\n", dir1, dir2, wild);

	// dir1, dir2 のファイル一覧を得る.
	vector<_finddata_t> files1, files2;
	vector<_finddata_t>::const_iterator i;
	MakeFileList(files1, dir1, wild);
	MakeFileList(files2, dir2, wild);

	// ファイル名をキーとするマップに、両ファイル一覧の要素を登録する.
	map<const char*, Entry, StrILess> list;
	map<const char*, Entry, StrILess>::const_iterator j;
	for (i = files1.begin(); i != files1.end(); ++i)
		list[i->name].Left = &*i;
	for (i = files2.begin(); i != files2.end(); ++i)
		list[i->name].Right = &*i;

	// 完成したマップの内容を表示する.
	for (j = list.begin(); j != list.end(); ++j) {
		int ret = j->second.print(stdout);

		// ファイルのdiffをとり、内容が本当に異なっているか確認する
		if (ret > 0 && gDiff) {
			char file1[_MAX_PATH];
			char file2[_MAX_PATH];
			_makepath(file1, NULL, dir1, j->second.Left->name, NULL);
			_makepath(file2, NULL, dir2, j->second.Right->name, NULL);
			_spawnlp(_P_WAIT, "diff", "diff", "-Bwqs", file1, file2, NULL);
		}
	}
}

//------------------------------------------------------------------------
/** メイン関数 */
int main(int argc, char* argv[])
{
	setlocale(LC_ALL, "");

	//--- コマンドライン上のオプションを解析する.
	while (argc > 1 && argv[1][0]=='-') {
		char* sw = &argv[1][1];
		if (strcmp(sw, "help") == 0)
			goto show_help;
		else {
			do {
				switch (*sw) {
				case 'h': case '?':
show_help:			error_abort(gUsage2);
					break;
				case 's':
					gIgnoreSameFileDate = true;
					break;
				case 'l':
					gIgnoreLeftOnlyFile = true;
					break;
				case 'r':
					gIgnoreRightOnlyFile = true;
					break;
				case 'u':
					gIgnoreLeftOnlyFile = gIgnoreRightOnlyFile = true;
					break;
				case 'd':
					gDiff = true;
					break;
				case 't':
					gTmFmt = "%c";	// locale time format.
					break;
				case 'T':
					gTmFmt = ISO8601FMT;
					break;
				default:
					error_abort("unknown option.\n");
					break;
				}
			} while (*++sw);
		}
//next_arg:
		++argv;
		--argc;
	}
	if (argc < 2) {
		error_abort("please specify DIR1\n");
	}

	//--- コマンドライン上の DIR1 [DIR2] [WILD] を取り出す.
	char dir1[_MAX_PATH];
	char dir2[_MAX_PATH];
	char wild[_MAX_PATH];
	strcpy(dir1, argv[1]);
	strcpy(dir2, argc > 2 ? argv[2] : ".");
	strcpy(wild, argc > 3 ? argv[3] : "*");
	if (argc <= 3 && has_wildcard(dir1))
		separate_pathname(dir1, dir1, wild);

	//--- フォルダ比較を実行する.
	Compare(dir1, dir2, wild);

	return EXIT_SUCCESS;
}
//------------------------------------------------------------------------
/**@page dirdiff-manual dirdiff.exe - show differ of directories

@version 1.6 (r57)

@author Hiroshi Kuno <http://code.google.com/p/win32cmdx/>

@par License:
	New BSD License
	<br>Copyright &copy; 1989, 1990, 2003, 2005, 2010 by Hiroshi Kuno
	<br>本ソフトウェアは無保証かつ無償で提供します。利用、再配布、改変は自由です。

<hr>
@section intro はじめに
	dirdiffは、ディレクトリ間のファイルリストの差異を比較するツールです.

@section dirdiff-func 特徴
	- ワイルドカードでリネーム対象を指定できます。

@section env 動作環境
	Windows2000以降を動作対象としています。
	WindowsXP にて動作確認済み。

@section install インストール方法
	配布ファイル dirdiff.exe を、PATHが通ったフォルダにコピーしてください。
	アインインストールするには、そのコピーしたファイルを削除してください。

@section dirdiff-usage 使い方
	@verbinclude dirdiff.usage

@section dirdiff-example 使用例
	@verbatim
	@@@Todo Here!!
	@endverbatim

@section todo 改善予定
	- なし.

@section links リンク
	- http://code.google.com/p/win32cmdx/ - dirdiff開発サイト

@section download ダウンロード
	- http://code.google.com/p/win32cmdx/downloads/list

@section changelog 改訂履歴
	- version-1.6 [Feb xx, 2010] 公開初版
	- version-1.5 [Oct 19, 2005] original
*/

// dirdiff.cpp - end.
