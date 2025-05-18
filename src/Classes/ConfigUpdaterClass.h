#pragma once
#include <string>
#include <cctype>		// str::tolower
#include <algorithm>	// std::equal
#include <vector>
#include <map>
#include <stdexcept>
#include <windows.h>
#include <pathcch.h>
#include <shlwapi.h>
#include "PluginDefinition.h"
#include "menuCmdID.h"
#include "tinyxml2.h"

class ConfigUpdater {
public:
	ConfigUpdater(HWND hwndNpp);
	void go(bool isIntermediateSorted = false);

private:
	// internal attributes
	UINT_PTR _uOutBufferID = 0;																					// stores BufferID for plugin output
	std::wstring _wsSavedComment;																				// TODO: stores TopLevel comment, if needed
	bool _bHasTopLevelComment;																					// TODO: tracks TopLevel comment, if needed
	//treeModel -- was an ETree::parse output object, but I'm not sure tinyxml2 needs such an intermediary... TBD
	std::map<std::string, std::string> _mapModelDefaultColors, _mapStylerDefaultColors;							// store default colors from .model. and the active styler

	// Checks if the plugin-"console" exists, creates it if necessary, activates the right view/index, and returns the correct scintilla HWND
	HWND _consoleCheck();

	// Prints messages to the plugin-"console" tab; recommended to use DIFF/git-diff nomenclature, where "^+ "=add, "^- "=del, "^! "=change, "^--- "=message
	void _consoleWrite(std::wstring wsStr);
	void _consoleWrite(std::string sStr);
	void _consoleWrite(LPCWSTR wcStr);
	void _consoleWrite(LPCSTR cStr);
	void _consoleWrite(tinyxml2::XMLNode* pNode);

	void _initInternalState(void);																				// sets internal attributes back to default
	void _getModelStyler(void);																					// gets the XML
	void _updateAllThemes(bool isIntermediateSorted = false);													// loops over the stylers.xml, <cfg>\Themes, and <app>\Themes
	bool _updateOneTheme(std::wstring themeDir, std::wstring themeName, bool isIntermediateSorted = false);		// Updates one particular theme or styler file
	void _addMissingLexerType(tinyxml2::XMLElement* pElModelLexerType, tinyxml2::XMLElement* pElThemeLexerStyles, bool keepModelColors);			// clones any missing lexers from model to theme (and fixes attributes)
	void _addMissingLexerStyles(tinyxml2::XMLElement* pElModelLexerType, tinyxml2::XMLElement* pElThemeLexerType, bool keepModelColors);			// clones any styles for a given lexer from model to theme (and fixes attributes)
	void _addMissingGlobalWidgets(tinyxml2::XMLElement* pElModelGlobalStyles, tinyxml2::XMLElement* pElThemeGlobalStyles, bool keepModelColors);	// clones any missing WidgetStyle entries from model to the theme (and fixes attributes)
	void _sortLexerTypesByName(tinyxml2::XMLElement* pElThemeLexerStyles);										// Sorts all of the LexerType elements in the LexerStyles container

	bool _isAskRestartCancelled = false;																		// Remember whether CANCEL was chosen when asking to restart while looping through themes

	bool ConfigUpdater::_is_dir_writable(const std::wstring& path);		// checks if a given directory is writeable
	std::wstring ConfigUpdater::getWritableTempDir(void);				// gets a reasonable directory for a Temp file

	////////////////////////////////
	// XML Helpers
	////////////////////////////////

	// grab the default style element out of the given theme XML
	tinyxml2::XMLElement* ConfigUpdater::_get_default_style_element(tinyxml2::XMLDocument& oDoc);

	// look for an element, based on {Parent, FirstChild, or both} which is of a specific ElementType, having a specific AttributeName with specific AttributeValue
	tinyxml2::XMLElement* ConfigUpdater::_find_element_with_attribute_value(tinyxml2::XMLElement* pParent, tinyxml2::XMLElement* pFirst, std::string sElementType, std::string sAttributeName, std::string sAttributeValue, bool caseSensitive=true);

	////////////////////////////////
	////////////////////////////////

	// keeps track of the tinyxml2 Document and Root Node for a given XML file
	struct stXml {
		tinyxml2::XMLDocument oDoc;
		tinyxml2::XMLNode* pRoot = NULL;
		std::wstring fName;
	};

	stXml _stylers_model_xml;				// need access to the stylers.model.xml structure throughout
	tinyxml2::XMLError _xml_check_result(tinyxml2::XMLError a_eResult, tinyxml2::XMLDocument* p_doc = NULL)
	{
		if (a_eResult != tinyxml2::XML_SUCCESS) {
			std::string sMsg = std::string("XML Error #") + std::to_string(static_cast<int>(a_eResult));
			if (p_doc != NULL) sMsg += std::string(": ") + std::string(p_doc->ErrorStr());
			::MessageBoxA(NULL, sMsg.c_str(), "XML Error", MB_ICONWARNING | MB_OK);
		}
		return a_eResult;
	};


	// Npp Metadata
	void _populateNppDirs(void);
	std::wstring
		_nppAppDir,
		_nppAppThemesDir,
		_nppCfgDir,
		_nppCfgUdlDir,
		_nppCfgFunctionListDir,
		_nppCfgAutoCompletionDir,
		_nppCfgThemesDir;

	HWND _hwndNPP;
};

