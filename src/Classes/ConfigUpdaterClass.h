#pragma once
#include <string>
#include <vector>
#include <map>
#include <stdexcept>
#include <windows.h>
#include <pathcch.h>
#include "PluginDefinition.h"
#include "tinyxml2.h"

class ConfigUpdater {
public:
	ConfigUpdater(HWND hwndNpp);
	void go(void);

private:
	// internal attributes
	std::wstring _wsSavedComment;
	bool _bHasTopLevelComment;
	//treeModel -- was an ETree::parse output object, but I'm not sure tinyxml2 needs such an intermediary... TBD
	std::map<std::string, std::string> _mapModelDefaultColors;

	void _initInternalState(void);			// sets internal attributes back to default
	void _getModelStyler(void);				// gets the XML

	// keeps track of the tinyxml2 Document and Root Node for a given XML file
	struct stXml {
		tinyxml2::XMLDocument oDoc;
		tinyxml2::XMLNode* pRoot = NULL;
		std::wstring fName;
	};

	stXml _stylers_model_xml;				// need access to the stylers.model.xml structure throughout
	tinyxml2::XMLError _xml_check_result(tinyxml2::XMLError a_eResult, tinyxml2::XMLDocument* p_doc = NULL) {
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
		_nppCfgDir,
		_nppCfgUdlDir,
		_nppCfgFunctionListDir,
		_nppCfgAutoCompletionDir,
		_nppCfgThemesDir;

	HWND _hwndNPP;
};

