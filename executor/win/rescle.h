// Copyright (c) 2013 GitHub, Inc. All rights reserved.
// Use of this source code is governed by MIT license that can be found in the
// LICENSE file.
//
// This file is modified from Rescle written by yoshio.okumura@gmail.com:
// http://code.google.com/p/rescle/

#pragma once

#include "Error.hpp"
#include <string>
#include <vector>
#include <map>
#include <memory> // unique_ptr
#include <windows.h>

struct Version
{
	unsigned short v1 = 0;
	unsigned short v2 = 0;
	unsigned short v3 = 0;
	unsigned short v4 = 0;
};

struct TypeNameValue
{
	std::wstring type;
	std::wstring name;
	std::wstring value;
};

namespace rescle {

struct IconsValue {
	typedef struct _ICONENTRY {
		BYTE width;
		BYTE height;
		BYTE colorCount;
		BYTE reserved;
		WORD planes;
		WORD bitCount;
		DWORD bytesInRes;
		DWORD imageOffset;
	} ICONENTRY;

	typedef struct _ICONHEADER {
		WORD reserved;
		WORD type;
		WORD count;
		std::vector<ICONENTRY> entries;
	} ICONHEADER;

	ICONHEADER header;
	std::vector<std::vector<BYTE>> images;
	std::vector<BYTE> grpHeader;
};

struct Translate 
{
	LANGID wLanguage;
	WORD wCodePage;
};

typedef std::pair<std::wstring, std::wstring> VersionString;
typedef std::pair<const BYTE* const, const size_t> OffsetLengthPair;

struct VersionStringTable 
{
	Translate encoding;
	std::vector<VersionString> strings;
};

class VersionInfo 
{
public:

	VersionInfo();
	VersionInfo(HMODULE hModule, WORD languageId);

	std::vector<BYTE> Serialize() const;

	bool HasFixedFileInfo() const;
	VS_FIXEDFILEINFO& GetFixedFileInfo();
	const VS_FIXEDFILEINFO& GetFixedFileInfo() const;
	void SetFixedFileInfo(const VS_FIXEDFILEINFO& value);

	std::vector<VersionStringTable> stringTables;
	std::vector<Translate> supportedTranslations;

private:

	void FillDefaultData();
	void DeserializeVersionInfo(const BYTE* pData, size_t size);

	VersionStringTable DeserializeVersionStringTable(const BYTE* tableData);
	void DeserializeVersionStringFileInfo(const BYTE* offset, size_t length, std::vector<VersionStringTable>& stringTables);
	void DeserializeVarFileInfo(const unsigned char* offset, std::vector<Translate>& translations);
	OffsetLengthPair GetChildrenData(const BYTE* entryData);

private:

	VS_FIXEDFILEINFO fixedFileInfo;
};

class ResourceUpdater 
{
public:

	typedef std::map<LANGID, VersionInfo> VersionStampMap;
	typedef std::map<UINT, std::unique_ptr<IconsValue>> IconTable;

	struct IconResInfo 
	{
		UINT maxIconId = 0;
		IconTable iconBundles;
	};

	typedef std::map<LANGID, IconResInfo> IconTableMap;

	ResourceUpdater();
	~ResourceUpdater();

	Error Load(const std::wstring& filename);
	void SetVersionString(WORD languageId, const std::wstring& name, const std::wstring& value);
	void SetVersionString(const std::wstring& name, const std::wstring& value);
	void SetStringData(TypeNameValue&& data);
	void SetFileData(TypeNameValue&& data);
	bool SetProductVersion(WORD languageId, const Version& ver);
	bool SetProductVersion(const Version& ver);
	bool SetFileVersion(WORD languageId, const Version& ver);
	bool SetFileVersion(const Version& ver);
	Error SetIcon(const std::wstring& path, const LANGID& langId, UINT iconBundle);
	Error SetIcon(const std::wstring& path, const LANGID& langId);
	Error SetIcon(const std::wstring& path);
	void SetExecutionLevel(const std::wstring& value);
	Error Commit();

 private:

	static BOOL CALLBACK OnEnumResourceName(HMODULE hModule, LPCWSTR lpszType, LPWSTR lpszName, LONG_PTR lParam);
	static BOOL CALLBACK OnEnumResourceManifest(HMODULE hModule, LPCWSTR lpszType, LPWSTR lpszName, LONG_PTR lParam);
	static BOOL CALLBACK OnEnumResourceLanguage(HANDLE hModule, LPCWSTR lpszType, LPCWSTR lpszName, WORD wIDLanguage, LONG_PTR lParam);

private:

	HMODULE module;
	std::wstring path;
	std::wstring executionLevel;
	std::wstring originalExecutionLevel;
	std::wstring manifestString;
	std::vector<TypeNameValue> files;
	std::vector<TypeNameValue> stringData;
	VersionStampMap versionStampMap;
	IconTableMap iconBundleMap;
};

class ScopedResourceUpdater 
{
public:

	ScopedResourceUpdater(const WCHAR* filename, bool deleteOld);
	~ScopedResourceUpdater();

	HANDLE Get() const;
	Error Commit();

private:

	Error EndUpdate(bool doesCommit);

private:

	HANDLE handle;
	bool commited = false;
};

}  // namespace rescle

