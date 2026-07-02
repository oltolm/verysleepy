/*=====================================================================
threadpicker.cpp
----------------
File created by ClassTemplate on Sun Mar 20 17:12:56 2005

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
#include "threadpicker.h"
#include "launchdlg.h"
#include "logview.h"
#include "optionsdlg.h"
#include "../profiler/symbolinfo.h"
#include <climits>
#include <wx/log.h>
#include <windows.h>
#include <wx/menu.h>
#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/progdlg.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/valnum.h>
#include "../utils/except.h"
#include "appinfo.h"
#include "processlist.h"
#include "profilergui.h"
#include "threadlist.h"

// IDs for the controls and the menu commands
enum
{
	// menu items
	ProcWin_Exit = 1,
	ProcWin_Refresh,
	ProcWin_Download,
	ProcWin_Launch,
	ProcWin_Options,
	ProcWin_TimeCtrl,
	ProcWin_TimeCheck,
	ProcWin_Help_Documentation,
	ProcWin_Help_Support,
	ProcWin_Recent,

	// it is important for the id corresponding to the "About" command to have
	// this standard value as otherwise it won't be handled properly under Mac
	// (where it is special and put into the "Apple" menu)
	ProcWin_Help_About = wxID_ABOUT
};

BEGIN_EVENT_TABLE(ThreadPicker, wxModalFrame)
EVT_BUTTON(wxID_OK, ThreadPicker::OnAttachProfiler)
EVT_BUTTON(wxID_SELECTALL, ThreadPicker::OnAttachProfilerAll)
EVT_MENU(wxID_OPEN, ThreadPicker::OnOpen)
EVT_LIST_ITEM_ACTIVATED(PROCESS_LIST, ThreadPicker::OnDoubleClicked)
EVT_MENU(ProcWin_Exit, ThreadPicker::OnQuit)
EVT_MENU(ProcWin_Refresh, ThreadPicker::OnRefresh)
EVT_MENU(ProcWin_Options, ThreadPicker::OnOptions)
EVT_MENU(ProcWin_Download, ThreadPicker::OnDownload)
EVT_MENU(ProcWin_Launch, ThreadPicker::OnLaunchExe)
EVT_MENU(ProcWin_Help_Documentation, ThreadPicker::OnDocumentation)
EVT_MENU(ProcWin_Help_Support, ThreadPicker::OnSupport)
EVT_MENU(ProcWin_Help_About, ThreadPicker::OnAbout)
EVT_BUTTON(ProcWin_Refresh, ThreadPicker::OnRefresh)
EVT_BUTTON(ProcWin_Download, ThreadPicker::OnDownload)
EVT_CLOSE(ThreadPicker::OnClose)
EVT_BUTTON(ProcWin_Exit, ThreadPicker::OnQuit)
EVT_CHECKBOX(ProcWin_TimeCheck, ThreadPicker::OnTimeCheck)
EVT_MENU_RANGE(wxID_FILE1, wxID_FILE9, ThreadPicker::OnMRUFile)
END_EVENT_TABLE()

wxProgressDialog *g_symProgress = NULL;

void symLogCallback(const wchar_t *text)
{
	wxLogMessage(L"%s", text);

	// Pulse the progress bar, if there is one.
	if (g_symProgress)
		g_symProgress->Pulse();
}

ThreadPicker::ThreadPicker()
	: wxModalFrame(NULL, wxID_ANY, APPNAME), attach_info(), m_recent(NULL)
{
	SetIcon(sleepy_icon);

	wxMenu *menuFile = new wxMenu;
	menuFile->Append(wxID_OPEN, _T("&Open...\tCtrl-O"), _T("Opens an existing profile"));
	menuFile->Append(ProcWin_Launch, _T("&Launch...\tCtrl-N"), _T("Launches a new executable to profile"));
	menuFile->AppendSeparator();

	auto recent = m_recent = new wxMenu;
	menuFile->Append(ProcWin_Recent, _T("&Recent files"), recent);
	wxGetApp().getFileHistory()->UseMenu(recent);

	if (wxGetApp().getFileHistory()->GetCount() == 0)
		menuFile->Enable(ProcWin_Recent, false);

	wxGetApp().getFileHistory()->AddFilesToMenu(recent);

	menuFile->Append(ProcWin_Exit, _T("E&xit\tAlt-X"), _T("Quit this program"));

	wxMenu *menuTools = new wxMenu;
	menuTools->Append(ProcWin_Refresh, _T("&Refresh\tF5"), _T("Refreshes the process list"));
	menuTools->Append(ProcWin_Download, _T("&Download Symbols"), _T("Downloads symbols from a symbol server"));
	menuTools->AppendSeparator();
	menuTools->Append(ProcWin_Options, _T("&Options..."), _T("Opens the options dialog"));

	wxMenu *menuHelp = new wxMenu;
	menuHelp->Append(ProcWin_Help_Documentation, _T("&Documentation\tF1"), _T("Visit the on-line documentation wiki on GitHub"));
	menuHelp->Append(ProcWin_Help_Support, _T("&Support"), _T("Visit the on-line issue list on GitHub"));
	menuHelp->AppendSeparator();
	menuHelp->Append(ProcWin_Help_About, _T("&About..."), _T("Show about dialog"));

	// now append the freshly created menu to the menu bar...
	wxMenuBar *menuBar = new wxMenuBar();
	menuBar->Append(menuFile, _T("&File"));
	menuBar->Append(menuTools, _T("&Tools"));
	menuBar->Append(menuHelp, _T("&Help"));

	// ... and attach this menu bar to the frame
	SetMenuBar(menuBar);

	wxBoxSizer *rootsizer = new wxBoxSizer(wxHORIZONTAL);
	wxBoxSizer *dlgsizer = new wxBoxSizer(wxVERTICAL);
	wxBoxSizer *topsizer = new wxBoxSizer(wxHORIZONTAL);
	wxBoxSizer *bottomsizer = new wxBoxSizer(wxVERTICAL);
	wxBoxSizer *leftsizer = new wxBoxSizer(wxVERTICAL);
	wxBoxSizer *rightsizer = new wxBoxSizer(wxVERTICAL);

	wxPanel *panel = new wxPanel(this);
	rootsizer->Add(panel, 1, wxEXPAND | wxALL);

	wxButton *ok_button = new wxButton(panel, wxID_OK, "Profile &Selected");
	ok_button->SetBitmap(wxBITMAP_PNG(button_profilesel));
	ok_button->SetBitmapPosition(wxRIGHT);
	ok_button->SetBitmapMargins(-1,-1);
	ok_button->SetToolTip("Begins profiling selected threads.");
	wxButton *all_button = new wxButton(panel, wxID_SELECTALL, "Profile &All");
	all_button->SetBitmap(wxBITMAP_PNG(button_profileall));
	all_button->SetBitmapPosition(wxRIGHT);
	all_button->SetBitmapMargins(-1,-1);
	all_button->Disable();
	all_button->SetDefault();
	all_button->SetToolTip("Begins profiling all threads in the selected process.");

	// RM: 20110614 Set time for profiler to run for
	time_value = 100;
	time_check = new wxCheckBox(panel, ProcWin_TimeCheck, "Profile for set time (s)");
	time_ctrl =
		new wxTextCtrl(panel, ProcWin_TimeCtrl, "100", wxDefaultPosition, FromDIP(wxSize(60, 20)),
					   0, wxIntegerValidator<int>(&time_value, 0, INT_MAX));
	time_ctrl->Disable();
	time_ctrl->SetToolTip(
		"When enabled, this will limit the profile to run for a set time in seconds.");

	// DE: 20090325 one list for processes and one list for selected process threads
	threadlist = new ThreadList(panel, ok_button, all_button);
	processlist = new ProcessList(panel, threadlist);

	leftsizer->Add(new wxStaticText(panel, wxID_ANY, "Select a process to profile:"), 0, wxTOP, FromDIP(5));
	leftsizer->Add(processlist, 1, wxEXPAND | wxTOP, FromDIP(3));

	// DE: 20090325 title for thread list
	rightsizer->Add(new wxStaticText(panel, wxID_ANY, "Select thread(s) to profile: (CTRL-click for multiple)"), 0, wxTOP, FromDIP(5));
	rightsizer->Add(threadlist, 1, wxEXPAND | wxTOP, FromDIP(3));

	wxButton *refreshButton = new wxButton(panel, ProcWin_Refresh, "Refresh");
	refreshButton->SetBitmap(wxBITMAP_PNG(button_refresh));
	refreshButton->SetBitmapPosition(wxRIGHT);
	refreshButton->SetBitmapMargins(-1,-1);
	refreshButton->SetToolTip("Refreshes the list of processes and threads.");

	wxButton *downloadButton = new wxButton(panel, ProcWin_Download, "Download");
	downloadButton->SetBitmap(wxBITMAP_PNG(button_download));
	downloadButton->SetBitmapPosition(wxRIGHT);
	downloadButton->SetBitmapMargins(-1,-1);
	downloadButton->SetToolTip("Downloads symbols from a remote symbol server.");

	wxSizer *buttons = new wxBoxSizer(wxHORIZONTAL);
	buttons->Add(refreshButton, 0, wxRIGHT, FromDIP(5));
	buttons->Add(downloadButton);
	buttons->AddStretchSpacer();
	buttons->Add(time_check, 0, wxALIGN_CENTER_VERTICAL);
	buttons->Add(time_ctrl, 0, wxALIGN_CENTER_VERTICAL);
	buttons->AddStretchSpacer();
	buttons->Add(all_button);
	buttons->Add(ok_button, 0, wxLEFT, FromDIP(5));

	bottomsizer->Add(buttons, 0, wxLEFT|wxRIGHT|wxEXPAND, FromDIP(10));
	bottomsizer->AddSpacer(FromDIP(8));

	log = new LogView(panel);
	bottomsizer->Add(log, 0, wxLEFT|wxRIGHT|wxBOTTOM|wxEXPAND, FromDIP(10));

	topsizer->Add(leftsizer, 1, wxEXPAND | wxLEFT, FromDIP(10));
	topsizer->AddSpacer(FromDIP(10));
	topsizer->Add(rightsizer, 1, wxEXPAND | wxRIGHT, FromDIP(10));
	dlgsizer->Add(topsizer, 1, wxEXPAND);
	dlgsizer->AddSpacer(FromDIP(8));
	dlgsizer->Add(bottomsizer, 0, wxEXPAND);

	panel->SetSizer(dlgsizer);
	panel->SetAutoLayout(TRUE);

	SetSizer(rootsizer);
	rootsizer->SetSizeHints(this);
	SetAutoLayout(TRUE);

	wxSize size = wxGetDisplaySize();
	size.Scale(0.8f, 0.8f);
	SetSize(size);
	Centre();

	g_symLog = symLogCallback;
}

void ThreadPicker::OnMRUFile(wxCommandEvent& event)
{
	open_filename = wxGetApp().getFileHistory()->GetHistoryFile(event.GetId() - wxID_FILE1);
	if (!open_filename.empty())
	{
		if (!wxFileExists(open_filename))
		{
			wxGetApp().getFileHistory()->RemoveFileFromHistory(event.GetId() - wxID_FILE1);
			return;
		}
		EndModal(OPEN);
	}
}

void ThreadPicker::OnOpen(wxCommandEvent& WXUNUSED(event))
{
	open_filename = ProfilerGUI::PromptOpen(this);
	if (!open_filename.empty())
	{
		wxGetApp().getFileHistory()->AddFileToHistory(open_filename);
		EndModal(OPEN);
	}
}

void ThreadPicker::OnAttachProfiler(wxCommandEvent& WXUNUSED(event))
{
	if (TryAttachToProcess(false))
	{
		EndModal(ATTACH);
	}
}

void ThreadPicker::OnAttachProfilerAll(wxCommandEvent& WXUNUSED(event))
{
	if (TryAttachToProcess(true))
	{
		EndModal(ATTACH);
	}
}

void ThreadPicker::OnDoubleClicked(wxListEvent& WXUNUSED(event))
{
	if (TryAttachToProcess(false))
	{
		EndModal(ATTACH);
	}
}

void ThreadPicker::OnClose(wxCloseEvent& WXUNUSED(event))
{
	EndModal(QUIT);
}

void ThreadPicker::OnQuit(wxCommandEvent& WXUNUSED(event))
{
	EndModal(QUIT);
}

void ThreadPicker::OnRefresh(wxCommandEvent& WXUNUSED(event))
{
	processlist->updateProcesses();
}

void ThreadPicker::OnOptions(wxCommandEvent& WXUNUSED(event))
{
	OptionsDlg dlg;
	if (dlg.ShowModal() != wxID_OK)
		return;
}

void ThreadPicker::OnDownload(wxCommandEvent& WXUNUSED(event))
{
	g_symProgress = new wxProgressDialog(APPNAME, "Downloading symbols...", 100, this);
	processlist->reloadSymbols(true);
	delete g_symProgress;
	g_symProgress = NULL;
}

void ThreadPicker::OnLaunchExe(wxCommandEvent& WXUNUSED(event))
{
	wxString prevCmdPath;
	config.Read("PrevLaunchPath", &prevCmdPath, "");
	wxString prevCwd;
	config.Read("PrevLaunchCwd", &prevCwd, "");

	LaunchDlg dlg(this);
	dlg.SetCmdValue(prevCmdPath);
	dlg.SetCwdValue(prevCwd);
	if (dlg.ShowModal() != wxID_OK)
		return;

	run_filename = dlg.GetCmdValue();
	config.Write("PrevLaunchPath", run_filename.c_str());
	run_cwd = dlg.GetCwdValue();
	config.Write("PrevLaunchCwd", run_cwd.c_str());
	EndModal(RUN);
}

void ThreadPicker::OnDocumentation(wxCommandEvent& WXUNUSED(event))
{
	wxLaunchDefaultBrowser(GITURL "/wiki");
}

void ThreadPicker::OnSupport(wxCommandEvent& WXUNUSED(event))
{
	wxLaunchDefaultBrowser(GITURL "/issues");
}

void ThreadPicker::OnAbout(wxCommandEvent& WXUNUSED(event))
{
	ProfilerGUI::ShowAboutBox();
}

void ThreadPicker::OnTimeCheck(wxCommandEvent& WXUNUSED(event))
{
	if( time_check->IsChecked() )
	{
		time_ctrl->Enable();
	}
	else
	{
		time_ctrl->Disable();
	}
}

ThreadPicker::~ThreadPicker()
{
	if (m_recent)
		wxGetApp().getFileHistory()->RemoveMenu(m_recent);
	g_symLog = NULL;
	delete log;
}

bool ThreadPicker::TryAttachToProcess(bool allThreads)
{
	try
	{
		AttachToProcess(allThreads);
	}
	catch (SleepyException &e)
	{
		wxLogError("%ls\n", e.wwhat());
		return false;
	}

	return true;
}

void ThreadPicker::AttachToProcess(bool allThreads)
{
	assert(IsModal());

	attach_info.reset(new AttachInfo);
	attach_info->attach_all_threads = allThreads;

	DWORD pid = processlist->getSelectedProcessId();
	enforce(pid, "No process selected");

	// RM: 20130614 Check if the user wants the profile to run for a set time period
	if (time_check->IsChecked() && time_ctrl->GetValidator()->TransferFromWindow())
	{
		attach_info->limit_profile_time = time_value;
	}
	else
	{
		attach_info->limit_profile_time = -1; // run until cancelled
	}

	//------------------------------------------------------------------------
	//Get handle to target process
	//------------------------------------------------------------------------
	handle_ptr process_handle(OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid));
	attach_info->process_id = pid;
	attach_info->sym_info = processlist->takeSymbolInfo();
	enforce(!!attach_info->sym_info, "No symbol info");

	// Check it didn't exit.
	DWORD ret = WaitForSingleObject(process_handle.get(), 0);
	if (ret == WAIT_OBJECT_0)
		attach_info->process_id = 0;

	enforce(attach_info->process_id, "Cannot attach to running process");

	// DE: 20090325 attaches to specific a list of threads
	std::vector<const ThreadInfo*> selectedThreads = threadlist->getSelectedThreads(allThreads);
	if (selectedThreads.size() == 0)
	{
		selectedThreads = threadlist->getSelectedThreads(true);
	}
	enforce(selectedThreads.size(), "No thread(s) selected");

	// DE: 20090325 attaches to specific a list of threads
	for (auto it = selectedThreads.begin(); it != selectedThreads.end(); ++it)
	{
		try
		{
			const ThreadInfo* threadInfo(*it);

			DWORD thread_id = threadInfo->getID();
			wenforce(thread_id, "Attaching to selected thread");
			attach_info->thread_ids.push_back(thread_id);
		}
		catch (SleepyException &e)
		{
			wxLogError("%ls\n", e.wwhat());
		}
	}

	// DE: 20090325 attaches to specific a list of threads
	enforce(attach_info->thread_ids.size(), "Cannot attach to any threads");
}
