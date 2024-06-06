/*=====================================================================
profilerthread.cpp
------------------
File created by ClassTemplate on Thu Feb 24 19:29:41 2005

Copyright (C) Nicholas Chapman
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

http://www.gnu.org/copyleft/gpl.html..
=====================================================================*/

#include "../appinfo.h"
#include "../utils/stringutils.h"
#include "../wxProfilerGUI/profilergui.h"
#include "debugger.h"
#include "profilerthread.h"
#include "symbolinfo.h"
#include "threadinfo.h"

#include <algorithm>
#include <memory>
#include <Psapi.h>
#include <random>
#include <timeapi.h>
#include <unordered_set>
#include <utility>
#include <vector>
#include <windows.h>
#include <wx/txtstrm.h>
#include <wx/wfstream.h>
#include <wx/zipstrm.h>

#ifdef _MSC_VER
#pragma comment(lib, "winmm.lib")
#endif

// DE: 20090325: Profiler has a list of threads to profile
// RM: 20130614: Profiler time can now be limited (-1 = until cancelled)
ProfilerThread::ProfilerThread(HANDLE target_process_, const std::vector<HANDLE>& target_threads, SymbolInfo *sym_info_, std::unique_ptr<Debugger> debugger_)
:	symbolsPermille(0),
	profilers(),
	debugger(std::move(debugger_)),
	duration(0),
	status(L"Initializing"),
	numsamplessofar(0),
	numThreadsRunning(target_threads.size()),
	done(false),
	paused(false),
	failed(false),
	cancelled(false),
	commit_suicide(false),
	target_process(target_process_),
	filename(wxFileName::CreateTempFileName(wxEmptyString)),
	sym_info(sym_info_)
{
	// AA: 20210822: If we have a debugger, it will report all available threads
	//               So, only use the passed vector when we have no debugger
	if (!debugger)
	{
		// DE: 20090325: Profiler has a list of threads to profile, one Profiler instance per thread
		profilers.reserve(target_threads.size());
		for (HANDLE hThread : target_threads)
		{
			DWORD dwThread = GetThreadId(hThread);
			profilers.emplace_back(target_process_, hThread, dwThread, callstacks);
			thread_names[dwThread] = getThreadDescriptorName(hThread);
		}
	}
}


void ProfilerThread::sample(const SAMPLE_TYPE timeSpent)
{
	// DE: 20090325: Profiler has a list of threads to profile, one Profiler instance per thread
	// RJM- We traverse them in random order. The act of profiling causes the Windows scheduler
	//      to re-schedule, and if we did them in sequence, it'll always schedule the first one.
	//      This starves the other N-1 threads. For lack of a better option, using a shuffle
	//      at least re-schedules them evenly.

	duration += timeSpent;

	if (profilers.empty())
		return;

	std::default_random_engine dre;
	std::shuffle(profilers.begin(), profilers.end(), dre);

	auto failedAndExited = [&](Profiler& p) -> bool {
		try
		{
			bool failed = !p.sampleTarget(timeSpent, sym_info);
			if (!failed)
				++numsamplessofar;
			return failed && p.targetExited();
		}
		catch (const ProfilerExcep& e)
		{
			error(_T("ProfilerExcep: ") + e.what());
			commit_suicide = true;
			return false;
		}
	};
	profilers.erase(std::remove_if(profilers.begin(), profilers.end(), failedAndExited), profilers.end());

	numThreadsRunning = profilers.size();
}

void ProfilerThread::sampleLoop()
{
	timeBeginPeriod(1);

	LARGE_INTEGER prev, now, start, freq;
	QueryPerformanceFrequency(&freq);
	QueryPerformanceCounter(&start);
	prev = start;

	bool minidump_saved = false;

	while(!commit_suicide)
	{
		if (paused)
		{
			Sleep(100);
			QueryPerformanceCounter(&prev);
			continue;
		}

		if (debugger)
			debugger->update();

		QueryPerformanceCounter(&now);

		__int64 diff = now.QuadPart - prev.QuadPart;
		double t = (double)diff / (double)freq.QuadPart;

		__int64 elapsed = now.QuadPart - start.QuadPart;
		if (!minidump_saved && prefs.saveMinidump.GetValue()>=0 && elapsed >= prefs.saveMinidump.GetValue() * freq.QuadPart)
		{
			minidump_saved = true;
			status = L"Saving minidump";
			minidump = sym_info->saveMinidump();
			status = NULL;
			continue;
		}

		sample(t);

		int ms = 100 / prefs.throttle.GetValue();
		Sleep(ms);

		prev = now;
	}

	timeEndPeriod(1);
}

void ProfilerThread::saveData()
{
	//get process id of the process the target thread is running in
	//const DWORD process_id = GetProcessIdOfThread(profiler.getTarget());

	wxFFileOutputStream out(filename);
	wxZipOutputStream zip(out);
	wxTextOutputStream txt(zip, wxEOL_NATIVE, wxConvAuto(wxFONTENCODING_UTF8));

	if (!out.IsOk() || !zip.IsOk())
	{
		error(L"Error writing to file");
		return;
	}

	//------------------------------------------------------------------------

	if (!minidump.empty())
	{
		zip.PutNextEntry(_T("minidump.dmp"));
		beginProgress(L"Copying minidump", 100);
		{
			wxFFileInputStream stream(minidump);
			zip.Write(stream);
		}
		wxRemoveFile(minidump);
	}

	//------------------------------------------------------------------------
	beginProgress(L"Saving stats", 100);
	zip.PutNextEntry(_T("Stats.txt"));

	wchar_t tmp[4096] = L"?";
	GetModuleFileNameEx(target_process, NULL, tmp, 4096);
	time_t rawtime;
	time(&rawtime);
	txt << "Filename: " << tmp << "\n";
	txt << "Duration: " << duration << "\n";
	txt << "Date: " << asctime(localtime(&rawtime));
	txt << "Samples: " << numsamplessofar << "\n";

	//------------------------------------------------------------------------
	beginProgress(L"Summarizing results");

	std::unordered_set<PROFILER_ADDR> used_addresses;
	std::unordered_set<DWORD> used_thread_ids;

	for (const auto & [callstack, cost] : callstacks)
	{
		used_thread_ids.insert(callstack.thread_id);
		for (size_t n=0;n<callstack.depth;n++)
		{
			used_addresses.insert(callstack.addr[n]);
		}
	}

	//------------------------------------------------------------------------
	beginProgress(L"Querying and saving symbols", used_addresses.size());
	zip.PutNextEntry(_T("Symbols.txt"));

	for (const auto & used_address : used_addresses)
	{
		PROFILER_ADDR addr = used_address;

		ProcedureInfo proc = sym_info->getProcForAddr(addr);
		txt <<  wxString::Format("%#llx", addr);
		txt << " ";
		std::wstring module_name = sym_info->getModuleNameForAddr(addr);
		writeQuote(txt, module_name);
		txt << " ";
		writeQuote(txt, proc.name);
		txt << " ";
		writeQuote(txt, proc.filepath);
		txt << " ";
		txt << proc.linenum;
		txt << '\n';

		if (updateProgress())
			return;
	}

	//------------------------------------------------------------------------
	beginProgress(L"Saving callstacks", callstacks.size());
	zip.PutNextEntry(_T("Callstacks.txt"));

	for (auto i = callstacks.begin(); i != callstacks.end();)
	{
		const CallStack &callstack = i->first;

		// write callstack addresses
		for( size_t d=0;d<callstack.depth;d++ )
			txt << " " << wxString::Format("%#llx", callstack.addr[d]);
		txt << "\n";

		// write pairs of thread_id and count for each identical callstack
		do
		{
			txt << " " << (unsigned)i->first.thread_id;
			txt << " " << i->second;
			++i;
		}
		while ( i != callstacks.end() && !callstack.isBefore(i->first, false) );
		txt << "\n";

		if (updateProgress())
			return;
	}

	//------------------------------------------------------------------------
	beginProgress(L"Saving threads", used_thread_ids.size());
	zip.PutNextEntry(_T("Threads.txt"));

	for (const auto tid : used_thread_ids)
	{
		txt << (unsigned)tid << "\n";
		txt << thread_names[tid] << "\n";

		if (updateProgress())
			return;
	}

	//------------------------------------------------------------------------
	// Change FORMAT_VERSION when the file format changes
	// (and becomes unreadable by older versions of Sleepy).
	zip.PutNextEntry(L"Version " _T(FORMAT_VERSION) L" required");
	txt << FORMAT_VERSION << "\n";


	if (!out.IsOk() || !zip.IsOk())
	{
		error(L"Error writing to file");
		return;
	}
}

void ProfilerThread::run()
{
	wxLog::EnableLogging();

	if (debugger)
		debugger->attach([this](Debugger::NotifyData const &notification) {
			if (notification.eventType != Debugger::NOTIFY_NEW_THREAD)
				return;
			profilers.emplace_back(target_process, notification.threadHandle, notification.threadId, callstacks);
			thread_names[notification.threadId] = getThreadDescriptorName(notification.threadHandle);
		});

	status = NULL;
	try
	{
		sampleLoop();
	} catch(ProfilerExcep& e) {
		// see if it's an actual error, or did the thread just finish naturally
		for (const auto & profiler : profilers)
		{
				if (!profiler.targetExited())
			{
				error(L"ProfilerExcep: " + e.what());
				return;
			}
		}

		numThreadsRunning = 0;
	}

	status = L"Exiting";

	if (debugger)
		debugger->detach();

	if (cancelled)
		return;

	setPriority(THREAD_PRIORITY_NORMAL);

	saveData();

	done = true;
}

void ProfilerThread::error(const std::wstring& what)
{
	failed = true;
	std::cerr << "ProfilerThread Error: " << what << std::endl;

	::MessageBox(NULL, std::wstring(L"Error: " + what).c_str(), L"Profiler Error", MB_OK);
}

void ProfilerThread::beginProgress(std::wstring stage, int total)
{
	symbolsStage = std::move(stage);
	symbolsDone = 0;
	symbolsTotal = total;
	symbolsPermille = 0;
}

bool ProfilerThread::updateProgress()
{
	symbolsDone++;
	symbolsPermille = MulDiv(symbolsDone, 1000, symbolsTotal);
	if (cancelled)
	{
		failed = true;
		return true;
	}
	return false;
}
