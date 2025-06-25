#pragma once
#include <string>
#include <comutil.h>
#include <comdef.h>
#pragma warning(push)
#pragma warning(disable: 4192)
// import DLL without ISequentialStream/_FILETIME exclusion warnings
#import <msxml6.dll>
#pragma warning(pop)

// uses MSXML6 to validate XML based on XSD
class ValidateXML {
public:
	// Constructor
	// on successful validation, set _ws_validation_message=L"OK" and _isValid to true
	// on validation fail, set _ws_validation_message as appropriate and _isValid to false
	ValidateXML(std::wstring wsXmlFileName, std::wstring wsXsdFileName)
	{
		_err_line_num = static_cast<UINT64>(-1);								// by default, no specific line number
		HRESULT hr = CoInitialize(NULL);
		if (FAILED(hr)) {
			_isValid = _setstatus_validation_failed(L"CoInitialize failed", hr);
			goto ValidationCleanUp;
		}

		try {
			// Create the XML DOM document		:: Don't populate it yet; need to have schema prepared first
			MSXML2::IXMLDOMDocument2Ptr xmlDoc;
			hr = xmlDoc.CreateInstance(__uuidof(MSXML2::DOMDocument60));
			if (FAILED(hr)) {
				_isValid = _setstatus_validation_failed(L"Failed to create XML DOM document", hr);
				goto ValidationCleanUp;
			}

			////////
			// Prepare the schema, so that when I load contents of XML, it can validate-on-load
			//		(thus hopefully keeping line number information)
			////////

			// Create the XML schema cache
			MSXML2::IXMLDOMSchemaCollectionPtr schemaCache;
			hr = schemaCache.CreateInstance(__uuidof(MSXML2::XMLSchemaCache60));
			if (FAILED(hr)) {
				_isValid = _setstatus_validation_failed(L"Failed to create XML schema cache", hr);
				goto ValidationCleanUp;
			}

			// Add the XSD schema to the cache
			_bstr_t xsdFile = wsXsdFileName.c_str(); // L"input.xsd";
			schemaCache->add(L"", xsdFile);

			// Associate the schema cache with the XML document
			//      many online examples show xmlDoc->schemas = schemaCache; -- which gave me error.
			//      but old MSDN example <https://learn.microsoft.com/en-us/previous-versions/windows/desktop/ms762787(v=vs.85)> showed using the method, which works
			xmlDoc->schemas = schemaCache.GetInterfacePtr();


			// Set async to false to ensure loading is synchronous; validateOnParse=true will allow errors when ->load() is called
			xmlDoc->async = VARIANT_FALSE;
			xmlDoc->validateOnParse = VARIANT_TRUE;
			xmlDoc->resolveExternals = VARIANT_TRUE;

			////////
			// Load the XML file and simultaneously validate due to the options set above
			////////

			// Load the XML file
			_bstr_t xmlFile = wsXmlFileName.c_str();
			VARIANT_BOOL isLoaded = xmlDoc->load(xmlFile);

			// check validation
			MSXML2::IXMLDOMParseErrorPtr validationError = xmlDoc->parseError;
			if ((isLoaded != VARIANT_TRUE) || (validationError->errorCode != 0)) {
				std::string msg = std::string(validationError->reason) + "\nLine#" + std::to_string(validationError->line) + ":\n" + std::string(validationError->srcText);
				_isValid = _setstatus_validation_failed(L"XML validation FAILED", msg, static_cast<UINT64>(validationError->line));
				goto ValidationCleanUp;
			}
			else {
				_isValid = _setstatus_validation_passed(L"XML validation PASSED.");
				goto ValidationCleanUp;
			}
		}
		catch (_com_error& e) {
			_isValid = _setstatus_validation_failed(L"COM error", e.ErrorMessage());
			goto ValidationCleanUp;
		}

	ValidationCleanUp:
		CoUninitialize();
	}

	// determine validation status
	bool isValid(void) { return _isValid; }

	// get the results in various varieties of strings
	std::wstring wsGetValidationMessage(void) { return _ws_validation_message; }
	std::string sGetValidationMessage(void) { return _wstring_to_utf8(_ws_validation_message); }
	UINT64 uGetValidationLineNum(void) { return _err_line_num; }


private:
	bool _isValid;
	std::wstring _ws_validation_message;
	UINT64 _err_line_num;

	// delete null characters from padded strings				// private function (invisible to outside world)
	void _delNull(std::wstring& str)
	{
		const auto pos = str.find(L'\0');
		if (pos != std::wstring::npos) {
			str.erase(pos);
		}
	};
	void _delNull(std::string& str)
	{
		const auto pos = str.find('\0');
		if (pos != std::string::npos) {
			str.erase(pos);
		}
	};

	// convert wstring to UTF8-encoded bytes in std::string		// private function (invisible to outside world)
	std::string _wstring_to_utf8(std::wstring& wstr)
	{
		if (wstr.empty()) return std::string();
		int szNeeded = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), static_cast<int>(wstr.size()), NULL, 0, NULL, NULL);
		if (szNeeded == 0) return std::string();
		std::string str(szNeeded, '\0');
		int result = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), static_cast<int>(wstr.size()), const_cast<LPSTR>(str.data()), szNeeded, NULL, NULL);
		if (result == 0) return std::string();
		if (result < szNeeded) _delNull(str);
		return str;
	}

	// convert UTF8-encoded bytes in std::string to wstring		// private function (invisible to outside world)
	std::wstring _utf8_to_wstring(std::string& str)
	{
		if (str.empty()) return std::wstring();
		int szNeeded = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), static_cast<int>(str.size()), NULL, 0);
		if (szNeeded == 0) return std::wstring();
		std::wstring wstr(szNeeded, L'\0');
		int result = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), static_cast<int>(str.size()), const_cast<LPWSTR>(wstr.data()), szNeeded);
		if (result == 0) return std::wstring();
		if (result < szNeeded) _delNull(wstr);
		return wstr;
	}


	bool _setstatus_validation_failed(std::wstring wsPrefix, HRESULT hr)
	{
		_ws_validation_message = wsPrefix + L": COM HRESULT#" + std::to_wstring(hr);
		_err_line_num = static_cast<UINT64>(-1);
		return false;
	}
	bool _setstatus_validation_failed(std::wstring wsPrefix, std::wstring wsDetails, UINT64 errLineNum = -1)
	{
		_ws_validation_message = wsPrefix + L": " + wsDetails;
		_err_line_num = errLineNum;
		return false;
	}
	bool _setstatus_validation_failed(std::wstring wsPrefix, std::string sDetails, UINT64 errLineNum = -1)
	{
		return _setstatus_validation_failed(wsPrefix, _utf8_to_wstring(sDetails), errLineNum);
	}
	bool _setstatus_validation_passed(std::wstring wsMessage)
	{
		_ws_validation_message = wsMessage;
		_err_line_num = static_cast<UINT64>(-1);
		return true;
	}

};
