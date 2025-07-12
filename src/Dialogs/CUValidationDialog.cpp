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
#include <commctrl.h>

HWND g_hwndCUValidationDlg, g_hwndCUHlpDlg;
ConfigValidator* g_pConfVal;	// private ConfigValidator object
const UINT_PTR _uniqueSubclassID = 750118;
bool g_IsDarkMode = false;

void _pushed_model_btn(HWND hwFileCbx, HWND hwErrorList, HWND hwModelBtn, std::wstring wsModelName, ConfigValidator* pConfVal);	// private: call this when the *.model.xml button is pushed
void _pushed_validate_btn(HWND hwFileCbx, HWND hwErrorList, ConfigValidator* pConfVal);	// private: call this routine when VALIDATE button is pushed
void _sglclk_errorlbx_entry(HWND hwFileCbx, HWND hwErrorList, HWND hwModelBtn);	// private: call this routine when ERRORLIST entry is single-clicked
void _dblclk_errorlbx_entry(HWND hwFileCbx, HWND hwErrorList, HWND hwModelBtn, ConfigValidator* pConfVal);	// private: call this routine when ERRORLIST entry is double-clicked
std::wstring _changed_filecbx_entry(HWND hwFileCbx, HWND hwErrorList, HWND hwModelBtn, ConfigValidator* pConfVal);	// private: call this routine when FILECBX entry is changed; returns model filename
static LRESULT CALLBACK _ListBoxSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);
void _copy_current_errlbx_entry(HWND hwErorList);	// copy current selected errorbox entry into clipboard (and return the value for no good reason)
void _pushed_help_btn(void);	// show the help dialog
std::vector<std::wstring> _make_human_readable(std::vector<std::wstring> vwsValidationErrors);

INT_PTR CALLBACK ciDlgCUValidationProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
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
			g_IsDarkMode = (bool)::SendMessage(nppData._nppHandle, NPPM_ISDARKMODEENABLED, 0, 0);

			////////
			// setup all the controls before triggering dark mode
			////////

			// need to instantiate the object
			if (!g_pConfVal) g_pConfVal = new ConfigValidator(nppData);

			// and store the file combobox and error listbox handles
			s_hwFileCbx = GetDlgItem(g_hwndCUValidationDlg, IDC_CU_VALIDATION_FILE_CBX);
			s_hwErrLbx = GetDlgItem(g_hwndCUValidationDlg, IDC_CU_VALIDATION_ERROR_LB);
			s_hwModelBtn = GetDlgItem(g_hwndCUValidationDlg, IDC_CU_VALIDATION_MODEL_BTN);

			// Iterate thru the XML Names to populate the combobox
			ComboBox_ResetContent(s_hwFileCbx);
			for (auto xmlName : g_pConfVal->getXmlNames()) {
				ComboBox_AddString(s_hwFileCbx, xmlName.c_str());
			}

			// Make sure Error listbox starts empty
			ListBox_ResetContent(s_hwErrLbx);

			// Start with the *.model.xml button disabled
			Button_Enable(s_hwModelBtn, false);

			////////
			// trigger darkmode
			////////
			if (g_IsDarkMode && versionNpp >= versionDarkDialog) {
				::SendMessage(nppData._nppHandle, NPPM_DARKMODESUBCLASSANDTHEME, static_cast<WPARAM>(NppDarkMode::dmfInit), reinterpret_cast<LPARAM>(g_hwndCUValidationDlg));
			}

			// Do my subclassing after N++ subclassing... I don't know whether they will conflict:
			SetWindowSubclass(s_hwErrLbx, _ListBoxSubclassProc, _uniqueSubclassID, 0);

			// Need to tell N++ that I want to handle shortcut keys myself
			::SendMessage(nppData._nppHandle, NPPM_MODELESSDIALOG, MODELESSDIALOGADD, reinterpret_cast<LPARAM>(hwndDlg));

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
					_pushed_validate_btn(s_hwFileCbx, s_hwErrLbx, g_pConfVal);
					return true;
				}
				case IDC_CU_VALIDATION_MODEL_BTN:
				{
					_pushed_model_btn(s_hwFileCbx, s_hwErrLbx, s_hwModelBtn, s_wsModelName, g_pConfVal);
				}
				case IDC_CU_VALIDATION_ERROR_LB:
				{
					switch (HIWORD(wParam))
					{
						case LBN_DBLCLK:
						{
							_dblclk_errorlbx_entry(s_hwFileCbx, s_hwErrLbx, s_hwModelBtn, g_pConfVal);
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
							s_wsModelName = _changed_filecbx_entry(s_hwFileCbx, s_hwErrLbx, s_hwModelBtn, g_pConfVal);
							return true;
						}
						default:
							return false;
					}
				}
				case IDC_CU_VALIDATION_HELP_BTN:
				{
					_pushed_help_btn();
					return false;
				}
				default:
				{
					return true;
				}
			}
		}
		case WM_SIZE:
		{
			// desired height and width
			int newWidth = LOWORD(lParam);
			int newHeight = HIWORD(lParam);

			// update error list box size
			RECT rectListBox;
			GetWindowRect(s_hwErrLbx, &rectListBox);
			MapWindowPoints(HWND_DESKTOP, hwndDlg, reinterpret_cast<LPPOINT>(&rectListBox), 2);
			int new_w = newWidth - 30 - 10;		// 30 is gaps/border, 10 is extra
			int new_h = newHeight - 70 - 45;	// 70=gaps+button, 45 is extra
			int new_x = rectListBox.left;
			int new_y = rectListBox.top;
			MoveWindow(s_hwErrLbx, new_x, new_y, new_w, new_h, TRUE);

			// update file cbox size
			RECT rectFileCbx;
			GetWindowRect(s_hwFileCbx, &rectFileCbx);
			MapWindowPoints(HWND_DESKTOP, hwndDlg, reinterpret_cast<LPPOINT>(&rectFileCbx), 2);
			new_x = rectFileCbx.left;
			new_y = rectFileCbx.top;
			new_w = newWidth - 30 - 10 - 25 - 18;		// 25 less than error listbox, plus extra
			new_h = rectFileCbx.bottom - rectFileCbx.top;
			MoveWindow(s_hwFileCbx, new_x, new_y, new_w, new_h, TRUE);

			// update Validation button
			RECT rectValidationBtn;
			HWND hwValidationBtn = GetDlgItem(g_hwndCUValidationDlg, IDC_CU_VALIDATION_BTN);
			GetWindowRect(hwValidationBtn, &rectValidationBtn);
			MapWindowPoints(HWND_DESKTOP, hwndDlg, reinterpret_cast<LPPOINT>(&rectValidationBtn), 2);
			new_x = rectValidationBtn.left;
			new_y = newHeight - 50 - 11;	// 11 extra buffer
			new_w = rectValidationBtn.right - rectValidationBtn.left;
			new_h = rectValidationBtn.bottom - rectValidationBtn.top;
			MoveWindow(hwValidationBtn, new_x, new_y, new_w, new_h, TRUE);

			// update Model button
			RECT rectModelBtn;
			GetWindowRect(s_hwModelBtn, &rectModelBtn);
			MapWindowPoints(HWND_DESKTOP, hwndDlg, reinterpret_cast<LPPOINT>(&rectModelBtn), 2);
			new_x = rectModelBtn.left;
			new_y = newHeight - 50 - 11;	// 11 extra buffer
			new_w = rectModelBtn.right - rectModelBtn.left;
			new_h = rectModelBtn.bottom - rectModelBtn.top;
			MoveWindow(s_hwModelBtn, new_x, new_y, new_w, new_h, TRUE);

			// update DONE(Cancel) button
			RECT rectDoneBtn;
			HWND hwDoneBtn = GetDlgItem(g_hwndCUValidationDlg, IDCANCEL);
			GetWindowRect(hwDoneBtn, &rectDoneBtn);
			MapWindowPoints(HWND_DESKTOP, hwndDlg, reinterpret_cast<LPPOINT>(&rectDoneBtn), 2);
			new_x = newWidth - 60 - 10 - 55;	// w=60, gap=10, unknown=55
			new_y = newHeight - 50 - 11;	// 11 extra buffer
			new_w = rectDoneBtn.right - rectDoneBtn.left;
			new_h = rectDoneBtn.bottom - rectDoneBtn.top;
			MoveWindow(hwDoneBtn, new_x, new_y, new_w, new_h, TRUE);

			return false;
		}
		case WM_GETMINMAXINFO:
		{
			LPMINMAXINFO lpMMI = (LPMINMAXINFO)lParam;
			// Set the minimum tracking size (the smallest size the user can drag the window to)
			lpMMI->ptMinTrackSize.x = 330 + 250; // Minimum width: don't know why using actual window dimensions (330) is not enough
			lpMMI->ptMinTrackSize.y = 205 + 195; // Minimum height: don't know why using actual window dimensions (205) is not enough
			return false;
		}
		case WM_DESTROY:
		{
			if (g_pConfVal) {
				delete g_pConfVal;
				g_pConfVal = nullptr;
			}
			s_hwFileCbx = nullptr;
			s_hwErrLbx = nullptr;

			::SendMessage(nppData._nppHandle, NPPM_MODELESSDIALOG, MODELESSDIALOGREMOVE, reinterpret_cast<LPARAM>(hwndDlg));

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
		pConfVal->vwsErrorHumanReadable.clear();
		pConfVal->vwsErrorContexts.clear();
	}
	else {
		pConfVal->vlErrorLinenums = oValidator.vlGetMultiLinenums();
		pConfVal->vwsErrorReasons = oValidator.vwsGetMultiReasons();
		pConfVal->vwsErrorHumanReadable = _make_human_readable(pConfVal->vwsErrorReasons);
		pConfVal->vwsErrorContexts = oValidator.vwsGetMultiContexts();
		std::wstring longestText = L"";
		size_t nErrors = oValidator.szGetMultiNumErrors();
		for (size_t e = 0; e < nErrors; e++) {
			std::wstring msg = std::wstring(L"#") + std::to_wstring(pConfVal->vlErrorLinenums[e]) + L": ";
			if (pConfVal->vwsErrorHumanReadable[e].size()) {
				msg += pConfVal->vwsErrorHumanReadable[e] + L" (" + pConfVal->vwsErrorReasons[e] + L")";
			}
			else {
				msg += pConfVal->vwsErrorReasons[e];
			}
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
			sLocalSearch = "<LexerType [^>]*name=\"[^\"]*\"";
			sModelSearch = "<LexerType [^>]*name=";
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
			sLocalSearch = "<Language [^>]*name=\"[^\"]*\"";
			sModelSearch = "<Language [^>]*name=";
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
			WPARAM iPosEndOfModel = ::SendMessage(hwModelSci, SCI_GETLENGTH, 0, 0);
			::SendMessage(hwModelSci, SCI_SETTARGETSTART, 0, 0);
			::SendMessage(hwModelSci, SCI_SETTARGETEND, iPosEndOfModel, 0);
			::SendMessage(hwModelSci, SCI_SETSEARCHFLAGS, SCFIND_REGEXP, 0);
			LRESULT iModelResultPos = ::SendMessageA(hwModelSci, SCI_SEARCHINTARGET, sModelSearch.size(), reinterpret_cast<LPARAM>(sModelSearch.c_str()));
			::SendMessage(hwModelSci, SCI_GOTOPOS, iModelResultPos, 0);
			if (deltaLines) {
				LRESULT iElementLine = ::SendMessage(hwModelSci, SCI_LINEFROMPOSITION, iModelResultPos, 0);		// figure out what model line the element starts at
				LRESULT iVisibleLine = ::SendMessage(hwModelSci, SCI_VISIBLEFROMDOCLINE, iElementLine, 0);		// convert from actual line number to "visible" line number (for wrapping/folding)
				::SendMessage(hwModelSci, SCI_SETFIRSTVISIBLELINE, iVisibleLine, 0);							// set that element to the top of the model file, too
				WPARAM iEstimatedModelLine = iElementLine + deltaLines - 1;										// move the model's caret the same number of lines down from
				::SendMessage(hwModelSci, SCI_GOTOLINE, iEstimatedModelLine, 0);								// <do the move>
			}
		}
		else {
			::SendMessage(hwModelSci, SCI_GOTOPOS, 0, 0);
		}

	}

}

static LRESULT CALLBACK _ListBoxSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR /*dwRefData*/)
{
	// Check for the universal Ctrl+C signal (works on all keyboard layouts)
	if (uMsg == WM_CHAR) {
		if (wParam == 0x03) {
			_copy_current_errlbx_entry(hWnd);
			//::MessageBox(hWnd, L"Got Ctrl+C", L"Got Ctrl+C", MB_OK); // Your function to copy text
			return 0; // Event handled
		}
	}

	// When the control is destroyed, it automatically removes this subclass
	if (uMsg == WM_NCDESTROY) {
		RemoveWindowSubclass(hWnd, _ListBoxSubclassProc, uIdSubclass);
	}

	// For all other messages, just pass them to the default handler
	return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

// copy current selected errorbox entry into clipboard (and return the value for no good reason)
//		(unfortunately, since I cannot pass the pConfVal into the subclass proc, I cannot pass it from there into here as a paramter, so I had to change it to file global)
void _copy_current_errlbx_entry(HWND hwErrorList)
{
	std::wstring msg = L"";
	bool needFree = false;
	HGLOBAL hGlobal = nullptr;

	if (!g_pConfVal->vlErrorLinenums.size()) return;		// nothing to copy if there are no errors

	LRESULT lbCurSel = ::SendMessage(hwErrorList, LB_GETCURSEL, 0, 0);
	if (LB_ERR == lbCurSel) return;

	if (!OpenClipboard(hwErrorList)) return;
	if (!EmptyClipboard()) goto cceeCloseAndExit;

	msg = std::wstring(L"#") + std::to_wstring(g_pConfVal->vlErrorLinenums[lbCurSel]) + L": ";
	if (g_pConfVal->vwsErrorHumanReadable[lbCurSel].size()) {
		msg += g_pConfVal->vwsErrorHumanReadable[lbCurSel] + L" (" + g_pConfVal->vwsErrorReasons[lbCurSel] + L")";
	} else 
	{
		msg += g_pConfVal->vwsErrorReasons[lbCurSel];
	}
	msg += g_pConfVal->vwsErrorContexts[lbCurSel];

	hGlobal = GlobalAlloc(GMEM_MOVEABLE, (msg.size() + 1) * sizeof(wchar_t));
	if (!hGlobal) goto cceeCloseAndExit;
	needFree = true;	// if something goes wrong after hGlobal has been allocated, need to free it

	wchar_t* buffer = static_cast<wchar_t*>(GlobalLock(hGlobal));
	if (!buffer) goto cceeCloseAndExit;

	memcpy(buffer, msg.c_str(), msg.size() * sizeof(wchar_t));
	buffer[msg.size()] = L'\0';	// null-terminate the string

	GlobalUnlock(hGlobal);

	if (SetClipboardData(CF_UNICODETEXT, hGlobal)) {
		needFree = false;	// clipboard will take control of it, so I don't need to free it
	}
	
cceeCloseAndExit:
	if (needFree) GlobalFree(hGlobal);

	CloseClipboard();
	return;
}

// takes common errors and gives a human-readable version
std::vector<std::wstring> _make_human_readable(std::vector<std::wstring> vwsValidationErrors)
{
	std::vector<std::wstring> retvals;
	for (auto vwsOrig : vwsValidationErrors) {
		std::wstring vwsNew = L"";
		if (vwsOrig.find(L"unique-WordsStyle-styleID") != std::wstring::npos) {
			vwsNew = L"Style ID must be unique, but found duplicate.";
		}
		else if (vwsOrig.find(L"violates enumeration constraint") != std::wstring::npos) {
			if (vwsOrig.find(L"keywordClass") != std::wstring::npos) {
				vwsNew = L"keywordClass attribute must have a valid value.";
			}
			else {
				vwsNew = L"Specified attribute must have a valid value.";
			}
		}
		retvals.push_back(vwsNew);
	}
	return retvals;
}

// show the help dialog
void _pushed_help_btn(void)
{
	buttoncall_ValidationHelpDlg();
}

// validation help-dialog callback
INT_PTR CALLBACK ciDlgCUValHelpProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	static std::wstring wsHelpText(L"");
	if (wsHelpText == L"") {
		wsHelpText += L"- Pick one of the stylers, themes, or langs.xml in the File selector.\r\n";
		wsHelpText += L"- When you VALIDATE, any errors will be listed in the box below the filename.\r\n";
		wsHelpText += L"  - It will list the line number in the file, along with the error message\r\n";
		wsHelpText += L"  - You can scroll and/or resize to see more of the text\r\n";
		wsHelpText += L"- Double-click a line to navigate to that error in the XML file.  You can edit and save the file in Notepad++ while the dialog is still open, and use VALIDATE again to see if that error has been fixed.\r\n";
		wsHelpText += L"- If you are uncertain about what the fix should look at, you can choose GO TO MODEL to try to line up the appropriate *.model.xml in the same region as the error in your original file.  You can use the example in the model to help determine how your XML should be modified.\r\n";
		wsHelpText += L"- If you select a different error (even without double-clicking it), the GO TO MODEL will bring you to that error in the original file and line it up with the appropriate section of the model file.\r\n\r\n";
		wsHelpText += L"The most common errors:\r\n";
		wsHelpText += L"- Style ID must be unique: no LexerType can contain two WordsStyle lines with the same Style ID.  If your XML does, you will need to compare to the model to see what the right Style ID is for each WordsStyle.\r\n";
		wsHelpText += L"- keywordClass must have a valid value: there are only a handful of valid values for keywordClass (all listed in the error message).  Compare to the model to see whether this WordsStyle should have a keywordClasss, and if so, which value it should be.\r\n";
		wsHelpText += L"  - Note: 'keywordClass=\"\"' is not valid; if a WordsStyle doesn't have an associated keywordClass, just delete the attribute completely.\r\n";
	}

	switch (uMsg) {
		case WM_INITDIALOG:
		{
			// populate with help text:
			HWND hEdit = ::GetDlgItem(hwndDlg, IDC_CUVH_BIGTEXT);
			::SetWindowText(hEdit, wsHelpText.c_str());

			// store hwnd
			g_hwndCUHlpDlg = hwndDlg;

			// determine dark mode
			g_IsDarkMode = (bool)::SendMessage(nppData._nppHandle, NPPM_ISDARKMODEENABLED, 0, 0);
			if (g_IsDarkMode) {
				::SendMessage(nppData._nppHandle, NPPM_DARKMODESUBCLASSANDTHEME, static_cast<WPARAM>(NppDarkMode::dmfInit), reinterpret_cast<LPARAM>(g_hwndCUHlpDlg));
			}

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
			rc.left -= static_cast<int>(lParam);
			rc.left -= static_cast<int>(wParam);
			rc.left += static_cast<int>(lParam);
			rc.left += static_cast<int>(wParam);

			// and position dialog
			RECT dlgRect;
			::GetClientRect(hwndDlg, &dlgRect);
			int x = center.x - (dlgRect.right - dlgRect.left) / 2;
			int y = center.y - (dlgRect.bottom - dlgRect.top) / 2;
			::SetWindowPos(hwndDlg, HWND_TOP, x, y, (dlgRect.right - dlgRect.left), (dlgRect.bottom - dlgRect.top), SWP_SHOWWINDOW);
		}

		return true;
		case WM_COMMAND:
			switch (LOWORD(wParam))
			{
				case IDCANCEL:
				case IDOK:
					EndDialog(hwndDlg, 0);
					DestroyWindow(hwndDlg);
					return true;
			}
			return false;
		case WM_CLOSE:
			EndDialog(hwndDlg, 0);
			DestroyWindow(hwndDlg);
			return true;
		case WM_SIZE:
		{
			// TODO: resize the textbox, then return FALSE
			return true;
		}
		default:
			return false;
	}
}
