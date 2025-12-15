#include "PopulateXSD.h"
#include <shlwapi.h>

namespace PopulateXSD {
    // private:
    BOOL RecursiveCreateDirectory(std::wstring wsPath)
    {
        std::wstring wsParent = wsPath;
        PathRemoveFileSpec(const_cast<LPWSTR>(wsParent.data()));
        if (!PathFileExists(wsParent.c_str())) {
            BOOL stat = RecursiveCreateDirectory(wsParent);
            if (!stat) return stat;
        }
        return CreateDirectory(wsPath.c_str(), NULL);
    }

    // private: 
    void _write_file_wrapper(std::wstring wsPath, std::string sContents)
    {
        std::wstring wsDir = wsPath;
        PathRemoveFileSpec(const_cast<LPWSTR>(wsDir.data()));

        if (!PathFileExists(wsDir.c_str())) {
            BOOL stat = RecursiveCreateDirectory(wsDir.c_str());
            if (!stat) return;	// cannot write a file in that directory if directory does not exist
        }

        HANDLE hFile = CreateFile(wsPath.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile == INVALID_HANDLE_VALUE) {
            DWORD errNum = GetLastError();
            LPWSTR messageBuffer = nullptr;
            FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                nullptr, errNum, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPWSTR)&messageBuffer, 0, nullptr);
            std::wstring errmsg = L"Error when trying to create \"" + wsPath + L"\": " + std::to_wstring(errNum) + L":\n" + messageBuffer + L"\n";
            ::MessageBox(NULL, errmsg.c_str(), L"XSD Error", MB_ICONERROR);
            LocalFree(messageBuffer);
            wsPath = L"";
            return;
        }

        DWORD bytesWritten = 0;
        DWORD bytesToWrite = static_cast<DWORD>(sContents.size() * sizeof(sContents[0]));
        BOOL success = WriteFile(hFile, sContents.c_str(), bytesToWrite, &bytesWritten, NULL);
        if (!success) {
            DWORD errNum = GetLastError();
            LPWSTR messageBuffer = nullptr;
            FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                nullptr, errNum, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPWSTR)&messageBuffer, 0, nullptr);
            std::wstring errmsg = L"Error when trying to write to \"" + wsPath + L"\": " + std::to_wstring(errNum) + L":\n" + messageBuffer + L"\n";
            ::MessageBox(NULL, errmsg.c_str(), L"XSD Error", MB_ICONERROR);
            LocalFree(messageBuffer);
            wsPath = L"";
            CloseHandle(hFile);
            return;
        }

        CloseHandle(hFile);
    }


	// write the contents of the Theme XSD
	void WriteThemeXSD(std::wstring wsPath)
	{
        _write_file_wrapper(wsPath, R"myMultiLineXsd(<?xml version="1.0" encoding="UTF-8" ?>
<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema">
  <xs:simpleType name="emptyInt"><!-- custom type: allows integer or empty string -->
    <xs:union>
      <xs:simpleType>
        <xs:restriction base="xs:string">
          <xs:length value="0"/>
        </xs:restriction>
      </xs:simpleType>
      <xs:simpleType>
        <xs:restriction base="xs:integer" />
      </xs:simpleType>
    </xs:union>
  </xs:simpleType>
  <xs:element name="NotepadPlus">
    <xs:complexType>
      <xs:sequence>
        <xs:element name="LexerStyles" maxOccurs="1">
          <xs:complexType>
            <xs:sequence>
              <xs:element name="LexerType" maxOccurs="unbounded">
                <xs:complexType>
                  <xs:sequence>
                    <xs:element name="WordsStyle" maxOccurs="unbounded">
                      <xs:complexType>
                        <xs:simpleContent>
                          <xs:extension base="xs:string">
                            <xs:attribute type="xs:string" name="name" use="required" />
                            <xs:attribute type="xs:integer" name="styleID" use="required" />
                            <xs:attribute type="xs:hexBinary" name="fgColor" use="optional" />
                            <xs:attribute type="xs:hexBinary" name="bgColor" use="optional" />
                            <xs:attribute type="xs:integer" name="colorStyle" use="optional" />
                            <xs:attribute type="xs:string" name="fontName" use="optional" />
                            <xs:attribute type="emptyInt" name="fontSize" use="optional" />
                            <xs:attribute type="emptyInt" name="fontStyle" use="optional" />
                            <xs:attribute name="keywordClass" use="optional">
                              <xs:simpleType>
                                <xs:restriction base="xs:string">
                                  <xs:enumeration value="instre1" />
                                  <xs:enumeration value="instre2" />
                                  <xs:enumeration value="type1" />
                                  <xs:enumeration value="type2" />
                                  <xs:enumeration value="type3" />
                                  <xs:enumeration value="type4" />
                                  <xs:enumeration value="type5" />
                                  <xs:enumeration value="type6" />
                                  <xs:enumeration value="type7" />
                                  <xs:enumeration value="substyle1" />
                                  <xs:enumeration value="substyle2" />
                                  <xs:enumeration value="substyle3" />
                                  <xs:enumeration value="substyle4" />
                                  <xs:enumeration value="substyle5" />
                                  <xs:enumeration value="substyle6" />
                                  <xs:enumeration value="substyle7" />
                                  <xs:enumeration value="substyle8" />
                                </xs:restriction>
                              </xs:simpleType>
                            </xs:attribute>
                          </xs:extension>
                       </xs:simpleContent>
                      </xs:complexType>
                    </xs:element>
                  </xs:sequence>
                  <xs:attribute type="xs:string" name="name" use="required" />
                  <xs:attribute type="xs:string" name="desc" use="required" />
                  <xs:attribute type="xs:string" name="ext" use="required" />
                </xs:complexType>
                <xs:unique name="unique-WordsStyle-styleID">
                  <xs:selector xpath="WordsStyle"/>
                  <xs:field xpath="@styleID" />
                </xs:unique>
              </xs:element>
            </xs:sequence>
          </xs:complexType>
        </xs:element>
        <xs:element name="GlobalStyles" maxOccurs="1">
          <xs:complexType>
            <xs:sequence>
              <xs:element name="WidgetStyle" maxOccurs="unbounded">
                <xs:complexType>
                  <xs:attribute type="xs:string" name="name" use="required" />
                  <xs:attribute type="xs:integer" name="styleID" use="required" />
                  <xs:attribute type="xs:hexBinary" name="fgColor" use="optional" />
                  <xs:attribute type="xs:hexBinary" name="bgColor" use="optional" />
                  <xs:attribute type="xs:integer" name="colorStyle" use="optional" />
                  <xs:attribute type="xs:string" name="fontName" use="optional" />
                  <xs:attribute type="emptyInt" name="fontSize" use="optional" />
                  <xs:attribute type="emptyInt" name="fontStyle" use="optional" />
                </xs:complexType>
              </xs:element>
            </xs:sequence>
          </xs:complexType>
          <xs:unique name="unique-widgetstyle-name">
            <xs:selector xpath="WidgetStyle"/>
            <xs:field xpath="@name" />
          </xs:unique>
        </xs:element>
      </xs:sequence>
      <xs:attribute name="modelDate" type="xs:integer" use="optional"/>
      <xs:attribute name="modelModifTimestamp" type="xs:integer" use="optional"/>
    </xs:complexType>
  </xs:element>
</xs:schema>
)myMultiLineXsd" );

	}

	// write the contents of the Langs XSD
	void WriteLangsXSD(std::wstring wsPath)
	{
        _write_file_wrapper(wsPath, R"myMultiLineXsd(<?xml version="1.0" encoding="utf-8"?>
<xs:schema attributeFormDefault="unqualified" elementFormDefault="qualified" xmlns:xs="http://www.w3.org/2001/XMLSchema">
  <xs:element name="NotepadPlus">
    <xs:complexType>
      <xs:sequence>
        <xs:element name="Languages">
          <xs:complexType>
            <xs:sequence>
              <xs:element maxOccurs="unbounded" name="Language">
                <xs:complexType>
                  <xs:sequence minOccurs="0">
                    <xs:element maxOccurs="unbounded" name="Keywords">
                      <xs:complexType>
                        <xs:simpleContent>
                          <xs:extension base="xs:string">
                            <xs:attribute name="name" use="required">
                              <xs:simpleType>
                                <xs:restriction base="xs:string">
                                  <xs:enumeration value="instre1"/>
                                  <xs:enumeration value="instre2"/>
                                  <xs:enumeration value="type1"/>
                                  <xs:enumeration value="type2"/>
                                  <xs:enumeration value="type3"/>
                                  <xs:enumeration value="type4"/>
                                  <xs:enumeration value="type5"/>
                                  <xs:enumeration value="type6"/>
                                  <xs:enumeration value="type7"/>
                                  <xs:enumeration value="substyle1"/>
                                  <xs:enumeration value="substyle2"/>
                                  <xs:enumeration value="substyle3"/>
                                  <xs:enumeration value="substyle4"/>
                                  <xs:enumeration value="substyle5"/>
                                  <xs:enumeration value="substyle6"/>
                                  <xs:enumeration value="substyle7"/>
                                  <xs:enumeration value="substyle8"/>
                                </xs:restriction>
                              </xs:simpleType>
                            </xs:attribute>
                          </xs:extension>
                        </xs:simpleContent>
                      </xs:complexType>
                    </xs:element>
                  </xs:sequence>
                  <xs:attribute name="name" type="xs:string" use="required"/>
                  <xs:attribute name="ext" type="xs:string" use="required"/>
                  <xs:attribute name="commentLine" type="xs:string" use="optional"/>
                  <xs:attribute name="commentStart" type="xs:string" use="optional"/>
                  <xs:attribute name="commentEnd" type="xs:string" use="optional"/>
                  <xs:attribute name="tabSettings" type="xs:integer" use="optional"/>
                  <xs:attribute name="backspaceUnindent" type="xs:string" use="optional"/>
                </xs:complexType>
                <xs:unique name="unique-keywords-name">
                  <xs:selector xpath="Keywords" />
                  <xs:field xpath="@name" />
                </xs:unique>
              </xs:element>
            </xs:sequence>
          </xs:complexType>
        </xs:element>
      </xs:sequence>
      <xs:attribute name="modelDate" type="xs:integer" use="optional"/>
      <xs:attribute name="modelModifTimestamp" type="xs:integer" use="optional"/>
    </xs:complexType>
  </xs:element>
</xs:schema>
)myMultiLineXsd");
	}
};
