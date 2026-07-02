/*=====================================================================
except.h
----------------

Copyright (C) 2015 Ashod Nakashian

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

http://www.gnu.org/copyleft/gpl.html
=====================================================================*/

#pragma once

#include <stdexcept>
#include <string>
#include "stringutils.h"

class SleepyException : public std::runtime_error
{
	std::wstring _what;

public:
	SleepyException(const std::string& what)
		: std::runtime_error(what),
		  _what(utf8ToWide(what))
	{
	}

	SleepyException(const std::wstring& what)
		: std::runtime_error(wideToUtf8(what)),
		  _what(what)
	{
	}

	SleepyException(const wchar_t *what)
		: std::runtime_error(wideToUtf8(what)),
		  _what(what)
	{
	}

	const std::wstring &wwhat() const { return _what; }
};

template<typename T, typename S>
static T enforce(T cond, const S &text)
{
	if (cond)
		return cond;
	throw SleepyException(text);
}

#include <windows.h>
#include <sstream>

template<class T, class S>
T wenforce(T cond, const S& where)
{
	if (cond)
		return cond;

	DWORD code = GetLastError();

	std::wostringstream message;
	message << where;

	if (code)
	{
		wchar_t *lpMsgBuf = NULL;
		FormatMessageW(
			FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
			NULL,
			code,
			MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
			(LPWSTR)&lpMsgBuf,
			0,
			NULL);

		if (lpMsgBuf)
		{
			message << ": " << lpMsgBuf;
			LocalFree(lpMsgBuf);
		}
		else
			message << " failed";

		message << " (error " << code << ")";
	}
	else
		message << " failed";

	throw SleepyException(message.str());
}
