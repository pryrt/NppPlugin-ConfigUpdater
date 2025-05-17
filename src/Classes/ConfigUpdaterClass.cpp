#include "ConfigUpdaterClass.h"
extern NppData nppData;

// delete null characters from padded wstrings				// private function (invisible to outside world)
void delNull(std::wstring& str)
{
	const auto pos = str.find(L'\0');
	if (pos != std::wstring::npos) {
		str.erase(pos);
	}
};

// convert wstring to UTF8-encoded bytes in std::string		// private function (invisible to outside world)
std::string wstring_to_utf8(std::wstring& wstr)
{
	if (wstr.empty()) return std::string();
	int szNeeded = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), static_cast<int>(wstr.size()), NULL, 0, NULL, NULL);
	if (szNeeded == 0) return std::string();
	std::string str(szNeeded, '\0');
	int result = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), static_cast<int>(wstr.size()), const_cast<LPSTR>(str.data()), szNeeded, NULL, NULL);
	if (result == 0) return std::string();
	return str;
}

// Checks if the plugin-"console" exists, creates it if necessary, activates the right view/index, and returns the correct scintilla HWND
HWND ConfigUpdater::_consoleCheck()
{
	LRESULT res = _uOutBufferID ? ::SendMessage(_hwndNPP, NPPM_GETPOSFROMBUFFERID, static_cast<WPARAM>(_uOutBufferID), 0) : -1;
	if (res == -1) {
		// creates it if necessary
		wchar_t wcTabName[] = L"ConfigUpdater.log";
		::SendMessage(_hwndNPP, NPPM_MENUCOMMAND, 0, IDM_FILE_NEW);
		_uOutBufferID = static_cast<UINT_PTR>(::SendMessage(_hwndNPP, NPPM_GETCURRENTBUFFERID, 0, 0));
		::SendMessage(_hwndNPP, NPPM_SETUNTITLEDNAME, static_cast<WPARAM>(_uOutBufferID), reinterpret_cast<LPARAM>(wcTabName));
		::SendMessage(_hwndNPP, NPPM_SETBUFFERLANGTYPE, static_cast<WPARAM>(_uOutBufferID), L_ERRORLIST);
		res = ::SendMessage(_hwndNPP, NPPM_GETPOSFROMBUFFERID, static_cast<WPARAM>(_uOutBufferID), 0);
	}

	// get the View and Index from the previous
	WPARAM view = (res & 0xC0000000) >> 30;	// upper bits are view
	LPARAM index = res & 0x3FFFFFFF;		// lower bits are index

	// activate
	::SendMessage(_hwndNPP, NPPM_ACTIVATEDOC, view, index);

	// return the correct scintilla HWND
	return view ? nppData._scintillaSecondHandle : nppData._scintillaMainHandle;
}

// Prints messages to the plugin-"console" tab; recommended to use DIFF/git-diff nomenclature, where "^+ "=add, "^- "=del, "^! "=change/error, "^--- "=message
void ConfigUpdater::_consoleWrite(std::wstring wsStr)
{
	std::string msg = wstring_to_utf8(wsStr) + "\n";
	_consoleWrite(msg.c_str());
}
void ConfigUpdater::_consoleWrite(std::string sStr)
{
	std::string msg = sStr + "\n";
	_consoleWrite(msg.c_str());
}
void ConfigUpdater::_consoleWrite(LPCWSTR wcStr)
{
	std::wstring msg(wcStr);
	_consoleWrite(msg);
}
void ConfigUpdater::_consoleWrite(LPCSTR cStr)
{
	HWND hEditor = _consoleCheck();
	::SendMessageA(hEditor, SCI_ADDTEXT, static_cast<WPARAM>(strlen(cStr)), reinterpret_cast<LPARAM>(cStr));
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
};

void ConfigUpdater::_initInternalState(void)
{
	_wsSavedComment = L"";
	_bHasTopLevelComment = false;
	//treeModel -- was an ETree::parse output object, but I'm not sure tinyxml2 needs such an intermediary... TBD
	_mapModelDefaultColors["fgColor"] = "";
	_mapModelDefaultColors["bgColor"] = "";
}

void ConfigUpdater::_populateNppDirs(void)
{
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
	_nppCfgAutoCompletionDir = exeDir + L"\\autoCompletion";

	// also, want to save the app directory and the themes relative to the app (because themes can be found in _both_ locations)
	_nppAppDir = exeDir;
	_nppAppThemesDir = _nppAppDir + L"\\themes";

	return;
}

void ConfigUpdater::go(bool isIntermediateSorted)
{
	_initInternalState();
	_getModelStyler();
	_updateAllThemes(isIntermediateSorted);
	if (isIntermediateSorted) return;
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

	auto elDefaultStyle = _get_default_style_element(_stylers_model_xml.oDoc);
	_mapModelDefaultColors["fgColor"] = elDefaultStyle->Attribute("fgColor");
	_mapModelDefaultColors["bgColor"] = elDefaultStyle->Attribute("bgColor");

	return;
}

// grab the default style element out of the given theme XML
tinyxml2::XMLElement* ConfigUpdater::_get_default_style_element(tinyxml2::XMLDocument& oDoc)
{
	// tinyxml2 doesn't have XPath, and my attempts at adding on tixml2ex.h's find_element() couldn't seem to ever find any element, even the root
	//	so switch to manual navigation.
	tinyxml2::XMLElement* pSearch = oDoc.FirstChildElement("NotepadPlus");
	if (pSearch) pSearch = pSearch->FirstChildElement("GlobalStyles");
	if (pSearch) pSearch = pSearch->FirstChildElement("WidgetStyle");
	while (pSearch) {
		if (pSearch->Attribute("styleID", "32")) {	// name="Default Style" styleID="32"
			break;
		}
		pSearch = pSearch->NextSiblingElement();
	}
	if (!pSearch) {
		_xml_check_result(tinyxml2::XML_ERROR_PARSING_ELEMENT, &oDoc);
	}
	return pSearch;
}

// loops over the stylers.xml, <cfg>\Themes, and <app>\Themes
void ConfigUpdater::_updateAllThemes(bool isIntermediateSorted)
{
	bool DEBUG = true;
	_updateOneTheme(_nppCfgDir, L"stylers.xml", isIntermediateSorted);
	if (DEBUG) return;

	DWORD szNeeded = GetCurrentDirectory(0, NULL);
	std::wstring curDir(szNeeded, L'\0');
	if (!GetCurrentDirectory(szNeeded, const_cast<LPWSTR>(curDir.data()))) return;

	HANDLE fhFound;
	WIN32_FIND_DATAW infoFound;
	std::vector<std::wstring> themeDirs{ _nppCfgThemesDir, _nppAppThemesDir };
	for (auto wsDir : themeDirs) {
		if (PathFileExists(wsDir.c_str())) {
			if (!SetCurrentDirectory(wsDir.c_str())) return;
			fhFound = FindFirstFile(L"*.xml", &infoFound);
			if (fhFound != INVALID_HANDLE_VALUE) {
				bool stillFound = true;
				while (stillFound) {
					// process this file
					stillFound &= _updateOneTheme(wsDir, infoFound.cFileName, isIntermediateSorted);

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
bool ConfigUpdater::_updateOneTheme(std::wstring themeDir, std::wstring themeName, bool isIntermediateSorted)
{
	// TODO: isWriteable test needs to go in here somewhere:
	//		when asking, YES=restart, NO=not for this file, CANCEL=stop asking;
	//		(use a new instance property to store the CANCEL answer)

	// get full path to file
	delNull(themeDir);
	delNull(themeName);
	std::wstring themePath = themeDir + L"\\" + themeName;
	std::string themePath8 = wstring_to_utf8(themePath);
	_consoleWrite(std::wstring(L"--- Checking Styler/Theme File: '") + themePath + L"'");

	// I don't know yet whether tinyxml2 has the TopLevelComment problem that py::xml.etree has.  I will have to watch out for that
	// remove comment from previous call of update_stylers(), otherwise no-comment myTheme.xml would inherit comment from commented MossyLawn.xml (from .py:2024-Aug-29 bugfix)
	_wsSavedComment = L"";
	_bHasTopLevelComment = false;

	// get the XML
	tinyxml2::XMLDocument oStylerDoc;
	tinyxml2::XMLError eResult = oStylerDoc.LoadFile(themePath8.c_str());
	_xml_check_result(eResult, &oStylerDoc);
	tinyxml2::XMLElement* pStylerRoot = oStylerDoc.RootElement();
	if (pStylerRoot == NULL) _xml_check_result(tinyxml2::XML_ERROR_FILE_READ_ERROR, &oStylerDoc);
	// TODO: if I need to track the top-level comment, I am hoping that I can test oStylerDoc.FirstChild vs pStylerRoot

	if (isIntermediateSorted) {
		1;	// TODO: make the unsorted intermediate file; skip that for now.
	}

	// Grab the default attributes from the <GlobalStyles><WidgetStyle name = "Global override" styleID = "0"...>
	tinyxml2::XMLElement* pDefaultStyle = _get_default_style_element(oStylerDoc);
	_mapStylerDefaultColors["fgColor"] = pDefaultStyle->Attribute("fgColor");
	_mapStylerDefaultColors["bgColor"] = pDefaultStyle->Attribute("bgColor");

	// grab the theme's LexerStyles node for future insertions
	tinyxml2::XMLElement* pElThemeLexerStyles = oStylerDoc.FirstChildElement("NotepadPlus")->FirstChildElement("LexerStyles");
	tinyxml2::XMLElement* pElModelLexerStyles = _stylers_model_xml.oDoc.FirstChildElement("NotepadPlus")->FirstChildElement("LexerStyles");

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

	// TODO: sort the LexerType nodes by name (keeping searchResult _last_)
	1;

	// Look for missing GlobalStyles::WidgetStyle entries as well
	tinyxml2::XMLElement* pElThemeGlobalStyles = oStylerDoc.FirstChildElement("NotepadPlus")->FirstChildElement("GlobalStyles")->ToElement();
	tinyxml2::XMLElement* pElModelGlobalStyles = _stylers_model_xml.oDoc.FirstChildElement("NotepadPlus")->FirstChildElement("GlobalStyles")->ToElement();
	_addMissingGlobalWidgets(pElModelGlobalStyles, pElThemeGlobalStyles, keepModelColors);

	// Write XML output
	3;


	return true;
}

// private: case-insensitive std::string comparison
bool _pvt_stringicmp(std::string a, std::string b)
{
	std::string a_copy = a;
	std::string b_copy = b;
	// ignore conversion of int to char implicit in the <algorithm>std::transform, which I have no control over
#pragma warning(push)
#pragma warning(disable: 4244)
	auto all_lower = [](std::string s) { std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); }); };
	//auto tl = [](unsigned char c) { return std::tolower(c); };
	//std::transform(sAttrRead.begin(), sAttrRead.end(), sAttrRead.begin(), tl);
	//std::transform(sAttrCmpr.begin(), sAttrCmpr.end(), sAttrCmpr.begin(), tl);
	all_lower(a_copy);
	all_lower(b_copy);
#pragma warning(pop)
	return a_copy == b_copy;
}

// look for an element, based on {Parent, FirstChild, or both} which is of a specific ElementType, having a specific AttributeName with specific AttributeValue
tinyxml2::XMLElement* ConfigUpdater::_find_element_with_attribute_value(tinyxml2::XMLElement* pParent, tinyxml2::XMLElement* pFirst, std::string sElementType, std::string sAttributeName, std::string sAttributeValue, bool caseSensitive)
{
	if (!pParent && !pFirst) return nullptr;
	tinyxml2::XMLElement* pMyParent = pParent ? pParent->ToElement() : pFirst->Parent()->ToElement();
	tinyxml2::XMLElement* pFoundElement = pFirst ? pFirst->ToElement() : pMyParent->FirstChildElement(sElementType.c_str())->ToElement();
	while (pFoundElement) {
		// if this node has the right attribute pair, great!
		if (caseSensitive) {
			if (pFoundElement->Attribute(sAttributeName.c_str(), sAttributeValue.c_str()))
				return pFoundElement;
		}
		else {
			const char* cAttrValue = pFoundElement->Attribute(sAttributeName.c_str());
			if (cAttrValue) {
				if (_pvt_stringicmp(sAttributeValue, cAttrValue))
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
	std::string msg = std::string("+ Add missing Lexer '") + pClone->Attribute("name") + "'";
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
	tinyxml2::XMLElement* pNewContainer = pStylerDoc->NewElement("NewGlobalStyles");
	pElThemeGlobalStyles->Parent()->InsertAfterChild(pElThemeGlobalStyles, pNewContainer);
	//tinyxml2::XMLElement* pCommentZero, * pRecentComment;

	// iterate through the nodes (comments and elements) of the model, and check if the theme has the equivalent or not
	tinyxml2::XMLNode* pModelNode = pElModelGlobalStyles->FirstChild();
	while (pModelNode) {
		if (pModelNode->ToComment()) {
			static std::string sCmpZero = " Attention : Don't modify the name of styleID=\"0\" ";
			if (pModelNode->Value() == sCmpZero)
				_consoleWrite(std::string("--- DEBUG: Found the ZeroComment: [") + pModelNode->Value() + "]");
			else
				_consoleWrite(std::string("--- DEBUG: Found Normal Comment: [") + pModelNode->Value() + "]");
		}
		else if (pModelNode->ToElement()) {
			std::string sModelName = pModelNode->ToElement()->Attribute("name");
			// do a case-insensitive search for this WidgetStyle's name in the theme:
			tinyxml2::XMLElement* pFoundWidget = _find_element_with_attribute_value(pElThemeGlobalStyles, nullptr, "WidgetStyle", "name", sModelName, false);
			if (pFoundWidget) {
				bool updated = false;
				// update missing attributes
				if (!pFoundWidget->Attribute("fgColor")) {
					pFoundWidget->SetAttribute("fgColor", _mapStylerDefaultColors["fgColor"].c_str());
					updated = true;
				}
				if (!pFoundWidget->Attribute("bgColor")) {
					pFoundWidget->SetAttribute("bgColor", _mapStylerDefaultColors["bgColor"].c_str());
					updated = true;
				}
				if (!pFoundWidget->Attribute("fontName")) {
					pFoundWidget->SetAttribute("fontName", "");
					updated = true;
				}
				if (!pFoundWidget->Attribute("fontStyle")) {
					pFoundWidget->SetAttribute("fontStyle", "");
					updated = true;
				}
				if (!pFoundWidget->Attribute("fontSize")) {
					pFoundWidget->SetAttribute("fontSize", "");
					updated = true;
				}

				// move found element to the new container
				pNewContainer->InsertEndChild(pFoundWidget);

				if (updated) {
					std::string msg = std::string("+ GlobalStyles: Updated attributes of existing WidgetStyle:'") + sModelName + "'";
					_consoleWrite(msg);
				}
			}
			else {
				// else need to clone it from model
				tinyxml2::XMLElement* pClone = pModelNode->DeepClone(pStylerDoc)->ToElement();
				//		- colors changed to theme defaults if !keepModelColors
				if (!keepModelColors) {
					pClone->SetAttribute("fgColor", _mapStylerDefaultColors["fgColor"].c_str());
					pClone->SetAttribute("bgColor", _mapStylerDefaultColors["bgColor"].c_str());
				}

				pNewContainer->InsertEndChild(pClone);

				std::string msg = std::string("+ GlobalStyles: Added missing WidgetStyle:'") + sModelName + "'";
				_consoleWrite(msg);
			}
		}
		// then move to next node
		pModelNode = pModelNode->NextSibling();
	}

	_consoleWrite(pStylerDoc);
}
