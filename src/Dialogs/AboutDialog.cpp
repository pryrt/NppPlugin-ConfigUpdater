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

#include "AboutDialog.h"
#include "PluginVersion.h"
#include "Hyperlinks.h"
#include "NppMetaClass.h"

HWND g_hwndAboutDlg = nullptr;

#ifdef _WIN64
#define BITNESS TEXT("(64 bit)")
#else
#define BITNESS TEXT("(32 bit)")
#endif

#pragma warning(push)
#pragma warning(disable: 4100)
INT_PTR CALLBACK abtDlgProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	switch (uMsg) {
		case WM_INITDIALOG:
		{
			// save HWND
			g_hwndAboutDlg = hwndDlg;

			// need to know versions for making darkMode decisions
			gNppMetaInfo.populate();
			LRESULT versionDarkDialog = gNppMetaInfo.verDotToLL(8, 5, 4, 0);	// requires 8.5.4.0 for NPPM_DARKMODESUBCLASSANDTHEME (NPPM_GETDARKMODECOLORS was 8.4.1, so covered)

			bool isDM = (bool)::SendMessage(nppData._nppHandle, NPPM_ISDARKMODEENABLED, 0, 0);
			if (isDM && gNppMetaInfo.isNppVerAtLeast(versionDarkDialog)) {
				NppDarkMode::Colors myColors = { 0 };
				if (::SendMessage(nppData._nppHandle, NPPM_GETDARKMODECOLORS, sizeof(NppDarkMode::Colors), reinterpret_cast<LPARAM>(&myColors))) {
					SetHyperlinkRGB(myColors.linkText);
				}
				else {
					SetHyperlinkRGB(RGB(64, 64, 255));
				}
			}
			else {
				SetHyperlinkRGB(RGB(0, 0, 192));
			}

			// Need to set up any controls _before_ running the subclass-and-theme
			//		(however, the hyperlink fix must come after subclass-and-theme, otherwise hyperlink is undone)
			wchar_t title[256];
			swprintf_s(title, L"%s v%s %s", L"CollectionInterface", VERSION_WSTR, BITNESS);
			Edit_SetText(GetDlgItem(hwndDlg, IDC_VERSION), title);

			// Now do subclass-and-them
			if (isDM && gNppMetaInfo.isNppVerAtLeast(versionDarkDialog)) {
				::SendMessage(nppData._nppHandle, NPPM_DARKMODESUBCLASSANDTHEME, static_cast<WPARAM>(NppDarkMode::dmfInit), reinterpret_cast<LPARAM>(g_hwndAboutDlg));
			}

			// then update the hyperlink
			ConvertStaticToHyperlink(hwndDlg, IDC_GITHUB);
			ConvertStaticToHyperlink(hwndDlg, IDC_TINYXML2);

			// Finally, Find Center and then position and display the window:
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
			switch (LOWORD(wParam)) {
				case IDCANCEL:
				case IDOK:
					EndDialog(hwndDlg, 0);
					DestroyWindow(hwndDlg);
					return true;
				case IDC_GITHUB:
					ShellExecute(hwndDlg, TEXT("open"), TEXT(VERSION_URL), NULL, NULL, SW_SHOWNORMAL);
					return true;
				case IDC_TINYXML2:
					ShellExecute(hwndDlg, TEXT("open"), TEXT("https://github.com/leethomason/tinyxml2"), NULL, NULL, SW_SHOWNORMAL);
					return true;
			}
		}
		case WM_DESTROY:
		{
			DestroyWindow(hwndDlg);
			return true;
		}
	}
	return false;
}
#pragma warning(pop)
