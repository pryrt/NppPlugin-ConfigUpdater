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

#include "CUStatusDialog.h"
#include <string>

std::wstring _editText = L"";
bool _InterruptFlag = false;

HWND _hwndDlg;

INT_PTR CALLBACK ciDlgCUStatusProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM /*lParam*/)
{
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        // save HWND
        _hwndDlg = hwndDlg;

        // Find Center and then position the window:

        // find App center
        RECT rc;
        HWND hParent = GetParent(hwndDlg);
        ::GetClientRect(hParent, &rc);
        POINT center;
        int w = rc.right - rc.left;
        int h = rc.bottom - rc.top;
        center.x = rc.left + w / 2;
        center.y = rc.top + h / 2;
        ::ClientToScreen(hParent, &center);

        // and position dialog
        RECT dlgRect;
        ::GetClientRect(hwndDlg, &dlgRect);
        int x = center.x - (dlgRect.right - dlgRect.left) / 2;
        int y = center.y - (dlgRect.bottom - dlgRect.top) / 2;
        ::SetWindowPos(hwndDlg, HWND_TOP, x, y, (dlgRect.right - dlgRect.left), (dlgRect.bottom - dlgRect.top), SWP_SHOWWINDOW);

        // not interrupted yet
        _InterruptFlag = false;

        // set progress bar
        custatus_SetProgress(5);

        // add some text
        custatus_ClearText();
        custatus_AppendText(L"--- ConfigUpdater: Starting ---\r\n");

        return true;
    }
    case WM_COMMAND:
    {
        switch (LOWORD(wParam))
        {
        case IDCANCEL:
            _InterruptFlag = true;
            EndDialog(hwndDlg, 0);
            DestroyWindow(hwndDlg);
            return true;
        default:
            return false;
        }
    }
    case WM_DESTROY:
        EndDialog(hwndDlg, 0);
        DestroyWindow(hwndDlg);
        return true;
    }
    return false;
}

void custatus_CloseWindow(void)
{
    ::SendMessage(_hwndDlg, WM_DESTROY, 0, 0);
}

void custatus_ClearText(void)
{
    _editText = L"";
}

void custatus_AppendText(LPWSTR newText)
{
    // update the stored text
    _editText += newText;

    // get edit control from dialog
    HWND hwndOutput = GetDlgItem(_hwndDlg, IDC_CU_STAT_TEXTOUT);

    // set the full text
    ::SendMessage(hwndOutput, WM_SETTEXT, 0, reinterpret_cast<LPARAM>(_editText.c_str()));

    // scroll to the end
    LPARAM moved = 1;
    while (moved) {
        moved = ::SendMessage(hwndOutput, EM_SCROLL, SB_PAGEDOWN, 0);
    }
}

void custatus_SetProgress(DWORD pct)
{
    ::SendDlgItemMessage(_hwndDlg, IDC_CU_STAT_PROGRESS1, PBM_SETPOS, pct, 0);
}

void custatus_AddProgress(DWORD pct)
{
    ::SendDlgItemMessage(_hwndDlg, IDC_CU_STAT_PROGRESS1, PBM_DELTAPOS, pct, 0);
}

bool custatus_GetInterruptFlag(void)
{
    return _InterruptFlag;
}
