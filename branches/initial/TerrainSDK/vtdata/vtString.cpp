//
// String.cpp
//
// Copyright (c) 2001 Virtual Terrain Project
// Free for all uses, see license.txt for details.
//

#include "vtString.h"
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>

#ifndef MIN
#define	MIN(a, b)	((a) < (b) ? (a) : (b))
#endif

#ifndef MAX
#define	MAX(a, b)	((a) > (b) ? (a) : (b))
#endif

#define InterlockInc(i) (*(i))++
#define InterlockDec(i) --(*(i))

/////////////////////////////////////////////////////////////////////////////
// static class data, special inlines

// vtChNil is left for backward compatibility
char vtChNil = '\0';

// For an empty string, m_pchData will point here
// (note: avoids special case of checking for NULL m_pchData)
// empty string data (and locked)
/* extern */ int _vtInitData[] = { -1, 0, 0, 0 };
/* extern */ vtStringData* _vtDataNil = (vtStringData*)&_vtInitData;
pcchar _vtPchNil = (pcchar)(((unsigned char*)&_vtInitData)+sizeof(vtStringData));
// special function to make vtEmptyString work even during initialization


//////////////////////////////////////////////////////////////////////////////
// Construction/Destruction

vtString::vtString(const vtString& stringSrc) {
	if (stringSrc.GetData()->nRefs >= 0) {
		m_pchData = stringSrc.m_pchData;
		InterlockInc(&GetData()->nRefs);
		}
	else {
		Init();
		*this = stringSrc.m_pchData;
	}
}


void vtString::AllocBuffer(int nLen)
// always allocate one extra character for '\0' termination
// assumes [optimistically] that data length will equal allocation length
{
	if (nLen == 0)
		Init();
	else
	{
		vtStringData* pData;
		pData = (vtStringData*)
			new unsigned char[sizeof(vtStringData) + (nLen+1)*sizeof(char)];
		pData->nAllocLength = nLen;
		pData->nRefs = 1;
		pData->data()[nLen] = '\0';
		pData->nDataLength = nLen;
		m_pchData = pData->data();
	}
}

void vtString::FreeData(vtStringData* pData)
{
	delete[] (unsigned char*)pData;
}

void vtString::Release()
{
	if (GetData() != _vtDataNil) {
		if (InterlockDec(&GetData()->nRefs) <= 0)
			FreeData(GetData());
		Init();
	}
}

void vtString::Release(vtStringData* pData)  /*  removed PASCAL  */
{
	if (pData != _vtDataNil) {
		if (InterlockDec(&pData->nRefs) <= 0)
			FreeData(pData);
	}
}

void vtString::Empty()
{
	if (GetData()->nDataLength == 0)
		return;
	if (GetData()->nRefs >= 0)
		Release();
	else
		*this = &vtChNil;
}

void vtString::CopyBeforeWrite()
{
	if (GetData()->nRefs > 1)
	{
		vtStringData* pData = GetData();
		Release();
		AllocBuffer(pData->nDataLength);
		memcpy(m_pchData, pData->data(), (pData->nDataLength+1)*sizeof(char));
	}
}

void vtString::AllocBeforeWrite(int nLen)
{
	if (GetData()->nRefs > 1 || nLen > GetData()->nAllocLength)
	{
		Release();
		AllocBuffer(nLen);
	}
}

vtString::~vtString()
//  free any attached data
{
	if (GetData() != _vtDataNil) {
		if (InterlockDec(&GetData()->nRefs) <= 0)
			FreeData(GetData());
	}
}

//////////////////////////////////////////////////////////////////////////////
// Helpers for the rest of the implementation

void vtString::AllocCopy(vtString& dest, int nCopyLen, int nCopyIndex,
	 int nExtraLen) const
{
	// will clone the data attached to this string
	// allocating 'nExtraLen' characters
	// Places results in uninitialized string 'dest'
	// Will copy the part or all of original data to start of new string

	int nNewLen = nCopyLen + nExtraLen;
	if (nNewLen == 0)
	{
		dest.Init();
	}
	else
	{
		dest.AllocBuffer(nNewLen);
		memcpy(dest.m_pchData, m_pchData+nCopyIndex, nCopyLen*sizeof(char));
	}
}

//////////////////////////////////////////////////////////////////////////////
// More sophisticated construction

vtString::vtString(pcchar szStr)
{
	Init();
	int nLen = SafeStrlen(szStr);
	if (nLen != 0)
	{
		AllocBuffer(nLen);
		memcpy(m_pchData, szStr, nLen*sizeof(char));
	}
}


//////////////////////////////////////////////////////////////////////////////
// Assignment operators
//  All assign a new value to the string
//      (a) first see if the buffer is big enough
//      (b) if enough room, copy on top of old buffer, set size and type
//      (c) otherwise free old string data, and create a new one
//
//  All routines return the new string (but as a 'const vtString&' so that
//      assigning it again will cause a copy, eg: s1 = s2 = "hi there".
//

void vtString::AssignCopy(int nSrcLen, pcchar szSrcData)
{
	AllocBeforeWrite(nSrcLen);
	memcpy(m_pchData, szSrcData, nSrcLen*sizeof(char));
	GetData()->nDataLength = nSrcLen;
	m_pchData[nSrcLen] = '\0';
}

const vtString& vtString::operator=(const vtString& stringSrc)
{
	if (m_pchData != stringSrc.m_pchData)
	{
		if ((GetData()->nRefs < 0 && GetData() != _vtDataNil) ||
			stringSrc.GetData()->nRefs < 0)
		{
			// actual copy necessary since one of the strings is locked
			AssignCopy(stringSrc.GetData()->nDataLength, stringSrc.m_pchData);
		}
		else
		{
			// can just copy references around
			Release();
			m_pchData = stringSrc.m_pchData;
			InterlockInc(&GetData()->nRefs);
		}
	}
	return *this;
}

const vtString& vtString::operator=(pcchar lpsz)
{
	AssignCopy(SafeStrlen(lpsz), lpsz);
	return *this;
}


//////////////////////////////////////////////////////////////////////////////
// concatenation

// NOTE: "operator+" is done as friend functions for simplicity
//      There are three variants:
//          vtString + vtString
// and for ? = char, pcchar
//          vtString + ?
//          ? + vtString

void vtString::ConcatCopy(int nSrc1Len, pcchar szSrc1Data,
	int nSrc2Len, pcchar szSrc2Data)
{
  // -- master concatenation routine
  // Concatenate two sources
  // -- assume that 'this' is a new vtString object

	int nNewLen = nSrc1Len + nSrc2Len;
	if (nNewLen != 0)
	{
		AllocBuffer(nNewLen);
		memcpy(m_pchData, szSrc1Data, nSrc1Len*sizeof(char));
		memcpy(m_pchData+nSrc1Len, szSrc2Data, nSrc2Len*sizeof(char));
	}
}

vtString WIN_UNIX_STDCALL operator+(const vtString& string1, const vtString& string2) {
	vtString s;
	s.ConcatCopy(string1.GetData()->nDataLength, string1.m_pchData,
		string2.GetData()->nDataLength, string2.m_pchData);
	return s;
}

vtString WIN_UNIX_STDCALL operator+(const vtString& string, pcchar lpsz) {
	vtString s;
	s.ConcatCopy(string.GetData()->nDataLength, string.m_pchData,
		vtString::SafeStrlen(lpsz), lpsz);
	return s;
}

vtString WIN_UNIX_STDCALL operator+(pcchar lpsz, const vtString& string) {
	vtString s;
	s.ConcatCopy(vtString::SafeStrlen(lpsz), lpsz, string.GetData()->nDataLength,
		string.m_pchData);
	return s;
}

//////////////////////////////////////////////////////////////////////////////
// concatenate in place

void vtString::ConcatInPlace(int nSrcLen, pcchar szSrcData)
{
	//  -- the main routine for += operators

	// concatenating an empty string is a no-op!
	if (nSrcLen == 0)
		return;

	// if the buffer is too small, or we have a width mis-match, just
	//   allocate a new buffer (slow but sure)
	if (GetData()->nRefs > 1 || GetData()->nDataLength + nSrcLen > GetData()->nAllocLength)
	{
		// we have to grow the buffer, use the ConcatCopy routine
		vtStringData* pOldData = GetData();
		ConcatCopy(GetData()->nDataLength, m_pchData, nSrcLen, szSrcData);
		vtString::Release(pOldData);
	}
	else
	{
		// fast concatenation when buffer big enough
		memcpy(m_pchData+GetData()->nDataLength, szSrcData, nSrcLen*sizeof(char));
		GetData()->nDataLength += nSrcLen;
		m_pchData[GetData()->nDataLength] = '\0';
	}
}

const vtString& vtString::operator+=(pcchar lpsz)
{
	ConcatInPlace(SafeStrlen(lpsz), lpsz);
	return *this;
}

const vtString& vtString::operator+=(const vtString& string)
{
	ConcatInPlace(string.GetData()->nDataLength, string.m_pchData);
	return *this;
}

//////////////////////////////////////////////////////////////////////////////
// less common string expressions

vtString WIN_UNIX_STDCALL operator+(const vtString& string1, char ch)
{
	vtString s;
	s.ConcatCopy(string1.GetData()->nDataLength, string1.m_pchData, 1, &ch);
	return s;
}

vtString WIN_UNIX_STDCALL operator+(char ch, const vtString& string)
{
	vtString s;
	s.ConcatCopy(1, &ch, string.GetData()->nDataLength, string.m_pchData);
	return s;
}

///////////////////////////////////////////////////////////////////////////////
// Advanced direct buffer access

pchar vtString::GetBuffer(int nMinBufLength)
{
	if (GetData()->nRefs > 1 || nMinBufLength > GetData()->nAllocLength)
	{
		// we have to grow the buffer
		vtStringData* pOldData = GetData();
		int nOldLen = GetData()->nDataLength;   // AllocBuffer will tromp it
		if (nMinBufLength < nOldLen)
			nMinBufLength = nOldLen;
		AllocBuffer(nMinBufLength);
		memcpy(m_pchData, pOldData->data(), (nOldLen+1)*sizeof(char));
		GetData()->nDataLength = nOldLen;
		vtString::Release(pOldData);
	}

	// return a pointer to the character storage for this string
	return m_pchData;
}

void vtString::ReleaseBuffer(int nNewLength)
{
	CopyBeforeWrite();  // just in case GetBuffer was not called

	if (nNewLength == -1)
		nNewLength = strlen(m_pchData); // zero terminated

	GetData()->nDataLength = nNewLength;
	m_pchData[nNewLength] = '\0';
}

pchar vtString::GetBufferSetLength(int nNewLength)
{
	GetBuffer(nNewLength);
	GetData()->nDataLength = nNewLength;
	m_pchData[nNewLength] = '\0';
	return m_pchData;
}

void vtString::FreeExtra()
{
	if (GetData()->nDataLength != GetData()->nAllocLength)
	{
		vtStringData* pOldData = GetData();
		AllocBuffer(GetData()->nDataLength);
		memcpy(m_pchData, pOldData->data(), pOldData->nDataLength*sizeof(char));
		vtString::Release(pOldData);
	}
}

pchar vtString::LockBuffer()
{
	pchar lpsz = GetBuffer(0);
	GetData()->nRefs = -1;
	return lpsz;
}

void vtString::UnlockBuffer()
{
	if (GetData() != _vtDataNil)
		GetData()->nRefs = 1;
}

///////////////////////////////////////////////////////////////////////////////
// Commonly used routines (rarely used routines in STREX.CPP)

int vtString::Find(char ch) const
{
	return Find(ch, 0);
}

int vtString::Find(char ch, int nStart) const
{
	int nLength = GetData()->nDataLength;
	if (nStart >= nLength)
		return -1;

	// find first single character
	pchar lpsz = strchr(m_pchData + nStart, (unsigned char)ch);

	// return -1 if not found and index otherwise
	return (lpsz == NULL) ? -1 : (int)(lpsz - m_pchData);
}

int vtString::FindOneOf(pcchar lpszCharSet) const
{
	pchar lpsz = strpbrk(m_pchData, lpszCharSet);
	return (lpsz == NULL) ? -1 : (int)(lpsz - m_pchData);
}

void vtString::MakeUpper()
{
	CopyBeforeWrite();
#ifdef WIN32
 	_strupr(m_pchData);
#else
	for (char *p = m_pchData; *p; p++ )
		*p = toupper(*p);
#endif
}

void vtString::MakeLower()
{
	CopyBeforeWrite();
#ifdef WIN32
 	_strlwr(m_pchData);
#else
	for (char *p = m_pchData; *p; p++ )
		*p = tolower(*p);
#endif
}

void vtString::MakeReverse()
{
	CopyBeforeWrite();
#ifdef WIN32
 	_strrev(m_pchData);
#else
	{
		int len = strlen( m_pchData );
		for ( int i = len / 2 - 1; i >= 0; i-- ) {
			char tmp = m_pchData[i];
			m_pchData[  i  ] = m_pchData[len-1-i];
			m_pchData[len-1-i] = tmp;
		}
 	}
#endif
}

int vtString::CompareNoCase(pcchar lpsz) const
{
#ifdef WIN32
#  define stricmp _stricmp   // MBCS/Unicode aware
#else
#  define stricmp strcasecmp
#endif
	return stricmp(m_pchData, lpsz);
}

// vtString::Collate is often slower than Compare but is MBSC/Unicode
//  aware as well as locale-sensitive with respect to sort order.
inline int vtString::Collate(pcchar lpsz) const
{
	return strcoll(m_pchData, lpsz);	// locale sensitive
}
inline int vtString::CollateNoCase(pcchar lpsz) const
#ifdef WIN32
#  define stricoll _stricoll
#else
#  define stricoll strcoll             /*  Doesn't exist on UNIX  */
#endif
{
	return stricoll(m_pchData, lpsz);   // locale sensitive
}


void vtString::SetAt(int nIndex, char ch)
{
	CopyBeforeWrite();
	m_pchData[nIndex] = ch;
}

// formatting (using wsprintf style formatting)
void WIN_UNIX_CDECL vtString::Format(pcchar lpszFormat, ...) {
	va_list argList;
	va_start(argList, lpszFormat);
	FormatV(lpszFormat, argList);
	va_end(argList);
}

void vtString::FormatV(pcchar lpszFormat, va_list argList)
{
	va_list argListSave = argList;

	// make a guess at the maximum length of the resulting string
	int nMaxLen = 0;
	for (pcchar lpsz = lpszFormat; *lpsz != '\0'; lpsz++)
	{
		// handle '%' character, but watch out for '%%'
		if (*lpsz != '%' || *(++lpsz) == '%')
		{
			nMaxLen++;
			continue;
		}

		int nItemLen = 0;

		// handle '%' character with format
		int nWidth = 0;
		for (; *lpsz != '\0'; lpsz++)
		{
			// check for valid flags
			if (*lpsz == '#')
				nMaxLen += 2;   // for '0x'
			else if (*lpsz == '*')
				nWidth = va_arg(argList, int);
			else if (*lpsz == '-' || *lpsz == '+' || *lpsz == '0' ||
				*lpsz == ' ')
				;
			else // hit non-flag character
				break;
		}
		// get width and skip it
		if (nWidth == 0)
		{
			// width indicated by
			nWidth = atoi(lpsz);
			for (; *lpsz != '\0' && isdigit(*lpsz); lpsz++)
				;
		}
		int nPrecision = 0;
		if (*lpsz == '.')
		{
			// skip past '.' separator (width.precision)
			lpsz++;

			// get precision and skip it
			if (*lpsz == '*')
			{
				nPrecision = va_arg(argList, int);
				lpsz++;
			}
			else
			{
				nPrecision = atoi(lpsz);
				for (; *lpsz != '\0' && isdigit(*lpsz); lpsz++)
					;
			}
		}

		// should be on type modifier or specifier
		if (strncmp(lpsz, "I64", 3) == 0)
		{
			lpsz += 3;
		}
		else
		{
			switch (*lpsz)
			{
			// modifiers that affect size
			case 'h':
				lpsz++;
				break;
			case 'l':
				lpsz++;
				break;

			// modifiers that do not affect size
			case 'F':
			case 'N':
			case 'L':
				lpsz++;
				break;
			}
		}

		// now should be on specifier
		switch (*lpsz)
		{
		// single characters
		case 'c':
		case 'C':
			nItemLen = 2;
			va_arg(argList, char);
			break;
		// strings
		  case 's':
			{
				const char *pstrNextArg = va_arg(argList, pcchar);
				if (pstrNextArg == NULL)
				   nItemLen = 6;  // "(null)"
				else
				{
				   nItemLen = strlen(pstrNextArg);
				   nItemLen = MAX(1, nItemLen);
				}
			}
			break;
		  case 'S':
				/*  FIXME:  This case should do wchar_t *'s  */
				const char *pstrNextArg = va_arg(argList, pcchar);
				if (pstrNextArg == NULL)
				   nItemLen = 6; // "(null)"
				else
				{
				   nItemLen = strlen(pstrNextArg);
				   nItemLen = MAX(1, nItemLen);
				}
			break;
		}

		// adjust nItemLen for strings
		if (nItemLen != 0) {
			if (nPrecision != 0)
				nItemLen = MIN(nItemLen, nPrecision);
			nItemLen = MAX(nItemLen, nWidth);
		}
		else
		{
			switch (*lpsz)
			{
			// integers
			case 'd':
			case 'i':
			case 'u':
			case 'x':
			case 'X':
			case 'o':
				va_arg(argList, int);
				nItemLen = 32;
				nItemLen = MAX(nItemLen, nWidth+nPrecision);
				break;

			case 'e':
			case 'g':
			case 'G':
				va_arg(argList, double);
				nItemLen = 128;
				nItemLen = MAX(nItemLen, nWidth+nPrecision);
				break;

			case 'f':
				va_arg(argList, double);
				nItemLen = 128; // width isn't truncated
				// 312 == strlen("-1+(309 zeroes).")
				// 309 zeroes == max precision of a double
				nItemLen = MAX(nItemLen, 312+nPrecision);
				break;

			case 'p':
				va_arg(argList, void*);
				nItemLen = 32;
				nItemLen = MAX(nItemLen, nWidth+nPrecision);
				break;

			// no output
			case 'n':
				va_arg(argList, int*);
				break;

			default:
				;
			}
		}

		// adjust nMaxLen for output nItemLen
		nMaxLen += nItemLen;
	}

	GetBuffer(nMaxLen);
	vsprintf(m_pchData, lpszFormat, argListSave);
	ReleaseBuffer();

	va_end(argListSave);
}


//////////////////////////////////////////////////////////////////////////////
// Very simple sub-string extraction

vtString vtString::Mid(int nFirst) const
{
	return Mid(nFirst, GetData()->nDataLength - nFirst);
}

vtString vtString::Mid(int nFirst, int nCount) const
{
	// out-of-bounds requests return sensible things
	if (nFirst < 0)
		nFirst = 0;
	if (nCount < 0)
		nCount = 0;

	if (nFirst + nCount > GetData()->nDataLength)
		nCount = GetData()->nDataLength - nFirst;
	if (nFirst > GetData()->nDataLength)
		nCount = 0;

	// optimize case of returning entire string
	if (nFirst == 0 && nFirst + nCount == GetData()->nDataLength)
		return *this;

	vtString dest;
	AllocCopy(dest, nCount, nFirst, 0);
	return dest;
}

vtString vtString::Right(int nCount) const
{
	if (nCount < 0)
		nCount = 0;
	if (nCount >= GetData()->nDataLength)
		return *this;

	vtString dest;
	AllocCopy(dest, nCount, GetData()->nDataLength-nCount, 0);
	return dest;
}

vtString vtString::Left(int nCount) const
{
	if (nCount < 0)
		nCount = 0;
	if (nCount >= GetData()->nDataLength)
		return *this;

	vtString dest;
	AllocCopy(dest, nCount, 0, 0);
	return dest;
}


//////////////////////////////////////////////////////////////////////////////
// Finding

int vtString::ReverseFind(char ch) const
{
	// find last single character
	const char *lpsz = strrchr(m_pchData, ch);

	// return -1 if not found, distance from beginning otherwise
	return (lpsz == NULL) ? -1 : (int)(lpsz - m_pchData);
}

// find a sub-string (like strstr)
int vtString::Find(pcchar szSub) const
{
	return Find(szSub, 0);
}

int vtString::Find(pcchar szSub, int nStart) const
{
	int nLength = GetData()->nDataLength;
	if (nStart > nLength)
		return -1;

	// find first matching substring
	const char *lpsz = strstr(m_pchData + nStart, szSub);

	// return -1 for not found, distance from beginning otherwise
	return (lpsz == NULL) ? -1 : (int)(lpsz - m_pchData);
}


///////////////////////////////////////////////////////////////////////////////