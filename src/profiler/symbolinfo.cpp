/*=====================================================================
symbolinfo.cpp
--------------
File created by ClassTemplate on Sat Mar 05 19:10:20 2005

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

http://www.gnu.org/copyleft/gpl.html
=====================================================================*/
#include "symbolinfo.h"
#include "../wxProfilerGUI/profilergui.h"

#include "../utils/osutils.h"
#include <utility>
#include <windows.h>
#include <psapi.h>
#include "../utils/dbginterface.h"
#include <algorithm>
#include <shlwapi.h>
#include <wx/file.h>
#include <wx/filename.h>
#include "../utils/except.h"
#include "appinfo.h"

SymLogFn *g_symLog = NULL;

struct SymbolInfoContext
{
	SymbolInfo* syminfo;
	DbgHelp* dbgHelp;
};

BOOL CALLBACK EnumModules(
	PCWSTR   ModuleName,
	DWORD64 BaseOfDll,
	PVOID   UserContext )
{
	SymbolInfoContext* context = static_cast<SymbolInfoContext*>(UserContext);

	HMODULE hMod;
	GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, ModuleName, &hMod);

	Module mod((PROFILER_ADDR)BaseOfDll, ModuleName, context->dbgHelp);
	context->syminfo->addModule(mod);

	return TRUE;
}

SymbolInfo::SymbolInfo() {}

BOOL CALLBACK symCallback(HANDLE WXUNUSED(hProcess), ULONG ActionCode, ULONG64 CallbackData, ULONG64 WXUNUSED(UserContext))
{
	switch(ActionCode)
	{
	case CBA_DEBUG_INFO:
		if (g_symLog)
		{
			g_symLog((const wchar_t*)CallbackData);
		}
		return TRUE;
	default:
		return FALSE;
	}
}

void symWineCallback(const char *msg)
{
#ifdef _DEBUG
	OutputDebugStringA(msg);
#endif

	if (!msg || !*msg)
		return;

	static bool newline = true;
	if (g_symLog)
	{
		if (newline)
			g_symLog(L"WINE: ");
		newline = msg[strlen(msg) - 1] == '\n';

		wchar_t tmp[2048];
		MultiByteToWideChar(CP_ACP, 0, msg, -1, tmp, _countof(tmp));
		g_symLog(tmp);
	}
}

void SymbolInfo::loadSymbolsUsing(DbgHelp* dbgHelp, const std::wstring& sympath)
{
	if (!dbgHelp->Loaded)
	{
		if (g_symLog)
		{
			g_symLog(dbgHelp->Name); g_symLog(L" is not loaded, skipping.\n");
		}
		return;
	}

	DWORD options = dbgHelp->SymGetOptions();

	if(!is64BitProcess) {
		options |= SYMOPT_INCLUDE_32BIT_MODULES;
	}

	options |= SYMOPT_LOAD_LINES | SYMOPT_DEBUG;

	dbgHelp->SymSetOptions(options);

	if (dbgHelp->SymSetDbgPrint)
		dbgHelp->SymSetDbgPrint(&symWineCallback);

	for( int n=0;n<4;n++ )
	{
		wenforce(dbgHelp->SymInitializeW(process_handle.get(), L"", FALSE), "SymInitialize");

		// Hook the debug output, so we actually can provide a clue as to
		// what's happening.
		dbgHelp->SymRegisterCallbackW64(process_handle.get(), symCallback, 0);

		// Add our PDB search paths.
		wenforce(dbgHelp->SymSetSearchPathW(process_handle.get(), sympath.c_str()),
				 "SymSetSearchPathW");

		if (modules.empty())
		{
			// Load symbol information for all modules.
			// Normally SymInitialize would do this, but we instead do it ourselves afterwards
			// so that we can hook the debug output for it.
			wenforce(dbgHelp->SymRefreshModuleList(process_handle.get()), "SymRefreshModuleList");

			SymbolInfoContext context;
			context.syminfo = this;
			context.dbgHelp = dbgHelp;

			wenforce(dbgHelp->SymEnumerateModulesW64(process_handle.get(), EnumModules, &context),
					 "SymEnumerateModules64");
		}
		else
		{
			// This is a secondary dbgHelp, so just complement debug
			// information for modules that have none.

			for (auto& mod : modules)
			{
				IMAGEHLP_MODULEW64 info;
				info.SizeOfStruct = sizeof(info);
				if (!mod.dbghelp->SymGetModuleInfoW64(process_handle.get(), mod.base_addr, &info))
					continue;

				// If we have a module with no symbol information from the previous (MS) dbghelp,
				// let the current one handle it instead.
				if (info.SymType == SymNone)
				{
					DWORD64 ret = dbgHelp->SymLoadModuleExW(
						process_handle.get(), NULL, info.ImageName, info.ModuleName,
						info.BaseOfImage, info.ImageSize, NULL, 0);
					if (ret)
						mod.dbghelp = dbgHelp;
				}
			}
		}

		if (!modules.empty())
			break;

		// Sometimes the module enumeration will fail (no error code, but no modules
		// will be returned). If we try again a little later it seems to work.
		// I suspect this may be if we try and enum modules too early on, before the process
		// has really had a chance to 'get going'.
		// Perhaps a better solution generally would be to manually load module symbols on demand,
		// as each sample comes in? That'd also solve the problem of modules getting loaded/unloaded
		// mid-profile. Yes, I'll probably do that some day.
		Sleep(100);
		dbgHelp->SymCleanup(process_handle.get());
	}
}

void SymbolInfo::loadSymbols(DWORD process_id, bool download)
{
	assert(!process_handle);

	process_handle.reset(OpenProcess(PROCESS_ALL_ACCESS, FALSE, process_id));

	wxBusyCursor busy;

	is64BitProcess = Is64BitProcess(process_handle.get());

	std::wstring sympath;
	{
		// Add the program's own directory to the search path.
		// Useful if someone's copied the EXE and PDB to a different machine or location.
		wchar_t szExePath[MAX_PATH] = L"";
		DWORD pathsize = MAX_PATH;
		BOOL gotImageName = FALSE;
		// GetModuleFileNameEx doesn't always work across 64->32 bit boundaries.
		// Use QueryFullProcessImageName if we have it.
		{
			typedef BOOL WINAPI QueryFullProcessImageNameFn(HANDLE hProcess, DWORD dwFlags, LPTSTR lpExeName, PDWORD lpdwSize);

			QueryFullProcessImageNameFn *fn = (QueryFullProcessImageNameFn *)GetProcAddress(GetModuleHandle(L"kernel32"), "QueryFullProcessImageNameW");
			if (fn)
				gotImageName = fn(process_handle.get(), 0, szExePath, &pathsize);
		}

		if (!gotImageName)
			gotImageName = GetModuleFileNameEx(process_handle.get(), NULL, szExePath, pathsize);

		if (gotImageName)
		{
			// Convert the EXE path to its containing folder and append the
			// resulting folder to the symbol search path.
			if (PathRemoveFileSpec(szExePath))
			{
				sympath += std::wstring(L";") + szExePath;
			}
		}

		prefs.AdjustSymbolPath(sympath, download);
	}

	loadSymbolsUsing(getGccDbgHelp(), sympath);

	if (g_symLog)
		g_symLog(L"\nFinished.\n");
	// Read each module's symbols now, while the target is still alive. dbghelp has
	// already loaded the PDB based ones, but the Dr. MinGW dbghelp reads DWARF and PE
	// symbols lazily, on the first query for an address inside the module. Those
	// queries only happen when the capture is written, by which point a target that
	// ran to completion is gone and the lookup fails, leaving every frame belonging
	// to it unresolved. One throwaway query per module fills the cache up front.
	primeModuleSymbols();
	sortModules();
}

void SymbolInfo::primeModuleSymbols()
{
	unsigned char buffer[1024];
	SYMBOL_INFOW *symbol_info = (SYMBOL_INFOW *)buffer;

	for (auto& mod : modules)
	{
		if (!mod.dbghelp->Loaded)
			continue;

		symbol_info->SizeOfStruct = sizeof(SYMBOL_INFOW);
		symbol_info->MaxNameLen = ((sizeof(buffer) - sizeof(SYMBOL_INFOW)) / sizeof(WCHAR)) - 1;

		DWORD64 displacement = 0;
		mod.dbghelp->SymFromAddrW(process_handle.get(), mod.base_addr, &displacement,
								  symbol_info);

		// Remember how far the module reaches, so getModuleForAddr can tell an address
		// inside it from one belonging to a module we never enumerated. Ask the loader
		// rather than dbghelp: a module base is its HMODULE, and this still answers for
		// modules whose symbols failed to load, which are the ones most likely to need it.
		MODULEINFO module_info;
		if (GetModuleInformation(process_handle.get(), (HMODULE)mod.base_addr, &module_info,
								 sizeof(module_info)))
			mod.size = module_info.SizeOfImage;
	}
}

DbgHelp* SymbolInfo::getGccDbgHelp()
{
	if (prefs.UseWine())
	{
		// We can't use the regular dbghelpw to profile 32-bit applications,
		// as it's got compiled-in things that assume 64-bit. So we instead have
		// a special Wow64 build, which is compiled as 64-bit code but using 32-bit
		// definitions. We load that instead.
		if (!is64BitProcess)
			return &dbgHelpWineWow64;
		else
			return &dbgHelpWine;
	}
	else
		return &dbgHelpDrMingw;
}

SymbolInfo::~SymbolInfo()
{
	//------------------------------------------------------------------------
	//clean up
	//------------------------------------------------------------------------
	if (process_handle)
	{
		DbgHelp *gcc = getGccDbgHelp();
		if (gcc->Loaded && !gcc->SymCleanup(process_handle.get()))
		{
			//error
		}
	}
}

Module *SymbolInfo::getModuleForAddr(PROFILER_ADDR addr)
{
	if(modules.empty())
		return NULL;

	if(addr < modules[0].base_addr)
		return NULL;

	Module *mod = &modules.back();
	for(unsigned int i=1; i<modules.size(); ++i)
		if(addr < modules[i].base_addr)
		{
			mod = &modules[i-1];
			break;
		}

	// An address past the end of the module nearest below it belongs to something we
	// never enumerated. Naming it after that module is worse than admitting we do not
	// know: callers fall back to asking dbghelp directly, which may well know the module
	// even when our list does not. Modules whose size we could not learn keep the old
	// behaviour of claiming everything above them.
	if(mod->size && addr >= mod->base_addr + mod->size)
		return NULL;

	return mod;
}

const std::wstring SymbolInfo::getModuleNameForAddr(PROFILER_ADDR addr)
{
	Module *mod = getModuleForAddr(addr);
	if (mod)
		return mod->name;
	else
		return L"";
}

void SymbolInfo::addModule(const Module& module)
{
	modules.push_back(module);
}

void SymbolInfo::sortModules()
{
	struct Sorter {
		bool operator() (const Module& a, const Module& b) const {
			return a.base_addr < b.base_addr;
		}
	};
	std::sort(modules.begin(), modules.end(), Sorter());
}

const std::wstring SymbolInfo::getProcForAddr(PROFILER_ADDR addr,
											  std::wstring& procfilepath_out, int& proclinenum_out)
{
	procfilepath_out = L"";
	proclinenum_out = 0;
	std::wstring name;

	Module *mod = getModuleForAddr(addr);
	DbgHelp *dbgHelp = mod ? mod->dbghelp : getGccDbgHelp();

	unsigned char buffer[1024];

	//blame MS for this abomination of a coding technique
	SYMBOL_INFOW* symbol_info = (SYMBOL_INFOW*)buffer;
	symbol_info->SizeOfStruct = sizeof(SYMBOL_INFOW);
	symbol_info->MaxNameLen = ((sizeof(buffer) - sizeof(SYMBOL_INFOW)) / sizeof(WCHAR)) - 1;

	DWORD64 displacement = 0;
	BOOL result = FALSE;
	if (mod)
	{
		auto it = mod->sym_cache.find(addr);
		if (it != mod->sym_cache.end())
		{
			result = TRUE;
			name = it->second;
		}
		else
		{
			result = dbgHelp->SymFromAddrW(process_handle.get(), (DWORD64)addr, &displacement,
										   symbol_info);
			if (result)
			{
				name = symbol_info->Name;
				mod->sym_cache.emplace(addr, name);
			}
		}
	}
	else
	{
		result =
			dbgHelp->SymFromAddrW(process_handle.get(), (DWORD64)addr, &displacement, symbol_info);
		if (result)
			name = symbol_info->Name;
	}

	if(!result)
	{
		if(is64BitProcess)
			return wxString::Format(L"[%016llX]", addr).wc_string();
		else
			return wxString::Format(L"[%08X]", (unsigned __int32)(addr)).wc_string();
	}

	//------------------------------------------------------------------------
	//lookup proc file and line num
	//------------------------------------------------------------------------
	getLineForAddr(addr, procfilepath_out, proclinenum_out);

	return name;
}

void SymbolInfo::getLineForAddr(PROFILER_ADDR addr, std::wstring& filepath_out, int& linenum_out)
{
	Module *mod = getModuleForAddr(addr);
	DbgHelp *dbgHelp = mod ? mod->dbghelp : getGccDbgHelp();

	if (mod)
	{
		auto it = mod->line_cache.find(addr);
		if (it != mod->line_cache.end())
		{
			filepath_out = it->second.first;
			linenum_out = it->second.second;
			return;
		}
	}
	DWORD displacement;
	IMAGEHLP_LINEW64 lineinfo;
	ZeroMemory(&lineinfo, sizeof(lineinfo));
	lineinfo.SizeOfStruct = sizeof(IMAGEHLP_LINEW64);
	BOOL result = dbgHelp->SymGetLineFromAddrW64(process_handle.get(), (DWORD64)addr, &displacement,
												 &lineinfo);

	if (result)
	{
		filepath_out = lineinfo.FileName;
		linenum_out = lineinfo.LineNumber;
	}
	else
	{
		filepath_out = L"[unknown]";
		linenum_out = 0;
	}
	if (mod)
		mod->line_cache.emplace(addr, std::make_pair(filepath_out, linenum_out));
}

std::wstring SymbolInfo::saveMinidump()
{
	if (!Is64BitProcess(process_handle.get()))
	{
		wxLogWarning(
			L"Warning: minidumps of 32-bit processes saved by 64-bit processes will most likely not be saved correctly.\n"
			L"Use the 32-bit version of " _T(APPNAME) L" to profile 32-bit processes if a minidump needs to be included."
		);
	}

	wxFile f;
	std::wstring dumppath = wxFileName::CreateTempFileName(wxEmptyString, &f).wc_string();
	wenforce(getGccDbgHelp()->MiniDumpWriteDump(
				 process_handle.get(), GetProcessId(process_handle.get()),
				 (HANDLE)_get_osfhandle(f.fd()), MiniDumpNormal, NULL, NULL, NULL),
			 "MiniDumpWriteDump");
	f.Close();
	return dumppath;
}
