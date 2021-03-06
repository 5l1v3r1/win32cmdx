/**@file renamex.cpp --- rename file with pattern.
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
//!@name option settings
//@{
/** -c: case sensitive scan */
bool gCaseSensitive = false;

/** -r: sub directory recursive scan */
bool gRecursive = false;

/** -n: test only */
bool gTestOnly = false;

/** -i: ignore error */
bool gIgnoreError = false;
//@}

//........................................................................
//!@name messages
//@{
/** short help-message */
const char* gUsage  = "usage :renamex [-h?cdni] FROM TO FILES\n";

/** detail help-message for options and version */
const char* gUsage2 =
	"  version 1.3 (r48)\n"
	"  -h -?  this help\n"
	"  -c     case sensitive scan\n"
	"  -d     sub directory recursive scan\n"
	"  -n     test only, don't rename FILES\n"
	"  -i     ignore error. non-stop mode\n"
	"  FROM   replace from pattern\n"
	"  TO     replace to pattern\n"
	"  FILES  file match pattern(default is '*')\n"
	;
//@}

//------------------------------------------------------------------------
/** リネームを実行する */
void Rename(const char* from, const char* to, const char* dir, const char* wild)
{
	char oldpath[_MAX_PATH + _MAX_FNAME + 10];
	char newpath[_MAX_PATH + _MAX_FNAME + 10];
	char newname[_MAX_FNAME];
	size_t from_len = strlen(from);
	size_t to_len = strlen(to);

	if (strlen(dir) >= _MAX_PATH)
		error_abort("too long folder name", dir);
	if (to_len >= _MAX_FNAME)
		error_abort("too long TO pattern", to);

	FindFile find;
	for (find.Open(dir, wild); find; find.Next()) {
		// フォルダはリネーム対象外.
		if (find.IsFolder())
			continue;

		// パターンに一致しなければ対象外.
		char* match = gCaseSensitive ? strstr(find.name, from) : stristr(find.name, from);
		if (!match)
			continue;

		// パターン置き換えしたファイル名を生成する.
		size_t pre_len = match - find.name;
		strncpy(newname, find.name, pre_len);
		strcpy(newname + pre_len, to);
		strcpy(newname + pre_len + to_len, match + from_len);

		make_pathname(oldpath, dir, find.name);
		make_pathname(newpath, dir, newname);

		// 状況を表示する.
		printf("%s%s => %s\n", dir, find.name, newname);
		if (gTestOnly)
			continue;

		// リネームを実行する.
		if (rename(oldpath, newpath) != 0) {
			print_win32error("rename");
			if (!gIgnoreError)
				error_abort();
		}
	}//.endfor find

	if (gRecursive) {
		// 各サブフォルダに対して再帰する.
		find.Close();
		for (find.Open(dir, "*"); find; find.Next()) {
			if (find.IsFolder() && !find.IsDotFolder()) {
				if (find.IsHidden() || find.IsSystem()) continue;
				if (strequ(find.name, "CVS")) continue;
				char subdir[_MAX_PATH + _MAX_FNAME + 10];
				make_pathname(subdir, dir, find.name);
				strcat(subdir, "\\");
				Rename(from, to, subdir, wild);
			}
		}//.endfor find dir
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
				case 'c':
					gCaseSensitive = true;
					break;
				case 'r':
					gRecursive = true;
					break;
				case 'n':
					gTestOnly = true;
					break;
				case 'i':
					gIgnoreError = true;
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
	if (argc < 3) {
		error_abort("please specify FROM TO FILES\n");
	}

	//--- コマンドライン上の FROM TO を取り出す.
	const char* from = const_cast<const char*>(argv[1]);
	const char* to   = const_cast<const char*>(argv[2]);

	//--- リネームを実行する.
	if (argc == 3) {
		Rename(from, to, "", "*"); // default files = "*"
	}
	else {
		for (int i = 3; i < argc; ++i) {
			char dir[_MAX_PATH];
			char name[_MAX_FNAME];
			separate_pathname(argv[i], dir, name);
			Rename(from, to, dir, name);
		}//.endfor
	}
	return EXIT_SUCCESS;
}
//------------------------------------------------------------------------
/**@page renamex-manual renamex.exe - rename file with pattern

@version 1.3 (r48)

@author Hiroshi Kuno <http://code.google.com/p/win32cmdx/>

@par License:
	New BSD License
	<br>Copyright &copy; 2003,2010 by Hiroshi Kuno
	<br>本ソフトウェアは無保証かつ無償で提供します。利用、再配布、改変は自由です。

<hr>
@section intro はじめに
	renamexは、ファイル名の一部分を一括パターン置換するコマンドです。

@section renamex-func 特徴
	- ワイルドカードでリネーム対象を指定できます。
	- フォルダ再帰検索でフォルダ以下を一括処理するこが可能です。

@section env 動作環境
	Windows2000以降を動作対象としています。
	WindowsXP にて動作確認済み。

@section install インストール方法
	配布ファイル renamex.exe を、PATHが通ったフォルダにコピーしてください。
	アインインストールするには、そのコピーしたファイルを削除してください。

@section usage 使い方
	@verbinclude renamex.usage

@section renamex-example 使用例
	@verbatim
	@@@Todo Here!!
	@endverbatim

@section todo 改善予定
	- なし。

@section links リンク
	- http://code.google.com/p/win32cmdx/ - renamex開発サイト

@section download ダウンロード
	- http://code.google.com/p/win32cmdx/downloads/list

@section changelog 改訂履歴
	- version-1.3 [Feb xx, 2010] 公開初版
*/

// renamex.cpp - end.
