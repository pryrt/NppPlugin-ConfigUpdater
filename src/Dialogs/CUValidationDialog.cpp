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
#include "pcjHelper.h"

HWND g_hwndCUValidationDlg;

void _pushed_model_btn(HWND hwFileCbx, HWND hwErrorList, HWND hwModelBtn, std::wstring wsModelName, ConfigValidator* pConfVal);	// private: call this when the *.model.xml button is pushed
void _pushed_validate_btn(HWND hwFileCbx, HWND hwErrorList, ConfigValidator* pConfVal);	// private: call this routine when VALIDATE button is pushed
void _sglclk_errorlbx_entry(HWND hwFileCbx, HWND hwErrorList, HWND hwModelBtn);	// private: call this routine when ERRORLIST entry is single-clicked
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
						case LBN_SELCHANGE:
						{
							_sglclk_errorlbx_entry(s_hwFileCbx, s_hwErrLbx, s_hwModelBtn);
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

// private: call this routine when ERRORLIST entry is single-clicked
void _sglclk_errorlbx_entry(HWND hwFileCbx, HWND hwErrorList, HWND hwModelBtn)
{
	// Get the active file (index)
	LRESULT cbCurSel = ::SendMessage(hwFileCbx, CB_GETCURSEL, 0, 0);
	if (CB_ERR == cbCurSel) return;

	// Get the active Error (index)
	LRESULT lbCurSel = ::SendMessage(hwErrorList, LB_GETCURSEL, 0, 0);
	if (LB_ERR == lbCurSel) return;

	// with a single-click here, it means that one of the errors is definitely selected, so safe to enable the *.model.xml button
	Button_Enable(hwModelBtn, true);

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
	//Button_SetText(hwModelBtn, wsModel.c_str());

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
	//size_t iModelLocation = 0;		// 0-based location in the model file
	long iErrorLine = pConfVal->vlErrorLinenums[lbCurSel];		// 1-based line number for the XML file (_not_ the *.model.xml file)

	// make sure the error-XML file is active
	std::wstring wsPath = (pConfVal->getXmlPaths())[cbCurSel];
	::SendMessage(pConfVal->getNppHwnd(), NPPM_DOOPEN, 0, reinterpret_cast<LPARAM>(wsPath.c_str()));
	::SendMessage(pConfVal->getActiveScintilla(), SCI_GOTOLINE, static_cast<WPARAM>(iErrorLine - 1), 0);

	// Because line-vs-element metadata isn't encoded with the parsed XML structure, I cannot extract the parent element's name
	// Instead, based on the error context, I need to look for a different substring.
	std::wstring wsContext = pConfVal->vwsErrorContexts[lbCurSel];
	std::string sLocalSearch = "";
	std::string sModelSearch = "";
	int bWantAttr = 0;			// set to the offset for the _quoted_ attribute on the "model" searches that need to look for the a particular name="..." attribute for the given element
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
			sLocalSearch = "<GlobalStyles";
		}
		else if (wsContext.find(L"LexerStyes") != std::wstring::npos) {	// whether it's the open or close of LexerStyles, want to find the start of LexerStyles in the model
			sLocalSearch = "<LexerStyles";
		}
		else if (wsContext.find(L"</LexerType") != std::wstring::npos
			|| wsContext.find(L"WordsStyle") != std::wstring::npos
			|| wsContext.find(L"<LexerType") != std::wstring::npos
			) {
			sLocalSearch = "<LexerType name=\"[^\"]*\"";
			sModelSearch = "<LexerType name=";
			bWantAttr = 16;
		}
		// else: anything else just goes to first line of model file
	}
	else if (wsModelName == L"langs.model.xml") {
		//			  CONTEXT						SEARCH XML FOR							MODEL ELEMENT
		//			- Language [no s]				<Language name=							<Language name=
		//			- Keywords						<Language name=							<Language name=
		//			- anything else					n/a										line 1
		if (wsContext.find(L"</Language>") != std::wstring::npos
			|| wsContext.find(L"<Language name=") != std::wstring::npos
			|| wsContext.find(L"<Keywords") != std::wstring::npos
			|| wsContext.find(L"</Keywords") != std::wstring::npos
			) {
			sLocalSearch = "<Language name=\"[^\"]*\"";
			sModelSearch = "<Language name=";
			bWantAttr = 15;
		}
	}

	// now, if needed, search the active file backward for the right line
	HWND hwSci = pConfVal->getActiveScintilla();
	HWND hwNpp = pConfVal->getNppHwnd();
	LRESULT deltaLines = 0;
	if (sLocalSearch != "") {
		// DEBUG: what file is actually open?
		std::wstring wsWhatFilename(MAX_PATH, L'\0');
		::SendMessage(hwNpp, NPPM_GETFILENAME, MAX_PATH, reinterpret_cast<LPARAM>(wsWhatFilename.data()));

		// start the search from the end of the context line
		WPARAM iPosEOL = ::SendMessage(hwSci, SCI_GETLINEENDPOSITION, iErrorLine - 1, 0);
		::SendMessage(hwSci, SCI_SETTARGETSTART, iPosEOL, 0);			// start search at the EOL
		::SendMessage(hwSci, SCI_SETTARGETEND, 0, 0);					// search backward to position 0
		::SendMessage(hwSci, SCI_SETSEARCHFLAGS, SCFIND_REGEXP, 0);		// scintilla regexp
		LRESULT iSearchResult = ::SendMessageA(hwSci, SCI_SEARCHINTARGET, sLocalSearch.size(), reinterpret_cast<LPARAM>(sLocalSearch.c_str()));
		std::string attributeValue = "";

		// scroll to the range, including both iSearchResult and iPosEOL (with iPosEOL taking precedence)
		if (iSearchResult != static_cast<LRESULT>(-1)) {
			//::SendMessage(hwSci, SCI_SCROLLRANGE, iSearchResult, iPosEOL);
			LRESULT iElementLine = ::SendMessage(hwSci, SCI_LINEFROMPOSITION, iSearchResult, 0);	// figure out what line the element starts at
			LRESULT iVisibleLine = ::SendMessage(hwSci, SCI_VISIBLEFROMDOCLINE, iElementLine, 0);	// convert from actual line number to "visible" line number (for wrapping/folding)
			::SendMessage(hwSci, SCI_SETFIRSTVISIBLELINE, iVisibleLine, 0);							// set that element to the top
			deltaLines = iErrorLine - iElementLine;													// figure out how many lines are between the element and the error
		}

		// model search will either be prefix + attribute value, or just the same search term
		if (iSearchResult && bWantAttr) {
			char buf[MAX_PATH] = { 0 };
			::SendMessage(hwSci, SCI_GETTARGETTEXT, 0, reinterpret_cast<LPARAM>(buf));
			attributeValue = &buf[bWantAttr];
			sModelSearch += std::string(attributeValue);
			pcjHelper::delNull(sModelSearch);
		}
		else {
			sModelSearch = sLocalSearch;
		}
	}

	// figure out which view is active
	LRESULT iCurView = ::SendMessage(hwNpp, NPPM_GETCURRENTVIEW, 0, 0);

	// open the *.model.xml into the _other_ view
	std::wstring wsModelPath = pConfVal->getNppDir(L"app") + L"\\" + wsModelName;
	if (::SendMessage(hwNpp, NPPM_DOOPEN, 0, reinterpret_cast<LPARAM>(wsModelPath.c_str()))) {
		LRESULT iModelView = ::SendMessage(hwNpp, NPPM_GETCURRENTVIEW, 0, 0);
		if (iModelView == iCurView) {
			::SendMessage(hwNpp, NPPM_MENUCOMMAND, 0, IDM_VIEW_GOTO_ANOTHER_VIEW);
		}

		// search for the right location in *.model.xml
		HWND hwModelSci = pConfVal->getActiveScintilla();

		if (sModelSearch != "") {
			WPARAM iPosEOLModel = ::SendMessage(hwModelSci, SCI_GETLINEENDPOSITION, iErrorLine - 1, 0);
			::SendMessage(hwModelSci, SCI_SETTARGETSTART, 0, 0);
			::SendMessage(hwModelSci, SCI_SETTARGETEND, iPosEOLModel, 0);
			::SendMessage(hwModelSci, SCI_SETSEARCHFLAGS, SCFIND_MATCHCASE, 0);
			LRESULT iModelResultPos = ::SendMessageA(hwModelSci, SCI_SEARCHINTARGET, sModelSearch.size(), reinterpret_cast<LPARAM>(sModelSearch.c_str()));
			::SendMessage(hwModelSci, SCI_GOTOPOS, iModelResultPos, 0);
			if (deltaLines) {
				LRESULT iElementLine = ::SendMessage(hwModelSci, SCI_LINEFROMPOSITION, iModelResultPos, 0);		// figure out what model line the element starts at
				LRESULT iVisibleLine = ::SendMessage(hwSci, SCI_VISIBLEFROMDOCLINE, iElementLine, 0);			// convert from actual line number to "visible" line number (for wrapping/folding)
				::SendMessage(hwModelSci, SCI_SETFIRSTVISIBLELINE, iVisibleLine, 0);							// set that element to the top of the model file, too
				WPARAM iEstimatedModelLine = iElementLine + deltaLines - 1;										// move the model's caret the same number of lines down from
				::SendMessage(hwModelSci, SCI_GOTOLINE, iEstimatedModelLine, 0);								// <do the move>

				// // start about the same distance below the current tag as was in the main XML
				// LRESULT iSecondary = iModelResultPos + deltaPos;
				// // figure out which line it was on
				// LRESULT iLine = ::SendMessage(hwModelSci, SCI_LINEFROMPOSITION, iSecondary, 0);
				// // and make sure that the _beginning_ of that line (not middle or end) is the secondary
				// iSecondary = ::SendMessage(hwModelSci, SCI_POSITIONFROMLINE, iLine, 0);
				// // then scroll to that... so that it will hopefully be scrolled all the way left...
				// ::SendMessage(hwModelSci, SCI_SCROLLRANGE, iSecondary, iModelResultPos);
			}
		}
		else {
			::SendMessage(hwModelSci, SCI_GOTOPOS, 0, 0);
		}

	}

}
