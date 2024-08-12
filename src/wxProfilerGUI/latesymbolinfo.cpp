/*=====================================================================
latesymbolinfo.cpp
--------------
Copyright (C) Vladimir Panteleev

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

http://www.gnu.org/copyleft/gpl.html..
=====================================================================*/

#include "../utils/dbginterface.h"
#include "../utils/except.h"
#include "latesymbolinfo.h"
#include "profilergui.h"

#include <comdef.h>
#include <Dbgeng.h>
#include <sstream>
#include <wrl/client.h>


#ifndef _MSC_VER
#define __in
#define __out
#endif

void comenforce(HRESULT result, const char* where = NULL)
{
	if (result == S_OK)
		return;

	std::wostringstream message;
	if (where)
		message << where;

	_com_error error(result);
	message << ": " << error.ErrorMessage();
	message << " (error " << result << ")";

	throw SleepyException(message.str());
}


LateSymbolInfo::LateSymbolInfo()
{
}

LateSymbolInfo::~LateSymbolInfo()
{
	unloadMinidump();
}

// Send debugger output to the wxWidgets current logging facility.
// The UI implements a logging facility in the form of a log panel.
struct DebugOutputCallbacksWide : public IDebugOutputCallbacksWide
{
	HRESULT	STDMETHODCALLTYPE QueryInterface(__in REFIID WXUNUSED(InterfaceId), __out PVOID* WXUNUSED(Interface)) noexcept { return E_NOINTERFACE; }
	ULONG	STDMETHODCALLTYPE AddRef() noexcept { return 1; }
	ULONG	STDMETHODCALLTYPE Release() noexcept { return 0; }

	HRESULT	STDMETHODCALLTYPE Output(__in ULONG WXUNUSED(Mask), __in PCWSTR Text) noexcept
	{
		//OutputDebugStringW(Text);
		wxLogMessage(L"%s", Text);
		return S_OK;
	}
};

static DebugOutputCallbacksWide *debugOutputCallbacksWide = new DebugOutputCallbacksWide();

void LateSymbolInfo::loadMinidump(std::wstring& dumppath, bool delete_when_done)
{
	// Method credit to http://stackoverflow.com/a/8119364/21501

	if (debugClient5 || debugControl4 || debugSymbols3)
	{
		//throw SleepyException(L"Minidump symbols already loaded.");

		// maybe the user moved a .pdb to somewhere where we can now find it?
		unloadMinidump();
	}

	Microsoft::WRL::ComPtr<IDebugClient> debugClient;

	SetLastError(0);
	comenforce(DebugCreate(IID_PPV_ARGS(&debugClient)), "DebugCreate");
	comenforce(debugClient.As(&debugClient5), "QueryInterface(IDebugClient5)" );
	comenforce(debugClient.As(&debugControl4), "QueryInterface(IDebugControl4)");
	comenforce(debugClient.As(&debugSymbols3), "QueryInterface(IDebugSymbols3)");
	comenforce(debugClient5->SetOutputCallbacksWide(debugOutputCallbacksWide), "IDebugClient5::SetOutputCallbacksWide");
	comenforce(debugSymbols3->SetSymbolOptions(SYMOPT_UNDNAME | SYMOPT_LOAD_LINES | SYMOPT_OMAP_FIND_NEAREST | SYMOPT_AUTO_PUBLICS | SYMOPT_DEBUG), "IDebugSymbols::SetSymbolOptions");

	std::wstring sympath;
	prefs.AdjustSymbolPath(sympath, true);

	comenforce(debugSymbols3->SetSymbolPathWide(sympath.c_str()), "IDebugSymbols::SetSymbolPathWide");
	comenforce(debugControl4->WaitForEvent(0, INFINITE), "IDebugControl::WaitForEvent");

	// Since we can't just enumerate all symbols in all modules referenced by the minidump,
	// we have to keep the debugger session open and query symbols as requested by the
	// profiler GUI.

	// If we are given a temporary file, clean it up later
	if (delete_when_done)
		file_to_delete = dumppath;
}

void LateSymbolInfo::unloadMinidump()
{
	if (debugClient5)
	{
		debugClient5->EndSession(DEBUG_END_ACTIVE_TERMINATE);
		debugClient5 = nullptr;
	}
	debugControl4 = nullptr;
	debugSymbols3 = nullptr;

	if (!file_to_delete.empty())
	{
		wxRemoveFile(file_to_delete);
		file_to_delete.clear();
	}
}

wchar_t LateSymbolInfo::buffer[4096];

void LateSymbolInfo::filterSymbol(Database::Address address, std::wstring &module, std::wstring &procname, std::wstring &sourcefile, unsigned &sourceline)
{
	if (debugSymbols3)
	{
		ULONG moduleindex;
		if (SUCCEEDED(debugSymbols3->GetModuleByOffset(address, 0, &moduleindex, NULL)))
			if (SUCCEEDED(debugSymbols3->GetModuleNameStringWide(DEBUG_MODNAME_MODULE, moduleindex, 0, buffer, _countof(buffer), NULL)))
				module = buffer;

		if (SUCCEEDED(debugSymbols3->GetNameByOffsetWide(address, buffer, _countof(buffer), NULL, NULL)))
		{
			std::wstring name = buffer;
			if (module != name)
			{
				procname = name;

				// Remove redundant "Module!" prefix
				auto prefix = module + L'!';
				if (procname.compare(0, prefix.size(), prefix) == 0)
					procname.erase(0, prefix.size());
			}
		}

		ULONG line;
		if (SUCCEEDED(debugSymbols3->GetLineByOffsetWide(address, &line, buffer, _countof(buffer), NULL, NULL)))
		{
			sourcefile = buffer;
			sourceline = line;
		}
	}
}
