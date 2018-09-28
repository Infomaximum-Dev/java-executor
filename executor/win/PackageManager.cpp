#include "PackageManager.h"
#include "Path.hpp"
#include "ZipArchive.h"
#include"StringConverter.hpp"
#include <Windows.h>

#define DEF_LANG_NEUTRAL	L"0000"
#define DEF_CHARSET_UNICODE	L"04B0"

//https://msdn.microsoft.com/en-us/library/windows/desktop/aa381054(v=vs.85).aspx

namespace
{

std::wstring FindStringFromBinaryResourceEx(HINSTANCE hinst, LPCWSTR id, UINT langId)
{
	std::wstring result;

	HRSRC hrsrc = FindResourceEx(hinst, RT_RCDATA, id, langId);
	if (hrsrc)
	{
		HGLOBAL hglob = LoadResource(hinst, hrsrc);
		if (hglob)
		{
			char* pUtf8Str = reinterpret_cast<char*>(LockResource(hglob));
			if (pUtf8Str)
			{
				DWORD resSize = SizeofResource(NULL, hrsrc);
				ConvertUtf8ToUtf16(pUtf8Str, resSize, result);
				UnlockResource(pUtf8Str);
			}
			FreeResource(hglob);
		}
	}

	return result;
}


LPCWSTR FindStringResourceEx(HINSTANCE hinst, UINT uId, UINT langId)
{
	// Convert the string ID into a bundle number
	LPCWSTR pwsz = NULL;
	HRSRC hrsrc = FindResourceEx(hinst, RT_STRING, MAKEINTRESOURCE(uId / 16 + 1), langId);
	if (hrsrc)
	{
		HGLOBAL hglob = LoadResource(hinst, hrsrc);
		if (hglob)
		{
			pwsz = reinterpret_cast<LPCWSTR>(LockResource(hglob));
			if (pwsz)
			{
				// okay now walk the string table
				for (UINT i = 0; i < (uId & 15); i++)
				{
					pwsz += 1 + (UINT)*pwsz;
				}
				UnlockResource(pwsz);
			}
			FreeResource(hglob);
		}
	}
	return pwsz;
}

struct UnpackParam
{
	Error err;
	std::wstring destDir;
};

BOOL WINAPI UnpackZip(HMODULE hModule, const WCHAR* type, WCHAR* resName, LONG_PTR param)
{
	UnpackParam* unpackParam = (UnpackParam*)param;
	if (!unpackParam->err.Succeeded())
	{
		return FALSE;
	}

	HRSRC hResource = FindResourceW(NULL, resName, type);
	if (hResource == NULL)
	{
		unpackParam->err = Error(GetLastError());
		return FALSE;
	}

	HGLOBAL hFileResource = LoadResource(NULL, hResource);
	if (hFileResource == NULL)
	{
		unpackParam->err = Error(GetLastError());
		return FALSE;
	}

	void* pResFile = LockResource(hFileResource);
	if (pResFile == NULL)
	{
		unpackParam->err = Error(GetLastError());
	}
	else
	{
		DWORD resSize = SizeofResource(NULL, hResource);
		unpackParam->err = zip_archive::UnpackToFolder(static_cast<uint8_t*>(pResFile), resSize, unpackParam->destDir);
		UnlockResource(pResFile);
	}
	FreeResource(hFileResource);

	return unpackParam->err.Succeeded() ? TRUE : FALSE;
}

BOOL WINAPI ExtractBinary(HMODULE hModule, const WCHAR* type, WCHAR* resName, LONG_PTR param)
{
	std::list<std::vector<uint8_t>>* dst = (std::list<std::vector<uint8_t>>*)param;

	HRSRC hResource = FindResourceW(NULL, resName, type);
	if (hResource == NULL)
	{
		return FALSE;
	}

	HGLOBAL hFileResource = LoadResource(NULL, hResource);
	if (hFileResource == NULL)
	{
		return FALSE;
	}

	void* pData = LockResource(hFileResource);
	if (pData != NULL)
	{
		const DWORD size = SizeofResource(NULL, hResource);
		if (size > 0)
		{
			std::vector<uint8_t> data;
			data.resize(size);
			memcpy(&data[0], pData, size);
			dst->emplace_back(std::move(data));
		}
		UnlockResource(pData);
	}
	FreeResource(hFileResource);

	return TRUE;
}

std::wstring QueryStringFileInfo(LPCVOID pVersionInfoBlock, const std::wstring& subName)
{
	void* pValue = NULL;
	UINT len = 0;

	std::wstring query;
	query.append(L"\\StringFileInfo\\" DEF_LANG_NEUTRAL DEF_CHARSET_UNICODE L"\\").append(subName);
	if (!VerQueryValueW(pVersionInfoBlock, query.c_str(), &pValue, &len) || pValue == NULL || len == 0)
	{
		return std::wstring();
	}

	for (DWORD i = 0; i < len; i++)
	{
		WCHAR symbol = ((WCHAR*)pValue)[i];
		if (symbol == 0 || !iswprint(symbol))
		{
			len = i;
			break;
		}
	}

	return std::wstring((WCHAR*)pValue, len);
}

} // namespace



std::wstring PackageManager::GetStringFromResource(const std::wstring id)
{
	return FindStringFromBinaryResourceEx(NULL, id.c_str(), MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL));
}

std::wstring PackageManager::GetStringFileInfo(const std::wstring& subName)
{
	HRSRC hVersion = FindResource(NULL, MAKEINTRESOURCE(VS_VERSION_INFO), RT_VERSION);
	if (hVersion == NULL)
	{
		return std::wstring();
	}

	HGLOBAL hGlobal = LoadResource(NULL, hVersion);
	if (hGlobal == NULL)
	{
		return std::wstring();
	}

	std::wstring result;
	LPVOID versionInfo = LockResource(hGlobal);
	if (versionInfo != NULL)
	{
		result = QueryStringFileInfo(versionInfo, subName);
		UnlockResource(hGlobal);
	}

	FreeResource(hGlobal);

	return result;
}

std::vector<uint8_t> PackageManager::GetBinaryResource(const std::wstring& subName)
{
	std::vector<uint8_t> result;

	HRSRC hrsrc = FindResourceEx(NULL, RT_RCDATA, subName.c_str(), MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL));
	if (hrsrc)
	{
		HGLOBAL hglob = LoadResource(NULL, hrsrc);
		if (hglob)
		{
			char* pData = reinterpret_cast<char*>(LockResource(hglob));
			if (pData)
			{
				const DWORD size = SizeofResource(NULL, hrsrc);
				if (size > 0)
				{
					result.resize(size);
					memcpy(&result[0], pData, size);
				}

				UnlockResource(pData);
			}
			FreeResource(hglob);
		}
	}

	return result;
}

std::list<std::vector<uint8_t>> PackageManager::GetAllBinaryResources(const std::wstring& id)
{
	std::list<std::vector<uint8_t>> result;
	EnumResourceNamesW(NULL, id.c_str(), ExtractBinary, (LONG_PTR)&result);

	return result;
}

Error PackageManager::UnpackZipResource(const std::wstring& destDir)
{
	UnpackParam param;
	param.destDir = destDir;
	EnumResourceNamesW(NULL, L"ZIP", UnpackZip, (LONG_PTR)&param);

	return param.err;
}
