/*=====================================================================
profilergui.cpp
---------------
File created by ClassTemplate on Sun Mar 13 18:16:34 2005

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

http://www.gnu.org/copyleft/gpl.html
=====================================================================*/
#include "profilergui.h"
#include <wx/cmdline.h>
#include <wx/image.h>
#include <wx/mstream.h>
#include <wx/apptrait.h>
#include <wx/msgdlg.h>
#include <memory>

#include "threadpicker.h"
#include "capturewin.h"
#include "mainwin.h"
#include "persistentlistctrl.h"
#include "../utils/dbginterface.h"
#include "../profiler/processinfo.h"
#include "../profiler/profilerthread.h"
#include "../profiler/debugger.h"
#include "../utils/osutils.h"
#include <wx/progdlg.h>
#include <wx/stdpaths.h>
#include <wx/filedlg.h>
#include <wx/scopeguard.h>
#include <wx/stopwatch.h>
#include <wx/timer.h>
#ifdef _MSC_VER
#include "../crashback/client/crashback.h"
#endif
#include "aboutdlg.h"
#include "../utils/except.h"
#include "appinfo.h"
#include <limits>

// DE: 20090325 Linking fails in debug target under visual studio 2005
// RJM: works for me :-/
// #include <wx/apptrait.h>
// #if wxUSE_STACKWALKER && defined( __WXDEBUG__ )
//// silly workaround for the link error with debug configuration:
//// \src\common\appbase.cpp
// wxString wxAppTraitsBase::GetAssertStackTrace()
//{
//    return wxT("");
// }
// #endif

static const wxCmdLineEntryDesc g_cmdLineDesc[] =
{
	{ wxCMD_LINE_SWITCH, "h", "", "Displays help on the command line parameters.",          wxCMD_LINE_VAL_NONE, wxCMD_LINE_OPTION_HELP },
	{ wxCMD_LINE_OPTION, "r", "", "Runs an executable and profiles it.",                    wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_OPTIONAL|wxCMD_LINE_NEEDS_SEPARATOR },
	{ wxCMD_LINE_OPTION, "a", "", "Attaches to a process (by its PID) and profiles it.",    wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_OPTIONAL|wxCMD_LINE_NEEDS_SEPARATOR },
	{ wxCMD_LINE_OPTION, "i", "", "Loads an existing profile from a file.",                 wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_OPTIONAL|wxCMD_LINE_NEEDS_SEPARATOR },
	{ wxCMD_LINE_OPTION, "o", "", "Saves the captured profile to the given file.",          wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_OPTIONAL|wxCMD_LINE_NEEDS_SEPARATOR },
	{ wxCMD_LINE_OPTION, "d", "", "Waits N seconds before beginning capture.",              wxCMD_LINE_VAL_NUMBER, wxCMD_LINE_PARAM_OPTIONAL },
	{ wxCMD_LINE_OPTION, "t", "", "Stops capturing automatically after N seconds time.",    wxCMD_LINE_VAL_NUMBER, wxCMD_LINE_PARAM_OPTIONAL },
	{ wxCMD_LINE_SWITCH, "q", "", "Quiet mode (no error messages will be shown).",          wxCMD_LINE_VAL_NONE },
	{wxCMD_LINE_SWITCH, "", "wine", "Use Wine DbgHelp.", wxCMD_LINE_VAL_NONE},
	{wxCMD_LINE_SWITCH, "", "mingw", "Use Dr. MinGW DbgHelp.", wxCMD_LINE_VAL_NONE},
	{ wxCMD_LINE_SWITCH, "mt", "", "When attaching a process, profiles only main thread.",  wxCMD_LINE_VAL_NONE },
	{ wxCMD_LINE_SWITCH, "mbt", "", "When attaching a process, profiles only most busy thread.",    wxCMD_LINE_VAL_NONE },
	{ wxCMD_LINE_OPTION, "thread", "", "Profiles the specified thread(s) in the process, multiple threads must be in a comma-delimited list without spaces (See /a for specifying the process ID). Examples: `/thread:2124` or `/thread:8086,24601,42`",    wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_OPTIONAL|wxCMD_LINE_NEEDS_SEPARATOR },

	{ wxCMD_LINE_OPTION, "minidump", "", "capture a minidump after N seconds time.",        wxCMD_LINE_VAL_NUMBER, wxCMD_LINE_PARAM_OPTIONAL },
	{ wxCMD_LINE_OPTION, "samplerate", "", "set the sample rate speed",                     wxCMD_LINE_VAL_NUMBER, wxCMD_LINE_PARAM_OPTIONAL },
	{ wxCMD_LINE_OPTION, "symsearchpath", "", "Specify the symbol search path.",            wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_OPTIONAL|wxCMD_LINE_NEEDS_SEPARATOR },
	{ wxCMD_LINE_OPTION, "symcachedir", "", "Specify the directory to use for the symbol cache.", wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_OPTIONAL|wxCMD_LINE_NEEDS_SEPARATOR },
	{ wxCMD_LINE_SWITCH, "usesymserver", "", "Use a symbol server.",                        wxCMD_LINE_VAL_NONE, wxCMD_LINE_SWITCH_NEGATABLE },
	{ wxCMD_LINE_OPTION, "symserver", "", "Specify the symbol server path/URL.",            wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_OPTIONAL|wxCMD_LINE_NEEDS_SEPARATOR },

	{ wxCMD_LINE_PARAM, NULL, NULL, "Loads an existing profile from a file.",               wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_OPTIONAL},

	{ wxCMD_LINE_NONE }
};

wxIcon sleepy_icon;
std::wstring cmdline_load, cmdline_save, cmdline_run, cmdline_attach;
long cmdline_delay = 0;
long cmdline_timeout = -1;  // -1 means profile until cancelled
std::vector<DWORD> cmdline_thread_ids;
std::vector<std::wstring> tmp_files;
Prefs prefs;
wxConfig config(_T(APPNAME), _T(VENDOR));

ProfilerGUI::ProfilerGUI()
{
	initialized = false;
	captureWin = NULL;
	InitSysInfo();
}


ProfilerGUI::~ProfilerGUI()
{

}


IMPLEMENT_APP(ProfilerGUI)

class ProfilerAppTraits : public wxGUIAppTraits
{
	virtual void SetLocale()
	{
		// wxWidgets, Y U futz with locale? Why?!
		// leave it alone dammit!
	}
};

wxAppTraits *ProfilerGUI::CreateTraits()
{
	return new ProfilerAppTraits;
}

void CleanupTempFiles()
{
	for (std::wstring& s : tmp_files)
	{
		DeleteFile(s.c_str());
	}
	tmp_files.clear();
}

void ProfilerGUI::ShowAboutBox()
{
	AboutDlg dlg;
	dlg.ShowModal();
}

wxString ProfilerGUI::PromptOpen(wxWindow *parent)
{
	wxFileDialog dlg(parent, "Open File", "", "", _T(APPNAME) L" Profiles (*.sleepy)|*.sleepy",
		wxFD_OPEN);
	if (dlg.ShowModal() != wxID_CANCEL)
		return dlg.GetPath();
	else
		return wxEmptyString;
}

/// Returns the path to the profile archive, or an empty string
/// if profiling was aborted by the user.
std::wstring ProfilerGUI::LaunchProfiler(std::unique_ptr<AttachInfo> info)
{
	// AA: 20210822 if we're attaching to all threads, launch a debugger to update the threads
	Debugger *debugger = NULL;
	if (info->attach_all_threads)
	{
		if (info->process_id)
			debugger = new Debugger(info->process_id);
	}

	//------------------------------------------------------------------------
	// create the profiler thread
	//------------------------------------------------------------------------
	// DE: 20090325 attaches to a specific list of threads
	auto profilerthread = std::make_unique<ProfilerThread>(info->process_id, info->thread_ids,
														   info->sym_info.get(), debugger);

	//------------------------------------------------------------------------
	//start the profiler thread
	//------------------------------------------------------------------------
	bool aborted = false;
	{
		captureWin->Show();

		profilerthread->launch(THREAD_PRIORITY_TIME_CRITICAL);

		wxStopWatch stopwatch;
		stopwatch.Start();

		class : public wxTimer
		{
		public:
			bool fired;
			void Notify() { fired = true; }
		} timer;
		timer.fired = false;
		timer.Start(100);

		while (true)
		{
			wxYieldIfNeeded();

			if (timer.fired)
			{
				timer.fired = false;

				std::wstring status = profilerthread->getStatus();
				int numSamples = profilerthread->getSampleProgress();
				int numThreads = profilerthread->getNumThreadsRunning();
				int timeout = info->limit_profile_time;
				double elapsed = profilerthread->getDuration();

				if (status.empty())
				{
					if (timeout == -1)
						status = wxString::Format(L"%i samples, %.1fs elapsed, %i threads running",
												  numSamples, elapsed, numThreads);
					else
						status =
							wxString::Format(L"%i samples, %.1fs/%ds elapsed, %i threads running",
											 numSamples, elapsed, timeout, numThreads);
				}

				double progress = timeout == -1 ? std::numeric_limits<double>::quiet_NaN() : (elapsed / timeout);

				if (!captureWin->UpdateProgress(status, progress))
					break;

				if (progress >= 1)
					break;
			}

			profilerthread->setPaused(captureWin->Paused());

			if (profilerthread->getNumThreadsRunning() <= 0)
				break;

			WaitMessage(); // in lieu of a wxWaitForEvent
		}
		aborted = captureWin->Cancelled();
		captureWin->Hide();
	}

	profilerthread->commitSuicide();
	wxLog::FlushActive();

	if (aborted)
	{
		profilerthread->cancel();
		profilerthread->join();
		return std::wstring();
	}

	{
		wxProgressDialog dlg(APPNAME, "Waiting for symbol query to start...", 1000, NULL,
			wxPD_APP_MODAL | wxPD_AUTO_HIDE | wxPD_CAN_ABORT);

		while (true)
		{
			int permille;
			std::wstring stage;
			profilerthread->getSymbolsProgress(&permille, &stage);
			if (profilerthread->getDone() || profilerthread->getFailed())
				break;
			if (!dlg.Update(permille, stage))
				profilerthread->cancel();
			profilerthread->waitFor(100);
		}
		profilerthread->join();
	}

	bool failed = profilerthread->getFailed();
	std::wstring output_filename = profilerthread->getFilename();

	if (failed)
		return std::wstring();

	enforce(!output_filename.empty(), "There was a problem creating the profile data.");

	tmp_files.push_back(output_filename);
	atexit(CleanupTempFiles);

	return output_filename;
}

AttachInfo::AttachInfo()
{
	attach_all_threads = true;
	sym_info = NULL;
	limit_profile_time = cmdline_timeout;
}

AttachInfo::~AttachInfo() {}

static DWORD getMostBusyThread(ProcessInfo& process_info)
{
	int max = -1;
	DWORD mostBusy = 0;
	for (auto thread_info = process_info.threads.begin(); thread_info != process_info.threads.end(); ++thread_info)
	{
		thread_info->recalcUsage(0);
		if (max < thread_info->totalCpuTimeMs)
		{
			max = thread_info->totalCpuTimeMs;
			mostBusy = thread_info->getID();
		}
	}

	return mostBusy;
}

static bool getAttachToAllThreads()
{
	return prefs.attachMode == ATTACH_ALL_THREAD;
}

static std::vector<DWORD> getThreadsByAttachMode(ProcessInfo& process_info)
{
	std::vector<DWORD> thread_ids;

	if (process_info.threads.empty())
		return thread_ids;

	switch (prefs.attachMode)
	{
	case ATTACH_MAIN_THREAD:
		thread_ids.push_back(process_info.threads.front().getID());
		return thread_ids;

	case ATTACH_MOST_BUSY_THREAD:
	{
		DWORD mostBusy = getMostBusyThread(process_info);
		if (mostBusy)
			thread_ids.push_back(mostBusy);
		return thread_ids;
	}

	default: // all thread
		thread_ids.reserve(process_info.threads.size());
		for (auto thread_info = process_info.threads.begin(); thread_info != process_info.threads.end(); ++thread_info)
		{
			thread_ids.push_back(thread_info->getID());
		}
		return thread_ids;
	}
}

std::unique_ptr<AttachInfo> ProfilerGUI::RunProcess(const std::wstring& run_cmd,
													const std::wstring& run_cwd)
{
	STARTUPINFO si = {sizeof(si)};
	PROCESS_INFORMATION pi = {};
	handle_ptr thread_handle(pi.hThread);
	handle_ptr process_handle(pi.hProcess);

	std::wstring run_cmd_dup = run_cmd; // CreateProcess lpCommandLine must be mutable
	wenforce(CreateProcess( NULL, &run_cmd_dup[0], NULL, NULL, FALSE, 0, NULL, run_cwd.size() ? run_cwd.c_str() : NULL, &si, &pi ), "CreateProcess");

	if (!CanProfileProcess(pi.hProcess))
	{
		throw SleepyException(L"Unsupported process. Cannot profile.");
	}

	std::unique_ptr<AttachInfo> output(new AttachInfo);
	output->process_id = pi.dwProcessId;

	if (cmdline_delay == 0)
	{
		output->thread_ids.push_back(pi.dwThreadId); // Main thread only
	}
	else
	{
		wxProgressDialog progressdlg(APPNAME, "Waiting...",
			cmdline_delay * 1000, theMainWin,
			wxPD_APP_MODAL|wxPD_AUTO_HIDE|wxPD_CAN_SKIP|wxPD_CAN_ABORT);
		int start = GetTickCount();
		int total = cmdline_delay * 1000;

		while (true)
		{
			wxYieldIfNeeded();
			int now = GetTickCount();
			int elapsed = now - start;
			if (elapsed > cmdline_delay * 1000)
				break;
			int remaining = total - elapsed;
			progressdlg.Update(elapsed);
			if (progressdlg.WasCancelled())
				throw SleepyException(L"User abort");
			if (progressdlg.WasSkipped())
				break;
			Sleep(std::min(remaining, 100));
		}

		// Re-query process information to learn about new threads that have since spawned
		ProcessInfo process_info = ProcessInfo::FindProcessById(pi.dwProcessId);
		output->thread_ids = getThreadsByAttachMode(process_info);
		output->attach_all_threads = getAttachToAllThreads();
	}

	output->sym_info = std::make_unique<SymbolInfo>();
	TryLoadSymbols(output.get());
	return output;
}

std::unique_ptr<AttachInfo> ProfilerGUI::AttachToProcess(const std::wstring& processId)
{
	DWORD processId_dw;
	try
	{
		processId_dw = std::stoul(processId);
	}
	catch (const std::exception&)
	{
		throw SleepyException(L"Not valid process id: " + processId);
	}
	ProcessInfo process_info = ProcessInfo::FindProcessById(processId_dw);
	auto attach_info = std::make_unique<AttachInfo>();
	attach_info->process_id = process_info.getID();
	attach_info->thread_ids = getThreadsByAttachMode(process_info);
	attach_info->attach_all_threads = getAttachToAllThreads();
	attach_info->sym_info = std::make_unique<SymbolInfo>();

	TryLoadSymbols(attach_info.get());
	return attach_info;
}

void ProfilerGUI::TryLoadSymbols(AttachInfo* output)
{
	// Load up the debug info for it.
	// This can fail initially, because it turns out that you can't query information
	// about a process until that process has registered itself fully with CSRSS.
	// So we wait a little and try again. I'm not sure what the correct solution is,
	// I think possibly monitoring for debug events might be the way to go.
	int retry = 100;
	while (retry--)
	{
		Sleep(10);
		try
		{
			output->sym_info->loadSymbols(output->process_id, false);
			break;
		}
		catch (...)
		{
			if (retry == 0)
				throw;
		}
	}
}

void ProfilerGUI::LoadProfileData(const std::wstring &filename)
{
	Database *database = new Database();
	database->loadFromPath(filename, config.Read("MainWinCollapseOS", 1) != 0, false);

	MainWin *frame = new MainWin(wxString::Format("%s - %s", APPNAME, filename), filename, database);
	frame->Show(TRUE);
	frame->Update();
	frame->Raise();
	frame->reset();
}

/// Returns the path to the profile archive, or an empty string
/// if the user quit the application.
std::wstring ProfilerGUI::ObtainProfileData()
{
	while (true)
	{
		std::unique_ptr<ThreadPicker> threadpicker(new ThreadPicker);
		int mode = threadpicker->ShowModal();
		wxLog::FlushActive();

		switch (mode)
		{
		case ThreadPicker::QUIT:
			return std::wstring();

		case ThreadPicker::OPEN:
			return threadpicker->open_filename;

		case ThreadPicker::ATTACH:
			{
			return LaunchProfiler(std::move(threadpicker->attach_info));
			}

		case ThreadPicker::RUN:
			// Create the window before we create the process,
			// so we don't steal focus from it.
			captureWin->Show();

			std::unique_ptr<AttachInfo> info;
			try
			{
				info = RunProcess(threadpicker->run_filename, threadpicker->run_cwd);
			}
			catch (SleepyException &e)
			{
				captureWin->Hide();
				wxLogError("%ls\n", e.wwhat());
				MessageBox(threadpicker->GetHWND(), std::wstring(L"Error: " + e.wwhat()).c_str(), L"Profiler Error", MB_OK);
				continue;
			}

			handle_ptr process_handle(OpenProcess(PROCESS_ALL_ACCESS, FALSE, info->process_id));
			wxScopeGuard sgTerm = wxMakeGuard(TerminateProcess, process_handle.get(), 0);
			wxUnusedVar(sgTerm);
			return LaunchProfiler(std::move(info));
		}
	}
}

bool ProfilerGUI::OnInit()
{
#ifndef _DEBUG
	if (getenv("SLEEPY_SILENT_CRASH"))
		SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
#ifdef _MSC_VER
	else
		cbStartup();
#endif
#endif

	wxInitAllImageHandlers();
	try
	{
		EnableDebugPrivilege();

		sleepy_icon = wxICON(sleepy);

		// Make a default cache in their user directory.
		wxString symCache = wxStandardPaths::Get().GetUserLocalDataDir();

		prefs.symSearchPath.SetConfigValue(config.Read("SymbolSearchPath", ""));
		prefs.useSymServer.SetConfigValue(config.Read("UseSymbolServer", 1) != 0);
		prefs.symServer.SetConfigValue(config.Read("SymbolServer", "http://msdl.microsoft.com/download/symbols"));
		prefs.symCacheDir.SetConfigValue(config.Read("SymbolCache", symCache));
		prefs.useWinePref = config.Read("UseWine", (long)0) != 0;
		prefs.appearance = (Theme)config.ReadLong("Appearance", System);
		prefs.useWinePref = config.Read("UseWine", (long)0) != 0;
		prefs.saveMinidump.SetConfigValue(config.Read("SaveMinidump", -1));
		prefs.throttle.SetConfigValue(prefs.ValidateThrottle(config.Read("SpeedThrottle", 100)));

#if wxCHECK_VERSION(3, 3, 0)
		switch (prefs.appearance)
		{
		case Light:
			SetAppearance(Appearance::Light);
			break;
		case Dark:
			SetAppearance(Appearance::Dark);
			break;
		case System:
			SetAppearance(Appearance::System);
			break;
		}
#endif

		m_fileHistory.Load(config);

		captureWin = new CaptureWin();

		if (!wxApp::OnInit())
			return false;

		InitializeGuiPersistence();

		return true;
	}
	catch (SleepyException &e)
	{
		wxLogError("%ls\n", e.wwhat());
	}
	catch (std::exception &e)
	{
		wxLogError("%s\n", e.what());
	}
	return false;
}

bool ProfilerGUI::OnExceptionInMainLoop()
{
	throw;
}

bool ProfilerGUI::ProcessIdle()
{
	bool result = wxApp::ProcessIdle();
	HandleInit();
	return result;
}

void ProfilerGUI::HandleInit()
{
	if (initialized)
		return;

	initialized = true;

	SetExitOnFrameDelete(false);

	int status = 0;
	try
	{
		if (Run())
		{
			SetExitOnFrameDelete(true);
			return;
		}
	}
	catch (SleepyException &e)
	{
		wxLogError("%ls\n", e.wwhat());
		status = 1;
	}
	wxEventLoop::GetActive()->Exit(status);
}

/// Returns true if a frame is still active.
bool ProfilerGUI::Run()
{
	if (!dbgHelpInit())
		return false;

	// Explicitly create and set the default logger, so other threads use it.
	// Otherwise, wxWidgets will create a default logger on request,
	// but only by the request of the main thread.
	// Log messages for other threads will be discarded.
	// note : logger was already created inside ProcessIdle that was called before Run, need to delete it
	delete wxLog::SetActiveTarget(new wxLogGui);

	std::wstring filename;

	if (!cmdline_run.empty())
	{
		std::unique_ptr<AttachInfo> info(RunProcess(cmdline_run, L""));
		handle_ptr process_handle(OpenProcess(PROCESS_ALL_ACCESS, FALSE, info->process_id));
		wxScopeGuard sgTerm = wxMakeGuard(TerminateProcess, process_handle.get(), 0);
		wxUnusedVar(sgTerm);
		filename = LaunchProfiler(std::move(info));
	}
	else if (!cmdline_attach.empty())
	{
		std::unique_ptr<AttachInfo> info(AttachToProcess(cmdline_attach));
		if (!cmdline_thread_ids.empty())
		{
			auto pred = [&](DWORD dwThreadId) -> bool {
				return std::find(cmdline_thread_ids.begin(), cmdline_thread_ids.end(),
								 dwThreadId) == cmdline_thread_ids.end();
			};
			info->thread_ids.erase(
				std::remove_if(info->thread_ids.begin(), info->thread_ids.end(), pred),
				info->thread_ids.end());
			// Do not attach to any new threads created after this point in time.
			info->attach_all_threads = false;
		}
		filename = LaunchProfiler(std::move(info));
	}
	else if (!cmdline_load.empty())
		filename = cmdline_load;
	else
	{
		filename = ObtainProfileData();
		if (filename.empty())
			return false; // Profiling was aborted
	}

	if (!cmdline_save.empty())
	{
		wenforce(CopyFile(filename.c_str(), cmdline_save.c_str(), FALSE), "Saving profile data");
		return false;	// No GUI, just save and exit
	}

	LoadProfileData(filename);
	return true;
}

int ProfilerGUI::OnExit()
{
	config.Write("SymbolSearchPath", prefs.symSearchPath.GetConfigValue());
	config.Write("UseSymbolServer", prefs.useSymServer.GetConfigValue());
	config.Write("SymbolServer", prefs.symServer.GetConfigValue());
	config.Write("SymbolCache", prefs.symCacheDir.GetConfigValue());
	config.Write("UseWine", prefs.useWinePref);
	config.Write("Appearance", (long)prefs.appearance);
	config.Write("SaveMinidump", prefs.saveMinidump.GetConfigValue());
	config.Write("SpeedThrottle", prefs.throttle.GetConfigValue());

	m_fileHistory.Save(config);

	return wxApp::OnExit();
}

void ProfilerGUI::OnInitCmdLine(wxCmdLineParser& parser)
{
	//parser.DisableLongOptions();
	parser.SetDesc(g_cmdLineDesc);
	parser.SetSwitchChars("/-");
}

bool ProfilerGUI::OnCmdLineParsed(wxCmdLineParser& parser)
{
	wxString param;
	long long_param;

	// command line options that override saved setting, will not replace
	//   the data in the saved config.

	if (parser.Found("q"))
		wxLog::EnableLogging(false);

	if (parser.Found("r") && parser.Found("i"))
	{
		parser.Usage();
		return false;
	}

	if (parser.Found("i", &param))
		cmdline_load = param;
	if (parser.GetParamCount())
		cmdline_load = parser.GetParam(0);
	if (parser.Found("o", &param))
		cmdline_save = param;
	if (!parser.Found("d", &cmdline_delay))
		cmdline_delay = 0;
	if (!parser.Found("t", &cmdline_timeout))
		cmdline_timeout = -1;
	if (parser.Found("r", &param))
		cmdline_run = param;
	if (parser.Found("a", &param))
		cmdline_attach = param;
	if (parser.Found("thread", &param))
	{
		auto tids_str = wxSplit(param,',');
		for (size_t i = 0; i < tids_str.size(); i++)
		{
			long tid;
			if (tids_str[i].ToLong(&tid))
			{
				cmdline_thread_ids.push_back(tid);
			}
			else
			{
				wxMessageBox(wxString::Format(wxT("Ignoring malformed thread ID in /thread option: %s"), tids_str[i]),
							 APPNAME,
							 wxICON_WARNING);
			}
		}
	}
	if (parser.Found("minidump",&long_param))
		prefs.saveMinidump.Override(long_param);
	if (parser.Found("samplerate",&long_param))
		prefs.throttle.Override(prefs.ValidateThrottle(long_param));
	if (parser.Found("wine"))
		prefs.useWineSwitch = true;
	if (parser.Found("mingw"))
		prefs.useMingwSwitch = true;
	if (parser.Found("mt", &param))
		prefs.attachMode = ATTACH_MAIN_THREAD;
	if (parser.Found("mbt", &param))
		prefs.attachMode = ATTACH_MOST_BUSY_THREAD;
	if (parser.Found("symsearchpath", &param))
		prefs.symSearchPath.Override(param);
	if (parser.Found("symcachedir", &param))
		prefs.symCacheDir.Override(param);
	if (auto state = parser.FoundSwitch("usesymserver"))
		prefs.useSymServer.Override(state == wxCMD_SWITCH_ON);
	if (parser.Found("symserver", &param))
		prefs.symServer.Override(param);

	return true;
}

wxFileHistory *ProfilerGUI::getFileHistory()
{
	return &m_fileHistory;
}
