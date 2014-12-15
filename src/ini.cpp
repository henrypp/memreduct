/************************************
*	Ini Wrapper
*	Copyright © 2013 Henry++
*
*	GNU General Public License v2
*	http://www.gnu.org/licenses/
*
*	http://www.henrypp.org/
*************************************/

// lastmod: 19/08/13

#include "ini.h"

BOOL INI::make_write(LPCWSTR lpcszSection, LPCWSTR lpcszKey, LPCWSTR lpcszValue)
{
	return WritePrivateProfileString(lpcszSection, lpcszKey, lpcszValue, this->path);
}
		
DWORD INI::make_read(LPCWSTR lpcszSection, LPCWSTR lpcszKey, LPWSTR lpcszReturned, DWORD dwSize, LPCWSTR lpcszDefault)
{
	return GetPrivateProfileString(lpcszSection, lpcszKey, lpcszDefault, lpcszReturned, dwSize, this->path);
}

// Set Ini Path
VOID INI::set_path(LPCWSTR lpcszPath)
{
	this->path = lpcszPath;
}

// Get Ini Path
CString INI::get_path()
{
	return this->path;
}

// Write Int
BOOL INI::write(LPCWSTR lpcszSection, LPCWSTR lpcszKey, DWORD dwValue)
{
	buffer.Format(L"%d\0", dwValue);
	return make_write(lpcszSection, lpcszKey, buffer);
}

// Write String
BOOL INI::write(LPCWSTR lpcszSection, LPCWSTR lpcszKey, LPCWSTR lpcszValue)
{
	return make_write(lpcszSection, lpcszKey, lpcszValue);
}

// Read Int
UINT INI::read(LPCWSTR lpcszSection, LPCWSTR lpcszKey, INT iDefault)
{
	return GetPrivateProfileInt(lpcszSection, lpcszKey, iDefault, this->path);
}

// Read String
CString INI::read(LPCWSTR lpcszSection, LPCWSTR lpcszKey, DWORD dwLenght, LPCWSTR lpcszDefault)
{
	make_read(lpcszSection, lpcszKey, buffer.GetBuffer(dwLenght), dwLenght, lpcszDefault);
	buffer.ReleaseBuffer();

	return buffer;
}