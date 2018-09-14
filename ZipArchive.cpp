#include "ZipArchive.h"
#include "File.h"
#include "Path.hpp"
#include "StringConverter.hpp"

#define ZIP_STATIC
#include <zip.h>

namespace
{

struct ZipError
{
	zip_error_t error;

	ZipError()
	{
		zip_error_init(&error);
	}

	~ZipError()
	{
		zip_error_fini(&error);
	}
};

struct ZipFile
{
public:

	zip_file_t* zf;

	ZipFile(zip_file_t* zf_)
	{
		zf = zf_;
	}

	~ZipFile()
	{
		if (zf != nullptr)
		{
			zip_fclose(zf);
		}
	}
};

struct ZipArchive
{
public:

	ZipArchive::~ZipArchive()
	{
		Close();
	}

	Error Open(uint8_t* pZipContent, size_t size)
	{
		ZipError zipError;

		zipSourceBuffer = zip_source_buffer_create(pZipContent, size, 0, &zipError.error);
		if (zipSourceBuffer == nullptr)
		{
			return Error(MakeZipErrorMsg(L"can not initialize zip buffer ", ToString(zipError.error)));
		}

		zipArchive = zip_open_from_source(zipSourceBuffer, 0, &zipError.error);
		if (zipArchive == nullptr)
		{
			zip_source_free(zipSourceBuffer);
			zipSourceBuffer = nullptr;

			return Error(MakeZipErrorMsg(L"can not open zip archive ", ToString(zipError.error)));
		}

		return Error();
	}

	void Close()
	{
		if (zipArchive)
		{
			zip_close(zipArchive);
			zipArchive = nullptr;
		}

		if (zipSourceBuffer)
		{
			zip_source_free(zipSourceBuffer);
			zipSourceBuffer = nullptr;
		}
	}

	Error Unpack(const std::wstring& destDir)
	{
		const zip_int64_t count = zip_get_num_entries(zipArchive, 0);
		for (zip_int64_t fileIndex = 0; fileIndex < count; fileIndex++)
		{
			zip_stat_t sb;
			if (zip_stat_index(zipArchive, fileIndex, 0, &sb) != 0)
			{
				continue;
			}

			std::wstring name;
			Error err = ConvertUtf8ToUtf16(sb.name, strlen(sb.name), name);
			if (!err.Succeeded())
			{
				return Error(MakeZipErrorMsg(L"can not convert filename to UTF16. ", err.getMessage()));
			}

			if (name.back() == L'/')
			{
				std::wstring destPath = std::wstring(destDir).append(L"\\").append(name);
				destPath.pop_back();
				Error err = Path::CreateDir(destPath);
				if (!err.Succeeded())
				{
					return Error(MakeZipErrorMsg(std::wstring(L"can not create dir '").append(destPath).append(L"'. "), err.getMessage()));
				}
			}
			else
			{
				const std::wstring destPath = std::wstring(destDir).append(L"\\").append(name);
				Error err = UnpackFile(fileIndex, sb.size, destPath);
				if (!err.Succeeded())
				{
					return err;
				}
			}
		}

		return Error();
	}

private:

	Error UnpackFile(zip_int64_t fileIndex, zip_int64_t uncompressedSize, const std::wstring& destPath)
	{
		static const size_t FileBufferSize = 64 * 1024;

		File dstFile;
		Error err = dstFile.OpenWrite(destPath);
		if (!err.Succeeded())
		{
			return Error(MakeZipErrorMsg(std::wstring(L"can not create file '").append(destPath).append(L"'. "), err.getMessage()));
		}

		ZipFile zipFile(zip_fopen_index(zipArchive, fileIndex, 0));
		if (zipFile.zf == nullptr)
		{
			return Error(MakeZipErrorMsg(L"can not open file from archive ", ToString(*zip_get_error(zipArchive))));
		}

		std::vector<uint8_t> buffer(FileBufferSize);
		zip_int64_t totalSize = 0;
		while (totalSize < uncompressedSize)
		{
			zip_int64_t size = zip_fread(zipFile.zf, &buffer[0], FileBufferSize);
			if (size < 0)
			{
				return Error(MakeZipErrorMsg(L"can not read file from archive ", ToString(*zip_file_get_error(zipFile.zf))));
			}

			Error err = dstFile.Write(&buffer[0], static_cast<DWORD>(size));
			if (!err.Succeeded())
			{
				return Error(MakeZipErrorMsg(std::wstring(L"can not write to file '").append(destPath).append(L"'. "), err.getMessage()));
			}

			totalSize += size;
		}

		return Error();
	}

	std::wstring MakeZipErrorMsg(const std::wstring& msg, const std::wstring& errorMsg)
	{
		static const std::wstring ZipErrorMessage = L"Error in zip archive: ";

		return std::wstring(ZipErrorMessage).append(msg).append(errorMsg);
	}

	std::wstring ToString(const zip_error_t& error)
	{
		std::wstring msg;
		msg.append(L"zip error = ").append(std::to_wstring(error.zip_err)).append(L", sys_err = ").append(std::to_wstring(error.sys_err));
		if (error.str != nullptr)
		{
			msg.append(L"str_err = ");
			std::wstring tmp;
			ConvertUtf8ToUtf16(error.str, tmp);
			msg.append(tmp);
		}

		return msg;
	}

private:

	zip_source_t* zipSourceBuffer = nullptr;
	zip_t* zipArchive = nullptr;
};

} // namespace

namespace zip_archive
{

Error UnpackToFolder(uint8_t* pZipContent, size_t size, const std::wstring& destPath)
{
	ZipArchive zipArchive;
	Error err = zipArchive.Open(pZipContent, size);
	if (!err.Succeeded())
	{
		return err;
	}

	err = zipArchive.Unpack(destPath);
	if (!err.Succeeded())
	{
		return err;
	}

	zipArchive.Close();
	return Error();
}

} // namespace zip_archive