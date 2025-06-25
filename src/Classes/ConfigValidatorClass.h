#pragma once
#include <string>
#include <cctype>		// str::tolower
#include <algorithm>	// std::equal
#include <vector>
#include <map>
#include <windows.h>
//#include <pathcch.h>
//#include <shlwapi.h>
#include "PluginDefinition.h"
#include "menuCmdID.h"
//#include "tinyxml2.h"
#include "CUValidationDialog.h"
//#include <comutil.h>

class ConfigValidator {
public:
	ConfigValidator(HWND hwndNpp, HWND hwndFileCbx, HWND hwndErrLbx);
	void go(bool isIntermediateSorted);

private:
	struct {
		HWND npp;
		HWND fileCbx;
		HWND errLbx;
	} _hw;
};
