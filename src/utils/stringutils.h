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
#pragma once


//NOTE: not all of this code has been used/tested for ages.
//Your mileage may vary; test the code before you use it!!!!

#include <functional>
#include <string>
#include <unordered_set>

std::wstring readQuote(std::wistream& stream);//reads string from between double quotes.

template<typename T>
void writeQuote(T& stream, const std::wstring& s, wchar_t escape = L'\\')
{
	stream << '"';
	for (wchar_t c : s)
	{
		if (c == escape || c == L'"')
			stream << escape;
		stream << c;
	}
	stream << '"';
}

struct StringCompIgnoreCase
{
	bool operator()(const std::wstring &a, const std::wstring &b) const { return wcsicmp(a.c_str(), b.c_str()) == 0; }
};

struct HashIgnoreCase
{
	size_t operator()(const std::wstring &a) const
	{
		std::wstring tmp = a;
		wcslwr(tmp.data());
		return std::hash<std::wstring>{}(tmp);
	};
};

template<bool caseCheck>
struct StringSet
{
	std::unordered_set<std::wstring,
					   std::conditional_t<caseCheck, std::hash<std::wstring>, HashIgnoreCase>,
					   std::conditional_t<caseCheck, std::equal_to<std::wstring>, StringCompIgnoreCase>>
		strings;

public:
	StringSet(const wchar_t *file);
	void Add(const wchar_t *string);
	void Remove(const wchar_t *string);
	bool Contains(const wchar_t *string) const;
};

class StringList
{
	std::wstring string;
public:
	StringList(const wchar_t *file);
	void Add(const wchar_t *str);
	const std::wstring& Get() const { return string; }
};
