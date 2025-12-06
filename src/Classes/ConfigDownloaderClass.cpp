#include "ConfigDownloaderClass.h"
#include "pcjHelper.h"
#include "NppMetaClass.h"

std::wstring _dl_errmsg = L"";

ConfigDownloader::ConfigDownloader(void)
{
	gNppMetaInfo.populate();	// find the directories to populate the full paths for the model files
	_wsPath.AppDir = gNppMetaInfo.dir.app;
	_wsPath.LangsModel = gNppMetaInfo.dir.app + L"\\langs.model.xml";
	_wsPath.StylersModel = gNppMetaInfo.dir.app + L"\\stylers.model.xml";

	// init the URLs:
	_wsURL.LangsModel = L"https://raw.githubusercontent.com/notepad-plus-plus/notepad-plus-plus/refs/heads/master/PowerEditor/src/langs.model.xml";
	_wsURL.StylersModel = L"https://raw.githubusercontent.com/notepad-plus-plus/notepad-plus-plus/refs/heads/master/PowerEditor/src/stylers.model.xml";
}

bool ConfigDownloader::go(void)
{
	cu_dl_model_SetProgress(10);

	// Get a temp directory for downloading into
	std::wstring wsTempDir = getWritableTempDir();
	std::wstring wsTempLangs = wsTempDir + L"\\ConfigDownloader.langs.model.xml";
	std::wstring wsTempStylers = wsTempDir + L"\\ConfigDownloader.stylers.model.xml";

	// exit on interruption (DONE/X/CANCEL)
	if (cu_dl_model_GetInterruptFlag()) return false;

	// download the langs.model.xml
	bool langs_ok = downloadFileToDisk(_wsURL.LangsModel, wsTempLangs);
	std::wstring msg = (langs_ok)
		? L"Downloaded langs.model.xml to temporary file...\r\n"
		: (L"ERROR downloading langs.model.xml:\r\n    " + _dl_errmsg + L" =>\r\n    " + _wsURL.LangsModel + L"\r\n");
	cu_dl_model_SetProgress(50);
	cu_dl_model_AppendText(const_cast<LPWSTR>(msg.data()));

	// exit on interruption (DONE/X/CANCEL)
	if (cu_dl_model_GetInterruptFlag()) return false;

	// download the stylers.model.xml
	bool stylers_ok = downloadFileToDisk(_wsURL.StylersModel, wsTempStylers);
	msg = (stylers_ok)
		? L"Downloaded stylers.model.xml to temporary file...\r\n"
		: (L"ERRPR downloading stylers.model.xml:\r\n    " + _dl_errmsg + L" =>\r\n    " + _wsURL.StylersModel + L"\r\n");
	cu_dl_model_SetProgress(90);
	cu_dl_model_AppendText(const_cast<LPWSTR>(msg.data()));

	// exit on interruption (DONE/X/CANCEL)
	if (cu_dl_model_GetInterruptFlag()) return false;

	// prepare the copy command
	LPCWSTR verb = pcjHelper::is_dir_writable(_wsPath.AppDir) ? nullptr : L"runas";	// want to be able to be NULL, so leave this as LPCWSTR
	std::wstring cmd = L"cmd.exe";
	std::wstring args = L"/C ";	// start with uninitialized number of copies

	// decide which files to move
	if (langs_ok) args += L"MOVE /Y \"" + wsTempLangs + L"\" \"" + _wsPath.LangsModel + L"\" & ";
	if (stylers_ok) args += L"MOVE /Y \"" + wsTempStylers + L"\" \"" + _wsPath.StylersModel + L"\" & ";

	// exit on interruption (DONE/X/CANCEL)
	if (cu_dl_model_GetInterruptFlag()) return false;

	// run the move, with the right verb
	if (langs_ok || stylers_ok) {
		ShellExecute(gNppMetaInfo.hwnd._nppHandle, verb, cmd.c_str(), args.c_str(), NULL, SW_SHOWMINIMIZED);
	}

	// DONE
	cu_dl_model_SetProgress(100);
	cu_dl_model_AppendText(L"DONE");

	// Automatically close the window on success
	if (langs_ok && stylers_ok) {
		Sleep(500);
		cu_dl_model_CloseWindow();
	}
	return true;
}

bool ConfigDownloader::ask_overwrite_if_exists(const std::wstring& path)
{
	if (!PathFileExists(path.c_str())) return true;	// if file doesn't exist, it's okay to "overwrite" nothing ;-)
	std::wstring msg = L"The path\r\n" + path + L"\r\nalready exists.  Should I overwrite it?";
	int ans = ::MessageBox(gNppMetaInfo.hwnd._nppHandle, msg.c_str(), L"Overwrite File?", MB_YESNO);
	return ans == IDYES;
}

bool ConfigDownloader::downloadFileToDisk(const std::wstring& url, const std::wstring& path)
{
	_dl_errmsg = L"";

	// first expand any variables in the the path
	DWORD dwExSize = 2 * static_cast<DWORD>(path.size());
	std::wstring expandedPath(dwExSize, L'\0');
	if (!ExpandEnvironmentStrings(path.c_str(), const_cast<LPWSTR>(expandedPath.data()), dwExSize)) {
		_dl_errmsg = L"Could not understand the path \"" + path + L"\": " + std::to_wstring(GetLastError());
		return false;
	}
	pcjHelper::delNull(expandedPath);

	// don't download and overwrite the file if it already exists
	if (!ask_overwrite_if_exists(expandedPath)) {
		_dl_errmsg = L"<not overwriting \"" + path + L"\">";
		return false;
	}

	// now that I know it's safe to write the file, try opening the internet connection
	HINTERNET hInternet = InternetOpen(L"ConfigUpdaterPluginForN++", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
	if (hInternet == NULL) {
		_dl_errmsg = L"Could not connect to internet when trying to download\r\n" + url;
		return false;
	}

	HINTERNET hConnect = InternetOpenUrl(hInternet, url.c_str(), NULL, 0, INTERNET_FLAG_RELOAD, 0);
	if (hConnect == NULL) {
		_dl_errmsg = L"Could not connect to internet when trying to download\r\n" + url;
		if (hInternet) InternetCloseHandle(hInternet);	// need to free hInternet if hConnect failed
		return false;
	}

	DWORD dwStatusCode = 0, dwSize = sizeof(DWORD);
	if (HttpQueryInfo(hConnect, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, &dwStatusCode, &dwSize, 0)) {
		if (dwStatusCode >= 400) {
			std::wstring wsBuf(256, '\0');
			DWORD buf_len = 256;
			if (HttpQueryInfo(hConnect, HTTP_QUERY_STATUS_TEXT, const_cast<LPWSTR>(wsBuf.data()), &buf_len, 0)) {
				pcjHelper::delNull(wsBuf);
				_dl_errmsg = std::to_wstring(dwStatusCode) + L": " + wsBuf;
			}
			else {
				_dl_errmsg = std::to_wstring(dwStatusCode) + L": <Problem Reading Status Message>";
			}
			return false;
		}
	}
	else {
		_dl_errmsg = L"DOWNLOAD ERROR: COULD NOT READ HTTP STATUS CODE";
		return false;
	}

	HANDLE hFile = CreateFile(expandedPath.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE) {
		DWORD errNum = GetLastError();
		LPWSTR messageBuffer = nullptr;
		FormatMessage(
			FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
			NULL,
			errNum,
			MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
			(LPWSTR)&messageBuffer,
			0,
			NULL
		);

		_dl_errmsg = L"Could not create the file \"" + expandedPath + L"\": " + std::to_wstring(GetLastError()) + L" => " + messageBuffer;

		// need to free hInternet, hConnect, and messageBuffer
		if (hConnect) InternetCloseHandle(hConnect);
		if (hConnect) InternetCloseHandle(hConnect);
		LocalFree(messageBuffer);
		return false;
	}

	std::vector<char> buffer(4096);
	for (DWORD dwBytesRd = 1; dwBytesRd > 0; ) {
		// read a chunk from the webfile
		BOOL stat = InternetReadFile(hConnect, buffer.data(), static_cast<DWORD>(buffer.size()), &dwBytesRd);
		if (!stat) {
			_dl_errmsg = L"Error reading from URL\"" + url + L"\": " + std::to_wstring(GetLastError());

			// free handles before returning
			if (hConnect) InternetCloseHandle(hConnect);
			if (hInternet) InternetCloseHandle(hInternet);
			if (hFile) CloseHandle(hFile);
			return false;
		}

		// don't need to write if no bytes were read (ie, EOF)
		if (!dwBytesRd) break;

		// write the chunk to the path
		DWORD dwBytesWr = 0;
		stat = WriteFile(hFile, buffer.data(), dwBytesRd, &dwBytesWr, NULL);
		if (!stat) {
			_dl_errmsg = L"Error writing to \"" + expandedPath + L"\": " + std::to_wstring(GetLastError());

			// free handles before returning
			if (hConnect) InternetCloseHandle(hConnect);
			if (hInternet) InternetCloseHandle(hInternet);
			if (hFile) CloseHandle(hFile);
			return false;
		}
	}

	if (hConnect) InternetCloseHandle(hConnect);
	if (hInternet) InternetCloseHandle(hInternet);
	if (hFile) CloseHandle(hFile);

	return true;
}

std::wstring ConfigDownloader::getWritableTempDir(void)
{
	// first try the system TEMP
	std::wstring tempDir(MAX_PATH + 1, L'\0');
	GetTempPath(MAX_PATH + 1, const_cast<LPWSTR>(tempDir.data()));
	pcjHelper::delNull(tempDir);

	// if that fails, try c:\tmp or c:\temp
	if (!pcjHelper::is_dir_writable(tempDir)) {
		tempDir = L"c:\\temp";
		pcjHelper::delNull(tempDir);
	}
	if (!pcjHelper::is_dir_writable(tempDir)) {
		tempDir = L"c:\\tmp";
		pcjHelper::delNull(tempDir);
	}

	// if that fails, try the %USERPROFILE%
	if (!pcjHelper::is_dir_writable(tempDir)) {
		tempDir.resize(MAX_PATH + 1);
		if (!ExpandEnvironmentStrings(L"%USERPROFILE%", const_cast<LPWSTR>(tempDir.data()), MAX_PATH + 1)) {
			std::wstring errmsg = L"getWritableTempDir::ExpandEnvirontmentStrings(%USERPROFILE%) failed: " + std::to_wstring(GetLastError()) + L"\n";
			::MessageBox(NULL, errmsg.c_str(), L"Directory Error", MB_ICONERROR);
			return L"";
		}
		pcjHelper::delNull(tempDir);
	}

	// last try: current directory
	if (!pcjHelper::is_dir_writable(tempDir)) {
		tempDir.resize(MAX_PATH + 1);
		GetCurrentDirectory(MAX_PATH + 1, const_cast<LPWSTR>(tempDir.data()));
		pcjHelper::delNull(tempDir);
	}

	// if that fails, no other ideas
	if (!pcjHelper::is_dir_writable(tempDir)) {
		std::wstring errmsg = L"getWritableTempDir() cannot find any writable directory; please make sure %TEMP% is defined and writable\n";
		::MessageBox(NULL, errmsg.c_str(), L"Directory Error", MB_ICONERROR);
		return L"";
	}
	return tempDir;
}
