/**@file strfunc.cpp --- C string functions.
 * @author Hiroshi Kuno <http://code.google.com/p/win32cmdx/>
 */
#include <string.h>
#include <mbstring.h>

//------------------------------------------------------------------------
/** s1とs2は等しいか? */
inline bool strequ(const char* s1, const char* s2)
{
	return strcmp(s1, s2) == 0;
}

/** 大文字小文字を無視すれば, s1とs2は等しいか? */
inline bool striequ(const char* s1, const char* s2)
{
	return _mbsicmp((const uchar*)s1, (const uchar*)s2) == 0;
}

/** 大文字小文字を無視すれば, s1とs2は等しいか? */
inline bool strniequ(const char* s1, const char* s2, size_t n)
{
	return _mbsnicmp((const uchar*)s1, (const uchar*)s2, n) == 0;
}

/** 大文字小文字を無視すれば, s1の中に部分文字列s2があるか? */
inline char* stristr(const char* s1, const char* s2)
{
	int up = toupper((uchar) s2[0]);
	int lo = tolower((uchar) s2[0]);
	int len = strlen(s2);
	while (*s1) {
		const char* upper = strchr(s1, up);
		const char* lower = strchr(s1, lo);
		s1 = (!lower || (upper && upper < lower)) ? upper : lower;
		if (!s1)
			return NULL;
		if (strniequ(s1, s2, len))
			return (char*)s1;
		++s1;
	}
	return NULL;
}

//------------------------------------------------------------------------
/** s1はs2より小さいか? */
inline bool strless(const char* s1, const char* s2)
{
	return _mbscmp((const uchar*)s1, (const uchar*)s2) < 0;
}

/** 大文字小文字を無視すれば, s1はs2より小さいか? */
inline bool striless(const char* s1, const char* s2)
{
	return _mbsicmp((const uchar*)s1, (const uchar*)s2) < 0;
}

class StrILess {
public:
	bool operator()(const char* s1, const char* s2) const {
		return striless(s1, s2);
	}
};

//------------------------------------------------------------------------
/** 印刷可能文字を返す. 印刷不可能文字に対しては'.'を返す */
inline int ascii(int c)
{
	return (!iscntrl(c) && isprint(c)) ? c : '.';
}

// strfunc.cpp - end.
