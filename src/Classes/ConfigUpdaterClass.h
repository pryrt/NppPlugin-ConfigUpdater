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
	std::map<std::wstring, std::wstring> _mapModelDefaultColors;

	void _initInternalState(void);	// sets internal attributes back to default

	// Npp Metadata
	void _populateNppDirs(void);
	std::wstring
		_nppCfgDir,
		_nppCfgUdlDir,
		_nppCfgFunctionListDir,
		_nppCfgAutoCompletionDir,
		_nppCfgThemesDir;

	HWND _hwndNPP;
};

