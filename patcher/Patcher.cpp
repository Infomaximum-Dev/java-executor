#include "../common/StringConverter.hpp"
#include "rescle.h"
#include <boost/program_options.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string.hpp>
#include <iostream>
#include <vector>
#include <Windows.h>

#define SEPARATOR ":"

#define VERSION_SEPARATOR "."

const std::string ExecutorPathArg("executor-path");
const std::string IconPathArg("icon-path");
const std::string RunAsAdminArg("run-as-admin");
const std::string CompanyNameArg("company-name");
const std::string FileDescriptionArg("description");
const std::string VersionArg("version");
const std::string CopyrightArg("copyright");
const std::string ProductNameArg("product-name");
const std::string StringResourceArg("string-resource");
const std::string FileResourceArg("file-resource");

bool ParseVersionString(const std::string& versionStr, Version& version)
{
	version = Version();

	std::vector<std::string> versions;
	boost::algorithm::split(versions, versionStr, boost::is_any_of(VERSION_SEPARATOR));
	try
	{
		switch (versions.size())
		{
		case 1:
			version.v1 = boost::lexical_cast<unsigned short>(versions[0]);
			break;
		case 2:
			version.v1 = boost::lexical_cast<unsigned short>(versions[0]);
			version.v2 = boost::lexical_cast<unsigned short>(versions[1]);
			break;
		case 3:
			version.v1 = boost::lexical_cast<unsigned short>(versions[0]);
			version.v2 = boost::lexical_cast<unsigned short>(versions[1]);
			version.v3 = boost::lexical_cast<unsigned short>(versions[2]);
			break;
		case 4:
			version.v1 = boost::lexical_cast<unsigned short>(versions[0]);
			version.v2 = boost::lexical_cast<unsigned short>(versions[1]);
			version.v3 = boost::lexical_cast<unsigned short>(versions[2]);
			version.v4 = boost::lexical_cast<unsigned short>(versions[3]);
			break;
		default:
			return false;
		}
	}
	catch (const boost::bad_lexical_cast &)
	{
		return false;
	}

	return true;
}

Error SetVersion(const boost::program_options::variables_map& src, rescle::ResourceUpdater& destination)
{
	auto it = src.find(VersionArg);
	if (it == src.end())
	{
		return Error();
	}

	const std::string versionStr = it->second.as<std::string>();

	std::wstring versionW;
	Error err = ConvertUtf8ToUtf16(versionStr, versionW);
	if (!err.Succeeded())
	{
		std::wstring msg;
		msg.append(L"cannot convert version to UTF-16 ").append(err.getMessage());
		return Error(std::move(msg));
	}

	Version version;
	if (!ParseVersionString(versionStr, version))
	{
		std::wstring msg;
		msg.append(L"cannot parse version string '").append(versionW).append(L"'");
		return Error(std::move(msg));
	}

	destination.SetProductVersion(version);
	destination.SetFileVersion(version);
	destination.SetVersionString(L"FileVersion", versionW);
	destination.SetVersionString(L"ProductVersion", versionW);

	return Error();
}

std::wstring FindValue(const boost::program_options::variables_map& vm, const std::string& key)
{
	std::wstring value;
	if (vm.count(key))
	{
		ConvertUtf8ToUtf16(vm[key].as<std::string>(), value);
	}

	return value;
}

void SetVersionStringValueIfExists(const std::string& sourceKey, const boost::program_options::variables_map& source, const std::wstring& destinationKey, rescle::ResourceUpdater& destination)
{
	const std::wstring value = FindValue(source, sourceKey);
	if (!value.empty())
	{
		destination.SetVersionString(destinationKey, value);
	}
}

Error GetTypeNameValue(const std::string& rawString, TypeNameValue& dest)
{
	dest = TypeNameValue();

	const size_t pos1 = rawString.find_first_of(SEPARATOR);
	const size_t pos2 = rawString.find_first_of(SEPARATOR, pos1 + 1);
	if (pos1 == std::string::npos || pos2 == std::string::npos)
	{
		return Error(L"cannot parse string resource");
	}

	Error err = ConvertUtf8ToUtf16(rawString.substr(0, pos1), dest.type);
	if (!err.Succeeded())
	{
		return err;
	}

	err = ConvertUtf8ToUtf16(rawString.substr(pos1 + 1, pos2 - pos1 - 1), dest.name);
	if (!err.Succeeded())
	{
		return err;
	}

	err = ConvertUtf8ToUtf16(rawString.substr(pos2 + 1), dest.value);
	if (!err.Succeeded())
	{
		return err;
	}

	return Error();
}

template<typename SetFunc>
Error SetCustomData(const boost::program_options::variables_map& options, const std::string& optionKey, SetFunc setFunc)
{
	auto it = options.find(optionKey);
	if (it == options.end())
	{
		return Error();
	}

	std::vector<std::string> rawDataList = it->second.as<std::vector<std::string>>();
	for (const std::string& rawData : rawDataList)
	{
		TypeNameValue data;
		Error err = GetTypeNameValue(rawData, data);
		if (!err.Succeeded())
		{
			return err;
		}

		setFunc(std::move(data));
	}

	return Error();
}

Error Patch(const boost::program_options::variables_map& options)
{
	const std::wstring installerPath = FindValue(options, ExecutorPathArg);
	if (installerPath.empty())
	{
		return Error(L"set installer path");
	}

	rescle::ResourceUpdater updater;
	Error err = updater.Load(installerPath);
	if (!err.Succeeded())
	{
		std::wstring msg;
		msg.append(L"cannot load file '").append(installerPath).append(L"', err = ");
		msg.append(err.getMessage());
		return Error(std::move(msg));
	}

	const std::wstring iconPath = FindValue(options, IconPathArg);
	if (!iconPath.empty())
	{
		Error err = updater.SetIcon(iconPath);
		if (!err.Succeeded())
		{
			return err;
		}
	}

	SetVersionStringValueIfExists(CompanyNameArg, options, L"CompanyName", updater);
	SetVersionStringValueIfExists(FileDescriptionArg, options, L"FileDescription", updater);
	SetVersionStringValueIfExists(CopyrightArg, options, L"LegalCopyright", updater);
	SetVersionStringValueIfExists(ProductNameArg, options, L"ProductName", updater);

	err = SetVersion(options, updater);
	if (!err.Succeeded())
	{
		return err;
	}

	if (options.count(RunAsAdminArg))
	{
		if (options[RunAsAdminArg].as<bool>())
		{
			updater.SetExecutionLevel(L"requireAdministrator");
		}
		else 
		{
			updater.SetExecutionLevel(L"asInvoker");
		}
	}

	err = SetCustomData(options, StringResourceArg, [&updater](TypeNameValue&& data)
	{
		updater.SetStringData(std::move(data));
	});
	if (!err.Succeeded())
	{
		return err;
	}

	err = SetCustomData(options, FileResourceArg, [&updater](TypeNameValue&& data)
	{
		updater.SetFileData(std::move(data));
	});
	if (!err.Succeeded())
	{
		return err;
	}

	err = updater.Commit();
	if (!err.Succeeded())
	{
		std::wstring msg;
		msg.append(L"cannot patch resource, file '").append(installerPath).append(L"', err = ");
		msg.append(err.getMessage());
		return Error(std::move(msg));
	}
	
	return Error();
}

int main(int argc, const char *argv[])
{
	using namespace boost::program_options;

	options_description desc("options");
	desc.add_options()
		(ExecutorPathArg.c_str(),	value<std::string>(),	"path to executable file, utf-8")
		(IconPathArg.c_str(),		value<std::string>(),	"[optional] path to icon, *.ico format")
		(CompanyNameArg.c_str(),	value<std::string>(),	"[optional] CompanyName, utf-8")
		(FileDescriptionArg.c_str(), value<std::string>(),	"[optional] FileDescription, utf-8")
		(VersionArg.c_str(),		value<std::string>(),	"[optional] Version, v1[.v2[.v3[.v4]]]")
		(CopyrightArg.c_str(),		value<std::string>(),	"[optional] Copyright, utf-8")
		(ProductNameArg.c_str(),	value<std::string>(),	"[optional] ProductName, utf-8")
		(RunAsAdminArg.c_str(),		value<bool>(),			"[optional] RunAsAdmin, true/false, default=false")
		(StringResourceArg.c_str(), value<std::vector<std::string>>(), "[optional] string resource, TYPE" SEPARATOR "NAME" SEPARATOR "value")
		(FileResourceArg.c_str(),	value<std::vector<std::string>>(), "[optional] file resource, TYPE" SEPARATOR "NAME" SEPARATOR "path");

	try
	{
		if (argc <= 1)
		{
			std::cout << desc;
			return EXIT_SUCCESS;
		}

		variables_map vm;
		store(parse_command_line(argc, argv, desc), vm);

		Error err = Patch(vm);
		if (!err.Succeeded())
		{
			std::wcout << err.getMessage();
			return EXIT_FAILURE;
		}

		return EXIT_SUCCESS;
	}
	catch (const error &ex)
	{
		std::cerr << ex.what();
		return EXIT_FAILURE;
	}
}
