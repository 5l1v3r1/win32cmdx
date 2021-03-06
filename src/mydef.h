/**@file mydef.h --- common type and constants.
 * @author Hiroshi Kuno <http://code.google.com/p/win32cmdx/>
 */
//........................................................................
//@{
/** alias of unsigned integer type */
typedef unsigned char  uchar;
typedef unsigned short ushort;
typedef unsigned int   uint;
typedef unsigned long  ulong;
typedef unsigned __int8  uint8;
typedef unsigned __int16 uint16;
typedef unsigned __int32 uint32;
typedef unsigned __int64 uint64;
//@}

//........................................................................
/** max file/path name length. Unicodeで_MAX_PATH文字なので、MBCSではその倍の長さとなる可能性がある. */
#define MY_MAX_PATH	(_MAX_PATH * 2)

// mydef.h - end.
