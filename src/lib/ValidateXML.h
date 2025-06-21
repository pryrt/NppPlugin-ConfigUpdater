#pragma once
#include <string>
#include <comutil.h>
#include <comdef.h>
#import <msxml6.dll>

// uses MSXML6 to validate XML based on XSD
namespace ValidateXML {

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


	std::wstring _ws_validation_message;
	bool _setstatus_validation_failed(std::wstring wsPrefix, HRESULT hr)
	{
		_ws_validation_message = wsPrefix + L": COM HRESULT#" + std::to_wstring(hr);
		return false;
	}
	bool _setstatus_validation_failed(std::wstring wsPrefix, std::wstring wsDetails)
	{
		_ws_validation_message = wsPrefix + L": " + wsDetails;
		return false;
	}
	bool _setstatus_validation_failed(std::wstring wsPrefix, std::string sDetails)
	{
		return _setstatus_validation_failed(wsPrefix, _utf8_to_wstring(sDetails));
	}
	bool _setstatus_validation_passed(std::wstring wsMessage)
	{
		_ws_validation_message = wsMessage;
		return true;
	}

	// get the results in various varieties of strings
	std::wstring wsGetValidationMessage(void) { return _ws_validation_message; }
	std::string sGetValidationMessage(void) { return _wstring_to_utf8(_ws_validation_message); }

	// on successful validation, set _ws_validation_message=L"OK" and return true
	// on validation fail, set _ws_validation_message as appropriate and return false
	bool validate_xml(std::wstring wsXmlFileName, std::wstring wsXsdFileName)
	{
		bool ret = false;
		HRESULT hr = CoInitialize(NULL);
		if (FAILED(hr)) {
			ret = _setstatus_validation_failed(L"CoInitialize failed", hr);
			goto ValidationCleanUp;
		}

		try {
			// Create the XML DOM document
			MSXML2::IXMLDOMDocument2Ptr xmlDoc;
			hr = xmlDoc.CreateInstance(__uuidof(MSXML2::DOMDocument60));
			if (FAILED(hr)) {
				ret = _setstatus_validation_failed(L"Failed to create XML DOM document", hr);
				goto ValidationCleanUp;
			}

			// Set async to false to ensure loading is synchronous
			xmlDoc->async = VARIANT_FALSE;
			xmlDoc->validateOnParse = VARIANT_FALSE;
			xmlDoc->resolveExternals = VARIANT_TRUE;

			// Load the XML file
			_bstr_t xmlFile = wsXmlFileName.c_str();
			VARIANT_BOOL isLoaded = xmlDoc->load(xmlFile);

			if (isLoaded != VARIANT_TRUE) {
				MSXML2::IXMLDOMParseErrorPtr parseError = xmlDoc->parseError;
				ret = _setstatus_validation_failed(L"Failed to load XML file", (const char*)_bstr_t(parseError->reason));
				goto ValidationCleanUp;
			}

			// Create the XML schema cache
			MSXML2::IXMLDOMSchemaCollectionPtr schemaCache;
			hr = schemaCache.CreateInstance(__uuidof(MSXML2::XMLSchemaCache60));
			if (FAILED(hr)) {
				ret = _setstatus_validation_failed(L"Failed to create XML schema cache", hr);
				goto ValidationCleanUp;
			}

			// Add the XSD schema to the cache
			_bstr_t xsdFile = wsXsdFileName.c_str(); // L"input.xsd";
			schemaCache->add(L"", xsdFile);

			// Associate the schema cache with the XML document
			//      many online examples show xmlDoc->schemas = schemaCache; -- which gave me error.
			//      but old MSDN example <https://learn.microsoft.com/en-us/previous-versions/windows/desktop/ms762787(v=vs.85)> showed using the method, which works
			xmlDoc->schemas = schemaCache.GetInterfacePtr();

			// Validate the XML document
			MSXML2::IXMLDOMParseErrorPtr validationError = xmlDoc->validate();
			if (validationError->errorCode != 0) {
				ret = _setstatus_validation_failed(L"XML validation FAILED", (const char*)_bstr_t(validationError->reason));
				goto ValidationCleanUp;
			}
			else {
				ret = _setstatus_validation_passed(L"XML validation PASSED.");
				goto ValidationCleanUp;
			}
		}
		catch (_com_error& e) {
			ret = _setstatus_validation_failed(L"COM error", e.ErrorMessage());
			goto ValidationCleanUp;
		}

	ValidationCleanUp:
		CoUninitialize();
		return ret;
	}

}
