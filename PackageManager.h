#pragma once

#include "Error.hpp"
#include <string>

class PackageManager
{
public:

	static std::wstring GetStringFromResource(const std::wstring id);
	static std::wstring GetStringFileInfo(const std::wstring& subName);
	static Error UnpackZipResource(const std::wstring& destDir);
};