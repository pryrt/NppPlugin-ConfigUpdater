#include "ConfigUpdaterClass.h"
#include "ValidateXML.h"
#include "PopulateXSD.h"
#include "pcjHelper.h"

extern NppData nppData;

bool ConfigUpdater::_is_dir_writable(const std::wstring& path)
{
	std::wstring tmpFileName = path;
	pcjHelper::delNull(tmpFileName);
	tmpFileName += L"\\~$TMPFILE.PRYRT";

	HANDLE hFile = CreateFile(tmpFileName.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE) {
		DWORD errNum = GetLastError();
		if (errNum != ERROR_ACCESS_DENIED) {
			std::wstring errmsg = L"Error when testing if \"" + path + L"\" is writeable: " + std::to_wstring(GetLastError()) + L"\n";
			::MessageBox(NULL, errmsg.c_str(), L"Directory error", MB_ICONERROR);
		}
		return false;
	}
	CloseHandle(hFile);
	DeleteFile(tmpFileName.c_str());
	return true;
}

// tests if writable, and asks for UAC if not; returns true only when writable
bool ConfigUpdater::_ask_dir_permissions(const std::wstring& path)
{
	if (_doAbort) return false;
	if (_is_dir_writable(path)) return true;
	if (_isAskRestartCancelled) {
		_consoleWrite(std::wstring(L"! Directory '") + path + L"' not writable.  Not asking if you want UAC, because you previously chose CANCEL.");
		return false;	// don't need to ask if already cancelled
	}
	std::wstring msg = path + L" is not writable. Would you like to restart Notpead++ with UAC?\n\n"
		+ L"- YES = Exit Notepad++, ask for UAC permission, and restart.\n"
		+ L"- NO = Do not exit Notepad++ for now, but ask again if permissions still needed.\n"
		+ L"- CANCEL = Do not exit Notepad++ for now, and don't ask me again.\n\n"
		+ L"(don't forget to rerun Plugins > ConfigUpdater > Update Config Files after Notepad++ restarts.";
	int res = ::MessageBox(_hwndNPP, msg.c_str(), L"Directory Not Writable", MB_YESNOCANCEL);
	switch (res) {
		case IDNO:
			_consoleWrite(std::wstring(L"! Directory '") + path + L"' not writable.  Do not prompt for UAC.");
			_isAskRestartCancelled = false;
			_isAskRestartYes = false;
			break;
		case IDCANCEL:
			_consoleWrite(std::wstring(L"! Directory '") + path + L"' not writable.  Do not prompt for UAC, and do not ask again.");
			_isAskRestartCancelled = true;
			_isAskRestartYes = false;
			break;
		case IDYES:
			::SendMessage(_hwndNPP, NPPM_MENUCOMMAND, 0, IDM_FILE_CLOSE);
			_consoleWrite(std::wstring(L"! Directory '") + path + L"' not writable.  Will prompt for UAC.");
			_consoleWrite(std::wstring(L"!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n!!!!! Run Plugins > ConfigUpdater > Update Config Files after Notepad++ restarts !!!!!\n!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n"));
			_isAskRestartCancelled = false;
			_isAskRestartYes = true;
			// prompt for UAC
			size_t szLen = ::SendMessage(_hwndNPP, NPPM_GETCURRENTCMDLINE, 0, 0);
			std::wstring wsArgs(szLen + 1, '\0');
			size_t szGot = ::SendMessage(_hwndNPP, NPPM_GETCURRENTCMDLINE, static_cast<WPARAM>(szLen + 1), reinterpret_cast<LPARAM>(wsArgs.data()));
			if (!szGot) wsArgs = L"";
			std::wstring wsRun = std::wstring(L"/C ECHO Wait 5sec while old N++ closes & TIMEOUT /T 5 & START \"Launch N++\" /B \"") + _nppExePath + L"\" " + wsArgs;
			ShellExecute(0, L"runas", L"cmd.exe", wsRun.c_str(), NULL, SW_SHOWDEFAULT);	/* SW_SHOWMINIMIZED */
			::SendMessage(_hwndNPP, NPPM_MENUCOMMAND, 0, IDM_FILE_EXIT);
			break;
	}
	return false;
}

// tests if Admin, and asks to restart normally; if not, asks if you want to restart to have it take effect
//		return value = if restarting, return true; otherwise, return false
bool ConfigUpdater::_ask_rerun_normal(void)
{
	HANDLE hToken = nullptr;
	TOKEN_ELEVATION elevation;
	DWORD dwSize;

	// don't need to ask if it's already in the midst of a restart
	if (_isAskRestartYes) return true;

	// check for elevated
	if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)) return false;		// if i cannot open token, no way to find out if elevated
	bool bGotInfo = GetTokenInformation(hToken, TokenElevation, &elevation, sizeof(elevation), &dwSize);
	CloseHandle(hToken);
	if (!bGotInfo) return false;					// if i cannot get the info, no way to find out if elevated
	std::wstring msg, ttl;

	// message depends on whether it's elevated or not
	if (elevation.TokenIsElevated) {
		msg = L"You are running as Administrator, with elevated UAC permission, so it is a good idea to restart as a normal user.\n\nAlso, your config updates will not take effect until you restart Notepad++.\n\nWould you like to restart Notepad++ as a normal user?";
		if (_hadValidationError) msg += L"\n\n(You had validation errors; you can say NO now and then YES when asked to validate, or run \"Plugins > Config Updater > Validate Config Files\" after restart.)";
		ttl = L"Restart Notpead++ Normally?";
	}
	else {
		msg = L"Your config updates will not take effect until you restart Notepad++.\n\nWould you like to restart Notepad++ at this time?";
		if (_hadValidationError) msg += L"\n\n(You had validation errors; you can say NO now and then YES when asked to validate, or run \"Plugins > Config Updater > Validate Config Files\" after restart.)";
		ttl = L"Restart Notpead++?";
	}

	// ask about restart, whether elevated or not
	int res = ::MessageBox(_hwndNPP, msg.c_str(), ttl.c_str(), MB_YESNO);
	switch (res) {
		case IDYES: {
			// derive new command line string
			size_t szLen = ::SendMessage(_hwndNPP, NPPM_GETCURRENTCMDLINE, 0, 0);
			std::wstring wsArgs(szLen + 1, '\0');
			size_t szGot = ::SendMessage(_hwndNPP, NPPM_GETCURRENTCMDLINE, static_cast<WPARAM>(szLen + 1), reinterpret_cast<LPARAM>(wsArgs.data()));
			if (!szGot) wsArgs = L"";
			std::wstring wsRun = std::wstring(L"/C echo Wait 5sec while old N++ exits. & TIMEOUT /T 5 & START \"Launch N++\" /B \"") + _nppExePath + L"\" " + wsArgs;

			// cannot use ShellExecute, because it doesn't de-elevate :-(
			//// ShellExecute(0, NULL, L"cmd.exe", wsRun.c_str(), NULL, SW_SHOWMINIMIZED);

			// try the method suggested here: <https://devblogs.microsoft.com/oldnewthing/20190425-00/?p=102443>
			HWND hwnd = GetShellWindow();

			DWORD pid;
			GetWindowThreadProcessId(hwnd, &pid);
			HANDLE process = OpenProcess(PROCESS_CREATE_PROCESS, FALSE, pid);

			SIZE_T size = 0;
			InitializeProcThreadAttributeList(nullptr, 1, 0, &size);
			auto p = (PPROC_THREAD_ATTRIBUTE_LIST)new char[size];

			InitializeProcThreadAttributeList(p, 1, 0, &size);
			UpdateProcThreadAttribute(p, 0,
				PROC_THREAD_ATTRIBUTE_PARENT_PROCESS,
				&process, sizeof(process),
				nullptr, nullptr);

			wchar_t cmd[] = L"C:\\Windows\\System32\\cmd.exe";
			STARTUPINFOEX siex = {};
			siex.lpAttributeList = p;
			siex.StartupInfo.cb = sizeof(siex);
			//siex.StartupInfo.dwFlags = STARTF_USESHOWWINDOW;
			//siex.StartupInfo.wShowWindow = SW_MINIMIZE;
			PROCESS_INFORMATION pi;

			std::wstring wsFullCommandLine = std::wstring(L"cmd.exe ") + wsRun;

			CreateProcess(
				cmd, const_cast<LPWSTR>(wsFullCommandLine.c_str()),
				nullptr, nullptr, FALSE,
				EXTENDED_STARTUPINFO_PRESENT,
				nullptr, nullptr,
				&siex.StartupInfo, &pi);

			CloseHandle(pi.hProcess);
			CloseHandle(pi.hThread);
			delete[](char*)p;
			CloseHandle(process);

			::SendMessage(_hwndNPP, NPPM_MENUCOMMAND, 0, IDM_FILE_EXIT);
			return true;
		}
		case IDNO:
		default:
			return false;
	}

}

std::wstring ConfigUpdater::_getWritableTempDir(void)
{
	// first try the system TEMP
	std::wstring tempDir(MAX_PATH + 1, L'\0');
	GetTempPath(MAX_PATH + 1, const_cast<LPWSTR>(tempDir.data()));
	pcjHelper::delNull(tempDir);

	// if that fails, try c:\tmp or c:\temp
	if (!_is_dir_writable(tempDir)) {
		tempDir = L"c:\\temp";
		pcjHelper::delNull(tempDir);
	}
	if (!_is_dir_writable(tempDir)) {
		tempDir = L"c:\\tmp";
		pcjHelper::delNull(tempDir);
	}

	// if that fails, try the %USERPROFILE%
	if (!_is_dir_writable(tempDir)) {
		tempDir.resize(MAX_PATH + 1);
		if (!ExpandEnvironmentStrings(L"%USERPROFILE%", const_cast<LPWSTR>(tempDir.data()), MAX_PATH + 1)) {
			std::wstring errmsg = L"_getWritableTempDir::ExpandEnvirontmentStrings(%USERPROFILE%) failed: " + std::to_wstring(GetLastError()) + L"\n";
			::MessageBox(NULL, errmsg.c_str(), L"Directory Error", MB_ICONERROR);
			return L"";
		}
		pcjHelper::delNull(tempDir);
	}

	// last try: current directory
	if (!_is_dir_writable(tempDir)) {
		tempDir.resize(MAX_PATH + 1);
		GetCurrentDirectory(MAX_PATH + 1, const_cast<LPWSTR>(tempDir.data()));
		pcjHelper::delNull(tempDir);
	}

	// if that fails, no other ideas
	if (!_is_dir_writable(tempDir)) {
		std::wstring errmsg = L"_getWritableTempDir() cannot find any writable directory; please make sure %TEMP% is defined and writable\n";
		::MessageBox(NULL, errmsg.c_str(), L"Directory Error", MB_ICONERROR);
		return L"";
	}
	return tempDir;
}

// Checks if the plugin-"console" exists, creates it if necessary, activates the right view/index, and returns the correct scintilla HWND
//		Consumer is required to CloseHandle() when done writing
HANDLE ConfigUpdater::_consoleCheck()
{
	static std::wstring wsConsoleFilePath = _nppCfgPluginConfigMyDir + L"\\ConfigUpdaterLog";

	// whether the file exists or not, need to open it for Append
	HANDLE hConsoleFile = CreateFile(
		wsConsoleFilePath.c_str(),					// name
		FILE_APPEND_DATA | FILE_GENERIC_READ,		// appending and locking
		FILE_SHARE_READ,							// allows multiple readers
		0,											// no security
		OPEN_ALWAYS,								// append or create
		FILE_ATTRIBUTE_NORMAL,						// normal file
		NULL);										// no attr template
	if (hConsoleFile == INVALID_HANDLE_VALUE) {
		DWORD errorCode = GetLastError();
		LPWSTR messageBuffer = nullptr;
		FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
			nullptr, errorCode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPWSTR)&messageBuffer, 0, nullptr);
		std::wstring errmsg = L"Error when trying to create/append \"" + wsConsoleFilePath + L"\": " + std::to_wstring(errorCode) + L":\n" + messageBuffer + L"\n";
		::MessageBox(NULL, errmsg.c_str(), L"Directory error", MB_ICONERROR);
		LocalFree(messageBuffer);
		return nullptr;
	}

	// check if the file is already open (so bufferID has been stored)
	LRESULT res = _uOutBufferID ? ::SendMessage(_hwndNPP, NPPM_GETPOSFROMBUFFERID, static_cast<WPARAM>(_uOutBufferID), 0) : -1;
	if (res == -1) {
		// file should exist now, because of CreateFile, so need to open it
		LRESULT status = ::SendMessage(_hwndNPP, NPPM_DOOPEN, 0, reinterpret_cast<LPARAM>(wsConsoleFilePath.c_str()));
		if (!status) {
			CloseHandle(hConsoleFile);
			return nullptr;
		}

		// save the bufferID for future use
		_uOutBufferID = static_cast<UINT_PTR>(::SendMessage(_hwndNPP, NPPM_GETCURRENTBUFFERID, 0, 0));

		// make sure it's in ERRORLIST mode, and Monitoring (tail -f) mode
		::SendMessage(_hwndNPP, NPPM_SETBUFFERLANGTYPE, static_cast<WPARAM>(_uOutBufferID), L_ERRORLIST);
		::SendMessage(_hwndNPP, NPPM_MENUCOMMAND, 0, IDM_VIEW_MONITORING);

		// find its position
		res = ::SendMessage(_hwndNPP, NPPM_GETPOSFROMBUFFERID, static_cast<WPARAM>(_uOutBufferID), 0);
	}

	// get the View and Index from the previous
	WPARAM view = (res & 0xC0000000) >> 30;	// upper bits are view
	LPARAM index = res & 0x3FFFFFFF;		// lower bits are index

	// activate
	::SendMessage(_hwndNPP, NPPM_ACTIVATEDOC, view, index);

	// return the correct scintilla HWND
	return hConsoleFile;
}

// Prints messages to the plugin-"console" tab; recommended to use DIFF/git-diff nomenclature, where "^+ "=add, "^- "=del, "^! "=change/error, "^--- "=message
void ConfigUpdater::_consoleWrite(std::wstring wsStr)
{
	std::string msg = pcjHelper::wstring_to_utf8(wsStr) + "\r\n";
	_consoleWrite(msg.c_str());
}
void ConfigUpdater::_consoleWrite(std::string sStr)
{
	std::string msg = sStr + "\r\n";
	_consoleWrite(msg.c_str());
}
void ConfigUpdater::_consoleWrite(LPCWSTR wcStr)
{
	std::wstring msg(wcStr);
	_consoleWrite(msg);
}
void ConfigUpdater::_consoleWrite(LPCSTR cStr)
{
	HANDLE hConsoleFile = _consoleCheck();
	if (!hConsoleFile) return;

	DWORD dwNBytesWritten = 0;
	BOOL bWriteOK = ::WriteFile(hConsoleFile, cStr, static_cast<DWORD>(strlen(cStr)), &dwNBytesWritten, NULL);
	if (!bWriteOK) {
		std::wstring msg = std::wstring(L"Problem writing to ConfigUpdaterLog: ") + std::to_wstring(GetLastError());
		::MessageBox(_hwndNPP, msg.c_str(), L"Logging Error", MB_ICONWARNING);
	}
	CloseHandle(hConsoleFile);
}
void ConfigUpdater::_consoleWrite(tinyxml2::XMLNode* pNode)
{
	tinyxml2::XMLPrinter printer;
	pNode->Accept(&printer);
	_consoleWrite(printer.CStr());
}

ConfigUpdater::ConfigUpdater(HWND hwndNpp)
{
	_hwndNPP = hwndNpp;
	_populateNppDirs();
	_initInternalState();
	_createPluginSettingsIfNeeded();
	_deleteOldFileIfNeeded(_nppCfgPluginConfigDir + L"\\ConfigUpdaterLog");	// don't keep logfile if it's in the old location
};

void ConfigUpdater::_deleteOldFileIfNeeded(std::wstring fname)
{
	if (PathFileExists(fname.c_str()))
		DeleteFile(fname.c_str());
}

void ConfigUpdater::_initInternalState(void)
{
	_mapModelDefaultColors["fgColor"] = "";
	_mapModelDefaultColors["bgColor"] = "";
	_doAbort = false;
	_doStopValidationPester = false;
	_hadValidationError = false;
	_initThemeValidatorXSD();
	_initLangsValidatorXSD();
}

// set the contents of the _bstr_ThemeValidatorXSD
void ConfigUpdater::_initThemeValidatorXSD(void)
{
	// Define the filename variable
	_wsThemeValidatorXsdFileName = _nppCfgPluginConfigMyDir + L"\\theme.xsd";

	// the WriteThemeXSD has recursive-directory-creator, so don't need to Make sure that the config directory exists before calling that
	if (!PathFileExists(_wsThemeValidatorXsdFileName.c_str())) {
		PopulateXSD::WriteThemeXSD(_wsThemeValidatorXsdFileName);
	}
}

// set the contents of the _bstr_LangsValidatorXSD
void ConfigUpdater::_initLangsValidatorXSD(void)
{
	// Define the filename variable
	_wsLangsValidatorXsdFileName = _nppCfgPluginConfigMyDir + L"\\langs.xsd";

	// the WriteLangsXSD has recursive-directory-creator, so don't need to Make sure that the config directory exists before calling that
	if (!PathFileExists(_wsLangsValidatorXsdFileName.c_str())) {
		PopulateXSD::WriteLangsXSD(_wsLangsValidatorXsdFileName);
	}
}


// creates the plugin's config file if it dooesn't exist
void ConfigUpdater::_createPluginSettingsIfNeeded(void)
{
	std::wstring wsPluginConfigOld = _nppCfgPluginConfigDir + L"\\ConfigUpdaterSettings.xml";
	std::wstring wsPluginConfigFile = _nppCfgPluginConfigMyDir + L"\\ConfigUpdaterSettings.xml";
	std::string sPluginConfigFile8 = pcjHelper::wstring_to_utf8(wsPluginConfigFile);

	// Make sure that the config directory exists
	if (!PathFileExists(_nppCfgPluginConfigMyDir.c_str())) {
		BOOL stat = CreateDirectory(_nppCfgPluginConfigMyDir.c_str(), NULL);
		if (!stat) return;	// cannot do the next checks if I cannot create it
	}

	// Make sure that the config file exists
	if (!PathFileExists(wsPluginConfigFile.c_str())) {
		// move the old config file if it's found in the old location
		if (PathFileExists(wsPluginConfigOld.c_str())) {
			MoveFile(wsPluginConfigOld.c_str(), wsPluginConfigFile.c_str());
			// i _could_ check status like above, but if it fails to move, I want to create it in the if() below, so there's no "return" to do, so no reason to save status
		}
		// if the file still doesn't exist (either it didn't before, or the move failed), create it
		if (!PathFileExists(wsPluginConfigFile.c_str())) {
			tinyxml2::XMLDocument wDoc;
			tinyxml2::XMLElement* pRoot = wDoc.NewElement("ConfigUpdaterSettings");
			wDoc.InsertFirstChild(pRoot);

			tinyxml2::XMLElement* pSetting = pRoot->InsertNewChildElement("Setting");
			pSetting->SetAttribute("name", "DEBUG");
			pSetting->SetAttribute("isIntermediateSorted", 0);

			wDoc.SaveFile(sPluginConfigFile8.c_str());

			// don't continue if the new location still doesn't exist
			if (!PathFileExists(wsPluginConfigFile.c_str()))
				return;
		}
	}

	// delete the old config file if it still exists
	_deleteOldFileIfNeeded(wsPluginConfigOld);
}

// reads the plugin's config file
void ConfigUpdater::_readPluginSettings(void)
{
	std::wstring wsPluginConfigFile = _nppCfgPluginConfigMyDir + L"\\ConfigUpdaterSettings.xml";
	std::string sPluginConfigFile8 = pcjHelper::wstring_to_utf8(wsPluginConfigFile);

	_createPluginSettingsIfNeeded();

	// read the file
	tinyxml2::XMLDocument oSettingsXML;
	tinyxml2::XMLError eResult = oSettingsXML.LoadFile(sPluginConfigFile8.c_str());
	if (_xml_check_result(eResult, &oSettingsXML, wsPluginConfigFile)) return;

	// search for the name=DEBUG element
	tinyxml2::XMLElement* pRoot = oSettingsXML.FirstChildElement("ConfigUpdaterSettings");
	tinyxml2::XMLElement* pSearch = pRoot->FirstChildElement("Setting");
	while (pSearch) {
		if (pSearch->Attribute("name", "DEBUG")) {	// name="Default Style" styleID="32"
			break;
		}
		pSearch = pSearch->NextSiblingElement("Setting");
	}
	if (!pSearch) {
		_xml_check_result(tinyxml2::XML_ERROR_PARSING_ELEMENT, &oSettingsXML);
		return;
	}
	_setting_isIntermediateSorted = pSearch->IntAttribute("isIntermediateSorted");
}

void ConfigUpdater::_populateNppDirs(void)
{
	// %AppData%\Notepad++\Plugins\Config or equiv
	LRESULT sz = 1 + ::SendMessage(_hwndNPP, NPPM_GETPLUGINSCONFIGDIR, 0, NULL);
	std::wstring pluginCfgDir(sz, '\0');
	::SendMessage(_hwndNPP, NPPM_GETPLUGINSCONFIGDIR, sz, reinterpret_cast<LPARAM>(pluginCfgDir.data()));
	pcjHelper::delNull(pluginCfgDir);
	_nppCfgPluginConfigDir = pluginCfgDir;
	_nppCfgPluginConfigMyDir = pluginCfgDir + L"\\ConfigUpdater";

	// %AppData%\Notepad++\Plugins or equiv
	//		since it's removing the tail, it will never be longer than pluginCfgDir; since it's in-place, initialize with the first
	std::wstring pluginDir = pluginCfgDir;
	PathCchRemoveFileSpec(const_cast<PWSTR>(pluginDir.data()), pluginCfgDir.size());
	pcjHelper::delNull(pluginDir);

	// %AppData%\Notepad++ or equiv is what I'm really looking for
	// _nppCfgDir				#py# _nppConfigDirectory = os.path.dirname(os.path.dirname(notepad.getPluginConfigDir()))
	_nppCfgDir = pluginDir;
	PathCchRemoveFileSpec(const_cast<PWSTR>(_nppCfgDir.data()), pluginDir.size());
	pcjHelper::delNull(_nppCfgDir);

	// _nppCfgUdlDir			#py# _nppCfgUdlDirectory = os.path.join(_nppConfigDirectory, 'userDefineLangs')
	_nppCfgUdlDir = _nppCfgDir + L"\\userDefineLangs";

	// _nppCfgFunctionListDir	#py# _nppCfgFunctionListDirectory = os.path.join(_nppConfigDirectory, 'functionList')
	_nppCfgFunctionListDir = _nppCfgDir + L"\\functionList";

	// _nppCfgThemesDir			#py# _nppCfgThemesDirectory = os.path.join(_nppConfigDirectory, 'themes')
	_nppCfgThemesDir = _nppCfgDir + L"\\themes";

	// AutoCompletion is _always_ relative to notepad++.exe, never in AppData or CloudConfig or SettingsDir
	// _nppCfgAutoCompletionDir	#py# _nppAppAutoCompletionDirectory = os.path.join(notepad.getNppDir(), 'autoCompletion')
	std::wstring exeDir(MAX_PATH, '\0');
	::SendMessage(_hwndNPP, NPPM_GETNPPDIRECTORY, MAX_PATH, reinterpret_cast<LPARAM>(exeDir.data()));
	pcjHelper::delNull(exeDir);
	_nppCfgAutoCompletionDir = exeDir + L"\\autoCompletion";

	// also, want to save the app directory and the themes relative to the app (because themes can be found in _both_ locations)
	_nppAppDir = exeDir;
	_nppAppThemesDir = _nppAppDir + L"\\themes";

	// executable path:
	std::wstring exePath(MAX_PATH, '\0');
	LRESULT szExe = ::SendMessage(_hwndNPP, NPPM_GETNPPFULLFILEPATH, 0, reinterpret_cast<LPARAM>(exePath.data()));
	if (szExe) {
		pcjHelper::delNull(exePath);
		_nppExePath = exePath;
	}

	return;
}

// timestamp the console
void ConfigUpdater::_consoleTimestamp(void)
{
	SYSTEMTIME st;
	GetLocalTime(&st);

	wchar_t date_str[256], time_str[256];

	GetDateFormat(LOCALE_USER_DEFAULT, DATE_LONGDATE, &st, nullptr, date_str, 256);
	GetTimeFormat(LOCALE_USER_DEFAULT, 0, &st, nullptr, time_str, 256);

	_consoleWrite(std::wstring(L"--- --- --- --- --- --- --- --- --- --- --- --- --- --- --- --- --- --- --- --- ---"));
	_consoleWrite(std::wstring(L"--- ConfigUpdater run at ") + std::wstring(date_str) + L" " + std::wstring(time_str));
}

// truncate the console (don't want ConfigUpdaterLog getting thousands of lines long over time)
void ConfigUpdater::_consoleTruncate(void)
{
	HANDLE hConsoleFile = _consoleCheck();
	if (!hConsoleFile) return;

	// Get the current scintilla
	int which = -1;
	::SendMessage(nppData._nppHandle, NPPM_GETCURRENTSCINTILLA, 0, (LPARAM)&which);
	HWND curScintilla = (which < 1) ? nppData._scintillaMainHandle : nppData._scintillaSecondHandle;

	// get the number of lines in the console file
	LRESULT lineCount = ::SendMessage(curScintilla, SCI_GETLINECOUNT, 0, 0);
	LRESULT textSize = ::SendMessage(curScintilla, SCI_GETTEXTLENGTH, 0, 0);

	// truncate if too long: >400 lines means at least 4 previous runs are still stored
	if (lineCount > 400) {
		// search backward to the most recent timestamp: --- --- ---
		Sci_CharacterRangeFull search_range{ textSize, 0 };	// min>max means search backwards
		Sci_CharacterRangeFull found_range{ 0, 0 };
		Sci_TextToFindFull search_text_obj{ search_range, "^--- ---", found_range };
		LRESULT found_position = ::SendMessage(curScintilla, SCI_FINDTEXTFULL, SCFIND_REGEXP, reinterpret_cast<LPARAM>(&search_text_obj));
		if (found_position != static_cast<LRESULT>(-1)) {	// if it's actually found
			// need to close my exclusive handle on the file _before_ trying to write to it
			CloseHandle(hConsoleFile);
			hConsoleFile = nullptr;

			// turn off monitoring, so I can edit
			::SendMessage(_hwndNPP, NPPM_MENUCOMMAND, 0, IDM_VIEW_MONITORING);

			// It wasn't always done, so give it some time; unfortunately, there is nothing I can poll to make sure it's actually done, so I just have to guess. ;-(
			Sleep(100);

			// delete from position 0 to found_position (implemented as from 0 with length=found_position)
			::SendMessage(curScintilla, SCI_DELETERANGE, 0, found_position);

			// save edits
			::SendMessage(_hwndNPP, NPPM_SAVECURRENTFILE, 0, 0);

			// When I try to just enable monitoring again, future writes to hConsoleFile don't work ("open in another process"),
			//		so need to reset by closing the file in N++; the next time I _consoleWrite() or similar, it will re-open the console file
			::SendMessage(_hwndNPP, NPPM_MENUCOMMAND, 0, IDM_FILE_CLOSE);

			// need to wait until file is actually closed (I really wish there were an NPPM command that waited for File>Close to complete, instead of having to MENUCOMMAND)
			//	so keep polling the current file name, and keep looping as long as it's still ConfigUpdaterLog; once it changes, the file is done closing, and it's safe to move on
			wchar_t bufFileName[MAX_PATH] = L"ConfigUpdaterLog";
			std::wstring wsFileName = bufFileName;
			while (pcjHelper::delNull(wsFileName) == L"ConfigUpdaterLog") {
				memset(bufFileName, 0, MAX_PATH);
				::SendMessage(_hwndNPP, NPPM_GETFILENAME, MAX_PATH, reinterpret_cast<LPARAM>(bufFileName));
				wsFileName = bufFileName;
			}

		}
	}
	if (hConsoleFile)
		CloseHandle(hConsoleFile);	// make sure the handle is closed in every _consoleXXX function
}


bool ConfigUpdater::go(bool isIntermediateSorted)
{
	_consoleTruncate();
	_consoleTimestamp();
	_initInternalState();
	_readPluginSettings();
	isIntermediateSorted |= _setting_isIntermediateSorted;
	_updateAllThemes(isIntermediateSorted);
	if (_doAbort) { _consoleWrite(L"!!! ConfigUpdater interrupted. !!!"); custatus_CloseWindow(); return false; }
	if (!custatus_GetInterruptFlag())
		_updateLangs(isIntermediateSorted);
	if (_doAbort) { _consoleWrite(L"!!! ConfigUpdater interrupted. !!!"); custatus_CloseWindow(); return false; }
	_consoleWrite(L"--- ConfigUpdater done. ---");
	custatus_SetProgress(100);
	custatus_AppendText(L"--- ConfigUpdater done. ---");
	if (_hadValidationError)
		_consoleWrite(L"!!! There was at least one validation error.  Recommend you run Plugins > ConfigUpdater > Validate Config Files !!!");
	custatus_CloseWindow();
	return _ask_rerun_normal();
}

tinyxml2::XMLDocument* ConfigUpdater::_getModelStyler(void)
{
	tinyxml2::XMLDocument* pDoc = new tinyxml2::XMLDocument;
	std::wstring fName = std::wstring(MAX_PATH, L'\0');
	PathCchCombine(const_cast<PWSTR>(fName.data()), MAX_PATH, _nppAppDir.c_str(), L"stylers.model.xml");
	pcjHelper::delNull(fName);
	tinyxml2::XMLError eResult = pDoc->LoadFile(pcjHelper::wstring_to_utf8(fName).c_str());
	if (_xml_check_result(eResult, pDoc, fName)) return pDoc;
	if (pDoc->RootElement() == NULL) {
		_xml_check_result(tinyxml2::XML_ERROR_FILE_READ_ERROR, pDoc);
	}

	auto elDefaultStyle = _get_default_style_element(pDoc);
	_mapModelDefaultColors["fgColor"] = elDefaultStyle->Attribute("fgColor");
	_mapModelDefaultColors["bgColor"] = elDefaultStyle->Attribute("bgColor");

	return pDoc;
}

// grab the default style element out of the given theme XML
tinyxml2::XMLElement* ConfigUpdater::_get_default_style_element(tinyxml2::XMLDocument& oDoc)
{
	return _get_default_style_element(&oDoc);
}
tinyxml2::XMLElement* ConfigUpdater::_get_default_style_element(tinyxml2::XMLDocument* pDoc)
{
	// tinyxml2 doesn't have XPath, and my attempts at adding on tixml2ex.h's find_element() couldn't seem to ever find any element, even the root
	//	so switch to manual navigation.
	tinyxml2::XMLElement* pSearch = pDoc->FirstChildElement("NotepadPlus");
	if (pSearch) pSearch = pSearch->FirstChildElement("GlobalStyles");
	if (pSearch) pSearch = pSearch->FirstChildElement("WidgetStyle");
	while (pSearch) {
		if (pSearch->Attribute("styleID", "32")) {	// name="Default Style" styleID="32"
			break;
		}
		pSearch = pSearch->NextSiblingElement();
	}
	if (!pSearch) {
		_xml_check_result(tinyxml2::XML_ERROR_PARSING_ELEMENT, pDoc);
	}
	return pSearch;
}

// loops over the stylers.xml, <cfg>\Themes, and <app>\Themes
void ConfigUpdater::_updateAllThemes(bool isIntermediateSorted)
{
	tinyxml2::XMLDocument* pModelStylerDoc = _getModelStyler();
	_updateOneTheme(pModelStylerDoc, _nppCfgDir, L"stylers.xml", isIntermediateSorted);
	if (_doAbort) return;	// need to abort


	DWORD szNeeded = GetCurrentDirectory(0, NULL);
	std::wstring curDir(szNeeded, L'\0');
	if (!GetCurrentDirectory(szNeeded, const_cast<LPWSTR>(curDir.data()))) return;

	HANDLE fhFound;
	WIN32_FIND_DATAW infoFound;
	std::vector<std::wstring> themeDirs{ _nppCfgThemesDir, _nppAppThemesDir };
	if (_nppCfgThemesDir == _nppAppThemesDir)
		themeDirs.pop_back();
	for (auto wsDir : themeDirs) {
		if (_doAbort) return;	// need to abort
		if (custatus_GetInterruptFlag()) break; // exit early on CANCEL
		if (PathFileExists(wsDir.c_str())) {
			if (!SetCurrentDirectory(wsDir.c_str())) return;
			fhFound = FindFirstFile(L"*.xml", &infoFound);
			if (fhFound != INVALID_HANDLE_VALUE) {
				bool stillFound = true;
				while (stillFound) {
					// process this file
					stillFound &= _updateOneTheme(pModelStylerDoc, wsDir, infoFound.cFileName, isIntermediateSorted);
					if (_doAbort) return;	// need to abort

					// look for next file
					stillFound &= static_cast<bool>(FindNextFile(fhFound, &infoFound));
					// static cast needed because bool and BOOL are not actually the same
				}
			}
			if (!SetCurrentDirectory(curDir.c_str())) return;
		}
	}

	return;
}

// Updates one particular theme or styler file
bool ConfigUpdater::_updateOneTheme(tinyxml2::XMLDocument* pModelStylerDoc, std::wstring themeDir, std::wstring themeName, bool isIntermediateSorted)
{
	// get full path to file
	pcjHelper::delNull(themeDir);
	pcjHelper::delNull(themeName);
	std::wstring themePath = themeDir + L"\\" + themeName;
	std::string themePath8 = pcjHelper::wstring_to_utf8(themePath);
	_consoleWrite(std::wstring(L"--- Checking Styler/Theme File: '") + themePath + L"'");

	// update the status dialog
	std::wstring wsLine = themeName + L"\r\n";
	custatus_AppendText(const_cast<LPWSTR>(wsLine.c_str()));

	// check for permissions, exit function if cannot write
	if (!_ask_dir_permissions(themeDir)) return false;

	// get the XML
	tinyxml2::XMLDocument oStylerDoc;
	tinyxml2::XMLError eResult = oStylerDoc.LoadFile(themePath8.c_str());
	if (_xml_check_result(eResult, &oStylerDoc, themePath)) return false;
	tinyxml2::XMLElement* pStylerRoot = oStylerDoc.RootElement();
	if (pStylerRoot == NULL)
		if (_xml_check_result(tinyxml2::XML_ERROR_FILE_READ_ERROR, &oStylerDoc, themePath))
			return false;

	// save a copy of the current XML output for future comparison
	tinyxml2::XMLPrinter xPrinter;
	oStylerDoc.Accept(&xPrinter);
	std::string sOrigThemeText = xPrinter.CStr();

	// grab the theme's and model's LexerStyles node for future insertions
	tinyxml2::XMLElement* pElThemeLexerStyles = oStylerDoc.FirstChildElement("NotepadPlus")->FirstChildElement("LexerStyles");
	tinyxml2::XMLElement* pElModelLexerStyles = pModelStylerDoc->FirstChildElement("NotepadPlus")->FirstChildElement("LexerStyles");

	if (isIntermediateSorted)
	{
		// Sort the original theme file and save to disk (for easy sort-based comparison of before/after)
		tinyxml2::XMLDocument oClonedOrig;
		oStylerDoc.DeepCopy(&oClonedOrig);
		tinyxml2::XMLElement* pElCloneLexerStyles = oClonedOrig.FirstChildElement("NotepadPlus")->FirstChildElement("LexerStyles");
		_sortLexerTypesByName(pElCloneLexerStyles, isIntermediateSorted);

		std::string sTmpFile = themePath8 + ".orig.sorted";
		oClonedOrig.SaveFile(sTmpFile.c_str());
	}

	// Grab the default attributes from the <GlobalStyles><WidgetStyle name = "Global override" styleID = "0"...>
	tinyxml2::XMLElement* pDefaultStyle = _get_default_style_element(oStylerDoc);
	_mapStylerDefaultColors["fgColor"] = pDefaultStyle->Attribute("fgColor");
	_mapStylerDefaultColors["bgColor"] = pDefaultStyle->Attribute("bgColor");

	// want to keep the colors from the .model. when editing stylers.xml, but none of the others
	bool keepModelColors = (themeName == std::wstring(L"stylers.xml"));

	// iterate through all the Model's lexer types, and see if there are any LexerTypes that cannot also be found in Theme
	tinyxml2::XMLElement* pElModelLexerType = pElModelLexerStyles->FirstChildElement("LexerType");
	tinyxml2::XMLElement* pElThemeLexerType = pElThemeLexerStyles->FirstChildElement("LexerType");
	while (pElModelLexerType) {
		// get the name of this ModelLexerType
		std::string sModelLexerTypeName = pElModelLexerType->Attribute("name");

		// Look through through this theme to find out if it has the a LexerType with the same name
		tinyxml2::XMLElement* pSearchResult = _find_element_with_attribute_value(pElThemeLexerStyles, pElThemeLexerType, "LexerType", "name", sModelLexerTypeName);
		if (!pSearchResult) {
			// PY::#212#	if LexerType not found in theme, need to copy the whole lexer type from the Model to the Theme
			_addMissingLexerType(pElModelLexerType, pElThemeLexerStyles, keepModelColors);
		}
		else {
			// PY::#215#	if LexerType exists in theme, need to check its contents
			_addMissingLexerStyles(pElModelLexerType, pSearchResult, keepModelColors);
		}

		// then move to next
		pElModelLexerType = pElModelLexerType->NextSiblingElement("LexerType");
	}

	// sort the LexerType nodes by name (keeping searchResult _last_)
	_sortLexerTypesByName(pElThemeLexerStyles);

	// Look for missing GlobalStyles::WidgetStyle entries as well
	tinyxml2::XMLElement* pElThemeGlobalStyles = oStylerDoc.FirstChildElement("NotepadPlus")->FirstChildElement("GlobalStyles")->ToElement();
	tinyxml2::XMLElement* pElModelGlobalStyles = pModelStylerDoc->FirstChildElement("NotepadPlus")->FirstChildElement("GlobalStyles")->ToElement();
	_addMissingGlobalWidgets(pElModelGlobalStyles, pElThemeGlobalStyles, keepModelColors);

	// Write XML output only if needed: use the saved copy of the XML text compared to the updated text
	xPrinter.ClearBuffer();
	oStylerDoc.Accept(&xPrinter);
	std::string sNewThemeText = xPrinter.CStr();

	if (sNewThemeText != sOrigThemeText)
		oStylerDoc.SaveFile(themePath8.c_str());

	// whether we wrote it this time or not, check it for validity
	ValidateXML themeValidator(themePath.c_str(), _wsThemeValidatorXsdFileName);
	if (themeValidator.isValid()) {
		std::wstring msg = std::wstring(L"+ Validation: Confirmed VALID Stylers/Theme XML for ") + themePath;
		_consoleWrite(msg);
	}
	else {
		UINT64 lnum = themeValidator.uGetValidationLineNum();
		std::wstring msg = std::wstring(L"Validation of ") + themePath + L" failed" + (lnum==-1 ? L"" : (L" on line#" + std::to_wstring(lnum))) + L":\n\n" + themeValidator.wsGetValidationMessage();
		_consoleWrite(std::wstring(L"! ") + msg);
		_hadValidationError = true;
#if 0
		if (!_doStopValidationPester) {
			msg += L"\n\nWould you like to edit that file?";	// don't want the question in the .log, so moved it after the _consoleWrite
			int ask = ::MessageBox(nullptr, msg.c_str(), L"Theme Validation Failed", MB_ICONWARNING | MB_YESNOCANCEL);
			switch (ask) {
				case IDCANCEL:
				{
					_doAbort = true;
					break;
				}
				case IDNO:
				{
					_doStopValidationPester = true;
					break;
				}
				case IDYES:
				{
					// open the file
					::SendMessage(_hwndNPP, NPPM_DOOPEN, 0, reinterpret_cast<LPARAM>(themePath.c_str()));

					// Get the current scintilla
					int which = -1;
					::SendMessage(nppData._nppHandle, NPPM_GETCURRENTSCINTILLA, 0, (LPARAM)&which);
					HWND curScintilla = (which < 1) ? nppData._scintillaMainHandle : nppData._scintillaSecondHandle;

					// go to the right line
					if (lnum != -1)
						::SendMessage(curScintilla, SCI_GOTOLINE, static_cast<WPARAM>(lnum - 1), 0);

					// stop looping
					_doAbort = true;
					break;
				}
			}
		}
#endif
	}

	// Update progress bar
	if (_doAbort)
		custatus_SetProgress(100);
	else
		custatus_AddProgress(1);

	return true;
}

// private: case-insensitive std::string equality check
bool _string_insensitive_eq(std::string a, std::string b)
{
	std::string a_copy = "";
	std::string b_copy = "";

	// ignore conversion of int to char implicit in the <algorithm>std::transform, which I have no control over
#pragma warning(push)
#pragma warning(disable: 4244)
	for (size_t i = 0; i < a.size(); i++) { a_copy += std::tolower(a[i]); }
	for (size_t i = 0; i < b.size(); i++) { b_copy += std::tolower(b[i]); }
#pragma warning(pop)
	return a_copy == b_copy;
}

// private: case-insensitive std::string less-than check
bool _string_insensitive_lt(std::string a, std::string b)
{
	std::string a_copy = "";
	std::string b_copy = "";

	// ignore conversion of int to char implicit in the <algorithm>std::transform, which I have no control over
#pragma warning(push)
#pragma warning(disable: 4244)
	for (size_t i = 0; i < a.size(); i++) { a_copy += std::tolower(a[i]); }
	for (size_t i = 0; i < b.size(); i++) { b_copy += std::tolower(b[i]); }
#pragma warning(pop)
	return a_copy < b_copy;
}

// look for an element, based on {Parent, FirstChild, or both} which is of a specific ElementType, having a specific AttributeName with specific AttributeValue
tinyxml2::XMLElement* ConfigUpdater::_find_element_with_attribute_value(tinyxml2::XMLElement* pParent, tinyxml2::XMLElement* pFirst, std::string sElementType, std::string sAttributeName, std::string sAttributeValue, bool caseSensitive)
{
	if (!pParent && !pFirst) return nullptr;
	tinyxml2::XMLElement* pMyParent = pParent ? pParent->ToElement() : pFirst->Parent()->ToElement();
	tinyxml2::XMLElement* pFCE = pMyParent->FirstChildElement(sElementType.c_str());
	if (!pFirst && !pFCE) return nullptr;
	tinyxml2::XMLElement* pFoundElement = pFirst ? pFirst->ToElement() : pFCE->ToElement();
	while (pFoundElement) {
		// if this node has the right attribute pair, great!
		if (caseSensitive) {
			if (pFoundElement->Attribute(sAttributeName.c_str(), sAttributeValue.c_str()))
				return pFoundElement;
		}
		else {
			const char* cAttrValue = pFoundElement->Attribute(sAttributeName.c_str());
			if (cAttrValue) {
				if (_string_insensitive_eq(sAttributeValue, cAttrValue))
					return pFoundElement;
			}
		}

		// otherwise, move on to next
		pFoundElement = pFoundElement->NextSiblingElement(sElementType.c_str());
	}
	return pFoundElement;
}

void ConfigUpdater::_addMissingLexerType(tinyxml2::XMLElement* pElModelLexerType, tinyxml2::XMLElement* pElThemeLexerStyles, bool keepModelColors)
{
	tinyxml2::XMLDocument* pStylerDoc = pElThemeLexerStyles->GetDocument();
	tinyxml2::XMLElement* pClone = pElModelLexerType->DeepClone(pStylerDoc)->ToElement();

	// loop through all the clone's children:
	tinyxml2::XMLElement* pThemeWordsStyle = pClone->FirstChildElement("WordsStyle");
	while (pThemeWordsStyle) {
		//		- font info gets cleared (never inherit fonts from .model.)
		pThemeWordsStyle->SetAttribute("fontName", "");
		pThemeWordsStyle->SetAttribute("fontStyle", "");
		pThemeWordsStyle->SetAttribute("fontSize", "");
		//		- colors changed to theme defaults if !keepModelColors
		if (!keepModelColors) {
			pThemeWordsStyle->SetAttribute("fgColor", _mapStylerDefaultColors["fgColor"].c_str());
			pThemeWordsStyle->SetAttribute("bgColor", _mapStylerDefaultColors["bgColor"].c_str());
		}
		// then move to next
		pThemeWordsStyle = pThemeWordsStyle->NextSiblingElement("WordsStyle");
	}

	pElThemeLexerStyles->InsertEndChild(pClone);
	std::string msg = std::string("+ LexerStyles: Add missing Lexer='") + pClone->Attribute("name") + "'";
	_consoleWrite(msg);
}

void ConfigUpdater::_addMissingLexerStyles(tinyxml2::XMLElement* pElModelLexerType, tinyxml2::XMLElement* pElThemeLexerType, bool keepModelColors)
{
	tinyxml2::XMLDocument* pStylerDoc = pElThemeLexerType->GetDocument();
	std::string sLexerName = pElModelLexerType->Attribute("name");

	// loop through all the model's children, and see if the Theme as 
	tinyxml2::XMLElement* pModelWordsStyle = pElModelLexerType->FirstChildElement("WordsStyle");
	while (pModelWordsStyle) {
		std::string sStyleID = pModelWordsStyle->Attribute("styleID");
		std::string sStyleName = pModelWordsStyle->Attribute("name");
		tinyxml2::XMLElement* pSearchResult = _find_element_with_attribute_value(pElThemeLexerType, nullptr, "WordsStyle", "styleID", sStyleID);
		if (!pSearchResult) {
			// this WordsStyle not found in theme, so need to add it
			tinyxml2::XMLElement* pClone = pModelWordsStyle->DeepClone(pStylerDoc)->ToElement();
			//		- font info gets cleared (never inherit fonts from .model.)
			pClone->SetAttribute("fontName", "");
			pClone->SetAttribute("fontStyle", "");
			pClone->SetAttribute("fontSize", "");
			//		- colors changed to theme defaults if !keepModelColors
			if (!keepModelColors) {
				pClone->SetAttribute("fgColor", _mapStylerDefaultColors["fgColor"].c_str());
				pClone->SetAttribute("bgColor", _mapStylerDefaultColors["bgColor"].c_str());
			}
			// add the clone as a child of the theme's LexerType element
			pSearchResult = pElThemeLexerType->InsertEndChild(pClone)->ToElement();
			std::string msg = std::string("+ Lexer '") + sLexerName + "': added missing style #" + sStyleID + "='" + sStyleName + "'";
			_consoleWrite(msg);
			//_consoleWrite(pElThemeLexerType);
		}
		else {
			// for names that have changed in the model, update the theme to match the model's name (keeps up-to-date with the most recent model)
			std::string sOldName = pSearchResult->Attribute("name");
			if (sStyleName != sOldName) {
				pSearchResult->SetAttribute("name", sStyleName.c_str());
				std::string msg = std::string("! Lexer '") + sLexerName + "': renamed styleID#" + sStyleID + " from '" + sOldName + "' to '" + sStyleName + "'";
				_consoleWrite(msg);
			}

			// [python:2024-Aug-28 BUGFIX] = for existing styles, check .model. to see if they need a keywordClass that they don't have
			std::string sModelKeywordClass = pModelWordsStyle->Attribute("keywordClass") ? pModelWordsStyle->Attribute("keywordClass") : "";
			if (sModelKeywordClass.length()) {
				// if the model has a keyword class, check the theme as well:
				std::string sThemeKeywordClass = pSearchResult->Attribute("keywordClass") ? pSearchResult->Attribute("keywordClass") : "";
				if (sThemeKeywordClass == "") {
					// theme had no keyword class
					pSearchResult->SetAttribute("keywordClass", sModelKeywordClass.c_str());
					std::string msg = std::string("+ Lexer '") + sLexerName + "': added missing keywordClass='" + sModelKeywordClass + "' to styleID#" + sStyleID + "='" + sStyleName + "'";
					_consoleWrite(msg);
				}
				else if (sModelKeywordClass != sThemeKeywordClass) {
					// theme had wrong keyword class
					pSearchResult->SetAttribute("keywordClass", sModelKeywordClass.c_str());
					std::string msg = std::string("! Lexer '") + sLexerName + "': fixed incorrect keywordClass='" + sThemeKeywordClass + "' to '" + sModelKeywordClass + "' in styleID#" + sStyleID + "='" + sStyleName + "'";
					_consoleWrite(msg);
				}
			}
			2;
		}
		pModelWordsStyle = pModelWordsStyle->NextSiblingElement("WordsStyle");
	}

	return;
}

// clones any missing WidgetStyle entries from model to the theme (and fixes attributes)
void ConfigUpdater::_addMissingGlobalWidgets(tinyxml2::XMLElement* pElModelGlobalStyles, tinyxml2::XMLElement* pElThemeGlobalStyles, bool keepModelColors)
{
	tinyxml2::XMLDocument* pStylerDoc = pElThemeGlobalStyles->GetDocument();
	tinyxml2::XMLElement* pNewContainer = pStylerDoc->NewElement("GlobalStyles");
	pElThemeGlobalStyles->Parent()->InsertAfterChild(pElThemeGlobalStyles, pNewContainer);
	//tinyxml2::XMLComment* pRecentComment;

	// iterate through the nodes (comments and elements) of the model, and check if the theme has the equivalent or not
	tinyxml2::XMLNode* pModelNode = pElModelGlobalStyles->FirstChild();
	while (pModelNode) {
		if (pModelNode->ToComment()) {
			static std::string sCmpZero = " Attention : Don't modify the name of styleID=\"0\" ";
			if (pModelNode->Value() == sCmpZero) {
				tinyxml2::XMLComment* pThemeZeroComment = pElThemeGlobalStyles->FirstChild()->ToComment();
				if (pThemeZeroComment && (std::string(pThemeZeroComment->Value()) == sCmpZero)) {
					; // doing nothing in true is more readable than negating the logic
				}
				else {
					pThemeZeroComment = pModelNode->DeepClone(pStylerDoc)->ToComment();
				}
				pNewContainer->InsertFirstChild(pThemeZeroComment);
			}
			else {
				std::string sModelComment = pModelNode->Value();
				tinyxml2::XMLComment* pThemeCommentMatch = nullptr;
				tinyxml2::XMLNode* pThemeNode = pElThemeGlobalStyles->FirstChild();
				while (pThemeNode) {
					if (pThemeNode->ToComment()) {
						if (sModelComment == std::string(pThemeNode->Value())) {
							pThemeCommentMatch = pThemeNode->ToComment();
							break;
						}
					}
					pThemeNode = pThemeNode->NextSibling();
				}
				if (!pThemeCommentMatch) {
					pThemeCommentMatch = pModelNode->DeepClone(pStylerDoc)->ToComment();
				}
				pNewContainer->InsertEndChild(pThemeCommentMatch);
			}
		}
		else if (pModelNode->ToElement()) {
			static std::vector<const char*> attribs{ "fgColor", "bgColor", "fontName", "fontStyle", "fontSize" };
			std::string sModelName = pModelNode->ToElement()->Attribute("name");
			// do a case-insensitive search for this WidgetStyle's name in the theme:
			tinyxml2::XMLElement* pFoundWidget = _find_element_with_attribute_value(pElThemeGlobalStyles, nullptr, "WidgetStyle", "name", sModelName, false);
			if (!pFoundWidget) {
				// need to clone it from model
				tinyxml2::XMLElement* pClone = pModelNode->DeepClone(pStylerDoc)->ToElement();
				//		- colors changed to theme defaults if !keepModelColors
				if (!keepModelColors) {
					pClone->SetAttribute("fgColor", _mapStylerDefaultColors["fgColor"].c_str());
					pClone->SetAttribute("bgColor", _mapStylerDefaultColors["bgColor"].c_str());
				}

				pFoundWidget = pClone;

				std::string msg = std::string("+ GlobalStyles: Added missing WidgetStyle:'") + sModelName + "'";
				_consoleWrite(msg);
			}

			// make sure only correct attributes exist: add any that were in model but not here; delete any that aren't in the model (because they can confuse N++)
			bool updated = false;
			for (auto a : attribs) {
				// populate any missing attributes
				if (pModelNode->ToElement()->Attribute(a) && !pFoundWidget->Attribute(a)) {
					pFoundWidget->SetAttribute(a, pModelNode->ToElement()->Attribute(a));
					updated = true;
				}
				if (!pModelNode->ToElement()->Attribute(a) && pFoundWidget->Attribute(a)) {
					pFoundWidget->DeleteAttribute(a);
					updated = true;
				}
			}
			if (updated) {
				std::string msg = std::string("+ GlobalStyles: Updated attributes for WidgetStyle:'") + sModelName + "'";
				_consoleWrite(msg);
			}

			// move found element to the new container
			pNewContainer->InsertEndChild(pFoundWidget);
		}
		// then move to next node
		pModelNode = pModelNode->NextSibling();
	}

	// finally, since the newContainer was already added and populated, it is safe to remove the old
	pElThemeGlobalStyles->Parent()->DeleteChild(pElThemeGlobalStyles);
}

// Sorts all of the LexerType elements in the LexerStyles container
void ConfigUpdater::_sortLexerTypesByName(tinyxml2::XMLElement* pElThemeLexerStyles, bool isIntermediateSorted)
{
	// use <algorithm>'s std::sort to sort a std::vector of LexerType elements
	//	1: populate vector
	//	2: sort
	//	3: update LexerStyles container (standard loop through vector, append each to end of container, which will move them as appropriate)

	// populate vector: standard loop over child elements, append to vector
	std::vector<tinyxml2::XMLElement*> vpElements;
	tinyxml2::XMLElement* pElLexerType = pElThemeLexerStyles->FirstChildElement("LexerType");
	if (!pElLexerType) return;	// don't need to sort when there are no LexerType children
	while (pElLexerType) {
		vpElements.push_back(pElLexerType);
		pElLexerType = pElLexerType->NextSiblingElement("LexerType");
	}

	// sort
	auto cmpLexerNames = [](tinyxml2::XMLElement* pA, tinyxml2::XMLElement* pB) {
		// Note: true means A goes before B (A<B)

		// strings to compare
		std::string sA = pA ? pA->Attribute("name") : "";
		std::string sB = pB ? pB->Attribute("name") : "";

		// searchResult needs to go last
		if (sB == "searchResult") return true;
		if (sA == "searchResult") return false;

		// otherwise, compare case-insensitive by name
		return _string_insensitive_lt(sA, sB);
		};
	std::sort(vpElements.begin(), vpElements.end(), cmpLexerNames);

	// update LexerStyles container: loop through the vector, use InsertEndChild to move that element to end of container
	for (auto pElToMove : vpElements) {
		pElThemeLexerStyles->InsertEndChild(pElToMove);
	}

	std::string msg = std::string("+ LexerStyles: Sorted LexerType elements alphabetically (keeping \"searchResults\" at end)");
	if (isIntermediateSorted) msg += " (for intermediate-sort output file)";
	_consoleWrite(msg);
}

// Sorts all of the Language elements in the Languages container
void ConfigUpdater::_sortLanguagesByName(tinyxml2::XMLElement* pElLanguages, bool isIntermediateSorted)
{
	// use <algorithm>'s std::sort to sort a std::vector of Language elements
	//	1: populate vector
	//		// modification: need to track comments, mapping the "next" node as key to this comment as value
	//	2: sort
	//	3: update LexerStyles container (standard loop through vector, append each to end of container, which will move them as appropriate)
	//		// modification: also unpack possibly-stacked comments

	// populate vector: standard loop over child elements, append to vector
	std::vector<tinyxml2::XMLElement*> vpElements;
	std::map<tinyxml2::XMLNode*, tinyxml2::XMLComment*> mapComments;
	tinyxml2::XMLNode* pNodeLang = pElLanguages->FirstChild();
	if (!pNodeLang) return;	// don't need to sort when there are no Language children
	while (pNodeLang) {
		if (pNodeLang->ToComment()) {
			tinyxml2::XMLNode* pNodeNext = pNodeLang->NextSibling();
			mapComments[pNodeNext] = pNodeLang->ToComment();
		}
		else {
			vpElements.push_back(pNodeLang->ToElement());
		}
		pNodeLang = pNodeLang->NextSibling();
	}

	// sort
	auto cmpLangNames = [](tinyxml2::XMLElement* pA, tinyxml2::XMLElement* pB) {
		// Note: true means A goes before B (A<B)

		// strings to compare
		std::string sA = pA ? pA->Attribute("name") : "";
		std::string sB = pB ? pB->Attribute("name") : "";

		// "normal" needs to go first
		if (sB == "normal") return false;
		if (sA == "normal") return true;

		// "searchResult" needs to go last
		if (sB == "searchResult") return true;
		if (sA == "searchResult") return false;

		// otherwise, compare case-insensitive by name
		return _string_insensitive_lt(sA, sB);
		};
	std::sort(vpElements.begin(), vpElements.end(), cmpLangNames);

	// update LexerStyles container: loop through the vector, use InsertEndChild to move that element to end of container
	for (auto pElToMove : vpElements) {
		tinyxml2::XMLNode* pIter = pElToMove;
		pElLanguages->InsertEndChild(pElToMove);
		tinyxml2::XMLNode* pPrevSib = pElToMove->PreviousSibling();
		while (mapComments.count(pIter)) {
			tinyxml2::XMLComment* pComment = mapComments[pIter];
			// logic: the pElToMove will now have a previous sibling (or will be the first)
			//		but that's not changing, so it's set outside the WHILE
			//	if has previous, then add this comment after the sibling
			if (pPrevSib) {
				pElLanguages->InsertAfterChild(pPrevSib, pComment);
			}
			//	if no previous, then add this comment as the first child of the pElLanguages parent
			else {
				pElLanguages->InsertFirstChild(pComment);
			}
			// either way, change the iterator to the comment that was just inserted, and see if we need to stack in next loop of WHILE
			pIter = pComment;
		}
	}

	std::string msg = std::string("+ Languages: Sorted Language elements alphabetically (keeping \"normal\" at the beginning and \"searchResults\" at end)");
	if (isIntermediateSorted) msg += " (for intermediate-sort output file)";
	_consoleWrite(msg);
}

// updates langs.xml to match langs.model.xml
void ConfigUpdater::_updateLangs(bool isIntermediateSorted)
{
	custatus_AppendText(const_cast<LPWSTR>(L"langs.xml\r\n"));
	custatus_SetProgress(80);

	// Prepare the filenames
	std::string sFilenameLangsModel = pcjHelper::wstring_to_utf8(_nppAppDir) + "\\langs.model.xml";
	std::string sFilenameLangsActive = pcjHelper::wstring_to_utf8(_nppCfgDir) + "\\langs.xml";
	std::wstring wsFilenameLangsModel = _nppAppDir + L"\\langs.model.xml";
	std::wstring wsFilenameLangsActive = _nppCfgDir + L"\\langs.xml";
	_consoleWrite(std::string("--- Checking Language File: '") + sFilenameLangsActive + "'");

	// check for permissions, exit function if cannot write
	if (!_ask_dir_permissions(_nppCfgDir)) return;

	// load the active and model documents
	tinyxml2::XMLDocument oDocLangsModel, oDocLangsActive;
	tinyxml2::XMLError eResult = oDocLangsModel.LoadFile(sFilenameLangsModel.c_str());
	if (_xml_check_result(eResult, &oDocLangsModel, wsFilenameLangsModel)) return;
	eResult = oDocLangsActive.LoadFile(sFilenameLangsActive.c_str());
	if (_xml_check_result(eResult, &oDocLangsActive, wsFilenameLangsActive)) return;

	// save a copy of the current XML output for future comparison
	tinyxml2::XMLPrinter xPrinter;
	oDocLangsActive.Accept(&xPrinter);
	std::string sOrigLangsText = xPrinter.CStr();

	// get the <Languages> element from each
	tinyxml2::XMLElement* pElLanguagesModel = oDocLangsModel.FirstChildElement("NotepadPlus")->FirstChildElement("Languages");
	if (!pElLanguagesModel) {
		_xml_check_result(tinyxml2::XML_ERROR_PARSING_ELEMENT, &oDocLangsModel);
		return;
	}
	tinyxml2::XMLElement* pElLanguagesActive = oDocLangsActive.FirstChildElement("NotepadPlus")->FirstChildElement("Languages");
	if (!pElLanguagesActive) {
		_xml_check_result(tinyxml2::XML_ERROR_PARSING_ELEMENT, &oDocLangsActive);
		return;
	}

	// If requested, Sort the original theme file and save to disk (for easy sort-based comparison of before/after)
	if (isIntermediateSorted)
	{
		tinyxml2::XMLDocument oClonedOrig;
		oDocLangsActive.DeepCopy(&oClonedOrig);
		tinyxml2::XMLElement* pElCloneLanguages = oClonedOrig.FirstChildElement("NotepadPlus")->FirstChildElement("Languages");
		_sortLanguagesByName(pElCloneLanguages, isIntermediateSorted);

		std::string sTmpFile = sFilenameLangsActive + ".orig.sorted";
		oClonedOrig.SaveFile(sTmpFile.c_str());
	}

	// Loop through model <Languages>, inserting missing data into active <Languages>
	tinyxml2::XMLElement* pElLangModel = pElLanguagesModel->FirstChildElement("Language");
	while (pElLangModel) {
		// get the name of this Model Language
		std::string sLangName = pElLangModel->Attribute("name");
		tinyxml2::XMLElement* pSearchLangActive = _find_element_with_attribute_value(pElLanguagesActive, nullptr, "Language", "name", sLangName);
		if (!pSearchLangActive) {
			// this <Language> not found in active, so need to clone it from Model
			tinyxml2::XMLElement* pClone = pElLangModel->DeepClone(&oDocLangsActive)->ToElement();

			// add the clone as a child of the active <Languages> element
			pSearchLangActive = pElLanguagesActive->InsertEndChild(pClone)->ToElement();

			// v2.0.1: if pSearchLangActive is NULL, it means something went wrong when adding the language.
			// this will change the message given to the logfile, and will prevent the loop-thru-keywords+comments WHILE loop (below) from running

			// inform the user
			std::string msg = std::string(pSearchLangActive ? "+ Languages: add missing Language='" : "! Languages: could NOT add missing Language='") + sLangName + "'";
			_consoleWrite(msg);
		}

		// loop through all the <Keywords> _and_ comments in the Model
		//		v2.0.1: need to prevent WHILE loop from throwing exception: don't run this WHILE loop if !pSearchLangActive
		tinyxml2::XMLNode* pNodeKeywordModel = pElLangModel->FirstChild();
		while (pNodeKeywordModel && pSearchLangActive) {
			bool isComment = pNodeKeywordModel->ToComment();
			std::string sKeywordName = isComment ? pNodeKeywordModel->Value() : pNodeKeywordModel->ToElement()->Attribute("name");
			tinyxml2::XMLNode* pSearchKeywordActive = nullptr;
			if (!isComment) {
				pSearchKeywordActive = _find_element_with_attribute_value(pSearchLangActive, nullptr, "Keywords", "name", sKeywordName);
			}
			else {
				// search through comments
				tinyxml2::XMLNode* pNodeCommentSearch = pSearchLangActive->FirstChild();
				while (pNodeCommentSearch) {
					if (pNodeCommentSearch->ToComment()) {
						// if it's a comment, check the value
						if (sKeywordName == pNodeCommentSearch->ToComment()->Value()) {
							// found a comment with the same text, so just reuse that one
							pSearchKeywordActive = pNodeCommentSearch;
						}
					}
					pNodeCommentSearch = pNodeCommentSearch->NextSibling();
				}
			}
			if (!pSearchKeywordActive) {
				// this <Keywords> not found in Active, so need to clone from Model
				tinyxml2::XMLNode* pCloneKw = pNodeKeywordModel->DeepClone(&oDocLangsActive);

				// add the clone as a child of the active <Language> element
				pSearchKeywordActive = pSearchLangActive->InsertEndChild(pCloneKw);

				// inform the user
				std::string msg = std::string("+ Language '") + sLangName + "': add missing " + (pSearchKeywordActive->ToComment() ? "Comment = " : "Keywords group:") + "'" + sKeywordName + "'";
				_consoleWrite(msg);
			}
			else {
				// it was found, so just move it
				pSearchLangActive->InsertEndChild(pSearchKeywordActive);
			}

			// for <Keyword> elements (not comments), dig into the actual data to see if any terms are missing
			if (pSearchKeywordActive->ToElement()) {
				// first, populate the active keywords
				std::vector<std::string> vActiveKeywords;
				std::map<std::string, bool> mActiveKeywords;
				const char* cstr = pSearchKeywordActive->ToElement()->GetText();
				if (cstr) {
					std::string token, sKeywordsText = cstr;
					std::istringstream ss(sKeywordsText);
					while (ss >> token) {
						vActiveKeywords.push_back(token);
						mActiveKeywords[token] = true;
					}
				}

				// now, for each keyword in the model, test if it's already in the active list; if not, add it
				cstr = pNodeKeywordModel->ToElement()->GetText();
				if (cstr) {
					std::string token, sKeywordsText = cstr;
					std::istringstream ss(sKeywordsText);
					size_t nAdded = 0;
					std::string sAdded = "";
					while (ss >> token) {
						if (!mActiveKeywords.count(token) && token.size()) {
							vActiveKeywords.push_back(token);
							mActiveKeywords[token] = true;
							sAdded += (nAdded ? " " : "") + token;
							++nAdded;
						}
					}
					if (nAdded) {
						std::string msg = std::string("+ Language '") + sLangName + "' group:'" + sKeywordName + "': add " + std::to_string(nAdded) + " keywords: " + sAdded;
						_consoleWrite(msg);
					}
				}

				// now sort the active keywords in normal case-sensitive alphabetical order
				std::sort(vActiveKeywords.begin(), vActiveKeywords.end());

				// join the list together into an updated string of keywords
				//		track the length of each "line" of text: if the new keyword would cause it to go beyond 8k, start a new line
				std::string sUpdatedKeywords = "";
				size_t lineLength = 0, maxLineLength = 8000;
				bool first = true;
				for (auto sKw : vActiveKeywords) {
					if (!first) {
						sUpdatedKeywords += " ";
						lineLength += 1;
					}
					if (lineLength + sKw.size() >= maxLineLength) {
						lineLength = 0;
						sUpdatedKeywords += "\n                ";
					}
					sUpdatedKeywords += sKw;
					lineLength += sKw.size();
					first = false;
				}

				// only run ->SetText if the list is non-empty (to avoid <Keywords name="xyz"></Keywords> replacing the better <Keywords name="xyz" />
				if (sUpdatedKeywords != "")
					pSearchKeywordActive->ToElement()->SetText(sUpdatedKeywords.c_str());
			}

			// move to next <Keywords> or comment
			pNodeKeywordModel = pNodeKeywordModel->NextSibling();
		}

		// then move to next <Language>
		pElLangModel = pElLangModel->NextSiblingElement("Language");
	}

	// Once done, sort the languages
	_sortLanguagesByName(pElLanguagesActive);

	// save only if needed: use the saved copy of the XML text compared to the updated text
	xPrinter.ClearBuffer();
	oDocLangsActive.Accept(&xPrinter);
	std::string sNewLangsText = xPrinter.CStr();

	if (sNewLangsText != sOrigLangsText)
		oDocLangsActive.SaveFile(sFilenameLangsActive.c_str());

	// whether we wrote it this time or not, check it for validity
	ValidateXML langsValidator(wsFilenameLangsActive, _wsLangsValidatorXsdFileName);
	if (langsValidator.isValid()) {
		std::wstring msg = std::wstring(L"+ Validation: Confirmed VALID Langs XML for ") + wsFilenameLangsActive;
		_consoleWrite(msg);
	}
	else {
		UINT64 lnum = langsValidator.uGetValidationLineNum();
		std::wstring msg = std::wstring(L"Validation of ") + wsFilenameLangsActive + L" failed" + (lnum == -1 ? L"" : (L" on line#" + std::to_wstring(lnum))) + L":\n\n" + langsValidator.wsGetValidationMessage();
		_consoleWrite(std::wstring(L"! ") + msg);
		_hadValidationError = true;
#if 0
		if (!_doStopValidationPester) {
			msg += L"\n\nWould you like to edit that file?";	// don't want the question in the .log, so moved it after the _consoleWrite
			int ask = ::MessageBox(nullptr, msg.c_str(), L"Langs.xml Validation Failed", MB_ICONWARNING | MB_YESNOCANCEL);
			switch (ask) {
				case IDCANCEL:
				{
					_doAbort = true;
					break;
				}
				case IDNO:
				{
					_doStopValidationPester = true;
					break;
				}
				case IDYES:
				{
					// open the file
					::SendMessage(_hwndNPP, NPPM_DOOPEN, 0, reinterpret_cast<LPARAM>(wsFilenameLangsActive.c_str()));

					// Get the current scintilla
					int which = -1;
					::SendMessage(nppData._nppHandle, NPPM_GETCURRENTSCINTILLA, 0, (LPARAM)&which);
					HWND curScintilla = (which < 1) ? nppData._scintillaMainHandle : nppData._scintillaSecondHandle;

					// go to the right line
					if (lnum != -1)
						::SendMessage(curScintilla, SCI_GOTOLINE, static_cast<WPARAM>(lnum - 1), 0);

					// stop looping
					_doAbort = true;
					break;
				}
			}
		}
#endif
	}


	// Update progress bar
	custatus_SetProgress(99);

	return;
}
