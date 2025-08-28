#pragma once
#include <string>
#include <Windows.h>

namespace PopulateXSD {
	void WriteThemeXSD(std::wstring wsPath);	// write the contents of the Theme XSD
	void WriteLangsXSD(std::wstring wsPath);	// write the contents of the Langs XSD
	BOOL RecursiveCreateDirectory(std::wstring wsPath); // recursively create all levels necessary for a given directory
};
