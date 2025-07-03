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

#include "CUDownloadModelDialog.h"
#include <string>

std::wstring _dlmEditText = L"";
bool _dlmInterruptFlag = false;
WPARAM _dlmScrollWidth = 400;

HWND g_hwndCUDownloadModelDlg;

INT_PTR CALLBACK ciDlgCUDownloadModelProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM /*lParam*/)
{
	switch (uMsg)
	{
		case WM_INITDIALOG:
		{
			// save HWND
			g_hwndCUDownloadModelDlg = hwndDlg;

			////////
			// need to know versions for making darkMode decisions
			////////
			LRESULT versionNpp = ::SendMessage(nppData._nppHandle, NPPM_GETNPPVERSION, 1, 0);	// HIWORD(nppVersion) = major version; LOWORD(nppVersion) = zero-padded minor (so 8|500 will come after 8|410)
			LRESULT versionDarkDialog = MAKELONG(540, 8);		// requires 8.5.4.0 for NPPM_DARKMODESUBCLASSANDTHEME (NPPM_GETDARKMODECOLORS was 8.4.1, so covered)
			bool isDM = (bool)::SendMessage(nppData._nppHandle, NPPM_ISDARKMODEENABLED, 0, 0);

			////////
			// setup all the controls before triggering dark mode
			////////

			// not interrupted yet
			_dlmInterruptFlag = false;

			// set progress bar
			cu_dl_model_SetProgress(5);

			// add some text
			cu_dl_model_ClearText();
			cu_dl_model_AppendText(L"--- ConfigUpdater: Starting Download of Newest Model Files ---\r\n");

			////////
			// trigger darkmode
			////////
			if (isDM && versionNpp >= versionDarkDialog) {
				::SendMessage(nppData._nppHandle, NPPM_DARKMODESUBCLASSANDTHEME, static_cast<WPARAM>(NppDarkMode::dmfInit), reinterpret_cast<LPARAM>(g_hwndCUDownloadModelDlg));
			}

			////////
			// Finally, Find Center and then position and display the window:
			////////
			RECT rc;
			HWND hParent = GetParent(hwndDlg);
			::GetClientRect(hParent, &rc);
			POINT center;
			int w = rc.right - rc.left;
			int h = rc.bottom - rc.top;
			center.x = rc.left + w / 2;
			center.y = rc.top + h / 2;
			::ClientToScreen(hParent, &center);
			RECT dlgRect;
			::GetClientRect(hwndDlg, &dlgRect);
			int x = center.x - (dlgRect.right - dlgRect.left) / 2;
			int y = center.y - (dlgRect.bottom - dlgRect.top) / 2;
			::SetWindowPos(hwndDlg, HWND_TOP, x, y, (dlgRect.right - dlgRect.left), (dlgRect.bottom - dlgRect.top), SWP_SHOWWINDOW);

			return true;
		}
		case WM_COMMAND:
		{
			switch (LOWORD(wParam))
			{
				case IDCANCEL:
					_dlmInterruptFlag = true;
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

void cu_dl_model_CloseWindow(void)
{
	::SendMessage(g_hwndCUDownloadModelDlg, WM_DESTROY, 0, 0);
}

void cu_dl_model_ClearText(void)
{
	_dlmEditText = L"";
	_dlmScrollWidth = 400;
}

void cu_dl_model_AppendText(LPWSTR newText)
{
	// update the stored text
	_dlmEditText += newText;

	// get edit control from dialog
	HWND hwndOutput = GetDlgItem(g_hwndCUDownloadModelDlg, IDC_CU_DLMODEL_TEXTOUT_EDITTXT);

	// set the full text
	::SendMessage(hwndOutput, WM_SETTEXT, 0, reinterpret_cast<LPARAM>(_dlmEditText.c_str()));

	// allow enough scrolling for the size of the text in the active font [cf: https://stackoverflow.com/a/29041620]
	HDC hDC = GetDC(hwndOutput);
	HGDIOBJ hOldFont = SelectObject(hDC, (HGDIOBJ)SendMessage(hwndOutput, WM_GETFONT, 0, 0));
	SIZE sz;
	GetTextExtentPoint32(hDC, newText, static_cast<int>(wcslen(newText)), &sz);
	if (static_cast<WPARAM>(sz.cx) > _dlmScrollWidth) {
		_dlmScrollWidth = static_cast<WPARAM>(sz.cx);
	}
	::SendMessage(hwndOutput, LB_SETHORIZONTALEXTENT, _dlmScrollWidth, 0);
	SelectObject(hDC, hOldFont);
	ReleaseDC(hwndOutput, hDC);

	// vertically scroll to the end
	LPARAM moved = 1;
	while (moved) {
		moved = ::SendMessage(hwndOutput, EM_SCROLL, SB_PAGEDOWN, 0);
	}

	// force a redraw right away
	RedrawWindow(g_hwndCUDownloadModelDlg, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);
}

void cu_dl_model_SetProgress(DWORD pct)
{
	::SendDlgItemMessage(g_hwndCUDownloadModelDlg, IDC_CU_DLMODEL_PROGRESS_PB, PBM_SETPOS, pct, 0);
	// force a redraw right away
	RedrawWindow(g_hwndCUDownloadModelDlg, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);
}

void cu_dl_model_AddProgress(DWORD pct)
{
	::SendDlgItemMessage(g_hwndCUDownloadModelDlg, IDC_CU_DLMODEL_PROGRESS_PB, PBM_DELTAPOS, pct, 0);
	// force a redraw right away
	RedrawWindow(g_hwndCUDownloadModelDlg, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);
}

bool cu_dl_model_GetInterruptFlag(void)
{
	return _dlmInterruptFlag;
}
