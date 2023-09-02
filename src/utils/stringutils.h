/*=====================================================================
stringutils.cpp
---------------

Copyright (C) Nicholas Chapman

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

http://www.gnu.org/copyleft/gpl.html.
=====================================================================*/
#ifndef __STRINGUTILS_H__
#define __STRINGUTILS_H__


//NOTE: not all of this code has been used/tested for ages.
//Your mileage may vary; test the code before you use it!!!!

#include <string>
#include <assert.h>
#include <vector>
#include <sstream>

void readQuote(std::wistream& stream, std::wstring& str_out);//reads string from between double quotes.

template<typename T>
void writeQuote(T& stream, const std::wstring& s, wchar_t escape = '\\')
{
	stream << '"';
	for (size_t i = 0; i < s.length(); i++)
	{
		wchar_t c = s[i];
		if (c == escape || c == '"')
			stream << escape;
		stream << c;
	}
	stream << '"';
}

struct StringSet
{
	std::vector<std::wstring>	strings;
	bool caseCheck;
public:
	StringSet(const wchar_t *file, bool caseCheck);
	void Add(const wchar_t *string);
	void Remove(const wchar_t *string);
	bool Contains(const wchar_t *string) const;
};


struct StringList
{
	std::wstring string;
public:
	StringList(const wchar_t *file);
	void Add(const wchar_t *str);
	const std::wstring& Get() const { return string; }
};

std::wstring toWideString(const std::string& str);
std::string toMultiByteString(const std::wstring& str);

#endif //__STRINGUTILS_H__
