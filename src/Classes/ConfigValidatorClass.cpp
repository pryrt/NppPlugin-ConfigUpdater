#include "ConfigValidatorClass.h"
#include "ValidateXML.h"
#include "PopulateXSD.h"


ConfigValidator::ConfigValidator(NppData& origNppData)
{
	_populateNppMetadata(origNppData);
	_populateValidationLists();
}

void ConfigValidator::_populateNppMetadata(NppData& origNppData)
{
	_npp.hwnd = origNppData;

	////////////////////////////////
	// Get the directory which stores the executable
	////////////////////////////////
	std::wstring exeDir(MAX_PATH, '\0');
	::SendMessage(_npp.hwnd._nppHandle, NPPM_GETNPPDIRECTORY, MAX_PATH, reinterpret_cast<LPARAM>(exeDir.data()));
	_delNull(exeDir);
	// and the excecutable file path (including `\notepad++.exe`)
	std::wstring exePath(MAX_PATH, '\0');
	LRESULT szExe = ::SendMessage(_npp.hwnd._nppHandle, NPPM_GETNPPFULLFILEPATH, 0, reinterpret_cast<LPARAM>(exePath.data()));
	if (szExe) {
		_delNull(exePath);
		_npp.dir.appExePath = exePath;
	}

	////////////////////////////////
	// Get the AppData or Portable folders, as being relative to the plugins/Config directory
	////////////////////////////////

	// %AppData%\Notepad++\Plugins\Config or equiv
	LRESULT sz = 1 + ::SendMessage(_npp.hwnd._nppHandle, NPPM_GETPLUGINSCONFIGDIR, 0, NULL);
	std::wstring pluginCfgDir(sz, '\0');
	::SendMessage(_npp.hwnd._nppHandle, NPPM_GETPLUGINSCONFIGDIR, sz, reinterpret_cast<LPARAM>(pluginCfgDir.data()));
	_delNull(pluginCfgDir);
	_npp.dir.cfgPluginConfig = pluginCfgDir;
	_npp.dir.cfgPluginConfigMyDir = pluginCfgDir + L"\\ConfigUpdater";

	// %AppData%\Notepad++\Plugins or equiv
	//		since it's removing the tail, it will never be longer than pluginCfgDir; since it's in-place, initialize with the first
	std::wstring pluginDir = pluginCfgDir;
	PathCchRemoveFileSpec(const_cast<PWSTR>(pluginDir.data()), pluginCfgDir.size());
	_delNull(pluginDir);

	// %AppData%\Notepad++ or equiv is what I'm really looking for
	// _nppCfgDir				#py# _nppConfigDirectory = os.path.dirname(os.path.dirname(notepad.getPluginConfigDir()))
	_npp.dir.cfg = pluginDir;
	PathCchRemoveFileSpec(const_cast<PWSTR>(_npp.dir.cfg.data()), pluginDir.size());
	_delNull(_npp.dir.cfg);

	// Save this version, because Cloud/SettingsDir will overwrite _npp.dir.cfg, but I will still need to be able to fall back to the AppData||Portable
	std::wstring appDataOrPortableDir = _npp.dir.cfg;

	////////////////////////////////
	// Check for cloud and -settingsDir config-override locations
	////////////////////////////////
	bool usesCloud = false;
	sz = ::SendMessage(_npp.hwnd._nppHandle, NPPM_GETSETTINGSONCLOUDPATH, 0, 0); // get number of wchars in settings-on-cloud path (0 means settings-on-cloud is disabled)
	if (sz) {
		usesCloud = true;
		std::wstring wsCloudDir(sz + 1, '\0');
		LRESULT szGot = ::SendMessage(_npp.hwnd._nppHandle, NPPM_GETSETTINGSONCLOUDPATH, sz + 1, reinterpret_cast<LPARAM>(wsCloudDir.data()));
		if (szGot == sz) {
			_delNull(wsCloudDir);
			_npp.dir.cfg = wsCloudDir;
		}
	}

	// -settingsDir: if command-line option is enabled, use that directory for some config files
	bool usesSettingsDir = false;
	std::wstring wsSettingsDir = _askSettingsDir();
	if (wsSettingsDir.length() > 0)
	{
		usesSettingsDir = true;
		_npp.dir.cfg = wsSettingsDir;
	}

	////////////////////////////////
	// Now that we've got all the info, decide on final locations for Function List, UDL, and Themes;
	// AutoCompletion is always relative to executable directory
	////////////////////////////////


	// FunctionList: must be AppData or PortableDir, because FL does NOT work in Cloud or Settings directory
	// _nppCfgFunctionListDir	#py# _nppCfgFunctionListDirectory = os.path.join(_nppConfigDirectory, 'functionList')
	_npp.dir.cfgFunctionList = appDataOrPortableDir + L"\\functionList";

	// UDL: follows SettingsDir >> Cloud Dir >> Portable >> AppData
	// _nppCfgUdlDir			#py# _nppCfgUdlDirectory = os.path.join(_nppConfigDirectory, 'userDefineLangs')
	_npp.dir.cfgUdl = _npp.dir.cfg + L"\\userDefineLangs";

	// THEMES: does NOT follow SettingsDir (might be a N++ bug, but must live with what IS, not what SHOULD BE); DOES follow Cloud Dir >> Portable >> AppData
	// _nppCfgThemesDir			#py# _nppCfgThemesDirectory = os.path.join(_nppConfigDirectory, 'themes')
	_npp.dir.cfgThemes = (usesSettingsDir ? appDataOrPortableDir : _npp.dir.cfg) + L"\\themes";

	// AutoCompletion is _always_ relative to notepad++.exe, never in AppData or CloudConfig or SettingsDir
	// _nppCfgAutoCompletionDir	#py# _nppAppAutoCompletionDirectory = os.path.join(notepad.getNppDir(), 'autoCompletion')
	_npp.dir.cfgAutoCompletion = exeDir + L"\\autoCompletion";

	// also, want to save the app directory and the themes relative to the app (because themes can be found in _both_ locations)
	_npp.dir.app = exeDir;
	_npp.dir.appThemes  = _npp.dir.app + L"\\themes";

	return;
}

// Parse the -settingsDir out of the current command line
//	FUTURE: if a future version of N++ includes PR#16946, then do an "if version>vmin, use new message" section in the code
std::wstring ConfigValidator::_askSettingsDir(void)
{
	LRESULT sz = ::SendMessage(_npp.hwnd._nppHandle, NPPM_GETCURRENTCMDLINE, 0, 0);
	if (!sz) return L"";
	std::wstring strCmdLine(sz + 1, L'\0');
	LRESULT got = ::SendMessage(_npp.hwnd._nppHandle, NPPM_GETCURRENTCMDLINE, sz + 1, reinterpret_cast<LPARAM>(strCmdLine.data()));
	if (got != sz) return L"";
	_delNull(strCmdLine);

	std::wstring wsSettingsDir = L"";
	size_t p = 0;
	if ((p = strCmdLine.find(L"-settingsDir=")) != std::wstring::npos) {
		std::wstring wsEnd = L" ";
		// start by grabbing from after the = to the end of the string
		wsSettingsDir = strCmdLine.substr(p + 13, strCmdLine.length() - p - 13);
		if (wsSettingsDir[0] == L'"') {
			wsSettingsDir = wsSettingsDir.substr(1, wsSettingsDir.length() - 1);
			wsEnd = L"\"";
		}
		p = wsSettingsDir.find(wsEnd);
		if (p != std::wstring::npos) {
			// found the ending space or quote, so do everything _before_ that (need last position=p-1, which means a count of p)
			wsSettingsDir = wsSettingsDir.substr(0, p);
		}
		else if (wsEnd == L"\"") {
			// could not find end quote; should probably throw an error or something, but for now, just pretend I found nothing...
			return L"";
		}	// none found and looking for space means it found end-of-string, which is fine with the space separator
	}

	return wsSettingsDir;
}

// create the list/mapping of XML and XSD
void ConfigValidator::_populateValidationLists(void)
{
	// start with stylers/themes XSD
	std::wstring xpath = _npp.dir.cfgPluginConfigMyDir + L"\\theme.xsd";
	if (!PathFileExists(xpath.c_str()))
		PopulateXSD::WriteThemeXSD(xpath);

	// work on stylers/themes XML
	std::wstring fname = L"stylers.xml";
	std::wstring fpath = _npp.dir.cfg + L"\\" + fname;
	if (PathFileExists(fpath.c_str())) {
		_xmlNames.push_back(fname);
		_xmlPaths.push_back(fpath);
		_xsdPaths.push_back(xpath);
	}

	// determine whether to use just cfg or also use app
	std::vector<std::wstring> themeDirs{ _npp.dir.cfgThemes, _npp.dir.appThemes };
	if (_npp.dir.cfgThemes == _npp.dir.appThemes)
		themeDirs.pop_back();

	// need to change directory to iterate the directory, so keep track of where I was before
	DWORD szNeeded = GetCurrentDirectory(0, NULL);
	std::wstring curDir(szNeeded, L'\0');
	if (!GetCurrentDirectory(szNeeded, const_cast<LPWSTR>(curDir.data()))) return;

	// loop through themes
	HANDLE fhFound;
	WIN32_FIND_DATAW infoFound;
	for (auto wsDir : themeDirs) {
		if (PathFileExists(wsDir.c_str())) {
			if (!SetCurrentDirectory(wsDir.c_str())) return;
			fhFound = FindFirstFile(L"*.xml", &infoFound);
			if (fhFound != INVALID_HANDLE_VALUE) {
				bool stillFound = true;
				while (stillFound) {
					// get the name and full path
					fname = infoFound.cFileName;	// cFileName is relative to current directory
					fpath = wsDir + L"\\" + fname;

					// add this to the themes
					_xmlNames.push_back(fname);
					_xmlPaths.push_back(fpath);
					_xsdPaths.push_back(xpath);

					// look for next file
					stillFound &= static_cast<bool>(FindNextFile(fhFound, &infoFound));
					// static cast needed because bool and BOOL are not actually the same
				}
			}
			if (!SetCurrentDirectory(curDir.c_str())) return;
		}
	}

	// finally, langs.xml using langs.xsd
	xpath = _npp.dir.cfgPluginConfigMyDir + L"\\langs.xsd";
	if (!PathFileExists(xpath.c_str()))
		PopulateXSD::WriteLangsXSD(xpath);
	fname = L"langs.xml";
	fpath = _npp.dir.cfg + L"\\" + fname;
	if (PathFileExists(fpath.c_str())) {
		_xmlNames.push_back(fname);
		_xmlPaths.push_back(fpath);
		_xsdPaths.push_back(xpath);
	}

}

// delete null characters from padded wstrings				// private function (invisible to outside world)
std::wstring ConfigValidator::_delNull(std::wstring& str)
{
	const auto pos = str.find(L'\0');
	if (pos != std::wstring::npos) {
		str.erase(pos);
	}
	return str;
};

// get the active scintilla handle
HWND ConfigValidator::_hwActiveScintilla(void)
{
	// Get the current scintilla
	int which = -1;
	::SendMessage(nppData._nppHandle, NPPM_GETCURRENTSCINTILLA, 0, (LPARAM)&which);
	return (which < 1) ? nppData._scintillaMainHandle : nppData._scintillaSecondHandle;
}

// retrieve one of the directories
std::wstring ConfigValidator::getNppDir(std::wstring wsWhichDir)
{
	if (wsWhichDir == L"app") return _npp.dir.app;
	if (wsWhichDir == L"appExe") return _npp.dir.appExePath;
	if (wsWhichDir == L"appThemes") return _npp.dir.appThemes;
	if (wsWhichDir == L"cfg") return _npp.dir.cfg;
	if (wsWhichDir == L"cfgAutoCompletion") return _npp.dir.cfgAutoCompletion;
	if (wsWhichDir == L"cfgFunctionList") return _npp.dir.cfgFunctionList;
	if (wsWhichDir == L"cfgPluginConfig") return _npp.dir.cfgPluginConfig;
	if (wsWhichDir == L"cfgPluginConfigMyDir") return _npp.dir.cfgPluginConfigMyDir;
	if (wsWhichDir == L"cfgThemes") return _npp.dir.cfgThemes;
	if (wsWhichDir == L"cfgUdl") return _npp.dir.cfgUdl;
	return L"";
}
