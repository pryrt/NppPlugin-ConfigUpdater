#pragma once
#include "PluginDefinition.h"
#include "PopulateXSD.h"
#include "pcjHelper.h"
#include "menuCmdID.h"
#include <string>

class NppMetaInfo {
public:
	// constructor
	NppMetaInfo(void);

	// Run this in the constructor for any class that uses it
	// (internally tracks whether it's been populated already, and doesn't duplicate effort)
	void populate(void);

	NppData& hwnd;

	struct {
		std::wstring
			app,
			appDataEquiv,
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

private:
	bool _isInitialized;
	std::wstring _askSettingsDir(void);
};

// make sure everyone who can see this class can see the global instance
extern NppMetaInfo gNppMetaInfo;
