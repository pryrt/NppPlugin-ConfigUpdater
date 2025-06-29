#include "pcjHelper.h"
#include <Windows.h>

namespace pcjHelper
{
	// delete null characters from padded wstrings
	std::wstring delNull(std::wstring& str)
	{
		const auto pos = str.find(L'\0');
		if (pos != std::wstring::npos) {
			str.erase(pos);
		}
		return str;
	};

	// delete null characters from padded wstrings
	std::string delNull(std::string& str)
	{
		const auto pos = str.find('\0');
		if (pos != std::string::npos) {
			str.erase(pos);
		}
		return str;
	};

	// convert wstring to UTF8-encoded bytes in std::string
	std::string wstring_to_utf8(std::wstring& wstr)
	{
		if (wstr.empty()) return std::string();
		int szNeeded = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), static_cast<int>(wstr.size()), NULL, 0, NULL, NULL);
		if (szNeeded == 0) return std::string();
		std::string str(szNeeded, '\0');
		int result = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), static_cast<int>(wstr.size()), const_cast<LPSTR>(str.data()), szNeeded, NULL, NULL);
		if (result == 0) return std::string();
		return str;
	}

	// convert UTF8-encoded bytes in std::string to std::wstring
	std::wstring utf8_to_wstring(std::string& str)
	{
		if (str.empty()) return std::wstring();
		int szNeeded = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), static_cast<int>(str.size()), NULL, 0);
		if (szNeeded == 0) return std::wstring();
		std::wstring wstr(szNeeded, L'\0');
		int result = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), static_cast<int>(str.size()), const_cast<LPWSTR>(wstr.data()), szNeeded);
		if (result == 0) return std::wstring();
		return wstr;
	}

};
