#pragma once

#include "Error.hpp"
#include <vector>
#include <string>

namespace zip_archive
{

Error UnpackToFolder(uint8_t* pZipContent, size_t size, const std::wstring& destPath);

}