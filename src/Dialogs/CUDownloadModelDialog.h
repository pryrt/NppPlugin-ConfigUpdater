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

#pragma once
#include <WindowsX.h>
#include "PluginDefinition.h"
#include "resource.h"
#include <CommCtrl.h>

INT_PTR CALLBACK ciDlgCUDownloadModelProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
void cu_dl_model_ClearText(void);
void cu_dl_model_AppendText(LPWSTR newText);
void cu_dl_model_SetProgress(DWORD pct);
void cu_dl_model_AddProgress(DWORD pct);
void cu_dl_model_CloseWindow(void);
bool cu_dl_model_GetInterruptFlag(void);
