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
#include <wininet.h>
#include <shlwapi.h>
#include "PluginDefinition.h"
#include "menuCmdID.h"
//#include "tinyxml2.h"
#include "CUDownloadModelDialog.h"
#include <comutil.h>


class ConfigDownloader {
public:
	ConfigDownloader(void);
	bool go(void);

private:
	struct {
		std::wstring
			AppDir,				// path for the application's directory
			LangsModel,			// path to langs.model.xml
			StylersModel;		// path to stylers.model.xml
	} _wsPath;
	struct {
		std::wstring
			LangsModel,			// URL for the most recent langs.model.xml
			StylersModel;		// URL for the most recent stylers.model.xml
	} _wsURL;

	std::wstring getWritableTempDir(void);
	bool downloadFileToDisk(const std::wstring& url, const std::wstring& path);
	bool ask_overwrite_if_exists(const std::wstring& path);

};
