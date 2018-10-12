#pragma once

#include "Error.hpp"
#include <string>
#include <Windows.h>

inline Error ConvertUtf8ToUtf16(const char* src, size_t srcSize,  std::wstring& dst)
{
	dst.clear();

	const int dstLen = MultiByteToWideChar(CP_UTF8, 0, src, static_cast<int>(srcSize), NULL, 0);
	if (dstLen <= 0)
	{
		return Error(GetLastError());
	}

	dst.resize(dstLen);
	int res = MultiByteToWideChar(CP_UTF8, 0, src, static_cast<int>(srcSize), &dst[0], dstLen);
	if (res <= 0)
	{
		return Error(GetLastError());
	}

	return Error();
}

inline Error ConvertUtf8ToUtf16(const std::string& utf8Src, std::wstring& dst)
{
	dst.clear();

	if (utf8Src.empty())
	{
		return Error();
	}

	return ConvertUtf8ToUtf16(utf8Src.c_str(), utf8Src.size(), dst);
}

inline Error ConvertUtf16ToUtf8(const std::wstring& src, std::string& dst)
{
	dst.clear();
	const int dstLen = WideCharToMultiByte(CP_UTF8, 0, &src[0], static_cast<int>(src.size()), NULL, 0, NULL, NULL);
	if (dstLen <= 0)
	{
		return Error(GetLastError());
	}

	dst.resize(dstLen);
	int res = WideCharToMultiByte(CP_UTF8, 0, &src[0], static_cast<int>(src.size()), &dst[0], dstLen, NULL, NULL);
	if (res <= 0)
	{
		return Error(GetLastError());
	}

	return Error();
}