/**@file zipdump.cpp --- ZIPファイルの構造ダンプを行う.
 * @author Hiroshi Kuno <http://code.google.com/p/win32cmdx/>
 */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <locale.h>
#include <io.h>
#include <ctype.h>

#include "mydef.h"
//using namespace std;

//------------------------------------------------------------------------
// 汎用関数群 - inline関数が多いので、分割コンパイルせずincludeで取り込む.
//........................................................................
#include "mylib\errfunc.cpp"
#include "mylib\strfunc.cpp"

//------------------------------------------------------------------------
// 型、定数、グローバル変数の定義.
//........................................................................
//!@name option settings
//@{
/** -f: full dump */
bool gIsFullDump = false;

/** -q: quiet mode */
bool gQuiet = false;

/** -o: omit same hexdump line */
bool gOmitSameHexDumpLine = false;

/** -s: output to stdout */
bool gIsStdout = false;

/** -r: recursive search */
bool gIsRecursive = false;

/** -d<DIR>: output folder */
const char* gOutDir = NULL;
//@}

//........................................................................
//!@name messages
//@{
/** short help-message */
const char* gUsage  = "usage :zipdump [-h?fqosr] [-d<DIR>] file1.zip file2.zip ...\n";

/** detail help-message for options and version */
const char* gUsage2 =
	"  version 1.2 (r43)\n"
	"  -h -?      this help\n"
	"  -f         full dump\n"
	"  -q         quiet mode\n"
	"  -o         omit same hexdump line\n"
	"  -s         output to stdout instend of files(*.zipdump)\n"
	"  -r         recursive search under the input-file's folder(wildcard needed)\n"
	"  -d<DIR>    output to DIR\n"
	"  fileN.zip  input-files. wildcard OK\n"
	;
//@}

//........................................................................
// fuction-proto-type decl.
void Dump_Data_descriptor(FILE* fin, FILE* fout);

//------------------------------------------------------------------------
// 汎用関数群
//........................................................................
//!@name ファイル処理系.
//@{
/** 入力ファイルオープン.
 * オープン失敗時にはリターンしないで、ここで終了する.
 */
FILE* OpenInput(const char* fname)
{
	FILE* fp = fopen(fname, "rb");
	if (fp == NULL) {
		fprintf(stderr, "can't open input file: %s\n", fname);
		exit(EXIT_FAILURE);
	}
	return fp;
}

/** 出力ファイルオープン.
 * 出力ファイル名は、渡されたファイル名の末尾に extname を追加したものとする.
 * オープン失敗時にはリターンしないで、ここで終了する.
 */
FILE* OpenOutput(const char* inputfname, const char* extname)
{
	char fname[MY_MAX_PATH+100];	// 拡張子には100文字もみておけば十分.
	if (gOutDir) {
		char base[MY_MAX_PATH];
		char ext[MY_MAX_PATH];
		_splitpath(inputfname, NULL, NULL, base, ext);
		_makepath(fname, NULL, gOutDir, base, ext);
	}
	else {
		strcpy(fname, inputfname);
	}
	strcat(fname, extname);

	FILE* fp = fopen(fname, "w");
	if (fp == NULL) {
		fprintf(stderr, "can't open output file: %s\n", fname);
		exit(EXIT_FAILURE);
	}
	return fp;
}
//@}

//........................................................................
//!@name little endian read.
//@{
bool Read8(FILE* fin, uint8& val)
{
	return fread(&val, sizeof(val), 1, fin) == 1;
}

bool Read16(FILE* fin, uint16& val)
{
	uint8 lo, hi;
	if (Read8(fin, lo) && Read8(fin, hi)) {
		val = ((uint16)hi << 8) + lo;
		return true;
	}
	return false;
}

bool Read32(FILE* fin, uint32& val)
{
	uint16 lo, hi;
	if (Read16(fin, lo) && Read16(fin, hi)) {
		val = ((uint32)hi << 16) + lo;
		return true;
	}
	return false;
}

bool Read64(FILE* fin, uint64& val)
{
	uint32 lo, hi;
	if (Read32(fin, lo) && Read32(fin, hi)) {
		val = ((uint64)hi << 32) + lo;
		return true;
	}
	return false;
}
//@}

//........................................................................
/** @name print a field.
 * 指定の形式でフィールドを表示する.
 * - Print_u: print for decimal
 * - Print_x: print for hexadecimal
 * - Print_ux: print for decimal and hexadecimal
 * - Print_uFF: print for decimal or "0xFFFF"
 * - Print_note: print string
 * - Printf_note: print format string
 * @{
 */
//........................................................................
void Print_u(FILE* fout, const char* prompt, uint8 a)
{
	fprintf(fout, "%32s : %u\n", prompt, a);
}

void Print_u(FILE* fout, const char* prompt, uint16 a)
{
	fprintf(fout, "%32s : %u\n", prompt, a);
}

void Print_u(FILE* fout, const char* prompt, uint32 a)
{
	fprintf(fout, "%32s : %u\n", prompt, a);
}

void Print_u(FILE* fout, const char* prompt, uint64 a)
{
	fprintf(fout, "%32s : %I64u\n", prompt, a);
}

//........................................................................
void Print_x(FILE* fout, const char* prompt, uint8 a)
{
	fprintf(fout, "%32s : 0x%02X\n", prompt, a);
}

void Print_x(FILE* fout, const char* prompt, uint16 a)
{
	fprintf(fout, "%32s : 0x%04X\n", prompt, a);
}

void Print_x(FILE* fout, const char* prompt, uint32 a)
{
	fprintf(fout, "%32s : 0x%08X\n", prompt, a);
}

void Print_x(FILE* fout, const char* prompt, uint64 a)
{
	fprintf(fout, "%32s : 0x%016I64X\n", prompt, a);
}

//........................................................................
void Print_ux(FILE* fout, const char* prompt, uint8 a)
{
	fprintf(fout, "%32s : %u(0x%02X)\n", prompt, a, a);
}

void Print_ux(FILE* fout, const char* prompt, uint16 a)
{
	fprintf(fout, "%32s : %u(0x%04X)\n", prompt, a, a);
}

void Print_ux(FILE* fout, const char* prompt, uint32 a)
{
	fprintf(fout, "%32s : %u(0x%08X)\n", prompt, a, a);
}

void Print_ux(FILE* fout, const char* prompt, uint64 a)
{
	fprintf(fout, "%32s : %u(0x%016I64X)\n", prompt, a, a);
}

//........................................................................
void Print_uFF(FILE* fout, const char* prompt, uint16 a)
{
	if (a != 0xffffU)
		Print_u(fout, prompt, a);
	else
		Print_x(fout, prompt, a);
}

void Print_uFF(FILE* fout, const char* prompt, uint32 a)
{
	if (a != 0xffffffffU)
		Print_u(fout, prompt, a);
	else
		Print_x(fout, prompt, a);
}

//........................................................................
void Print_note(FILE* fout, const char* note)
{
	fprintf(fout, "%32s * %s\n", "", note);
}

void Printf_note(FILE* fout, const char* fmt, ...)
{
	fprintf(fout, "%32s * ", "");
	va_list ap;
	va_start(ap, fmt);
	vfprintf(fout, fmt, ap);
	va_end(ap);
	fputc('\n', fout);
}
//@}

//........................................................................
//!@name print a structure head.
//@{
void Print_section(FILE* fout, const char* section, __int64 offset, int n = -1)
{
	if (n < 0) {
		fprintf(fout, "\n[%s]", section);
	}
	else {
		fprintf(fout, "\n[%s #%d]", section, n);
	}
	if (offset >= 0 && !gQuiet) {
		fprintf(fout, " offset : %I64d(0x%016I64X)", offset, offset);
	}
	fputc('\n', fout);
}

void Print_header(FILE* fout, const char* section, uint32 signature, __int64 offset, int n = -1)
{
	Print_section(fout, section, offset, n);
	Print_x(fout, "header signature", signature);
}

void Print_extra(FILE* fout, const char* section, uint16 id, uint16 length)
{
	fprintf(fout, "\n[-%s]\n", section);
	Print_x(fout, "extra tag", id);
	Print_ux(fout, "extra size", length);
}
//@}

//........................................................................
//!@name dump a field MACRO.
//@{
#define DUMP1(prompt,var,format)			if (Read8 (fin, var)) { Print_##format(fout, prompt, var); }
#define DUMP2(prompt,var,format)			if (Read16(fin, var)) { Print_##format(fout, prompt, var); }
#define DUMP4(prompt,var,format)			if (Read32(fin, var)) { Print_##format(fout, prompt, var); }
#define DUMP8(prompt,var,format)			if (Read64(fin, var)) { Print_##format(fout, prompt, var); }
#define DUMP1x(prompt,var,format,detail)	if (Read8 (fin, var)) { Print_##format(fout, prompt, var); if (!gQuiet) { detail; } }
#define DUMP2x(prompt,var,format,detail)	if (Read16(fin, var)) { Print_##format(fout, prompt, var); if (!gQuiet) { detail; } }
#define DUMP4x(prompt,var,format,detail)	if (Read32(fin, var)) { Print_##format(fout, prompt, var); if (!gQuiet) { detail; } }
#define DUMP8x(prompt,var,format,detail)	if (Read64(fin, var)) { Print_##format(fout, prompt, var); if (!gQuiet) { detail; } }
//@}

//------------------------------------------------------------------------
// ZIP format処理関数群
//........................................................................
//!@name ZIPフィールド内容のダンプ処理.
//@{
void Print_UTC(FILE* fout, uint32 time)
{
	time_t t = (time_t) time;
	char buf[200];
	size_t n = strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S UTC", gmtime(&t));
	size_t m = strftime(buf + n, sizeof(buf)-n, " (%c %z)", localtime(&t));
	Print_note(fout, buf);
}

void Print_filetime(FILE* fout, const FILETIME& ft)
{
	// use windows API
	SYSTEMTIME st;
	::FileTimeToSystemTime(&ft, &st);

	char buf[20*6];
	sprintf_s(buf, sizeof(buf), "%u-%02u-%02uT%02d:%02u:%02u",
		st.wYear,
		st.wMonth,
		st.wDay,
		st.wHour,
		st.wMinute,
		st.wSecond);
	Print_note(fout, buf);
}

void Print_filetime(FILE* fout, const ULARGE_INTEGER& ii)
{
	FILETIME ft;

	ft.dwLowDateTime = ii.LowPart;
	ft.dwHighDateTime = ii.HighPart;
	Print_filetime(fout, ft);

}

void Print_date_and_time(FILE* fout, uint16 mod_time, uint16 mod_date)
{
	FILETIME ft;
	::DosDateTimeToFileTime(mod_date, mod_time, &ft);
	Print_filetime(fout, ft);
}

void Print_internal_file_attributes(FILE* fout, uint16 attr)
{
/*
      internal file attributes: (2 bytes)

          Bits 1 and 2 are reserved for use by PKWARE.

          The lowest bit of this field indicates, if set, that
          the file is apparently an ASCII or text file.  If not
          set, that the file apparently contains binary data.
          The remaining bits are unused in version 1.0.

          The 0x0002 bit of this field indicates, if set, that a 
          4 byte variable record length control field precedes each 
          logical record indicating the length of the record. The 
          record length control field is stored in little-endian byte
          order.  This flag is independent of text control characters, 
          and if used in conjunction with text data, includes any 
          control characters in the total length of the record. This 
          value is provided for mainframe data transfer support.

*/
	if (attr & 1) Print_note(fout, "text file");
	if (attr & 2) Print_note(fout, "????"); // Todo:説明文が意味不明なので調べること.
}

void Print_external_file_attributes(FILE* fout, uint32 attr)
{
/*
      external file attributes: (4 bytes)

          The mapping of the external attributes is
          host-system dependent (see 'version made by').  For
          MS-DOS, the low order byte is the MS-DOS directory
          attribute byte.  If input came from standard input, this
          field is set to zero.
*/
	// Todo: 何か表示すべき??
}

void Print_general_purpose_bit_flag(FILE* fout, uint16 flags, uint16 method)
{
/*
      general purpose bit flag: (2 bytes)

          Bit 0: If set, indicates that the file is encrypted.

          (For Method 6 - Imploding)
          Bit 1: If the compression method used was type 6,
                 Imploding, then this bit, if set, indicates
                 an 8K sliding dictionary was used.  If clear,
                 then a 4K sliding dictionary was used.
          Bit 2: If the compression method used was type 6,
                 Imploding, then this bit, if set, indicates
                 3 Shannon-Fano trees were used to encode the
                 sliding dictionary output.  If clear, then 2
                 Shannon-Fano trees were used.

          (For Methods 8 and 9 - Deflating)
          Bit 2  Bit 1
            0      0    Normal (-en) compression option was used.
            0      1    Maximum (-exx/-ex) compression option was used.
            1      0    Fast (-ef) compression option was used.
            1      1    Super Fast (-es) compression option was used.

          (For Method 14 - LZMA)
          Bit 1: If the compression method used was type 14,
                 LZMA, then this bit, if set, indicates
                 an end-of-stream (EOS) marker is used to
                 mark the end of the compressed data stream.
                 If clear, then an EOS marker is not present
                 and the compressed data size must be known
                 to extract.

          Note:  Bits 1 and 2 are undefined if the compression
                 method is any other.

          Bit 3: If this bit is set, the fields crc-32, compressed 
                 size and uncompressed size are set to zero in the 
                 local header.  The correct values are put in the 
                 data descriptor immediately following the compressed
                 data.  (Note: PKZIP version 2.04g for DOS only 
                 recognizes this bit for method 8 compression, newer 
                 versions of PKZIP recognize this bit for any 
                 compression method.)

          Bit 4: Reserved for use with method 8, for enhanced
                 deflating. 

          Bit 5: If this bit is set, this indicates that the file is 
                 compressed patched data.  (Note: Requires PKZIP 
                 version 2.70 or greater)

          Bit 6: Strong encryption.  If this bit is set, you should
                 set the version needed to extract value to at least
                 50 and you must also set bit 0.  If AES encryption
                 is used, the version needed to extract value must 
                 be at least 51.

          Bit 7: Currently unused.

          Bit 8: Currently unused.

          Bit 9: Currently unused.

          Bit 10: Currently unused.

          Bit 11: Language encoding flag (EFS).  If this bit is set,
                  the filename and comment fields for this file
                  must be encoded using UTF-8. (see APPENDIX D)

          Bit 12: Reserved by PKWARE for enhanced compression.

          Bit 13: Used when encrypting the Central Directory to indicate 
                  selected data values in the Local Header are masked to
                  hide their actual values.  See the section describing 
                  the Strong Encryption Specification for details.

          Bit 14: Reserved by PKWARE.

          Bit 15: Reserved by PKWARE.
*/
	uint16 w;

	if (flags & 0x0001) Print_note(fout, "Bit 0: encrypted");

	switch (method) {
	case 6:
		// (For Method 6 - Imploding)
		if (flags & 0x0002) Print_note(fout, "Bit 1: Method6: 8K sliding dictionary");
		if (flags & 0x0004) Print_note(fout, "Bit 2: Method6: 3 Shannon-Fano trees");
		break;

	case 8:
	case 9:
		// (For Methods 8 and 9 - Deflating)
		w = (flags >> 1) & 3;
		if (w == 1) Print_note(fout, "Bit 1-2: Method8/9: Maximum (-exx/-ex) compression");
		if (w == 2) Print_note(fout, "Bit 1-2: Method8/9: Fast (-ef) compression");
		if (w == 3) Print_note(fout, "Bit 1-2: Method8/9: Super Fast (-es) compression");
		break;

	case 14:
		// (For Method 14 - LZMA)
		if (flags & 0x0002) Print_note(fout, "Bit 1: Method14: end-of-stream marker used to mark the end of the compressed data stream");
		break;
	}//.endswitch
	
	if (flags & 0x0008) Print_note(fout, "Bit 3: crc-32, compressed size and uncompressed size are set to zero");
	if (flags & 0x0010) Print_note(fout, "Bit 4: Reserved for use with method 8, for enhanced deflating");
	if (flags & 0x0020) Print_note(fout, "Bit 5: compressed patched data.  (Note: Requires PKZIP version 2.70 or greater)");
	if (flags & 0x0040) Print_note(fout, "Bit 6: Strong encryption.");
	if (flags & 0x0080) Print_note(fout, "Bit 7: Currently unused.");
	if (flags & 0x0100) Print_note(fout, "Bit 8: Currently unused.");
	if (flags & 0x0200) Print_note(fout, "Bit 9: Currently unused.");
	if (flags & 0x0400) Print_note(fout, "Bit 10: Currently unused.");
	if (flags & 0x0800) Print_note(fout, "Bit 11: Language encoding flag (EFS). the filename and comment fields for this file must be encoded using UTF-8.");
	if (flags & 0x1000) Print_note(fout, "Bit 12: Reserved by PKWARE for enhanced compression.");
	if (flags & 0x2000) Print_note(fout, "Bit 13: Used when encrypting the Central Directory to indicate selected data values in the Local Header are masked to hide their actual values.");
	if (flags & 0x4000) Print_note(fout, "Bit 14: Reserved by PKWARE.");
	if (flags & 0x8000) Print_note(fout, "Bit 15: Reserved by PKWARE.");
}


void Print_version(FILE* fout, uint16 ver)
{
/*
      version made by (2 bytes)

          The upper byte indicates the compatibility of the file
          attribute information.  If the external file attributes 
          are compatible with MS-DOS and can be read by PKZIP for 
          DOS version 2.04g then this value will be zero.  If these 
          attributes are not compatible, then this value will 
          identify the host system on which the attributes are 
          compatible.  Software can use this information to determine
          the line record format for text files etc.  The current
          mappings are:

          0 - MS-DOS and OS/2 (FAT / VFAT / FAT32 file systems)
          1 - Amiga                     2 - OpenVMS
          3 - UNIX                      4 - VM/CMS
          5 - Atari ST                  6 - OS/2 H.P.F.S.
          7 - Macintosh                 8 - Z-System
          9 - CP/M                     10 - Windows NTFS
         11 - MVS (OS/390 - Z/OS)      12 - VSE
         13 - Acorn Risc               14 - VFAT
         15 - alternate MVS            16 - BeOS
         17 - Tandem                   18 - OS/400
         19 - OS/X (Darwin)            20 thru 255 - unused

          The lower byte indicates the ZIP specification version 
          (the version of this document) supported by the software 
          used to encode the file.  The value/10 indicates the major 
          version number, and the value mod 10 is the minor version 
          number.  

      version needed to extract (2 bytes)

          The minimum supported ZIP specification version needed to 
          extract the file, mapped as above.  This value is based on 
          the specific format features a ZIP program must support to 
          be able to extract the file.  If multiple features are
          applied to a file, the minimum version should be set to the 
          feature having the highest value. New features or feature 
          changes affecting the published format specification will be 
          implemented using higher version numbers than the last 
          published value to avoid conflict.

          Current minimum feature versions are as defined below:

          1.0 - Default value
          1.1 - File is a volume label
          2.0 - File is a folder (directory)
          2.0 - File is compressed using Deflate compression
          2.0 - File is encrypted using traditional PKWARE encryption
          2.1 - File is compressed using Deflate64(tm)
          2.5 - File is compressed using PKWARE DCL Implode
          2.7 - File is a patch data set
          4.5 - File uses ZIP64 format extensions
          4.6 - File is compressed using BZIP2 compression*
          5.0 - File is encrypted using DES
          5.0 - File is encrypted using 3DES
          5.0 - File is encrypted using original RC2 encryption
          5.0 - File is encrypted using RC4 encryption
          5.1 - File is encrypted using AES encryption
          5.1 - File is encrypted using corrected RC2 encryption**
          5.2 - File is encrypted using corrected RC2-64 encryption**
          6.1 - File is encrypted using non-OAEP key wrapping***
          6.2 - Central directory encryption
*/
	unsigned os_type = (ver >> 8) & 0xff;
	unsigned zip_ver = ver & 0xff;

	switch (os_type) {
	case 0:  Print_note(fout, "0 - MS-DOS and OS/2 (FAT / VFAT / FAT32 file systems)"); break;
	case 1:  Print_note(fout, "1 - Amiga"); break;
	case 2:  Print_note(fout, "2 - OpenVMS"); break;
	case 3:  Print_note(fout, "3 - UNIX"); break;
	case 4:  Print_note(fout, "4 - VM/CMS"); break;
	case 5:  Print_note(fout, "5 - Atari ST"); break;
	case 6:  Print_note(fout, "6 - OS/2 H.P.F.S."); break;
	case 7:  Print_note(fout, "7 - Macintosh"); break;
	case 8:  Print_note(fout, "8 - Z-System"); break;
	case 9:  Print_note(fout, "9 - CP/M"); break;
	case 10: Print_note(fout, "10 - Windows NTFS or TOPS-20(by Info-ZIP)"); break;
	case 11: Print_note(fout, "11 - MVS (OS/390 - Z/OS) or NTFS(by Info-ZIP)"); break;
	case 12: Print_note(fout, "12 - VSE or SMS/QDOS(by Info-ZIP)"); break;
	case 13: Print_note(fout, "13 - Acorn Risc"); break;
	case 14: Print_note(fout, "14 - VFAT"); break;
	case 15: Print_note(fout, "15 - alternate MVS"); break;
	case 16: Print_note(fout, "16 - BeOS"); break;
	case 17: Print_note(fout, "17 - Tandem"); break;
	case 18: Print_note(fout, "18 - OS/400"); break;
	case 19: Print_note(fout, "19 - OS/X (Darwin)"); break;
	case 30: Print_note(fout, "30 - AtheOS/Syllable(by Info-ZIP)"); break;
	default: Printf_note(fout, "%u - unused", os_type); break;
	}//.endswitch

	Printf_note(fout, "ver %u.%u", zip_ver / 10, zip_ver % 10);
}
//@}

//........................................................................
//!@name generic dump
//@{
void Dump_string(FILE* fin, FILE* fout, uint64 length)
{
	int c;
	while (length-- && (c = getc(fin)) != EOF) {
		if (iscntrl(c)) {
			fprintf(fout, "^%c", c + '@'); // print control-code as visible.
		}
		else {
			fputc(c, fout);
		}
	}
	fputc('\n', fout);
}

void Dump_bytes(FILE* fin, FILE* fout, uint64 length)
{
	int c;
	char prev[16], current[16];
	char hex_dump[16*3+1];
	char ascii_dump[16+1];
	size_t i = 0;
	uint64 offset = 0;
	uint64 omit_lines = 0;
	while (length-- && (c = getc(fin)) != EOF) {
		++offset;
		sprintf(hex_dump + i*3, "%02X%c", c, i==7 ? '-' : ' ');
		ascii_dump[i] = ascii(c);
		current[i] = c;
		if (++i >= 16) {
			if (gOmitSameHexDumpLine && offset > 16 && memcmp(prev, current, sizeof(prev)) == 0) {
				if (++omit_lines == 1)
					fprintf(fout, " *\n"); // print omit mark.
			}
			else {
				omit_lines = 0;
				ascii_dump[16] = 0;
				fprintf(fout, "+%08I64X : %-48s:%-16s\n", offset-i, hex_dump, ascii_dump);
			}
			i = 0;
			memcpy(prev, current, sizeof(prev));
		}
	}//.endwhile
	if (i != 0) {
		ascii_dump[i] = 0;
		fprintf(fout, "+%08I64X : %-48s:%-16s\n", offset-i, hex_dump, ascii_dump);
	}
}

void Dump_if_fulldump(FILE* fin, FILE* fout, const char* caption, uint64 length)
{
	if (gIsFullDump) {
		Dump_bytes(fin, fout, length);
	}
	else {
		_fseeki64(fin, length, SEEK_CUR);
		if (!gQuiet) fprintf(fout, "; skip %s(%I64u bytes), use -f option to dump the data\n", caption, length);
	}
}
//@}

//........................................................................
//!@name 未知データのスキップ処理.
//@{
/** 指定のバイト数を読み飛ばす */
void SkipUnknownData(FILE* fin, FILE* fout, uint64 skipsize)
{
	fprintf(fout, "!! Skip unknown data %I64u(0x%I64X) bytes\n", skipsize, skipsize);
	if (gIsFullDump) {
		Dump_bytes(fin, fout, skipsize);
	}
	else {
		_fseeki64(fin, skipsize, SEEK_CUR);
	}
}

/** 次のZIP構造ヘッダまで読み飛ばす. */
bool SkipToNextPK(FILE* fin, FILE* fout)
{
	__int64 skipsize = 0;
	int c;
	int prev = 0;

	while ((c = getc(fin)) != EOF) {
		if (prev == 'P' && c == 'K') {
			_fseeki64(fin, -2, SEEK_CUR);
			--skipsize;
			break;
		}
		prev = c;
		++skipsize;
	}
	if (skipsize > 0) {
		_fseeki64(fin, -skipsize, SEEK_CUR);
		SkipUnknownData(fin, fout, skipsize);
	}
	return !feof(fin);
}
//@}

//........................................................................
//!@name ZIP構造ヘッダのダンプ処理.
//@{
void Dump_extra_Zip64_Extended_Info(FILE* fin, FILE* fout, size_t length)
{
/*
         -Zip64 Extended Information Extra Field (0x0001):

          The following is the layout of the zip64 extended 
          information "extra" block. If one of the size or
          offset fields in the Local or Central directory
          record is too small to hold the required data,
          a Zip64 extended information record is created.
          The order of the fields in the zip64 extended 
          information record is fixed, but the fields will
          only appear if the corresponding Local or Central
          directory record field is set to 0xFFFF or 0xFFFFFFFF.

          Note: all fields stored in Intel low-byte/high-byte order.

          Value      Size       Description
          -----      ----       -----------
  (ZIP64) 0x0001     2 bytes    Tag for this "extra" block type
          Size       2 bytes    Size of this "extra" block
          Original 
          Size       8 bytes    Original uncompressed file size
          Compressed
          Size       8 bytes    Size of compressed data
          Relative Header
          Offset     8 bytes    Offset of local header record
          Disk Start
          Number     4 bytes    Number of the disk on which
                                this file starts 

          This entry in the Local header must include BOTH original
          and compressed file size fields. If encrypting the 
          central directory and bit 13 of the general purpose bit
          flag is set indicating masking, the value stored in the
          Local Header for the original file size will be zero.
*/
	uint32 w32;
	uint64 w64;
	size_t offset = 0;

	//    "12345678901234567890123456789012",
	DUMP8("Original Size",                   w64, ux); offset += 8;
	DUMP8("Compressed Size",                 w64, ux); offset += 8;
	DUMP8("Relative Header Offset",          w64, ux); offset += 8;
	DUMP4("Disk Start Number",               w32,  u); offset += 4;

	if (length > offset) {
		SkipUnknownData(fin, fout, length - offset);
	}
}

void Dump_extra_OS2_Extended_Attributes(FILE* fin, FILE* fout, size_t length)
{
/*
         -OS/2 Extra Field (0x0009):

          The following is the layout of the OS/2 attributes "extra" 
          block.  (Last Revision  09/05/95)

          Note: all fields stored in Intel low-byte/high-byte order.

          Value       Size          Description
          -----       ----          -----------
  (OS/2)  0x0009      2 bytes       Tag for this "extra" block type
          TSize       2 bytes       Size for the following data block
          BSize       4 bytes       Uncompressed Block Size
          CType       2 bytes       Compression type
          EACRC       4 bytes       CRC value for uncompress block
          (var)       variable      Compressed block

          The OS/2 extended attribute structure (FEA2LIST) is 
          compressed and then stored in it's entirety within this 
          structure.  There will only ever be one "block" of data in 
          VarFields[].

  [Info-ZIP appnote]
         -OS/2 Extended Attributes Extra Field (0x0009):
          =============================================

          The following is the layout of the OS/2 extended attributes "extra"
          block.  (Last Revision 19960922)

          Note: all fields stored in Intel low-byte/high-byte order.

          Local-header version:

          Value         Size        Description
          -----         ----        -----------
  (OS/2)  0x0009        Short       tag for this extra block type
          TSize         Short       total data size for this block
          BSize         Long        uncompressed EA data size
          CType         Short       compression type
          EACRC         Long        CRC value for uncompressed EA data
          (var.)        variable    compressed EA data

          Central-header version:

          Value         Size        Description
          -----         ----        -----------
  (OS/2)  0x0009        Short       tag for this extra block type
          TSize         Short       total data size for this block (4)
          BSize         Long        size of uncompressed local EA data

          The value of CType is interpreted according to the "compression
          method" section above; i.e., 0 for stored, 8 for deflated, etc.

          The OS/2 extended attribute structure (FEA2LIST) is
          compressed and then stored in its entirety within this
          structure.  There will only ever be one "block" of data in
          the variable-length field.

*/
	uint16 w16;
	uint32 w32;
	size_t offset = 0;

	//    "12345678901234567890123456789012",
	DUMP4("uncompressed EA data size",       w32, ux); offset += 4;
	if (length <= offset) return;
	DUMP2("compression type",                w16,  u); offset += 2;
	DUMP4("CRC",                             w32,  x); offset += 4;

	if (length > offset) {
		fputs("compressed EA data:\n", fout);
		Dump_if_fulldump(fin, fout, "compressed EA data", length - offset);
	}
}

void Dump_extra_NTFS(FILE* fin, FILE* fout, size_t length)
{
/*
         -NTFS Extra Field (0x000a):

          The following is the layout of the NTFS attributes 
          "extra" block. (Note: At this time the Mtime, Atime
          and Ctime values may be used on any WIN32 system.)  

          Note: all fields stored in Intel low-byte/high-byte order.

          Value      Size       Description
          -----      ----       -----------
  (NTFS)  0x000a     2 bytes    Tag for this "extra" block type
          TSize      2 bytes    Size of the total "extra" block
          Reserved   4 bytes    Reserved for future use
          Tag1       2 bytes    NTFS attribute tag value #1
          Size1      2 bytes    Size of attribute #1, in bytes
          (var.)     Size1      Attribute #1 data
          .
          .
          .
          TagN       2 bytes    NTFS attribute tag value #N
          SizeN      2 bytes    Size of attribute #N, in bytes
          (var.)     SizeN      Attribute #N data

          For NTFS, values for Tag1 through TagN are as follows:
          (currently only one set of attributes is defined for NTFS)

          Tag        Size       Description
          -----      ----       -----------
          0x0001     2 bytes    Tag for attribute #1 
          Size1      2 bytes    Size of attribute #1, in bytes
          Mtime      8 bytes    File last modification time
          Atime      8 bytes    File last access time
          Ctime      8 bytes    File creation time
*/
	uint16 tag, size;
	uint32 w32;
	ULARGE_INTEGER ii;
	size_t extra_offset = 0;

	DUMP4("reserved", w32, x); extra_offset += 4;
	while (Read16(fin, tag) && Read16(fin, size)) {
		size_t tag_offset = 0;
		switch (tag) {
		case 1:
			Print_extra(fout, "NTFS file time", tag, size);
			//     "12345678901234567890123456789012",
			DUMP8x("last mod time",      ii.QuadPart, x, Print_filetime(fout, ii)); tag_offset += 8;
			DUMP8x("last access time",   ii.QuadPart, x, Print_filetime(fout, ii)); tag_offset += 8;
			DUMP8x("last creation time", ii.QuadPart, x, Print_filetime(fout, ii)); tag_offset += 8;
			break;

		default:
			Print_extra(fout, "!! Unknown NTFS Extra Field", tag, size);
			Dump_bytes(fin, fout, size); tag_offset += size;
			break;
		}//.endswitch tag
		if (size > tag_offset) {
			SkipUnknownData(fin, fout, size - tag_offset);
		}
		extra_offset += size;
	}//.endwhile

	if (length > extra_offset) {
		SkipUnknownData(fin, fout, length - extra_offset);
	}
}

void Dump_extra_WindowsNT_SD(FILE* fin, FILE* fout, size_t length)
{
/*
         -Windows NT Security Descriptor Extra Field (0x4453):
          ===================================================

          The following is the layout of the NT Security Descriptor (another
          type of ACL) extra block.  (Last Revision 19960922)

          Local-header version:

          Value         Size        Description
          -----         ----        -----------
  (SD)    0x4453        Short       tag for this extra block type ("SD")
          TSize         Short       total data size for this block
          BSize         Long        uncompressed SD data size
          Version       Byte        version of uncompressed SD data format
          CType         Short       compression type
          EACRC         Long        CRC value for uncompressed SD data
          (var.)        variable    compressed SD data

          Central-header version:

          Value         Size        Description
          -----         ----        -----------
  (SD)    0x4453        Short       tag for this extra block type ("SD")
          TSize         Short       total data size for this block (4)
          BSize         Long        size of uncompressed local SD data
*/
	uint32 w32;
	uint8 ver;
	uint16 w16;
	size_t offset = 0;

	//    "12345678901234567890123456789012",
	DUMP4("uncompressed SD data size",       w32, ux); offset += 4;
	if (length <= offset) return;

	DUMP1("version",                         ver,  u); offset += 1;
	DUMP2("compression type",                w16,  u); offset += 2;
	DUMP4("crc",                             w32,  x); offset += 4;

	if (length > offset) {
		fputs("compressed SD data:\n", fout);
		Dump_if_fulldump(fin, fout, "compressed SD data", length - offset);
	}
}

void Dump_extra_ExtendedTimestamp(FILE* fin, FILE* fout, size_t length)
{
/*
         -Extended Timestamp Extra Field:
          ==============================

          The following is the layout of the extended-timestamp extra block.
          (Last Revision 19970118)

          Local-header version:

          Value         Size        Description
          -----         ----        -----------
  (time)  0x5455        Short       tag for this extra block type ("UT")
          TSize         Short       total data size for this block
          Flags         Byte        info bits
          (ModTime)     Long        time of last modification (UTC/GMT)
          (AcTime)      Long        time of last access (UTC/GMT)
          (CrTime)      Long        time of original creation (UTC/GMT)

          Central-header version:

          Value         Size        Description
          -----         ----        -----------
  (time)  0x5455        Short       tag for this extra block type ("UT")
          TSize         Short       total data size for this block
          Flags         Byte        info bits (refers to local header!)
          (ModTime)     Long        time of last modification (UTC/GMT)
*/
	uint8 flags;
	uint32 time;
	size_t offset = 0;

	DUMP1("Flags", flags, x); offset += 1;

	if ((flags & 1) && length > offset) { DUMP4x("last mod time",    time, x, Print_UTC(fout, time)); offset += 4; }
	if ((flags & 2) && length > offset) { DUMP4x("last access time", time, x, Print_UTC(fout, time)); offset += 4; }
	if ((flags & 4) && length > offset) { DUMP4x("last create time", time, x, Print_UTC(fout, time)); offset += 4; }

	if (length > offset) {
		SkipUnknownData(fin, fout, length - offset);
	}
}

void Dump_extra_UnicodeComment(FILE* fin, FILE* fout, size_t length)
{
/*
         -Info-ZIP Unicode Comment Extra Field (0x6375):

          Stores the UTF-8 version of the file comment as stored in the
          central directory header. (Last Revision 20070912)

          Value         Size        Description
          -----         ----        -----------
   (UCom) 0x6375        Short       tag for this extra block type ("uc")
          TSize         Short       total data size for this block
          Version       1 byte      version of this extra field, currently 1
          ComCRC32      4 bytes     Comment Field CRC32 Checksum
          UnicodeCom    Variable    UTF-8 version of the entry comment

          Currently Version is set to the number 1.  If there is a need
          to change this field, the version will be incremented.  Changes
          may not be backward compatible so this extra field should not be
          used if the version is not recognized.

          The ComCRC32 is the standard zip CRC32 checksum of the File Comment
          field in the central directory header.  This is used to verify that
          the comment field has not changed since the Unicode Comment extra field
          was created.  This can happen if a utility changes the File Comment 
          field but does not update the UTF-8 Comment extra field.  If the CRC 
          check fails, this Unicode Comment extra field should be ignored and 
          the File Comment field in the header should be used instead.

          The UnicodeCom field is the UTF-8 version of the File Comment field
          in the header.  As UnicodeCom is defined to be UTF-8, no UTF-8 byte
          order mark (BOM) is used.  The length of this field is determined by
          subtracting the size of the previous fields from TSize.  If both the
          File Name and Comment fields are UTF-8, the new General Purpose Bit
          Flag, bit 11 (Language encoding flag (EFS)), can be used to indicate
          both the header File Name and Comment fields are UTF-8 and, in this
          case, the Unicode Path and Unicode Comment extra fields are not
          needed and should not be created.  Note that, for backward
          compatibility, bit 11 should only be used if the native character set
          of the paths and comments being zipped up are already in UTF-8. It is
          expected that the same file comment storage method, either general
          purpose bit 11 or extra fields, be used in both the Local and Central
          Directory Header for a file.
*/
	uint8 ver;
	uint32 w32;
	size_t offset = 0;

	DUMP1("version", ver, u); offset += 1;
	DUMP4("crc",     w32, x); offset += 4;
	if (length > offset) {
		fputs("entry comment encoded UTF-8:\n", fout);
		Dump_bytes(fin, fout, length - offset);
	}
}

void Dump_extra_UnicodePath(FILE* fin, FILE* fout, size_t length)
{
/*
         -Info-ZIP Unicode Path Extra Field (0x7075):

          Stores the UTF-8 version of the file name field as stored in the
          local header and central directory header. (Last Revision 20070912)

          Value         Size        Description
          -----         ----        -----------
  (UPath) 0x7075        Short       tag for this extra block type ("up")
          TSize         Short       total data size for this block
          Version       1 byte      version of this extra field, currently 1
          NameCRC32     4 bytes     File Name Field CRC32 Checksum
          UnicodeName   Variable    UTF-8 version of the entry File Name

          Currently Version is set to the number 1.  If there is a need
          to change this field, the version will be incremented.  Changes
          may not be backward compatible so this extra field should not be
          used if the version is not recognized.

          The NameCRC32 is the standard zip CRC32 checksum of the File Name
          field in the header.  This is used to verify that the header
          File Name field has not changed since the Unicode Path extra field
          was created.  This can happen if a utility renames the File Name but
          does not update the UTF-8 path extra field.  If the CRC check fails,
          this UTF-8 Path Extra Field should be ignored and the File Name field
          in the header should be used instead.

          The UnicodeName is the UTF-8 version of the contents of the File Name
          field in the header.  As UnicodeName is defined to be UTF-8, no UTF-8
          byte order mark (BOM) is used.  The length of this field is determined
          by subtracting the size of the previous fields from TSize.  If both
          the File Name and Comment fields are UTF-8, the new General Purpose
          Bit Flag, bit 11 (Language encoding flag (EFS)), can be used to
          indicate that both the header File Name and Comment fields are UTF-8
          and, in this case, the Unicode Path and Unicode Comment extra fields
          are not needed and should not be created.  Note that, for backward
          compatibility, bit 11 should only be used if the native character set
          of the paths and comments being zipped up are already in UTF-8. It is
          expected that the same file name storage method, either general
          purpose bit 11 or extra fields, be used in both the Local and Central
          Directory Header for a file.
*/
	uint8 ver;
	uint32 w32;
	size_t offset = 0;

	DUMP1("version", ver, u); offset += 1;
	DUMP4("crc",     w32, x); offset += 4;
	if (length > offset) {
		fputs("file name encoded UTF-8:\n", fout);
		Dump_bytes(fin, fout, length - offset);
	}
}

void Dump_extra_field(FILE* fin, FILE* fout, size_t length)
{
/*
          In order to allow different programs and different types
          of information to be stored in the 'extra' field in .ZIP
          files, the following structure should be used for all
          programs storing data in this field:

          header1+data1 + header2+data2 . . .

          Each header should consist of:

            Header ID - 2 bytes
            Data Size - 2 bytes

          Note: all fields stored in Intel low-byte/high-byte order.
*/
	uint16 id, size;
	size_t offset = 0;
	while (offset < length && Read16(fin, id) && Read16(fin, size)) {
		switch (id) {
		case 0x0001:
			Print_extra(fout, "Zip64 Extended Information Extra Field", id, size);
			Dump_extra_Zip64_Extended_Info(fin, fout, size);
			break;
		case 0x0009:
			Print_extra(fout, "OS/2 Extended Attributes Extra Field", id, size);
			Dump_extra_OS2_Extended_Attributes(fin, fout, size);
			break;

		case 0x000a:
			Print_extra(fout, "NTFS Extra Field", id, size);
			Dump_extra_NTFS(fin, fout, size);
			break;

		case 0x4453:
			Print_extra(fout, "Windows NT Security Descriptor Extra Field", id, size);
			Dump_extra_WindowsNT_SD(fin, fout, size);
			break;
		case 0x5455:
			Print_extra(fout, "Extended Timestamp Extra Field", id, size);
			Dump_extra_ExtendedTimestamp(fin, fout, size);
			break;
		case 0x6375:
			Print_extra(fout, "Info-ZIP Unicode Comment Extra Field", id, size);
			Dump_extra_UnicodeComment(fin, fout, size);
			break;
		case 0x7075:
			Print_extra(fout, "Info-ZIP Unicode Path Extra Field", id, size);
			Dump_extra_UnicodePath(fin, fout, size);
			break;
		default:
			Print_extra(fout, "!! Unknown Extra Field", id, size);
			Dump_bytes(fin, fout, size);
			break;
		}
		offset += size + 4;
	}
}

void Dump_Local_file(FILE* fin, FILE* fout, int n)
{
/*
  A.  Local file header:

        local file header signature     4 bytes  (0x04034b50)
        version needed to extract       2 bytes
        general purpose bit flag        2 bytes
        compression method              2 bytes
        last mod file time              2 bytes
        last mod file date              2 bytes
        crc-32                          4 bytes
        compressed size                 4 bytes
        uncompressed size               4 bytes
        file name length                2 bytes
        extra field length              2 bytes

        file name (variable size)
        extra field (variable size)
*/
	uint16 w16, flags=0, method=0, mod_time=0, mod_date=0, file_name_length=0, extra_field_length=0;
	uint32 w32, compressed_size=0;

	//     "12345678901234567890123456789012",
	DUMP2x("version needed to extract",       w16,                 x, Print_version(fout, w16));
	DUMP2 ("general purpose bit flag",        flags,               x);
	DUMP2x("compression method",              method,              x, Print_general_purpose_bit_flag(fout, flags, method));
	DUMP2 ("last mod file time",              mod_time,            x);
	DUMP2x("last mod file date",              mod_date,            x, Print_date_and_time(fout, mod_time, mod_date));
	DUMP4 ("crc-32",                          w32,                 x); // ZIP64では、0xFFFFFFFFに固定する.
	DUMP4 ("compressed size",                 compressed_size,    ux); // ZIP64では、0xFFFFFFFFに固定する.
	DUMP4 ("uncompressed size",               w32,                ux); // ZIP64では、0xFFFFFFFFに固定する.
	DUMP2 ("file name length",                file_name_length,   ux); // 65,535以上は不可.
	DUMP2 ("extra field length",              extra_field_length, ux); // 65,535以上は不可.
	if (file_name_length) {
		Print_section(fout, "Local file name", _ftelli64(fin), n);
		Dump_string(fin, fout, file_name_length);
	}
	if (extra_field_length) {
		Print_section(fout, "Local extra field", _ftelli64(fin), n);
		Dump_extra_field(fin, fout, extra_field_length);
	}

/*
  B.  File data

      Immediately following the local header for a file
      is the compressed or stored data for the file. 
      The series of [local file header][file data][data
      descriptor] repeats for each file in the .ZIP archive. 
*/
	if (compressed_size == 0xFFFFFFFF)
		return;	// ZIP64 フォーマットのため、後続データのサイズが不明なので File data と Data descriptor のダンプは止める.

	if (compressed_size) {
		Print_section(fout, "File data", _ftelli64(fin), n);
		Dump_if_fulldump(fin, fout, "file data", compressed_size);
	}

	if (flags & 0x0008) { // Bit 3: on
		Print_section(fout, "Data descriptor", _ftelli64(fin), n);
		Dump_Data_descriptor(fin, fout);
	}
}

void Dump_Data_descriptor(FILE* fin, FILE* fout)
{
/*
  C.  Data descriptor:

      [Info-ZIP discrepancy:
       The Info-ZIP zip program starts the data descriptor with a 4-byte
       PK-style signature.  Despite the specification, none of the PKWARE
       programs supports the data descriptor.  PKZIP 4.0 -fix function
       (and PKZIPFIX 2.04) ignores the data descriptor info even when bit 3
       of the general purpose bit flag is set.
        data descriptor signature       4 bytes  (0x08074b50)
      ]

        crc-32                          4 bytes
        compressed size                 4 bytes
        uncompressed size               4 bytes

      This descriptor exists only if bit 3 of the general
      purpose bit flag is set (see below).  It is byte aligned
      and immediately follows the last byte of compressed data.
      This descriptor is used only when it was not possible to
      seek in the output .ZIP file, e.g., when the output .ZIP file
      was standard output or a non-seekable device.  For ZIP64(tm) format
      archives, the compressed and uncompressed sizes are 8 bytes each.

      When compressing files, compressed and uncompressed sizes 
      should be stored in ZIP64 format (as 8 byte values) when a 
      files size exceeds 0xFFFFFFFF.   However ZIP64 format may be 
      used regardless of the size of a file.  When extracting, if 
      the zip64 extended information extra field is present for 
      the file the compressed and uncompressed sizes will be 8
      byte values.  
*/
	uint32 w32;
	//    "12345678901234567890123456789012",
	DUMP4("crc-32",                          w32,  x); // ZIP64では、0xFFFFFFFFに固定する.
	DUMP4("compressed size",                 w32, ux); // ZIP64では、0xFFFFFFFFに固定する.
	DUMP4("uncompressed size",               w32, ux); // ZIP64では、0xFFFFFFFFに固定する.
}

void Dump_Archive_extra_data_record(FILE* fin, FILE* fout)
{
/*
  E.  Archive extra data record: 

        archive extra data signature    4 bytes  (0x08064b50)
        extra field length              4 bytes
        extra field data                (variable size)

      The Archive Extra Data Record is introduced in version 6.2
      of the ZIP format specification.  This record exists in support
      of the Central Directory Encryption Feature implemented as part of 
      the Strong Encryption Specification as described in this document.
      When present, this record immediately precedes the central 
      directory data structure.  The size of this data record will be
      included in the Size of the Central Directory field in the
      End of Central Directory record.  If the central directory structure
      is compressed, but not encrypted, the location of the start of
      this data record is determined using the Start of Central Directory
      field in the Zip64 End of Central Directory record.  
*/
	uint32 extra_field_length = 0;

	DUMP4("extra field length", extra_field_length, ux);
	if (extra_field_length) {
		Print_section(fout, "extra field data", _ftelli64(fin));
		Dump_extra_field(fin, fout, extra_field_length);
	}
}

void Dump_Central_directory_file_header(FILE* fin, FILE* fout, int n)
{
/*
  F.  Central directory structure:

      [file header 1]
      .
      .
      . 
      [file header n]
      [digital signature] 

      File header:

        central file header signature   4 bytes  (0x02014b50)
        version made by                 2 bytes
        version needed to extract       2 bytes
        general purpose bit flag        2 bytes
        compression method              2 bytes
        last mod file time              2 bytes
        last mod file date              2 bytes
        crc-32                          4 bytes
        compressed size                 4 bytes
        uncompressed size               4 bytes
        file name length                2 bytes
        extra field length              2 bytes
        file comment length             2 bytes
        disk number start               2 bytes
        internal file attributes        2 bytes
        external file attributes        4 bytes
        relative offset of local header 4 bytes

        file name (variable size)
        extra field (variable size)
        file comment (variable size)
*/
	uint16 w16, flags=0, method=0, mod_time=0, mod_date=0, file_name_length=0, extra_field_length=0, file_comment_length=0;
	uint32 w32, compressed_size=0;

	//     "12345678901234567890123456789012",
	DUMP2x("version made by",                 w16,                  x, Print_version(fout, w16));
	DUMP2x("version needed to extract",       w16,                  x, Print_version(fout, w16));
	DUMP2 ("general purpose bit flag",        flags,                x);
	DUMP2x("compression method",              method,               x, Print_general_purpose_bit_flag(fout, flags, method));
	DUMP2 ("last mod file time",              mod_time,             x);
	DUMP2x("last mod file date",              mod_date,             x, Print_date_and_time(fout, mod_time, mod_date));
	DUMP4 ("crc-32",                          w32,                  x); // ZIP64では、0xFFFFFFFFに固定する.
	DUMP4 ("compressed size",                 compressed_size,     ux); // ZIP64では、0xFFFFFFFFに固定する.
	DUMP4 ("uncompressed size",               w32,                 ux); // ZIP64では、0xFFFFFFFFに固定する.
	DUMP2 ("file name length",                file_name_length,    ux); // 65,535以上は不可.
	DUMP2 ("extra field length",              extra_field_length,  ux); // 65,535以上は不可.
	DUMP2 ("file comment length",             file_comment_length, ux); // 65,535以上は不可.
	DUMP2 ("disk number start",               w16,                uFF); // ZIP64では、0xFFFFに固定する.
	DUMP2x("internal file attributes",        w16,                  x, Print_internal_file_attributes(fout, w16));
	DUMP4x("external file attributes",        w32,                  x, Print_external_file_attributes(fout, w32));
	DUMP4 ("relative offset of local header", w32,                 ux); // ZIP64では、0xFFFFFFFFに固定する.

	if (file_name_length) {
		Print_section(fout, "file name", _ftelli64(fin), n);
		Dump_string(fin, fout, file_name_length);
	}
	if (extra_field_length) {
		Print_section(fout, "extra field", _ftelli64(fin), n);
		Dump_extra_field(fin, fout, extra_field_length);
	}
	if (file_comment_length) {
		Print_section(fout, "file comment", _ftelli64(fin), n);
		Dump_string(fin, fout, file_comment_length);
	}
}

void Dump_Central_directory_digital_signature(FILE* fin, FILE* fout)
{
/*
      Digital signature:

        header signature                4 bytes  (0x05054b50)
        size of data                    2 bytes
        signature data (variable size)

      With the introduction of the Central Directory Encryption 
      feature in version 6.2 of this specification, the Central 
      Directory Structure may be stored both compressed and encrypted. 
      Although not required, it is assumed when encrypting the
      Central Directory Structure, that it will be compressed
      for greater storage efficiency.  Information on the
      Central Directory Encryption feature can be found in the section
      describing the Strong Encryption Specification. The Digital 
      Signature record will be neither compressed nor encrypted.
*/
	uint16 size = 0;

	DUMP2("size of data", size, ux);
	if (size) {
		Print_section(fout, "signature data", _ftelli64(fin));
		Dump_bytes(fin, fout, size);
	}
}

void Dump_Zip64_end_of_central_directory_record(FILE* fin, FILE* fout)
{
/*
  G.  Zip64 end of central directory record

        zip64 end of central dir 
        signature                       4 bytes  (0x06064b50)
        size of zip64 end of central
        directory record                8 bytes
        version made by                 2 bytes
        version needed to extract       2 bytes
        number of this disk             4 bytes
        number of the disk with the 
        start of the central directory  4 bytes
        total number of entries in the
        central directory on this disk  8 bytes
        total number of entries in the
        central directory               8 bytes
        size of the central directory   8 bytes
        offset of start of central
        directory with respect to
        the starting disk number        8 bytes
        zip64 extensible data sector    (variable size)

        The value stored into the "size of zip64 end of central
        directory record" should be the size of the remaining
        record and should not include the leading 12 bytes.
  
        Size = SizeOfFixedFields + SizeOfVariableData - 12.

        The above record structure defines Version 1 of the 
        zip64 end of central directory record. Version 1 was 
        implemented in versions of this specification preceding 
        6.2 in support of the ZIP64 large file feature. The 
        introduction of the Central Directory Encryption feature 
        implemented in version 6.2 as part of the Strong Encryption 
        Specification defines Version 2 of this record structure. 
        Refer to the section describing the Strong Encryption 
        Specification for details on the version 2 format for 
        this record.

        Special purpose data may reside in the zip64 extensible data
        sector field following either a V1 or V2 version of this
        record.  To ensure identification of this special purpose data
        it must include an identifying header block consisting of the
        following:

           Header ID  -  2 bytes
           Data Size  -  4 bytes

        The Header ID field indicates the type of data that is in the 
        data block that follows.

        Data Size identifies the number of bytes that follow for this
        data block type.

        Multiple special purpose data blocks may be present, but each
        must be preceded by a Header ID and Data Size field.  Current
        mappings of Header ID values supported in this field are as
        defined in APPENDIX C.

*/
	uint16 w16;
	uint32 w32;
	uint64 w64;
	uint64 size = 0;

	//     "12345678901234567890123456789012",
	DUMP8 ("size of this record",             size, ux);
	DUMP2x("version made by",                 w16,  x, Print_version(fout, w16));
	DUMP2x("version needed to extract",       w16,  x, Print_version(fout, w16));
	DUMP4 ("number of this disk",             w32,  u);
	DUMP4 ("disk of starting directory",      w32,  u);
	DUMP8 ("directory-entries on this disk",  w64,  u);
	DUMP8 ("directory-entries in all disks",  w64,  u);
	DUMP8 ("size of the directory",           w64,  ux);
	DUMP8 ("offset of starting directory",    w64,  ux);

	Print_section(fout, "zip64 extensible data sector", _ftelli64(fin));
	Dump_bytes(fin, fout, size - 2*2 - 4*2 - 8*4); // sizeフィールド以後の固定長を減ずる.
	// Todo: ID(2),SIZE(4) の詳細ダンプを行う.
}

void Dump_Zip64_end_of_central_directory_locator(FILE* fin, FILE* fout)
{
/*
  H.  Zip64 end of central directory locator

        zip64 end of central dir locator 
        signature                       4 bytes  (0x07064b50)
        number of the disk with the
        start of the zip64 end of 
        central directory               4 bytes
        relative offset of the zip64
        end of central directory record 8 bytes
        total number of disks           4 bytes
*/
	uint32 w32;
	uint64 w64;

	//    "12345678901234567890123456789012",
	DUMP4("disk of starting directory",      w32, u);
	DUMP8("relative offset of zip64 record", w64, ux);
	DUMP4("total number of disks",           w32, u);
}

void Dump_End_of_central_directory_record(FILE* fin, FILE* fout)
{
/*
  I.  End of central directory record:

        end of central dir signature    4 bytes  (0x06054b50)
        number of this disk             2 bytes
        number of the disk with the
        start of the central directory  2 bytes
        total number of entries in the
        central directory on this disk  2 bytes
        total number of entries in
        the central directory           2 bytes
        size of the central directory   4 bytes
        offset of start of central
        directory with respect to
        the starting disk number        4 bytes
        .ZIP file comment length        2 bytes
        .ZIP file comment       (variable size)
*/
	uint16 w16, zipfile_comment_length = 0;
	uint32 w32;

	//    "12345678901234567890123456789012",
	DUMP2("number of this disk",             w16, uFF); // ZIP64では、0xFFFFに固定する.
	DUMP2("disk of starting directory",      w16, uFF); // ZIP64では、0xFFFFに固定する.
	DUMP2("directory-entries on this disk",  w16, uFF); // ZIP64では、0xFFFFに固定する.
	DUMP2("directory-entries in all disks",  w16, uFF); // ZIP64では、0xFFFFに固定する.
	DUMP4("size of the directory",           w32, ux);  // ZIP64では、0xFFFFFFFFに固定する.
	DUMP4("offset of starting directory",    w32, ux);  // ZIP64では、0xFFFFFFFFに固定する.
	DUMP2(".ZIP file comment length", zipfile_comment_length, ux); // 65,535以上は不可.
	if (zipfile_comment_length) {
		Print_section(fout, ".ZIP file comment", _ftelli64(fin));
		Dump_string(fin, fout, zipfile_comment_length);
	}
}
//@}

//........................................................................
/** finからのPKZIPファイル入力に対して、foutにダンプ出力する. */
void ZipDumpFile(FILE* fin, FILE* fout)
{
	uint32 signature = 0;
	int file_count = 0;
	int dir_count = 0;

	while (SkipToNextPK(fin, fout)) {
		__int64 offset = _ftelli64(fin);
		if (!Read32(fin, signature)) {
			continue;
		}
		switch (signature) {
		case 0x04034b50:
			Print_header(fout, "Local file header", signature, offset, ++file_count);
			Dump_Local_file(fin, fout, file_count);
			break;

		case 0x08074b50:
			Print_header(fout, "Data descriptor header", signature, offset, file_count);
			Dump_Data_descriptor(fin, fout);
			break;

		case 0x08064b50:
			Print_header(fout, "Archive extra data record", signature, offset);
			Dump_Archive_extra_data_record(fin, fout);
			break;
		case 0x02014b50:
			Print_header(fout, "Central file header", signature, offset, ++dir_count);
			Dump_Central_directory_file_header(fin, fout, dir_count);
			break;
		case 0x05054b50:
			Print_header(fout, "Digital signature", signature, offset);
			Dump_Central_directory_digital_signature(fin, fout);
			break;
		case 0x06064b50:
			Print_header(fout, "Zip64 end of central directory record", signature, offset);
			Dump_Zip64_end_of_central_directory_record(fin, fout);
			break;
		case 0x07064b50:
			Print_header(fout, "Zip64 end of central directory locator", signature, offset);
			Dump_Zip64_end_of_central_directory_locator(fin, fout);
			break;
		case 0x06054b50:
			Print_header(fout, "End of central directory record", signature, offset);
			Dump_End_of_central_directory_record(fin, fout);
			break;
		default:
			Print_header(fout, "!! Unknown record", signature, offset);
			break;
		}//.endswitch signature
	}//.endwhile NextPK
}

//........................................................................
// ZIPファイルダンプのメイン関数.
/** fnameを読み込み、コメントと余分な空白を除去し、fname+".zipdump"に出力する. */
void DumpMain(const char* fname)
{
	FILE* fin = OpenInput(fname);
	FILE* fout;
	if (gIsStdout) {
		fout = stdout;
		printf("<<< %s >>> begin.\n", fname);
	}
	else {
		fout = OpenOutput(fname, ".zipdump");
	}
	fprintf(fout, "*** zipdump of \"%s\" ***\n", fname);

	ZipDumpFile(fin, fout);

	if (ferror(fin)) {
		print_win32error(fname);
	}
	fclose(fin);
	if (gIsStdout) {
		printf("<<< %s >>> end.\n\n", fname);
	}
	else {
		fclose(fout);
	}
}

/** ワイルドカード展開と再帰探索付きの DumpMain */
void DumpWildMain(const char* fname)
{
	if (strpbrk(fname, "*?") == NULL) {
		//----- ワイルドカードを含まないパス名の処理
		DumpMain(fname);
	}
	else {
		//----- ワイルドカードを含むパス名の処理
		char path[MY_MAX_PATH + 1000];
		char drv[_MAX_DRIVE];
		char dir[MY_MAX_PATH + 1000];
		char base[MY_MAX_PATH];
		char ext[MY_MAX_PATH];
		_splitpath(fname, drv, dir, base, ext);

		_finddata_t find;
		long h = _findfirst(fname, &find);
		if (h != -1) {
			do {
				if (find.attrib & _A_SUBDIR)
					continue;
				_makepath(path, drv, dir, find.name, NULL);
				// fprintf(stderr, "zipdump: %s\n", find.name);
				DumpMain(path);
			} while (_findnext(h, &find) == 0);
			_findclose(h);
		}
		if (!gIsRecursive)
			return;

		// サブフォルダを検索し、それぞれに対して再帰する
		_makepath(path, drv, dir, "*.*", NULL);
		h = _findfirst(path, &find);
		if (h != -1) {
			do {
				if (!(find.attrib & _A_SUBDIR))
					continue;
				if (strequ(find.name, ".") || strequ(find.name, ".."))
					continue;
				_makepath(path, drv, dir, find.name, NULL);
				strcat(path, "\\");
				strcat(path, base);
				strcat(path, ext);
				// fprintf(stderr, "zipdump recursive: %s\n", path);
				DumpWildMain(path); // 再帰呼び出し.
			} while (_findnext(h, &find) == 0);
			_findclose(h);
		}
	}
}

//------------------------------------------------------------------------
/** メイン関数 */
int main(int argc, char* argv[])
{
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
				case 'f':
					gIsFullDump = true;
					break;
				case 'q':
					gQuiet = true;
					break;
				case 'o':
					gOmitSameHexDumpLine = true;
					break;
				case 's':
					gIsStdout = true;
					break;
				case 'r':
					gIsRecursive = true;
					break;
				case 'd':
					gOutDir = sw+1;		// -d<DIR>
					if (!*gOutDir) {	// -d <DIR>
						gOutDir = argv[2]; ++argv; --argc;
					}
					goto next_arg;
				default:
					error_abort("unknown option.\n");
					break;
				}
			} while (*++sw);
		}
next_arg:
		++argv;
		--argc;
	}
	if (argc == 1) {
		error_abort("please specify input file.\n");
	}

	//--- 日付表示のためにカレントロカールを設定する.
	setlocale(LC_TIME, "");

	//--- コマンドライン上の各入力ファイルを処理する.
	for (int i = 1; i < argc; i++)
		DumpWildMain(argv[i]);

	return EXIT_SUCCESS;
}
//------------------------------------------------------------------------
/**@page zipdump-manual zipdump.exe - dump zip file structure

@version 1.2 (r43)

@author Hiroshi Kuno <http://code.google.com/p/win32cmdx/>

@par License:
	New BSD License
	<br>Copyright &copy; 2010 by Hiroshi Kuno
	<br>本ソフトウェアは無保証かつ無償で提供します。利用、再配布、改変は自由です。

<hr>
@section intro はじめに
	zipdumpは、ZIPファイルの構造をダンプ出力するコンソールアプリケーションです。
	壊れたZIPファイルの解析に便利です。

@section func 特徴
	- PKZIP APPNOTE.TXT Version 6.3.2[September 28, 2007] のファイルフォーマット仕様に基づき、構造単位でのダンプを行います。
	- 対象ZIPファイル名に ".zipdump" を付加した名前のテキストファイルを生成し、ダンプ結果を出力します。

@section env 動作環境
	Windows2000以降を動作対象としています。
	WindowsXP にて動作確認済み。

@section install インストール方法
	配布ファイル zipdump.exe を、PATHが通ったフォルダにコピーしてください。
	アインインストールするには、そのコピーしたファイルを削除してください。

@section usage 使い方
	@verbinclude zipdump.usage

@section example 出力例
	@verbinclude zipdump.example

@section todo 改善予定
	- "zip64 extensible data sector" の構造ダンプを行う.

@section links リンク
	- http://code.google.com/p/win32cmdx/ - zipdump開発サイト
	- http://www.pkware.com/documents/casestudies/APPNOTE.TXT - .ZIP File Format Specification
	- http://www.info-zip.org/doc/appnote-iz-latest.zip - Info-ZIP appnote

@section download ダウンロード
	- http://code.google.com/p/win32cmdx/downloads/list

@section changelog 改訂履歴
	- version-1.2 [Jan 28, 2010] 最新版
		- 主要な extra field を構造ダンプする.
		- 出力ファイルの拡張子を ".zipdump.txt" から ".zipdump" に変更した.
		- フィールド説明文が32文字を越える箇所は短い文章に改めて、出力レイアウトが崩れないようにした.
	- version-1.1 [Jan 19, 2010]
		- 文字列ダンプにて、制御コードを ^@ 形式でエスケープする.
		- big-endianマシン対応.
		- Info-ZIPによる "version made by" の解釈を加える.
		- フルダンプ時は、スキップデータもバイトダンプする.
		- 未知のsignatureレコードを、未知レコードヘッダ＋スキップデータとして出力する.
		- 同一内容のバイトダンプ行出力を抑制するオプション -o を追加した.
	- version-1.0 [Jan 17, 2010] 公開初版
*/

// zipdump.cpp - end.
