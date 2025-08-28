#include "ConfigValidatorClass.h"
#include "ValidateXML.h"
#include "PopulateXSD.h"
#include "NppMetaClass.h"

ConfigValidator::ConfigValidator(NppData& /*origNppData*/)
{
	gNppMetaInfo.populate();
	_populateValidationLists();
}

// create the list/mapping of XML and XSD
void ConfigValidator::_populateValidationLists(void)
{
	// start with stylers/themes XSD
	std::wstring xpath = gNppMetaInfo.dir.cfgPluginConfigMyDir + L"\\theme.xsd";
	if (!PathFileExists(xpath.c_str()))
		PopulateXSD::WriteThemeXSD(xpath);

	// work on stylers/themes XML
	std::wstring fname = L"stylers.xml";
	std::wstring fpath = gNppMetaInfo.dir.cfg + L"\\" + fname;
	if (PathFileExists(fpath.c_str())) {
		_xmlNames.push_back(fname);
		_xmlPaths.push_back(fpath);
		_xsdPaths.push_back(xpath);
	}

	// determine whether to use just cfg or also use app
	std::vector<std::wstring> themeDirs{ gNppMetaInfo.dir.cfgThemes, gNppMetaInfo.dir.appThemes };
	if (gNppMetaInfo.dir.cfgThemes == gNppMetaInfo.dir.appThemes)
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
	xpath = gNppMetaInfo.dir.cfgPluginConfigMyDir + L"\\langs.xsd";
	if (!PathFileExists(xpath.c_str()))
		PopulateXSD::WriteLangsXSD(xpath);
	fname = L"langs.xml";
	fpath = gNppMetaInfo.dir.cfg + L"\\" + fname;
	if (PathFileExists(fpath.c_str())) {
		_xmlNames.push_back(fname);
		_xmlPaths.push_back(fpath);
		_xsdPaths.push_back(xpath);
	}

}

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
	if (wsWhichDir == L"app") return gNppMetaInfo.dir.app;
	if (wsWhichDir == L"appExe") return gNppMetaInfo.dir.appExePath;
	if (wsWhichDir == L"appThemes") return gNppMetaInfo.dir.appThemes;
	if (wsWhichDir == L"cfg") return gNppMetaInfo.dir.cfg;
	if (wsWhichDir == L"cfgAutoCompletion") return gNppMetaInfo.dir.cfgAutoCompletion;
	if (wsWhichDir == L"cfgFunctionList") return gNppMetaInfo.dir.cfgFunctionList;
	if (wsWhichDir == L"cfgPluginConfig") return gNppMetaInfo.dir.cfgPluginConfig;
	if (wsWhichDir == L"cfgPluginConfigMyDir") return gNppMetaInfo.dir.cfgPluginConfigMyDir;
	if (wsWhichDir == L"cfgThemes") return gNppMetaInfo.dir.cfgThemes;
	if (wsWhichDir == L"cfgUdl") return gNppMetaInfo.dir.cfgUdl;
	return L"";
}
