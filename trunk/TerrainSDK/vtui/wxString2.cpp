//
// wxString2.cpp
//
// Copyright (c) 2003 Virtual Terrain Project
// Free for all uses, see license.txt for details.
//

#include "wx/wxprec.h"

#ifndef WX_PRECOMP
#include "wx/wx.h"
#endif

#include "wxString2.h"

// Construction
wxString2::wxString2(const wxChar *psz)
{
	(void) wxString::operator=(psz);
}

#if wxUSE_UNICODE	// supply the conversion that wxChar doesn't
wxString2::wxString2(const char *psz)
{
	*this = psz;
}
#else
wxString2::wxString2(const wchar_t *psz)
{
	*this = psz;
}
#endif

wxString2::wxString2(const std::string &in)
{
	*this = in.c_str();
}

wxString2::wxString2(const vtString &vtstr)
{
	*this = (const char *) vtstr;
}

wxString2::wxString2(const vtString *vtstrp)
{
	*this = (const char *) (*vtstrp);
}

// Assignment
wxString2& wxString2::operator=(const wxChar *psz)
{
	(void) wxString::operator=(psz);
	return *this;
}

#if wxUSE_UNICODE
wxString2& wxString2::operator=(const char *psz)
{
	int len = strlen(psz);
	wxChar *buf = new wchar_t[len+1];
	int result = wxMB2WC(buf, psz, len+1);
	(void) wxString::operator=(buf);
	delete buf;
	return *this;
}
/*void wxString2::operator+=(const char *psz)
{
	int len = strlen(psz);
	wxChar *buf = new wchar_t[len+1];
	int result = wxMB2WC(buf, psz, len+1);
	(void) wxString::operator+=(buf);
	delete buf;
}*/
#else
wxString2& wxString2::operator=(const wchar_t *psz)
{
#if wxUSE_WCHAR_T
	int len = wxWcslen(psz);
#else
	int len = wcslen(psz);
#endif

	char *buf = new char[len+1];
	int result = wxWC2MB(buf, psz, len+1);
	(void) wxString::operator=(buf);
	delete buf;
	return *this;
}
/*void wxString2::operator+=(const wchar_t *psz)
{
	int len = wcslen(psz);
	char *buf = new char[len+1];
	int result = wxWC2MB(buf, psz, len+1);
	(void) wxString::operator+=(buf);
	delete buf;
}*/
#endif

wxString2& wxString2::operator=(const wxString &str)
{
	wxString *ptr = (wxString *)this;
	*ptr = str;
	return *this;
}

wxString2& wxString2::operator=(const vtString &vtstr)
{
	*this = (const char *) vtstr;
	return *this;
}

wxString2& wxString2::operator=(const vtString *vtstrp)
{
	*this = (const char *) (*vtstrp);
	return *this;
}

wxString2::operator vtString() const
{
	return vtString(mb_str());
}

wxString2::operator const char*() const
{
	return mb_str();
}

#if wxUSE_UNICODE
char wxString2::s_buffer[MAX_WXSTRING2_SIZE];
const char *wxString2::mb_str() const
{
	// watch out: multi-byte is NOT (unfortunately) the same as UTF-8
//#if SUPPORT_WSTRING
//	wstring2 ws(c_str());
//	return ws.to_utf8();
//#else
	int result = wxWC2MB(s_buffer, c_str(), MAX_WXSTRING2_SIZE);
	return s_buffer;
//#endif
}
#endif

const char *wxString2::to_utf8() const
{
#if wxUSE_UNICODE
	int result = wxConvUTF8.WC2MB(s_buffer, c_str(), MAX_WXSTRING2_SIZE);
	return s_buffer;
#else
	return mb_str();
#endif
}

void wxString2::from_utf8(const char *input)
{
#if wxUSE_UNICODE
	wxChar buf[MAX_WXSTRING2_SIZE];
	int result = wxConvUTF8.MB2WC(buf, input, MAX_WXSTRING2_SIZE);
	*this = buf;
#else
	*this = wxString::FromAscii(input);
#endif
}


vtString wxString2::vt_str() const
{
//#if SUPPORT_WSTRING && wxUSE_UNICODE
//	wstring2 ws(c_str());
//	vtString result(ws.to_utf8());
//	return result;
//#else
	vtString result(mb_str());
	return result;
//#endif
}


