#pragma once

#include "Error.hpp"
#include <string>
#include <vector>
#include <list>

class PackageManager
{
public:

	static std::wstring GetStringFromResource(const std::wstring id);
	static std::wstring GetStringFileInfo(const std::wstring& subName);
	static std::vector<uint8_t> GetBinaryResource(const std::wstring& subName);
	static std::list<std::vector<uint8_t>> GetAllBinaryResources(const std::wstring& id);
	static Error UnpackZipResource(const std::wstring& destDir);
};