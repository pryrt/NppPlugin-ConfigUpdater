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

void _pushed_model_btn(HWND hwFileCbx, HWND hwErrorList, HWND hwModelBtn, std::wstring wsModelName, ConfigValidator* pConfVal);	// private: call this when the *.model.xml button is pushed
void _pushed_validate_btn(HWND hwFileCbx, HWND hwErrorList, ConfigValidator* pConfVal);	// private: call this routine when VALIDATE button is pushed
void _dblclk_errorlbx_entry(HWND hwFileCbx, HWND hwErrorList, HWND hwModelBtn, ConfigValidator* pConfVal);	// private: call this routine when ERRORLIST entry is double-clicked
std::wstring _changed_filecbx_entry(HWND hwFileCbx, HWND hwErrorList, HWND hwModelBtn, ConfigValidator* pConfVal);	// private: call this routine when FILECBX entry is changed; returns model filename

INT_PTR CALLBACK ciDlgCUValidationProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM /*lParam*/)
{
	static ConfigValidator* s_pConfVal;	// private ConfigValidator
	static HWND s_hwFileCbx = nullptr, s_hwErrLbx = nullptr, s_hwModelBtn = nullptr;
	static std::wstring s_wsModelName = L"";
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
			s_hwModelBtn = GetDlgItem(g_hwndCUValidationDlg, IDC_CU_VALIDATION_MODEL_BTN);

			// Iterate thru the XML Names to populate the combobox
			ComboBox_ResetContent(s_hwFileCbx);
			for (auto xmlName : s_pConfVal->getXmlNames()) {
				ComboBox_AddString(s_hwFileCbx, xmlName.c_str());
			}

			// Make sure Error listbox starts empty
			ListBox_ResetContent(s_hwErrLbx);

			// Start with the *.model.xml button disabled
			Button_Enable(s_hwModelBtn, false);

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
				case IDC_CU_VALIDATION_MODEL_BTN:
				{
					_pushed_model_btn(s_hwFileCbx, s_hwErrLbx, s_hwModelBtn, s_wsModelName, s_pConfVal);
				}
				case IDC_CU_VALIDATION_ERROR_LB:
				{
					switch (HIWORD(wParam))
					{
						case LBN_DBLCLK:
						{
							_dblclk_errorlbx_entry(s_hwFileCbx, s_hwErrLbx, s_hwModelBtn, s_pConfVal);
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
							s_wsModelName = _changed_filecbx_entry(s_hwFileCbx, s_hwErrLbx, s_hwModelBtn, s_pConfVal);
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
		std::wstring longestText = L"";
		size_t nErrors = oValidator.szGetMultiNumErrors();
		for (size_t e = 0; e < nErrors; e++) {
			std::wstring msg = std::wstring(L"#") + std::to_wstring(pConfVal->vlErrorLinenums[e]) + L": " + pConfVal->vwsErrorReasons[e];
			ListBox_AddString(hwErrorList, msg.c_str());
			if (longestText.size() < msg.size()) longestText = msg;
		}

		// allow enough scrolling for the size of the text in the active font [cf: https://stackoverflow.com/a/29041620]
		HDC hDC = GetDC(hwErrorList);
		HGDIOBJ hOldFont = SelectObject(hDC, (HGDIOBJ)SendMessage(hwErrorList, WM_GETFONT, 0, 0));
		SIZE sz;
		GetTextExtentPoint32(hDC, longestText.c_str(), static_cast<int>(longestText.size()), &sz);
		::SendMessage(hwErrorList, LB_SETHORIZONTALEXTENT, sz.cx, 0);
		SelectObject(hDC, hOldFont);
		ReleaseDC(hwErrorList, hDC);
	}

	return;
}

// private: call this routine when ERRORLIST entry is double-clicked
void _dblclk_errorlbx_entry(HWND hwFileCbx, HWND hwErrorList, HWND hwModelBtn, ConfigValidator* pConfVal)
{
	// Get the active file (index)
	LRESULT cbCurSel = ::SendMessage(hwFileCbx, CB_GETCURSEL, 0, 0);
	if (CB_ERR == cbCurSel) return;

	// Get the active Error (index)
	LRESULT lbCurSel = ::SendMessage(hwErrorList, LB_GETCURSEL, 0, 0);
	if (LB_ERR == lbCurSel) return;

	// with a double-click here, it means that one of the errors is definitely selected, so safe to enable the *.model.xml button

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

	// if there are no errors, disable the *.model.xml button, otherwise, it's safe to enable
	Button_Enable(hwModelBtn, nErrors != 0);

	return;
}

// private: call this routine when FILECBX entry is changed; returns model filename
std::wstring _changed_filecbx_entry(HWND hwFileCbx, HWND hwErrorList, HWND hwModelBtn, ConfigValidator* pConfVal)
{
	// Get the active file (index)
	LRESULT cbCurSel = ::SendMessage(hwFileCbx, CB_GETCURSEL, 0, 0);
	if (CB_ERR == cbCurSel) return L"";

	// clear the Error ListBox
	ListBox_ResetContent(hwErrorList);

	// disable the model button, but set the model string based on the selected file
	Button_Enable(hwModelBtn, false);
	std::wstring wsName = (pConfVal->getXmlNames())[cbCurSel];
	std::wstring wsModel = (wsName == L"langs.xml") ? L"langs.model.xml" : L"stylers.model.xml";	// pick the right model file
	Button_SetText(hwModelBtn, wsModel.c_str());

	// return the right *.model.xml
	return wsModel;
}

// private: call this when the *.model.xml button is pushed
void _pushed_model_btn(HWND hwFileCbx, HWND hwErrorList, HWND hwModelBtn, std::wstring wsModelName, ConfigValidator* pConfVal)
{
	// Get the active file (index)
	LRESULT cbCurSel = ::SendMessage(hwFileCbx, CB_GETCURSEL, 0, 0);
	if (CB_ERR == cbCurSel) return;

	// Get the active Error (index) -- it might be LB_ERR if no error is selected (in which case, we'll want to go to line 1 (1-based))
	LRESULT lbCurSel = ::SendMessage(hwErrorList, LB_GETCURSEL, 0, 0);
	if (LB_ERR == lbCurSel) return;

	// if there are no errors, disable the *.model.xml button and exit
	size_t nErrors = pConfVal->vlErrorLinenums.size();
	if (!nErrors) {
		Button_Enable(hwModelBtn, false);
		return;
	}

	// assume that we will go to first line in model
	size_t iModelLocation = 0;		// 0-based location in the model file
	long iErrorLine = pConfVal->vlErrorLinenums[lbCurSel];		// 1-based line number for the XML file (_not_ the *.model.xml file)

	// Because line-vs-element metadata isn't encoded with the parsed XML structure, I cannot extract the parent element's name
	// Instead, based on the error context, I need to look for a different substring.
	std::wstring wsContext = pConfVal->vwsErrorContexts[lbCurSel];
	std::wstring wsModelElement = L"";
	std::wstring wsModelAttrVal = L"";
	bool bWantAttr = false;			// set true on the "model" searches that need to look for the a particular name="..." attribute for the given element
	if (wsModelName == L"stylers.model.xml") {
		//			  CONTEXT						SEARCH XML FOR							MODEL ELEMENT
		//			- /NotepadPlus					n/a										<GlobalStyles>
		//			- /GlobalStyles					n/a										<GlobalStyles>
		//			- WidgetStyle					n/a										<GlobalStyles>
		//			- /LexerStyles					n/a										<LexerStyles>
		//			- /LexerType					<LexerType name="..."					<LexerType name="..."
		//			- WordsStyle					<LexerType name="..."					<LexerType name="..."
		//			- LexerType						this line								<LexerType name="..."
		//			- <LexerStyles					n/a										<LexerStyles>
		//			- NotepadPlus					n/a										line 1
		//			- anything else					n/a										line 1
		// NOTE: if I always start the search at the END of the error line, then even the "this line" searches will work right
		if (wsContext.find(L"</NotepadPlus") != std::wstring::npos
			|| wsContext.find(L"</GlobalStyles") != std::wstring::npos
			|| wsContext.find(L"WidgetStyle") != std::wstring::npos
			) {
			wsModelElement = L"GlobalStyles";
		}
		else if (wsContext.find(L"LexerStyes") != std::wstring::npos) {	// whether it's the open or close of LexerStyles, want to find the start of LexerStyles in the model
			wsModelElement = L"LexerStyles";
		}
		else if (wsContext.find(L"</LexerType") != std::wstring::npos
			|| wsContext.find(L"WordsStyle") != std::wstring::npos
			) {
			wsModelElement = L"LexerType";
			bWantAttr = true;
		}
		else if (wsContext.find(L"<LexerType") != std::wstring::npos) {
			wsModelElement = L"LexerType";
			bWantAttr = true;
			1; // TODO: this one is special, and needs to search _this_ line, rather than searching backward...
		}
		// else: anything else just goes to first line of model file
	}
	else if (wsModelName == L"langs.model.xml") {
		//			  CONTEXT						SEARCH XML FOR							MODEL ELEMENT
	}

	// now, if needed, search the active file backward for the right line
	HWND hwSci = pConfVal->getActiveScintilla();
	if (wsModelElement != L"") {
		// start the search from the end of the context line
		LRESULT iLinePos = ::SendMessage(hwSci, SCI_GETLINEENDPOSITION, iErrorLine - 1, 0);
		::SendMessage(hwSci, SCI_SETTARGETSTART, iLinePos, 0);	// start search at the EOL
		::SendMessage(hwSci, SCI_SETTARGETEND, 0, 0);			// search backward to position 0
		// ::SendMessage
		// !!TODO!!: start here
	}


	
	// grab the error information for the specific error item from pConfVal

}
