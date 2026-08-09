/*=====================================================================
profilergui.h
-------------
File created by ClassTemplate on Sun Mar 13 18:16:34 2005

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
#pragma once

#include <algorithm>
#include <memory>
#include <wx/config.h>
#include <wx/app.h>
#include <wx/filehistory.h>
#include <string>
#include <vector>

extern wxIcon sleepy_icon;

class SymbolInfo;
class ThreadPicker;

enum Theme
{
	Light,
	Dark,
	System
};

enum AttachMode
{
	ATTACH_ALL_THREAD, // default
	ATTACH_MAIN_THREAD,
	ATTACH_MOST_BUSY_THREAD,
};

struct AttachInfo
{
	AttachInfo();
	~AttachInfo();

	DWORD process_id;
	std::vector<DWORD> thread_ids;
	bool attach_all_threads;
	std::unique_ptr<SymbolInfo> sym_info;
	int limit_profile_time;
};

// Encapsulate a (config) value that can be overridden.
//   `T` must be copyable and have a (sensible) default constructor.
template<typename T>
class OverridableOption
{
public:
	OverridableOption()
		: is_overridden(false)
	{
	}

	OverridableOption(const T& config_value)
		: config_value(config_value),
		  is_overridden(false)
	{
	}

	// setters
	void SetConfigValue(const T& new_value)
	{
		is_overridden = false;
		config_value = new_value;
	}

	void Override(const T& ovr_value)
	{
		is_overridden = true;
		override_value = ovr_value;
	}

	T GetValue() const { return is_overridden ? override_value : config_value; }

	T GetConfigValue() const { return config_value; }

	bool IsOverridden() const { return is_overridden; }

protected:
	T config_value;
	bool is_overridden;
	T override_value;
};

class Prefs
{
public:
	Prefs()
		: useSymServer(false),
		  saveMinidump(-1),
		  throttle(100)
	{
		useWinePref = useWineSwitch = useMingwSwitch = false;
		attachMode = ATTACH_ALL_THREAD;
	}

	OverridableOption<wxString> symSearchPath;
	OverridableOption<bool> useSymServer;
	OverridableOption<wxString> symCacheDir;
	OverridableOption<wxString> symServer;
	OverridableOption<int> saveMinidump; // Save minidump after X seconds. -1 = disabled
	OverridableOption<int> throttle;

	bool useWinePref, useWineSwitch, useMingwSwitch;
	Theme appearance;
	AttachMode attachMode;

	bool UseWine() { return useMingwSwitch ? false : useWineSwitch ? true : useWinePref; }

	// Add any configured search paths, and the symbol server if enabled.
	void AdjustSymbolPath(std::wstring& sympath, bool download)
	{
		if (!symSearchPath.GetValue().empty())
		{
			if (!sympath.empty())
				sympath += L";";
			sympath += symSearchPath.GetValue();
		}

		if (useSymServer.GetValue())
		{
			if (!sympath.empty())
				sympath += L";";
			sympath += L"SRV*";
			sympath += symCacheDir.GetValue();
			if (download)
				sympath += L"*" + symServer.GetValue();
		}
	}

	static int ValidateThrottle(int value) { return std::clamp(value, 1, 100); }
};

/*=====================================================================
ProfilerGUI
-----------
the main app
=====================================================================*/
class ProfilerGUI : public wxApp
{
public:
	ProfilerGUI();
	virtual ~ProfilerGUI();
	bool OnInit() override;
	bool OnExceptionInMainLoop() override;
	bool ProcessIdle() override;
	int OnExit() override;

	static void ShowAboutBox();
	static wxString PromptOpen(wxWindow *parent);

	wxAppTraits *CreateTraits() override;

	wxFileHistory *getFileHistory();

protected:
	void OnInitCmdLine(wxCmdLineParser& parser) override;
	bool OnCmdLineParsed(wxCmdLineParser& parser) override;

private:
	void HandleInit();
	bool Run();

	std::wstring LaunchProfiler(std::unique_ptr<AttachInfo> info);
	std::unique_ptr<AttachInfo> RunProcess(const std::wstring& run_cmd,
										   const std::wstring& run_cwd);
	std::unique_ptr<AttachInfo> AttachToProcess(const std::wstring& processId);
	static void TryLoadSymbols(AttachInfo *output);
	void LoadProfileData(const std::wstring& filename);
	std::wstring ObtainProfileData();

	class CaptureWin *captureWin;
	bool initialized;
	wxFileHistory m_fileHistory;
};

wxDECLARE_APP(ProfilerGUI);

extern Prefs prefs;
extern wxConfig config;
