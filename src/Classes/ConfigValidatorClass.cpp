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

	// _nppCfgUdlDir			#py# _nppCfgUdlDirectory = os.path.join(_nppConfigDirectory, 'userDefineLangs')
	_npp.dir.cfgUdl = _npp.dir.cfg + L"\\userDefineLangs";

	// _nppCfgFunctionListDir	#py# _nppCfgFunctionListDirectory = os.path.join(_nppConfigDirectory, 'functionList')
	_npp.dir.cfgFunctionList = _npp.dir.cfg + L"\\functionList";

	// _nppCfgThemesDir			#py# _nppCfgThemesDirectory = os.path.join(_nppConfigDirectory, 'themes')
	_npp.dir.cfgThemes = _npp.dir.cfg + L"\\themes";

	// AutoCompletion is _always_ relative to notepad++.exe, never in AppData or CloudConfig or SettingsDir
	// _nppCfgAutoCompletionDir	#py# _nppAppAutoCompletionDirectory = os.path.join(notepad.getNppDir(), 'autoCompletion')
	std::wstring exeDir(MAX_PATH, '\0');
	::SendMessage(_npp.hwnd._nppHandle, NPPM_GETNPPDIRECTORY, MAX_PATH, reinterpret_cast<LPARAM>(exeDir.data()));
	_delNull(exeDir);
	_npp.dir.cfgAutoCompletion = exeDir + L"\\autoCompletion";

	// also, want to save the app directory and the themes relative to the app (because themes can be found in _both_ locations)
	_npp.dir.app = exeDir;
	_npp.dir.appThemes  = _npp.dir.app + L"\\themes";

	// executable path:
	std::wstring exePath(MAX_PATH, '\0');
	LRESULT szExe = ::SendMessage(_npp.hwnd._nppHandle, NPPM_GETNPPFULLFILEPATH, 0, reinterpret_cast<LPARAM>(exePath.data()));
	if (szExe) {
		_delNull(exePath);
		_npp.dir.appExePath = exePath;
	}

	return;
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
