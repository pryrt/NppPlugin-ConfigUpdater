#pragma once
#include <string>
#include <cctype>		// str::tolower
#include <algorithm>	// std::equal
#include <vector>
#include <map>
#include <sstream>
#include <stdexcept>
#include <windows.h>
#include <pathcch.h>
#include <shlwapi.h>
#include "PluginDefinition.h"
#include "menuCmdID.h"
#include "tinyxml2.h"
#include "CUStatusDialog.h"

class ConfigUpdater {
public:
	ConfigUpdater(HWND hwndNpp);
	void go(bool isIntermediateSorted);

private:
	// internal attributes
	HANDLE _hOutConsoleFile = 0;																				// stores filehandle for plugin console output
	UINT_PTR _uOutBufferID = 0;																					// stores BufferID for plugin output
	std::wstring _wsSavedComment;																				// TODO: stores TopLevel comment, if needed
	bool _bHasTopLevelComment;																					// TODO: tracks TopLevel comment, if needed
	//treeModel -- was an ETree::parse output object, but I'm not sure tinyxml2 needs such an intermediary... TBD
	std::map<std::string, std::string> _mapModelDefaultColors, _mapStylerDefaultColors;							// store default colors from .model. and the active styler

	// Checks if the plugin-"console" exists, creates it if necessary, activates the right view/index, and returns the handle 
	HANDLE _consoleCheck();

	// Prints messages to the plugin-"console" tab; recommended to use DIFF/git-diff nomenclature, where "^+ "=add, "^- "=del, "^! "=change, "^--- "=message
	void _consoleWrite(std::wstring wsStr);
	void _consoleWrite(std::string sStr);
	void _consoleWrite(LPCWSTR wcStr);
	void _consoleWrite(LPCSTR cStr);
	void _consoleWrite(tinyxml2::XMLNode* pNode);

	void _initInternalState(void);																													// sets internal attributes back to default
	void ConfigUpdater::_createPluginSettingsIfNeeded(void);																							// creates the plugin's config file if it dooesn't exist
	void _readPluginSettings(void);																													// reads the plugin's config file
	tinyxml2::XMLDocument* _getModelStyler(void);																									// gets the XML
	void _updateAllThemes(bool isIntermediateSorted);																								// loops over the stylers.xml, <cfg>\Themes, and <app>\Themes
	bool _updateOneTheme(tinyxml2::XMLDocument* pModelStylerDoc, std::wstring themeDir, std::wstring themeName, bool isIntermediateSorted);			// Updates one particular theme or styler file
	void _addMissingLexerType(tinyxml2::XMLElement* pElModelLexerType, tinyxml2::XMLElement* pElThemeLexerStyles, bool keepModelColors);			// clones any missing lexers from model to theme (and fixes attributes)
	void _addMissingLexerStyles(tinyxml2::XMLElement* pElModelLexerType, tinyxml2::XMLElement* pElThemeLexerType, bool keepModelColors);			// clones any styles for a given lexer from model to theme (and fixes attributes)
	void _addMissingGlobalWidgets(tinyxml2::XMLElement* pElModelGlobalStyles, tinyxml2::XMLElement* pElThemeGlobalStyles, bool keepModelColors);	// clones any missing WidgetStyle entries from model to the theme (and fixes attributes)
	void _sortLexerTypesByName(tinyxml2::XMLElement* pElThemeLexerStyles, bool isIntermediateSorted = false);										// Sorts all of the LexerType elements in the LexerStyles container
	void _sortLanguagesByName(tinyxml2::XMLElement* pElLanguages, bool isIntermediateSorted = false);												// Sorts all of the Language elements in the Languages container
	void _updateLangs(bool isIntermediateSorted);																									// updates langs.xml to match langs.model.xml

	bool _doAbort = false;																															// "Abort" => Exit out without continuing with more files
	bool _isAskRestartCancelled = false;																											// Remember whether CANCEL was chosen when asking to restart while looping through themes
	bool _isAskRestartYes = false;																													// Remember if YES was pressed, so don't ask at end
	bool _setting_isIntermediateSorted = false;																										// from settings file, whether or not to generate an intermediate "sorted" file, which has no additions/fixes, but is in same order as final file

	bool _is_dir_writable(const std::wstring& path);																								// checks if a given directory is writeable
	std::wstring _getWritableTempDir(void);																											// gets a reasonable directory for a Temp file
	bool _ask_dir_permissions(const std::wstring& path);																							// tests if writable, and asks for UAC if not
	void _ask_rerun_normal(void);																													// tests if Admin, and asks to restart normally; if not, asks if you want to restart to have it take effect

	////////////////////////////////
	// XML Helpers
	////////////////////////////////

	// grab the default style element out of the given theme XML
	tinyxml2::XMLElement* _get_default_style_element(tinyxml2::XMLDocument& oDoc);
	tinyxml2::XMLElement* _get_default_style_element(tinyxml2::XMLDocument* pDoc);

	// look for an element, based on {Parent, FirstChild, or both} which is of a specific ElementType, having a specific AttributeName with specific AttributeValue
	tinyxml2::XMLElement* _find_element_with_attribute_value(tinyxml2::XMLElement* pParent, tinyxml2::XMLElement* pFirst, std::string sElementType, std::string sAttributeName, std::string sAttributeValue, bool caseSensitive=true);

	// compares the XMLError result to XML_SUCCESS, and returns a TRUE boolean to indicate failure
	bool _xml_check_result(tinyxml2::XMLError a_eResult, tinyxml2::XMLDocument* p_doc = NULL, std::wstring wsFilePath = std::wstring(L""))
	{
		if (a_eResult != tinyxml2::XML_SUCCESS) {
			std::string sMsg = std::string("XML Error #") + std::to_string(static_cast<int>(a_eResult));
			if (p_doc != NULL) {
				sMsg += std::string(": ") + std::string(p_doc->ErrorStr());
				if (p_doc->ErrorLineNum()) {
					sMsg += "\n\nI will try to open the file near that error.";
				}
			}
			::MessageBoxA(NULL, sMsg.c_str(), "XML Error", MB_ICONWARNING | MB_OK);
			if (p_doc!=NULL && p_doc->ErrorLineNum() && wsFilePath.size()) {
				if (::SendMessage(_hwndNPP, NPPM_DOOPEN, 0, reinterpret_cast<LPARAM>(wsFilePath.c_str()))) {
					extern NppData nppData;	// not in PluginDefinition.h

					// Get the current scintilla
					int which = -1;
					::SendMessage(nppData._nppHandle, NPPM_GETCURRENTSCINTILLA, 0, (LPARAM)&which);
					HWND curScintilla = (which < 1) ? nppData._scintillaMainHandle : nppData._scintillaSecondHandle;

					// SCI_GOTOLINE in the current scintilla instance
					WPARAM zeroLine = static_cast<WPARAM>(p_doc->ErrorLineNum() - 1);
					::SendMessage(curScintilla, SCI_GOTOLINE, zeroLine, 0);

					// do annotation
					::SendMessage(curScintilla, SCI_ANNOTATIONCLEARALL, 0, 0);
					::SendMessage(curScintilla, SCI_ANNOTATIONSETVISIBLE, ANNOTATION_BOXED, 0);
					::SendMessageA(curScintilla, SCI_ANNOTATIONSETTEXT, zeroLine, reinterpret_cast<LPARAM>(p_doc->ErrorStr()));

					// need to stop
					_doAbort = true;
				};
			}
			return true;
		}
		return false;
	};

	////////////////////////////////
	// Npp Metadata
	////////////////////////////////

	void _populateNppDirs(void);
	std::wstring
		_nppAppDir,
		_nppAppThemesDir,
		_nppExePath,
		_nppCfgDir,
		_nppCfgPluginConfigDir,
		_nppCfgUdlDir,
		_nppCfgFunctionListDir,
		_nppCfgAutoCompletionDir,
		_nppCfgThemesDir;

	HWND _hwndNPP;
};

