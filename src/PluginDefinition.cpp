/*
    Copyright (C) 2025  Peter C. Jones <pryrtcode@pryrt.com>

    This file is part of the source code for the ConfigUpdater plugin for Notepad++

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#include "PluginDefinition.h"
#include "menuCmdID.h"
#include "ConfigUpdaterClass.h"
#include "ConfigDownloaderClass.h"
#include "CUStatusDialog.h"
#include "CUValidationDialog.h"
#include "CUDownloadModelDialog.h"
#include "PluginVersion.h"
#include "AboutDialog.h"

static HANDLE _hModule;

//
// The plugin data that Notepad++ needs
//
FuncItem funcItem[nbFunc];

//
// The data of Notepad++ that you can use in your plugin commands
//
NppData nppData;

//
// Initialize your plugin data here
// It will be called while plugin loading
void pluginInit(HANDLE hModule)
{
    _hModule = hModule;
}

//
// Here you can do the clean up, save the parameters (if any) for the next session
//
void pluginCleanUp()
{
}

//
// Initialization of your plugin commands
// You should fill your plugins commands here
void commandMenuInit()
{

    //--------------------------------------------//
    //-- STEP 3. CUSTOMIZE YOUR PLUGIN COMMANDS --//
    //--------------------------------------------//
    // with function :
    // setCommand(int index,                      // zero based number to indicate the order of command
    //            TCHAR *commandName,             // the command name that you want to see in plugin menu
    //            PFUNCPLUGINCMD functionPointer, // the symbol of function (function pointer) associated with this command. The body should be defined below. See Step 4.
    //            ShortcutKey *shortcut,          // optional. Define a shortcut to trigger this command
    //            bool check0nInit                // optional. Make this menu item be checked visually
    //            );
    setCommand(0, TEXT("Update Config Files"), menucall_UpdateConfigFiles , NULL, false);
    setCommand(1, TEXT("Download Newest Model Files"), menucall_DownloadModelFiles, NULL, false);
    setCommand(2, TEXT("Validate Config Files"), menucall_ValidateConfigFiles , NULL, false);
    setCommand(3, TEXT(""), NULL, NULL, false);
    setCommand(4, TEXT("About ConfigUpdater"), menucall_AboutDlg, NULL, false);
}

//
// Here you can do the clean up (especially for the shortcut)
//
void commandMenuCleanUp()
{
	// Don't forget to deallocate your shortcut here
}


//
// This function help you to initialize your plugin commands
//
bool setCommand(size_t index, TCHAR *cmdName, PFUNCPLUGINCMD pFunc, ShortcutKey *sk, bool check0nInit)
{
    if (index >= nbFunc)
        return false;

    if (!pFunc)
        return false;

    lstrcpy(funcItem[index]._itemName, cmdName);
    funcItem[index]._pFunc = pFunc;
    funcItem[index]._init2Check = check0nInit;
    funcItem[index]._pShKey = sk;

    return true;
}

//----------------------------------------------//
//-- STEP 4. DEFINE YOUR ASSOCIATED FUNCTIONS --//
//----------------------------------------------//
void menucall_UpdateConfigFiles(void)
{
    static ConfigUpdater oConf(nppData._nppHandle);

    // non-modal allows to still interact with the parent
    CreateDialogParam((HINSTANCE)_hModule, MAKEINTRESOURCE(IDD_CU_STATUS_DLG), nppData._nppHandle, (DLGPROC)ciDlgCUStatusProc, (LPARAM)NULL);

    // isIntermediateSorted will be overridden by settings later in the flow
    bool isIntermediateSorted = false;
    bool isRestarting = oConf.go(isIntermediateSorted);

    // if staying in the current N++ instance and there were validation errors, ask if user wants to run the full validation dialog
    if (!isRestarting && oConf.hadValidationError()) {
        std::wstring msg = L"Found at least one XML validation error during the config-file updating.\n\nWould you like to run the full Config Validation dialog?";
        int ask = ::MessageBox(nullptr, msg.c_str(), L"Update Validation Failed", MB_ICONWARNING | MB_YESNO);
        if (ask == IDYES) {
            menucall_ValidateConfigFiles();
        }
    }
}

void menucall_ValidateConfigFiles(void)
{
    // non-modal allows to still interact with the parent
    CreateDialogParam((HINSTANCE)_hModule, MAKEINTRESOURCE(IDD_CU_VALIDATION_DLG), nppData._nppHandle, (DLGPROC)ciDlgCUValidationProc, (LPARAM)NULL);
}

void menucall_DownloadModelFiles(void)
{
    // want the downloader object to stay around, so it doesn't have to re-instantiate every time the dialog is run
    static ConfigDownloader oDownloader;

    // non-modal allows to still interact with the parent
    CreateDialogParam((HINSTANCE)_hModule, MAKEINTRESOURCE(IDD_CU_DLMODEL_DLG), nppData._nppHandle, (DLGPROC)ciDlgCUDownloadModelProc, (LPARAM)NULL);

    // now that the dialog exists, can start the process
    bool stat = oDownloader.go();

    if (!stat) return;  // dummy to remind me that I've got a status available; might eventually decide to do something with the return value, but I don't know what, yet
}

void menucall_AboutDlg(void)
{
    // modal freezes the parent
    DialogBoxParam((HINSTANCE)_hModule, MAKEINTRESOURCE(IDD_ABOUTDLG), nppData._nppHandle, (DLGPROC)abtDlgProc, (LPARAM)NULL);
    return;
}

void buttoncall_ValidationHelpDlg(void)
{
    // modal freezes the parent
    DialogBoxParam((HINSTANCE)_hModule, MAKEINTRESOURCE(IDC_CU_VALIDATION_HELP_BTN), nppData._nppHandle, (DLGPROC)ciDlgCUValHelpProc, (LPARAM)NULL);
    return;
}
