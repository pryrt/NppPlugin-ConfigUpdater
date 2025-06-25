#pragma once
#include <string>
#include <cctype>		// str::tolower
#include <algorithm>	// std::equal
#include <vector>
#include <map>
#include <windows.h>
#include <pathcch.h>
#include <shlwapi.h>
#include "PluginDefinition.h"
#include "menuCmdID.h"
#include "CUValidationDialog.h"

class ConfigValidator {
public:
	ConfigValidator(NppData& origNppData);
	void go(bool isIntermediateSorted);

private:
	void _populateValidationLists(void);		// create the list/mapping of XML and XSD
	std::vector<std::wstring>	// all share the same index number, so index i of each array will be the same value
		_xmlNames,								// vector to store the short name for each XML to validate
		_xmlPaths,								// vector to store the full path for each XML to validate
		_xsdPaths;								// vector to store the full path for each XSD to use for validation

	////////////////////////////////
	// Private Helpers
	////////////////////////////////

	// delete NULL characters at end of wstring
	std::wstring _delNull(std::wstring& str);

	// get the active scintilla handle
	HWND _hwActiveScintilla(void);

	////////////////////////////////
	// Npp Metadata
	////////////////////////////////
	void _populateNppMetadata(NppData& origNppData);	// using the HWND information, poll N++ for the correct directory information and other metadata
	struct {
		NppData hwnd;
		struct {
			std::wstring
				app,
				appThemes,
				appExePath,
				cfg,
				cfgPluginConfig,
				cfgPluginConfigMyDir,
				cfgUdl,
				cfgFunctionList,
				cfgAutoCompletion,
				cfgThemes;
		} dir;
	} _npp;

};
