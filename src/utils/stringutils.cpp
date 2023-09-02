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
#include <algorithm>
#include <shlwapi.h>


// Reads string from between double quotes.
void readQuote(std::wistream& stream, std::wstring& str_out)
{
	wchar_t c;
	str_out = L"";

	// Parse to first "
	for (;;)
	{
		stream.get(c);
		enforce(stream.good(), "Expected quoted string, got end of stream");
		if (c == '"')
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
			str_out.push_back(c);
			escaping = false;
		}
		else
		{
			if (c == '\\')
				escaping = true;
			else
				if (c == '"')
					break;
			else
				str_out.push_back(c);
		}
	}
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

		if (*start == 0)
			continue;

		wchar_t *end = line+wcslen(line)-1;
		while(end != start && isspace(*end))
			*end-- = 0;

		dst->Add(start);
	}
	fclose(fp);
}

StringSet::StringSet(const wchar_t *file, bool caseCheck)
{
	this->caseCheck = caseCheck;
	Parse(file, this);
}

void StringSet::Add(const wchar_t *string)
{
	wchar_t *tmp = wcsdup(string);
	if (!caseCheck)
		wcslwr(tmp);
	strings.push_back(tmp);
	free(tmp);

	std::sort(strings.begin(), strings.end());
}

void StringSet::Remove(const wchar_t *string)
{
	wchar_t *tmp = wcsdup(string);
	if (!caseCheck)
		wcslwr(tmp);

	for (size_t n=0;n<strings.size();n++)
	{
		if (strings[n] == tmp)
		{
			strings.erase(strings.begin()+n);
			break;
		}
	}

	free(tmp);
	std::sort(strings.begin(), strings.end());
}

bool StringSet::Contains(const wchar_t *str) const
{
	size_t low = 0, high = strings.size();
	while(low < high)
	{
		size_t guess = (low + high) >> 1;
		const wchar_t *cmp = strings[guess].c_str();
		int match;

		if (caseCheck)
			match = wcscmp(str, cmp);
		else
			match = wcsicmp(str, cmp);

		if (match < 0)
			high = guess;
		else if (match > 0)
			low = guess+1;
		else
			return true;
	}

	return false;
}

StringList::StringList(const wchar_t *file)
{
	Parse(file, this);
}

void StringList::Add(const wchar_t *str)
{
	string.append(str);
	string.append(L" ");
}

std::wstring toWideString(const std::string& str)
{
	int len = MultiByteToWideChar(CP_ACP, 0, str.c_str(), str.size(), nullptr, 0);
	std::wstring wstr;
	wstr.resize(len);
	MultiByteToWideChar(CP_ACP, 0, str.c_str(), str.size(), &wstr[0], wstr.size());
	return wstr;
}

std::string toMultiByteString(const std::wstring& str)
{
	BOOL usedDefaultChar;
	CHAR defaultChar{};
	int len = WideCharToMultiByte(CP_ACP, 0, str.c_str(), str.size(), nullptr, 0, &defaultChar, &usedDefaultChar);
	std::string mbstr;
	mbstr.resize(len);
	WideCharToMultiByte(CP_ACP, 0, str.c_str(), str.size(), &mbstr[0], mbstr.size(), &defaultChar, &usedDefaultChar);
	return mbstr;
}
