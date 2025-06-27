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

#include "CUValidationDialog.h"
#include "ConfigValidatorClass.h"
#include "ValidateXML.h"
#include <string>

HWND g_hwndCUValidationDlg;

void _pushed_validate_btn(HWND hwFileCbx, HWND hwErrorList, ConfigValidator* pConfVal);	// private: call this routine when VALIDATE button is pushed
void _dblclk_errorlbx_entry(HWND hwFileCbx, HWND hwErrorList, ConfigValidator* pConfVal);	// private: call this routine when ERRORLIST entry is double-clicked

INT_PTR CALLBACK ciDlgCUValidationProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM /*lParam*/)
{
	static ConfigValidator* s_pConfVal;	// private ConfigValidator
	static HWND s_hwFileCbx = nullptr, s_hwErrLbx = nullptr;
	switch (uMsg)
	{
		case WM_INITDIALOG:
		{
			// save HWND
			g_hwndCUValidationDlg = hwndDlg;

			////////
			// need to know versions for making darkMode decisions
			////////
			LRESULT versionNpp = ::SendMessage(nppData._nppHandle, NPPM_GETNPPVERSION, 1, 0);	// HIWORD(nppVersion) = major version; LOWORD(nppVersion) = zero-padded minor (so 8|500 will come after 8|410)
			LRESULT versionDarkDialog = MAKELONG(540, 8);		// requires 8.5.4.0 for NPPM_DARKMODESUBCLASSANDTHEME (NPPM_GETDARKMODECOLORS was 8.4.1, so covered)
			bool isDM = (bool)::SendMessage(nppData._nppHandle, NPPM_ISDARKMODEENABLED, 0, 0);

			////////
			// setup all the controls before triggering dark mode
			////////

			// need to instantiate the object
			if (!s_pConfVal) s_pConfVal = new ConfigValidator(nppData);

			// and store the file combobox and error listbox handles
			s_hwFileCbx = GetDlgItem(g_hwndCUValidationDlg, IDC_CU_VALIDATION_FILE_CBX);
			s_hwErrLbx = GetDlgItem(g_hwndCUValidationDlg, IDC_CU_VALIDATION_ERROR_LB);

			// Iterate thru the XML Names to populate the combobox
			ComboBox_ResetContent(s_hwFileCbx);
			for (auto xmlName : s_pConfVal->getXmlNames()) {
				ComboBox_AddString(s_hwFileCbx, xmlName.c_str());
			}

			// Make sure Error listbox starts empty
			ListBox_ResetContent(GetDlgItem(g_hwndCUValidationDlg, IDC_CU_VALIDATION_ERROR_LB));

			////////
			// trigger darkmode
			////////
			if (isDM && versionNpp >= versionDarkDialog) {
				::SendMessage(nppData._nppHandle, NPPM_DARKMODESUBCLASSANDTHEME, static_cast<WPARAM>(NppDarkMode::dmfInit), reinterpret_cast<LPARAM>(g_hwndCUValidationDlg));
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
				{
					// will then call WM_DESTROY, so handle full destruction of any objects then
					EndDialog(hwndDlg, 0);
					DestroyWindow(hwndDlg);
					return true;
				}
				case IDC_CU_VALIDATION_BTN:
				{
					_pushed_validate_btn(s_hwFileCbx, s_hwErrLbx, s_pConfVal);
					return true;
				}
				case IDC_CU_VALIDATION_ERROR_LB:
				{
					switch (HIWORD(wParam))
					{
						case LBN_DBLCLK:
						{
							_dblclk_errorlbx_entry(s_hwFileCbx, s_hwErrLbx, s_pConfVal);
							return true;
						}
						default:
							return false;
					}
				}
				case IDC_CU_VALIDATION_FILE_CBX:
				{
					switch (HIWORD(wParam))
					{
						case CBN_SELCHANGE:
						{
							ListBox_ResetContent(s_hwErrLbx);
							return true;
						}
						default:
							return false;
					}
				}
				default:
				{
					return false;
				}
			}
		}
		case WM_DESTROY:
		{
			if (s_pConfVal) {
				delete s_pConfVal;
				s_pConfVal = nullptr;
			}
			s_hwFileCbx = nullptr;
			s_hwErrLbx = nullptr;

			EndDialog(hwndDlg, 0);
			DestroyWindow(hwndDlg);
			return true;
		}
	}
	return false;
}

// private: call this routine when VALIDATE button is pushed
void _pushed_validate_btn(HWND hwFileCbx, HWND hwErrorList, ConfigValidator* pConfVal)
{
	// start with an empty listbox
	ListBox_ResetContent(hwErrorList);

	// Get the active file (index)
	LRESULT cbCurSel = ::SendMessage(hwFileCbx, CB_GETCURSEL, 0, 0);
	if (CB_ERR == cbCurSel) return;

	// validate the active file
	std::wstring wsPath = (pConfVal->getXmlPaths())[cbCurSel];
	std::wstring wsXSD = (pConfVal->getXsdPaths())[cbCurSel];
	ValidateXML oValidator(wsPath, wsXSD, true);

	if (oValidator.isValid()) {
		ListBox_AddString(hwErrorList, oValidator.wsGetValidationMessage().c_str());
		pConfVal->vlErrorLinenums.clear();
		pConfVal->vwsErrorReasons.clear();
		pConfVal->vwsErrorContexts.clear();
	}
	else {
		pConfVal->vlErrorLinenums = oValidator.vlGetMultiLinenums();
		pConfVal->vwsErrorReasons = oValidator.vwsGetMultiReasons();
		pConfVal->vwsErrorContexts = oValidator.vwsGetMultiContexts();
		size_t nErrors = oValidator.szGetMultiNumErrors();
		for (size_t e = 0; e < nErrors; e++) {
			std::wstring msg = std::wstring(L"#") + std::to_wstring(pConfVal->vlErrorLinenums[e]) + L": " + pConfVal->vwsErrorReasons[e];
			ListBox_AddString(hwErrorList, msg.c_str());
		}
	}

	return;
}

// private: call this routine when ERRORLIST entry is double-clicked
void _dblclk_errorlbx_entry(HWND hwFileCbx, HWND hwErrorList, ConfigValidator* pConfVal)
{
	// Get the active file (index)
	LRESULT cbCurSel = ::SendMessage(hwFileCbx, CB_GETCURSEL, 0, 0);
	if (CB_ERR == cbCurSel) return;

	// Get the active Error (index)
	LRESULT lbCurSel = ::SendMessage(hwErrorList, LB_GETCURSEL, 0, 0);
	if (LB_ERR == lbCurSel) return;

	// grab the mapped XML/XSD strings for the active file
	std::wstring wsName = (pConfVal->getXmlNames())[cbCurSel];
	std::wstring wsPath = (pConfVal->getXmlPaths())[cbCurSel];
	std::wstring wsXSD = (pConfVal->getXsdPaths())[cbCurSel];

	// grab the error information for the specific error item from pConfVal
	size_t nErrors = pConfVal->vlErrorLinenums.size();
	long iErrorLine = nErrors ? pConfVal->vlErrorLinenums[lbCurSel] : 0;		// 1-based line number
	std::wstring wsErrorMsg = nErrors ? pConfVal->vwsErrorReasons[lbCurSel] : L"SUCCESS";

	// navigate to file and line
	::SendMessage(pConfVal->getNppHwnd(), NPPM_DOOPEN, 0, reinterpret_cast<LPARAM>(wsPath.c_str()));
	if (iErrorLine < 0) iErrorLine = 0;
	::SendMessage(pConfVal->getActiveScintilla(), SCI_GOTOLINE, static_cast<WPARAM>(iErrorLine - 1), 0);

	return;
}
