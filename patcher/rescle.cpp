// Copyright (c) 2013 GitHub, Inc. All rights reserved.
// Use of this source code is governed by MIT license that can be found in the
// LICENSE file.
//
// This file is modified from Rescle written by yoshio.okumura@gmail.com:
// http://code.google.com/p/rescle/

#include "rescle.h"
#include "../common/StringConverter.hpp"
#include "../common/unique_resource.h"
#include <sstream> // wstringstream
#include <iomanip> // setw, setfill


namespace rescle {

namespace {

#pragma pack(push,2)
typedef struct _GRPICONENTRY {
  BYTE width;
  BYTE height;
  BYTE colourCount;
  BYTE reserved;
  BYTE planes;
  BYTE bitCount;
  WORD bytesInRes;
  WORD bytesInRes2;
  WORD reserved2;
  WORD id;
} GRPICONENTRY;
#pragma pack(pop)

#pragma pack(push,2)
typedef struct _GRPICONHEADER {
  WORD reserved;
  WORD type;
  WORD count;
  GRPICONENTRY entries[1];
} GRPICONHEADER;
#pragma pack(pop)

#pragma pack(push,1)
typedef struct _VS_VERSION_HEADER {
  WORD wLength;
  WORD wValueLength;
  WORD wType;
} VS_VERSION_HEADER;
#pragma pack(pop)

#pragma pack(push,1)
typedef struct _VS_VERSION_STRING {
  VS_VERSION_HEADER Header;
  WCHAR szKey[1];
} VS_VERSION_STRING;
#pragma pack(pop)

#pragma pack(push,1)
typedef struct _VS_VERSION_ROOT_INFO {
  WCHAR szKey[16];
  WORD  Padding1[1];
  VS_FIXEDFILEINFO Info;
} VS_VERSION_ROOT_INFO;
#pragma pack(pop)

#pragma pack(push,1)
typedef struct _VS_VERSION_ROOT {
  VS_VERSION_HEADER Header;
  VS_VERSION_ROOT_INFO Info;
} VS_VERSION_ROOT;
#pragma pack(pop)

// The default en-us LANGID.

const LANGID langDefault = MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT);
const LANGID langEnUs = 1033; // RT_MANIFEST default uses
const LANGID codePageEnUs = 1200;

template<typename T>
inline T round(T value, int modula = 4) 
{
	return value + ((value % modula > 0) ? (modula - value % modula) : 0);
}

class ScopedFile 
{
public:
	ScopedFile(const WCHAR* path)
		: hFile(CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL))
	{
	}

	~ScopedFile() 
	{
		CloseHandle(hFile);
	}

	operator HANDLE() 
	{
		return hFile;
	}

private:

	HANDLE hFile;
};

struct VersionStampValue 
{
	WORD valueLength = 0; // stringfileinfo, stringtable: 0; string: Value size in WORD; var: Value size in bytes
	WORD type = 0; // 0: binary data; 1: text data
	std::wstring key; // stringtable: 8-digit hex stored as UTF-16 (hiword: hi6: sublang, lo10: majorlang; loword: code page); must include zero words to align next member on 32-bit boundary
	std::vector<BYTE> value; // string: zero-terminated string; var: array of language & code page ID pairs
	std::vector<VersionStampValue> children;

	size_t GetLength() const;
	std::vector<BYTE> Serialize() const;
};

}  // namespace

VersionInfo::VersionInfo() 
{
	FillDefaultData();
}

VersionInfo::VersionInfo(HMODULE hModule, WORD languageId) 
{
	HRSRC hRsrc = FindResourceExW(hModule, RT_VERSION, MAKEINTRESOURCEW(1), languageId);

	if (hRsrc == NULL) 
	{
		throw std::system_error(GetLastError(), std::system_category());
	}

	HGLOBAL hGlobal = LoadResource(hModule, hRsrc);
	if (hGlobal == NULL) 
	{
		throw std::system_error(GetLastError(), std::system_category());
	}

	void* p = LockResource(hGlobal);
	if (p == NULL) 
	{
		throw std::system_error(GetLastError(), std::system_category());
	}

	DWORD size = SizeofResource(hModule, hRsrc);
	if (size == 0) 
	{
		throw std::system_error(GetLastError(), std::system_category());
	}

	DeserializeVersionInfo(static_cast<BYTE*>(p), size);
	FillDefaultData();
}

bool VersionInfo::HasFixedFileInfo() const 
{
	return fixedFileInfo.dwSignature == 0xFEEF04BD;
}

VS_FIXEDFILEINFO& VersionInfo::GetFixedFileInfo() 
{
	return fixedFileInfo;
}

const VS_FIXEDFILEINFO& VersionInfo::GetFixedFileInfo() const 
{
	return fixedFileInfo;
}

void VersionInfo::SetFixedFileInfo(const VS_FIXEDFILEINFO& value) 
{
	fixedFileInfo = value;
}

std::vector<BYTE> VersionInfo::Serialize() const 
{
	VersionStampValue versionInfo;
	versionInfo.key = L"VS_VERSION_INFO";
	versionInfo.type = 0;

	if (HasFixedFileInfo()) 
	{
		const auto size = sizeof(VS_FIXEDFILEINFO);
		versionInfo.valueLength = size;

		auto& dst = versionInfo.value;
		dst.resize(size);

		memcpy(&dst[0], &GetFixedFileInfo(), size);
	}

	{
		VersionStampValue stringFileInfo;
		stringFileInfo.key = L"StringFileInfo";
		stringFileInfo.type = 1;
		stringFileInfo.valueLength = 0;

		for (const auto& iTable : stringTables) 
		{
			VersionStampValue stringTableRaw;
			stringTableRaw.type = 1;
			stringTableRaw.valueLength = 0;

			{
				auto& translate = iTable.encoding;
				std::wstringstream ss;
				ss << std::hex << std::setw(8) << std::setfill(L'0') << (translate.wLanguage << 16 | translate.wCodePage);
				stringTableRaw.key = ss.str();
			}

			for (const auto& iString : iTable.strings) 
			{
				const auto& stringValue = iString.second;
				auto strLenNullTerminated = stringValue.length() + 1;

				VersionStampValue stringRaw;
				stringRaw.type = 1;
				stringRaw.key = iString.first;
				stringRaw.valueLength = (WORD)strLenNullTerminated;

				auto size = strLenNullTerminated * sizeof(WCHAR);
				auto& dst = stringRaw.value;
				dst.resize(size);

				auto src = stringValue.c_str();

				memcpy(&dst[0], src, size);

				stringTableRaw.children.push_back(std::move(stringRaw));
			}

			stringFileInfo.children.push_back(std::move(stringTableRaw));
		}

		versionInfo.children.push_back(std::move(stringFileInfo));
	}

	{
		VersionStampValue varFileInfo;
		varFileInfo.key = L"VarFileInfo";
		varFileInfo.type = 1;
		varFileInfo.valueLength = 0;

		{
			VersionStampValue varRaw;
			varRaw.key = L"Translation";
			varRaw.type = 0;

			{
				auto newValueSize = sizeof(DWORD);
				auto& dst = varRaw.value;
				dst.resize(supportedTranslations.size() * newValueSize);

				for (auto iVar = 0; iVar < supportedTranslations.size(); ++iVar) 
				{
					auto& translate = supportedTranslations[iVar];
					auto var = DWORD(translate.wCodePage) << 16 | translate.wLanguage;
					memcpy(&dst[iVar * newValueSize], &var, newValueSize);
				}

				varRaw.valueLength = (WORD)varRaw.value.size();
			}

			varFileInfo.children.push_back(std::move(varRaw));
		}

		versionInfo.children.push_back(std::move(varFileInfo));
	}

	return std::move(versionInfo.Serialize());
}

void VersionInfo::FillDefaultData() 
{
	if (stringTables.empty()) 
	{
		Translate enUsTranslate = { langEnUs, codePageEnUs};
		stringTables.push_back({enUsTranslate});
		supportedTranslations.push_back(enUsTranslate);
	}
	if (!HasFixedFileInfo()) 
	{
		fixedFileInfo = {0};
		fixedFileInfo.dwSignature = 0xFEEF04BD;
		fixedFileInfo.dwFileType = VFT_APP;
	}
}

void VersionInfo::DeserializeVersionInfo(const BYTE* pData, size_t size) 
{
	auto pVersionInfo = reinterpret_cast<const VS_VERSION_ROOT*>(pData);
	WORD fixedFileInfoSize = pVersionInfo->Header.wValueLength;

	if (fixedFileInfoSize > 0)
	{
		SetFixedFileInfo(pVersionInfo->Info.Info);
	}

	const BYTE* fixedFileInfoEndOffset = reinterpret_cast<const BYTE*>(&pVersionInfo->Info.szKey) + (wcslen(pVersionInfo->Info.szKey) + 1) * sizeof(WCHAR) + fixedFileInfoSize;
	const BYTE* pVersionInfoChildren = reinterpret_cast<const BYTE*>(round(reinterpret_cast<ptrdiff_t>(fixedFileInfoEndOffset)));
	size_t versionInfoChildrenOffset = pVersionInfoChildren - pData;
	size_t versionInfoChildrenSize = pVersionInfo->Header.wLength - versionInfoChildrenOffset;

	const auto childrenEndOffset = pVersionInfoChildren + versionInfoChildrenSize;
	const auto resourceEndOffset = pData + size;
	for (auto p = pVersionInfoChildren; p < childrenEndOffset && p < resourceEndOffset;) 
	{
		auto pKey = reinterpret_cast<const VS_VERSION_STRING*>(p)->szKey;
		auto versionInfoChildData = GetChildrenData(p);
		if (wcscmp(pKey, L"StringFileInfo") == 0) 
		{
			DeserializeVersionStringFileInfo(versionInfoChildData.first, versionInfoChildData.second, stringTables);
		}
		else if (wcscmp(pKey, L"VarFileInfo") == 0) 
		{
			DeserializeVarFileInfo(versionInfoChildData.first, supportedTranslations);
		}

		p += round(reinterpret_cast<const VS_VERSION_STRING*>(p)->Header.wLength);
	}
}

VersionStringTable VersionInfo::DeserializeVersionStringTable(const BYTE* tableData) 
{
	auto strings = GetChildrenData(tableData);
	auto stringTable = reinterpret_cast<const VS_VERSION_STRING*>(tableData);
	auto end_ptr = const_cast<WCHAR*>(stringTable->szKey + (8 * sizeof(WCHAR)));
	auto langIdCodePagePair = static_cast<DWORD>(wcstol(stringTable->szKey, &end_ptr, 16));

	VersionStringTable tableEntry;

	// unicode string of 8 hex digits
	tableEntry.encoding.wLanguage = langIdCodePagePair >> 16;
	tableEntry.encoding.wCodePage = (WORD)langIdCodePagePair;

	for (auto posStrings = 0U; posStrings < strings.second;) 
	{
		const auto stringEntry = reinterpret_cast<const VS_VERSION_STRING* const>(strings.first + posStrings);
		const auto stringData = GetChildrenData(strings.first + posStrings);
		tableEntry.strings.push_back(std::pair<std::wstring, std::wstring>(stringEntry->szKey, std::wstring(reinterpret_cast<const WCHAR* const>(stringData.first), stringEntry->Header.wValueLength)));

		posStrings += round(stringEntry->Header.wLength);
	}

	return tableEntry;
}

void VersionInfo::DeserializeVersionStringFileInfo(const BYTE* offset, size_t length, std::vector<VersionStringTable>& stringTables) 
{
	for (auto posStringTables = 0U; posStringTables < length;) 
	{
		auto stringTableEntry = DeserializeVersionStringTable(offset + posStringTables);
		stringTables.push_back(stringTableEntry);
		posStringTables += round(reinterpret_cast<const VS_VERSION_STRING*>(offset + posStringTables)->Header.wLength);
	}
}

void VersionInfo::DeserializeVarFileInfo(const unsigned char* offset, std::vector<Translate>& translations) 
{
	const auto translatePairs = GetChildrenData(offset);

	const auto top = reinterpret_cast<const DWORD* const>(translatePairs.first);
	for (auto pTranslatePair = top; pTranslatePair < top + translatePairs.second; pTranslatePair += sizeof(DWORD)) 
	{
		auto codePageLangIdPair = *pTranslatePair;
		Translate translate;
		translate.wLanguage = (LANGID)codePageLangIdPair;
		translate.wCodePage = codePageLangIdPair >> 16;
		translations.push_back(translate);
	}
}

OffsetLengthPair VersionInfo::GetChildrenData(const BYTE* entryData) 
{
	auto entry = reinterpret_cast<const VS_VERSION_STRING*>(entryData);
	auto headerOffset = entryData;
	auto headerSize = sizeof(VS_VERSION_HEADER);
	auto keySize = (wcslen(entry->szKey) + 1) * sizeof(WCHAR);
	auto childrenOffset = round(headerSize + keySize);

	auto pChildren = headerOffset + childrenOffset;
	auto childrenSize = entry->Header.wLength - childrenOffset;
	return OffsetLengthPair(pChildren, childrenSize);
}

size_t VersionStampValue::GetLength() const 
{
	size_t bytes = sizeof(VS_VERSION_HEADER);
	bytes += static_cast<size_t>(key.length() + 1) * sizeof(WCHAR);
	if (!value.empty())
	{
		bytes = round(bytes) + value.size();
	}

	for (const auto& child : children)
	{
		bytes = round(bytes) + static_cast<size_t>(child.GetLength());
	}

	return bytes;
}

std::vector<BYTE> VersionStampValue::Serialize() const 
{
	std::vector<BYTE> data = std::vector<BYTE>(GetLength());

	size_t offset = 0;

	VS_VERSION_HEADER header = { static_cast<WORD>(data.size()), valueLength, type };
	memcpy(&data[offset], &header, sizeof(header));
	offset += sizeof(header);

	auto keySize = static_cast<size_t>(key.length() + 1) * sizeof(WCHAR);
	memcpy(&data[offset], key.c_str(), keySize);
	offset += keySize;

	if (!value.empty()) 
	{
		offset = round(offset);
		memcpy(&data[offset], &value[0], value.size());
		offset += value.size();
	}

	for (const auto& child : children) 
	{
		offset = round(offset);
		size_t childLength = child.GetLength();
		std::vector<BYTE> src = child.Serialize();
		memcpy(&data[offset], &src[0], childLength);
		offset += childLength;
	}

	return std::move(data);
}

ResourceUpdater::ResourceUpdater() 
	: module(NULL) 
{
}

ResourceUpdater::~ResourceUpdater() 
{
	if (module != NULL) 
	{
		FreeLibrary(module);
		module = NULL;
	}
}

Error ResourceUpdater::Load(const std::wstring& path_)
{
	path = path_;
    module = LoadLibraryExW(path.c_str(), NULL, DONT_RESOLVE_DLL_REFERENCES | LOAD_LIBRARY_AS_DATAFILE);

	if (module == NULL) 
	{
		return Error(GetLastError());
	}

	EnumResourceNamesW(module, RT_STRING, OnEnumResourceName, reinterpret_cast<LONG_PTR>(this));
	EnumResourceNamesW(module, RT_VERSION, OnEnumResourceName, reinterpret_cast<LONG_PTR>(this));
	EnumResourceNamesW(module, RT_GROUP_ICON, OnEnumResourceName, reinterpret_cast<LONG_PTR>(this));
	EnumResourceNamesW(module, RT_ICON, OnEnumResourceName, reinterpret_cast<LONG_PTR>(this));
	EnumResourceNamesW(module, RT_MANIFEST, OnEnumResourceManifest, reinterpret_cast<LONG_PTR>(this));

	return Error();
}

void ResourceUpdater::SetExecutionLevel(const std::wstring& value)
{
	executionLevel = value;
}

void ResourceUpdater::SetVersionString(WORD languageId, const std::wstring& name, const std::wstring& value) 
{
	auto& stringTables = versionStampMap[languageId].stringTables;
	for (auto j = stringTables.begin(); j != stringTables.end(); ++j) 
	{
		auto& stringPairs = j->strings;
		for (auto k = stringPairs.begin(); k != stringPairs.end(); ++k) 
		{
			if (k->first == name) 
			{
				k->second = value;
				return;
			}
		}

		// Not found, append one for all tables.
		stringPairs.push_back(VersionString(name, value));
	}
}

void ResourceUpdater::SetVersionString(const std::wstring& name, const std::wstring& value) 
{
	if (versionStampMap.empty())
	{
		return SetVersionString(langDefault, name, value);
	}

	for (auto& kv : versionStampMap)
	{
		SetVersionString(kv.first, name, value);
	}
}

void ResourceUpdater::SetFileData(TypeNameValue&& data)
{
	files.emplace_back(std::move(data));
}

void ResourceUpdater::SetStringData(TypeNameValue&& data)
{
	stringData.emplace_back(std::move(data));
}

bool ResourceUpdater::SetProductVersion(WORD languageId, const Version& ver)
{
	VersionInfo& versionInfo = versionStampMap[languageId];
	if (!versionInfo.HasFixedFileInfo()) 
	{
		return false;
	}

	VS_FIXEDFILEINFO& root = versionInfo.GetFixedFileInfo();
	root.dwProductVersionMS = ver.v1 << 16 | ver.v2;
	root.dwProductVersionLS = ver.v3 << 16 | ver.v4;

	return true;
}

bool ResourceUpdater::SetProductVersion(const Version& ver)
{
	if (versionStampMap.empty())
	{
		return SetProductVersion(langDefault, ver);
	}

	for (auto& kv : versionStampMap)
	{
		if (!SetProductVersion(kv.first, ver))
		{
			return false;
		}
	}

	return true;
}

bool ResourceUpdater::SetFileVersion(WORD languageId, const Version& ver)
{
	VersionInfo& versionInfo = versionStampMap[languageId];
	if (!versionInfo.HasFixedFileInfo()) 
	{
		return false;
	}

	VS_FIXEDFILEINFO& root = versionInfo.GetFixedFileInfo();
	root.dwFileVersionMS = ver.v1 << 16 | ver.v2;
	root.dwFileVersionLS = ver.v3 << 16 | ver.v4;

	return true;
}

bool ResourceUpdater::SetFileVersion(const Version& ver)
{
	if (versionStampMap.empty())
	{
		return SetFileVersion(langDefault, ver);
	}

	for (auto& kv : versionStampMap)
	{
		if (!SetFileVersion(kv.first, ver))
		{
			return false;
		}
	}

	return true;
}

Error ResourceUpdater::SetIcon(const std::wstring& path, const LANGID& langId, UINT iconBundle) 
{
	std::unique_ptr<IconsValue>& pIcon = iconBundleMap[langId].iconBundles[iconBundle];
	if (!pIcon)
	{
		pIcon = std::make_unique<IconsValue>();
	}

	auto& icon = *pIcon;
	DWORD bytes;

	ScopedFile file(path.c_str());
	if (file == INVALID_HANDLE_VALUE) 
	{
		std::wstring msg;
		msg.append(L"cannot open icon file '").append(path).append(L"', err = ").append(Error(GetLastError()).getMessage());
		return Error(std::move(msg));
	}

	IconsValue::ICONHEADER& header = icon.header;
	if (!ReadFile(file, &header, 3 * sizeof(WORD), &bytes, NULL)) 
	{
		std::wstring msg;
		msg.append(L"cannot read icon header for file '").append(path).append(L"', err = ").append(Error(GetLastError()).getMessage());
		return Error(std::move(msg));
	}

	if (header.reserved != 0 || header.type != 1) 
	{
		std::wstring msg;
		msg.append(L"Reserved header is not 0 or image type is not icon for file '").append(path).append(L"'");
		return Error(std::move(msg));
	}

	header.entries.resize(header.count);
	if (!ReadFile(file, header.entries.data(), header.count * sizeof(IconsValue::ICONENTRY), &bytes, NULL)) 
	{
		std::wstring msg;
		msg.append(L"cannot read icon metadata for file '").append(path).append(L"', err = ").append(Error(GetLastError()).getMessage());
		return Error(std::move(msg));
	}

	icon.images.resize(header.count);
	for (size_t i = 0; i < header.count; ++i) 
	{
		icon.images[i].resize(header.entries[i].bytesInRes);
		SetFilePointer(file, header.entries[i].imageOffset, NULL, FILE_BEGIN);
		if (!ReadFile(file, icon.images[i].data(), (DWORD)(icon.images[i].size()), &bytes, NULL)) 
		{
			std::wstring msg;
			msg.append(L"cannot read icon data for file '").append(path).append(L"', err = ").append(Error(GetLastError()).getMessage());
			return Error(std::move(msg));
		}
	}

	icon.grpHeader.resize(3 * sizeof(WORD) + header.count * sizeof(GRPICONENTRY));
	GRPICONHEADER* pGrpHeader = reinterpret_cast<GRPICONHEADER*>(icon.grpHeader.data());
	pGrpHeader->reserved = 0;
	pGrpHeader->type = 1;
	pGrpHeader->count = header.count;
	for (WORD i = 0; i < header.count; ++i) 
	{
		GRPICONENTRY* entry = pGrpHeader->entries + i;
		entry->bitCount = 0;
		entry->bytesInRes = header.entries[i].bitCount;
		entry->bytesInRes2 = (WORD)header.entries[i].bytesInRes;
		entry->colourCount = header.entries[i].colorCount;
		entry->height = header.entries[i].height;
		entry->id = i + 1;
		entry->planes = (BYTE)header.entries[i].planes;
		entry->reserved = header.entries[i].reserved;
		entry->width = header.entries[i].width;
		entry->reserved2 = 0;
	}

	return Error();
}

Error ResourceUpdater::SetIcon(const std::wstring& path, const LANGID& langId) 
{
	UINT iconBundle;

	IconResInfo& info = iconBundleMap[langId];
	if (info.iconBundles.empty())
	{
		iconBundle = 0;
	}
	else
	{
		iconBundle = info.iconBundles.begin()->first;
	}

	return SetIcon(path, langId, iconBundle);
}

Error ResourceUpdater::SetIcon(const std::wstring& path) 
{
	if (iconBundleMap.empty())
	{
		return SetIcon(path, langDefault);
	}

	for (auto& kv : iconBundleMap)
	{
		Error err = SetIcon(path, kv.first);
		if (!err.Succeeded())
		{
			return err;
		}
	}

	return Error();
}

Error ResourceUpdater::Commit() 
{
	if (module == NULL) 
	{
		return Error();
	}

	FreeLibrary(module);
	module = NULL;

	ScopedResourceUpdater ru(path.c_str(), false);
	if (ru.Get() == NULL) 
	{
		return Error(GetLastError());
	}

	// update version info.
	for (const auto& i : versionStampMap) 
	{
		LANGID langId = i.first;
		std::vector<BYTE> out = i.second.Serialize();

		if (!UpdateResourceW(ru.Get(), RT_VERSION, MAKEINTRESOURCEW(1), langId,	&out[0], static_cast<DWORD>(out.size()))) 
		{
			return Error(GetLastError());
		}
	}

	for (const TypeNameValue& data: stringData)
	{
		if (!UpdateResource(ru.Get(), data.type.c_str(), data.name.c_str(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPVOID)&data.value[0], (DWORD)data.value.size() * sizeof(wchar_t)))
		{
			return Error(GetLastError());
		}
	}

	for (const TypeNameValue& data: files)
	{
		auto file = croco::make_unique_resource_checked(
			CreateFileW(data.value.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL),
			INVALID_HANDLE_VALUE,
			CloseHandle);

		if (file.get() == INVALID_HANDLE_VALUE)
		{
			std::wstring msg;
			msg.append(L"cannot open file '").append(data.value).append(L"', err = ").append(Error(GetLastError()).getMessage());
			return Error(std::move(msg));
		}

		LARGE_INTEGER size = { 0, 0 };
		if (!GetFileSizeEx(file, &size))
		{
			std::wstring msg;
			msg.append(L"GetFileSizeEx failed, file '").append(data.value).append(L"', err = ").append(Error(GetLastError()).getMessage());
			return Error(std::move(msg));
		}

		if (size.QuadPart == 0)
		{
			std::wstring msg;
			msg.append(L"file '").append(data.value).append(L"', is empty");
			return Error(std::move(msg));
		}
		else if (size.HighPart > 0)
		{
			std::wstring msg;
			msg.append(L"file '").append(data.value).append(L"' too big");
			return Error(std::move(msg));
		}

		auto fileMap = croco::make_unique_resource_checked(
			CreateFileMapping(file.get(), NULL, PAGE_READONLY, 0, size.LowPart, NULL),
			HANDLE(NULL),
			CloseHandle);

		if (fileMap.get() == NULL)
		{
			std::wstring msg;
			msg.append(L"CreateFileMapping failed, file '").append(data.value).append(L"', err = ").append(Error(GetLastError()).getMessage());
			return Error(std::move(msg));
		}

		auto lpBaseAddress = croco::make_unique_resource_checked(
			(LPCVOID)MapViewOfFile(fileMap.get(), FILE_MAP_READ, 0, 0, 0),
			(LPCVOID)NULL,
			UnmapViewOfFile);

		if (lpBaseAddress.get() == NULL)
		{
			std::wstring msg;
			msg.append(L"MapViewOfFile failed, file '").append(data.value).append(L"', err = ").append(Error(GetLastError()).getMessage());
			return Error(std::move(msg));
		}

		if (!UpdateResource(ru.Get(), data.type.c_str(), data.name.c_str(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPVOID)lpBaseAddress.get(), size.LowPart))
		{
			std::wstring msg;
			msg.append(L"UpdateResource failed, file '").append(data.value).append(L"', err = ").append(Error(GetLastError()).getMessage());
			return Error(std::move(msg));
		}
	}

	// update the execution level
	if (!executionLevel.empty()) 
	{
		// string replace with requested executionLevel
		std::wstring::size_type pos = 0u;
		while ((pos = manifestString.find(originalExecutionLevel, pos)) != std::string::npos) 
		{
			manifestString.replace(pos, originalExecutionLevel.length(), executionLevel);
			pos += executionLevel.length();
		}

		// clean old padding and add new padding, ensuring that the size is a multiple of 4
		std::wstring::size_type padPos = manifestString.find(L"</assembly>");
		// trim anything after the </assembly>, 11 being the length of </assembly> (ie, remove old padding)
		std::wstring trimmedStr = manifestString.substr(0, padPos + 11);
		std::wstring padding = L"\n<!--Padding to make filesize even multiple of 4 X -->";

		int offset = (trimmedStr.length() + padding.length()) % 4;
		// multiple X by the number in offset
		pos = 0u;
		for (int posCount = 0; posCount < offset; posCount = posCount + 1) 
		{
			if ((pos = padding.find(L"X", pos)) != std::string::npos) 
			{
				padding.replace(pos, 1, L"XX");
				pos += executionLevel.length();
			}
		}

		// convert the wchar back into char, so that it encodes correctly for Windows to read the XML.
		const std::wstring stringSectionW = trimmedStr + padding;
		std::string stringSection;
		ConvertUtf16ToUtf8(stringSectionW, stringSection);

		if (!UpdateResourceW(ru.Get(), RT_MANIFEST, MAKEINTRESOURCEW(1), langEnUs, &stringSection.at(0), (DWORD)(sizeof(char) * stringSection.size()))) 
		{
			return Error(GetLastError());
		}
	}

	for (const auto& iLangIconInfoPair : iconBundleMap) 
	{
		auto langId = iLangIconInfoPair.first;
		auto maxIconId = iLangIconInfoPair.second.maxIconId;
		for (const auto& iNameBundlePair : iLangIconInfoPair.second.iconBundles) 
		{
			UINT bundleId = iNameBundlePair.first;
			const std::unique_ptr<IconsValue>& pIcon = iNameBundlePair.second;
			if (!pIcon)
			{
				continue;
			}

			auto& icon = *pIcon;
			// update icon.
			if (icon.grpHeader.size() > 0)
			{
				if (!UpdateResourceW(ru.Get(), RT_GROUP_ICON, MAKEINTRESOURCEW(bundleId), langId, icon.grpHeader.data(), (DWORD)(icon.grpHeader.size())))
				{
					return Error(GetLastError());
				}

				for (size_t i = 0; i < icon.header.count; ++i)
				{
					if (!UpdateResourceW(ru.Get(), RT_ICON, MAKEINTRESOURCEW(i + 1), langId, icon.images[i].data(), (DWORD)(icon.images[i].size())))
					{
						return Error(GetLastError());
					}
				}

				for (size_t i = icon.header.count; i < maxIconId; ++i)
				{
					if (!UpdateResourceW(ru.Get(), RT_ICON, MAKEINTRESOURCEW(i + 1), langId, nullptr, 0))
					{
						return Error(GetLastError());
					}
				}
			}
		}
	}

	return ru.Commit();
}

BOOL CALLBACK ResourceUpdater::OnEnumResourceLanguage(HANDLE hModule, LPCWSTR lpszType, LPCWSTR lpszName, WORD wIDLanguage, LONG_PTR lParam)
{
	ResourceUpdater* instance = reinterpret_cast<ResourceUpdater*>(lParam);
	if (IS_INTRESOURCE(lpszName) && IS_INTRESOURCE(lpszType))
	{
		switch (reinterpret_cast<ptrdiff_t>(lpszType))
		{
		case reinterpret_cast<ptrdiff_t>(RT_VERSION) :
		{
			try
			{
				instance->versionStampMap[wIDLanguage] = VersionInfo(instance->module, wIDLanguage);
			}
			catch (const std::system_error& /*e*/)
			{
				return false;
			}
			break;
		}
		case reinterpret_cast<ptrdiff_t>(RT_ICON) :
		{
			UINT iconId = (UINT)reinterpret_cast<ptrdiff_t>(lpszName);
			UINT maxIconId = instance->iconBundleMap[wIDLanguage].maxIconId;
			if (iconId > maxIconId)
			{
				maxIconId = iconId;
			}
			break;
		}
		case reinterpret_cast<ptrdiff_t>(RT_GROUP_ICON) :
		{
			UINT iconId = (UINT)(reinterpret_cast<ptrdiff_t>(lpszName));
			instance->iconBundleMap[wIDLanguage].iconBundles[iconId] = nullptr;
			break;
		}
		default:
			break;
		}
	}
	return TRUE;
}

BOOL CALLBACK ResourceUpdater::OnEnumResourceName(HMODULE hModule, LPCWSTR lpszType, LPWSTR lpszName, LONG_PTR lParam)
{
	EnumResourceLanguagesW(hModule, lpszType, lpszName, (ENUMRESLANGPROCW)OnEnumResourceLanguage, lParam);
	return TRUE;
}

// courtesy of http://stackoverflow.com/questions/420852/reading-an-applications-manifest-file
BOOL CALLBACK ResourceUpdater::OnEnumResourceManifest(HMODULE hModule, LPCTSTR lpType, LPWSTR lpName, LONG_PTR lParam)
{
	static const int BOM_SIZE = 3;
	static const BYTE BOM[BOM_SIZE] = { 0xEF, 0xBB, 0xBF };

	ResourceUpdater* instance = reinterpret_cast<ResourceUpdater*>(lParam);
	HRSRC hResInfo = FindResource(hModule, lpName, lpType);
	DWORD cbResource = SizeofResource(hModule, hResInfo);
	HGLOBAL hResData = LoadResource(hModule, hResInfo);

	char* pManifest = (char* )LockResource(hResData);
	size_t len = strlen(reinterpret_cast<const char*>(pManifest));
	if (len > BOM_SIZE && memcmp(BOM, pManifest, BOM_SIZE) == 0)
	{
		pManifest += BOM_SIZE;
		len -= BOM_SIZE;
	}
	std::wstring manifestStringLocal;
	ConvertUtf8ToUtf16(pManifest, len, manifestStringLocal);

	size_t found = manifestStringLocal.find(L"requestedExecutionLevel");
	size_t end = manifestStringLocal.find(L"uiAccess");
	instance->originalExecutionLevel = manifestStringLocal.substr(found + 31 , end - found - 33);

	// also store original manifestString
	instance->manifestString = manifestStringLocal;

	UnlockResource(hResData);
	FreeResource(hResData);

	return TRUE;   // Keep going
}

ScopedResourceUpdater::ScopedResourceUpdater(const WCHAR* filename, bool deleteOld)
    : handle(BeginUpdateResourceW(filename, deleteOld)) 
{
}

ScopedResourceUpdater::~ScopedResourceUpdater() 
{
	if (!commited) 
	{
		EndUpdate(false);
	}
}

HANDLE ScopedResourceUpdater::Get() const 
{
	return handle;
}

Error ScopedResourceUpdater::Commit() 
{
	commited = true;
	return EndUpdate(true);
}

Error ScopedResourceUpdater::EndUpdate(bool doesCommit) 
{
	BOOL fDiscard = doesCommit ? FALSE : TRUE;
	if (!EndUpdateResourceW(handle, fDiscard))
	{
		return Error(GetLastError());
	}
	return Error();
}

}  // namespace rescle
