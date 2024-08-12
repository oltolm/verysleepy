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
#include "stringutils.h"
#include "except.h"
#include <shlwapi.h>
#include <string>


// Reads string from between double quotes.
std::wstring readQuote(std::wistream& stream)
{
	wchar_t c;
	std::wstring result;

	// Parse to first "
	for (;;)
	{
		stream.get(c);
		enforce(stream.good(), "Expected quoted string, got end of stream");
		if (c == L'"')
			break;
		enforce(isspace(c), std::wstring(L"Expected quoted string, got ") + c);
	}

	// Parse quoted text
	bool escaping = false;
	for (;;)
	{
		stream.get(c);
		enforce(stream.good(), "Unexpected end of stream while reading quoted string");
		if (escaping)
		{
			result.push_back(c);
			escaping = false;
		}
		else
		{
			if (c == L'\\')
				escaping = true;
			else
				if (c == L'"')
					break;
			else
				result.push_back(c);
		}
	}
	return result;
}

template<typename T>
static void Parse(const wchar_t *file, T* dst)
{
	FILE *fp;
	wchar_t path[MAX_PATH];
	wcscpy(path, file);

	fp = _wfopen(path, L"r");

	if (!fp) {
		GetModuleFileName(NULL, path, MAX_PATH);
		PathRemoveFileSpec(path);

		do
		{
			PathAppend(path, file);
			fp = _wfopen(path, L"r");
		}
		while (!fp && PathRemoveFileSpec(path) && PathRemoveFileSpec(path));
	}

	if (!fp)
		return;

	wchar_t line[4096];
	while(fgetws(line, 4096, fp))
	{
		wchar_t *start = line;
		while(*start && isspace(*start))
			start++;

		if (*start == L'\0')
			continue;

		wchar_t *end = line+wcslen(line)-1;
		while(end != start && isspace(*end))
			*end-- = L'\0';

		dst->Add(start);
	}
	fclose(fp);
}

template<bool caseCheck>
StringSet<caseCheck>::StringSet(const wchar_t *file)
{
	Parse(file, this);
}

template<bool caseCheck>
void StringSet<caseCheck>::Add(const wchar_t *string)
{
	strings.insert(string);
}

template<bool caseCheck>
void StringSet<caseCheck>::Remove(const wchar_t *string)
{
	auto it = strings.find(string);
	if (it != strings.end())
		strings.erase(it);
}

template<bool caseCheck>
bool StringSet<caseCheck>::Contains(const wchar_t *str) const
{
	return strings.count(str) != 0;
}

// explicit template instantiation
template struct StringSet<true>;
template struct StringSet<false>;

StringList::StringList(const wchar_t *file)
{
	Parse(file, this);
}

void StringList::Add(const wchar_t *str)
{
	string.append(str);
	string.append(L" ");
}
