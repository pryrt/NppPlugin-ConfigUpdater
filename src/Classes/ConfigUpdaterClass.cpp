#include "ConfigUpdaterClass.h"

// delete null characters from padded wstrings				// private function (invisible to outside world)
void delNull(std::wstring& str) {
	const auto pos = str.find(L'\0');
	if (pos != std::wstring::npos) {
		str.erase(pos);
	}
};

// convert wstring to UTF8-encoded bytes in std::string		// private function (invisible to outside world)
std::string wstring_to_utf8(std::wstring& wstr) {
	if (wstr.empty()) return std::string();
	int szNeeded = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), static_cast<int>(wstr.size()), NULL, 0, NULL, NULL);
	if (szNeeded == 0) return std::string();
	std::string str(szNeeded, '\0');
	int result = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), static_cast<int>(wstr.size()), const_cast<LPSTR>(str.data()), szNeeded, NULL, NULL);
	if (result == 0) return std::string();
	return str;
}


ConfigUpdater::ConfigUpdater(HWND hwndNpp) {
	_hwndNPP = hwndNpp;
	_populateNppDirs();
	_initInternalState();
};

void ConfigUpdater::_initInternalState(void) {
	_wsSavedComment = L"";
	_bHasTopLevelComment = false;
	//treeModel -- was an ETree::parse output object, but I'm not sure tinyxml2 needs such an intermediary... TBD
	_mapModelDefaultColors["fgColor"] = "";
	_mapModelDefaultColors["bgColor"] = "";
}

void ConfigUpdater::_populateNppDirs(void) {
	// %AppData%\Notepad++\Plugins\Config or equiv
	LRESULT sz = 1 + ::SendMessage(_hwndNPP, NPPM_GETPLUGINSCONFIGDIR, 0, NULL);
	std::wstring pluginCfgDir(sz, '\0');
	::SendMessage(_hwndNPP, NPPM_GETPLUGINSCONFIGDIR, sz, reinterpret_cast<LPARAM>(pluginCfgDir.data()));
	delNull(pluginCfgDir);

	// %AppData%\Notepad++\Plugins or equiv
	//		since it's removing the tail, it will never be longer than pluginCfgDir; since it's in-place, initialize with the first
	std::wstring pluginDir = pluginCfgDir;
	PathCchRemoveFileSpec(const_cast<PWSTR>(pluginDir.data()), pluginCfgDir.size());
	delNull(pluginDir);

	// %AppData%\Notepad++ or equiv is what I'm really looking for
	// _nppCfgDir				#py# _nppConfigDirectory = os.path.dirname(os.path.dirname(notepad.getPluginConfigDir()))
	_nppCfgDir = pluginDir;
	PathCchRemoveFileSpec(const_cast<PWSTR>(_nppCfgDir.data()), pluginDir.size());
	delNull(_nppCfgDir);

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
	delNull(exeDir);
	_nppAppDir = exeDir;
	_nppCfgAutoCompletionDir = exeDir + L"\\autoCompletion";

	return;
}

void ConfigUpdater::go(void)
{
	_initInternalState();
	_getModelStyler();
	return;
}

void ConfigUpdater::_getModelStyler(void)
{
	//_stylers_model_xml.oDoc;
	_stylers_model_xml.fName = std::wstring(MAX_PATH, L'\0');
	PathCchCombine(const_cast<PWSTR>(_stylers_model_xml.fName.data()), MAX_PATH, _nppAppDir.c_str(), L"stylers.model.xml");
	delNull(_stylers_model_xml.fName);
	tinyxml2::XMLError eResult = _stylers_model_xml.oDoc.LoadFile(wstring_to_utf8(_stylers_model_xml.fName).c_str());
	_xml_check_result(eResult, &_stylers_model_xml.oDoc);
	_stylers_model_xml.pRoot = _stylers_model_xml.oDoc.RootElement();
	if (_stylers_model_xml.pRoot == NULL) {
		_xml_check_result(tinyxml2::XML_ERROR_FILE_READ_ERROR, &_stylers_model_xml.oDoc);
	}

	// tinyxml2 doesn't have XPath, and my attempts at adding on tixml2ex.h's find_element() couldn't seem to ever find any element, even the root
	//	so switch to manual navigation.
	tinyxml2::XMLElement* pSearch = _stylers_model_xml.oDoc.FirstChildElement("NotepadPlus");
	if (pSearch) pSearch = pSearch->FirstChildElement("GlobalStyles");
	if (pSearch) pSearch = pSearch->FirstChildElement("WidgetStyle");
	while (pSearch) {
		if (pSearch->Attribute("styleID", "32")) {	// name="Default Style" styleID="32"
			break;
		}
		pSearch = pSearch->NextSiblingElement();
	}
	if (pSearch) {
		_mapModelDefaultColors["fgColor"] = pSearch->Attribute("fgColor");
		_mapModelDefaultColors["bgColor"] = pSearch->Attribute("bgColor");
	}
	else {
		_xml_check_result(tinyxml2::XML_ERROR_PARSING_ELEMENT, &_stylers_model_xml.oDoc);
	}

	return;
}
