// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
/*******************************************************************************
*
*	PACK format with compression & encryption.
*
*	OFFSET DATA
*	0      "2PAK"
*	4      entries offset
*	12     entries size
*	20     uncompressed file size
*	28     compressed & encrypted data
*	...
*	10000  filename
*	10240  offset in PACK file
*	10248  compressed file size + 8 bytes (see offset 20)
*
*******************************************************************************/
#include <verus.h>

using namespace verus;

struct FileEntry
{
	char  _name[240] = {};
	INT64 _offset = 0;
	INT64 _size = 0;

	FileEntry()
	{
		VERUS_CT_ASSERT(sizeof(FileEntry) == 256);
	}
};

WCHAR                 g_filename[MAX_PATH] = {};
WCHAR                 g_inputDir[MAX_PATH] = {};
WCHAR                 g_outputDir[MAX_PATH] = {};
WIN32_FIND_DATA       g_fd = {};
int                   g_depth = 0;
std::queue<FileEntry> g_fileEntries;

void TraverseDirectory(CWSZ parentDir, CWSZ foundDir, IO::RFile pakFile);

void Run()
{
	std::wcout << _T("PACK Builder 1.2.1") << std::endl;
	std::wcout << _T("Copyright (c) 2006-2020 Dmitry Maluev") << std::endl;
	std::wcout << _T("Arguments: [PAK/folder name] [input directory] [PAK output directory]") << std::endl;

	int argCount;
	LPWSTR* argArray = CommandLineToArgvW(GetCommandLine(), &argCount);
	if (argCount < 2)
	{
		std::wcout << _T("Enter PAK/folder name: ");
		std::wcin.getline(g_filename, MAX_PATH);
	}
	else
	{
		wcscpy_s(g_filename, MAX_PATH, argArray[1]);
	}
	if (argCount > 2)
	{
		wcscpy_s(g_inputDir, MAX_PATH, argArray[2]);
	}
	else
	{
		wcscpy_s(g_inputDir, MAX_PATH, argArray[0]);
		PathRemoveFileSpec(g_inputDir);
	}
	if (argCount > 3)
	{
		wcscpy_s(g_outputDir, MAX_PATH, argArray[3]);
	}
	else
	{
		wcscpy_s(g_outputDir, MAX_PATH, argArray[0]);
		PathRemoveFileSpec(g_outputDir);
	}
	LocalFree(argArray);

	std::wcout << _T("  Input directory: ") << g_inputDir << std::endl;
	std::wcout << _T("  Output directory: ") << g_outputDir << std::endl;

	std::wstringstream ssPathName;
	ssPathName << g_outputDir << _T("\\") << g_filename << _T(".pak");
	IO::File pakFile;
	if (!pakFile.Open(_C(Str::WideToUtf8(ssPathName.str())), "wb"))
	{
		std::wcerr << _T("ERROR: Unable to create file ") << g_filename << _T(".pak. Error code: 0x") << std::hex << GetLastError() << std::endl;
		throw std::exception();
	}
	const UINT32 magic = 'KAP2';
	pakFile << magic;
	INT64 temp = 0;
	pakFile << temp;
	pakFile << temp;

	std::wcout << std::endl;
	TraverseDirectory(_T("."), g_filename, pakFile);
	std::wcout << std::endl;

	const INT64 entriesOffset = pakFile.GetSize();
	pakFile.Seek(4, SEEK_SET);
	pakFile << entriesOffset;
	pakFile.Seek(0, SEEK_END);
	std::wcout << std::setw(56) << _T("[FILE NAME]") << _T(" ");
	std::wcout << std::setw(12) << _T("[OFFSET]") << _T(" ");
	std::wcout << std::setw(10) << _T("[SIZE]") << std::endl;
	INT64 fileCount = 0;
	while (!g_fileEntries.empty())
	{
		const FileEntry& fe = g_fileEntries.front();
		std::wcout << std::setw(56) << fe._name << _T(" ");
		std::wcout << std::setw(12) << fe._offset << _T(" ");
		std::wcout << std::setw(10) << fe._size << std::endl;
		pakFile << fe;
		g_fileEntries.pop();
		fileCount++;
	}

	std::wcout << std::endl;
	std::wcout << _T("Total files: ") << fileCount << std::endl;
	const INT64 entriesSize = fileCount * sizeof(FileEntry);
	pakFile.Seek(12, SEEK_SET);
	pakFile << entriesSize;
	pakFile.Close();
	if (argCount < 2)
		system("pause");
}

INT64 WriteData(CWSZ name, RcString password, IO::RFile pakFile, const BYTE* p, INT64 size)
{
	pakFile << size;
	uLongf zipSize = uLongf(size) * 2;
	Vector<BYTE> vZip(zipSize);
	const int res = compress(vZip.data(), &zipSize, p, static_cast<uLong>(size));
	if (res != Z_OK)
	{
		std::wcerr << _T("ERROR: Function compress() returned ") << res << _T(". File: ") << name << std::endl;
		throw std::exception();
	}
	vZip.resize(zipSize);
	Vector<BYTE> vCip(zipSize);
	Security::CipherRC4::Encrypt(password, vZip, vCip);
	pakFile.Write(vCip.data(), zipSize);
	return sizeof(INT64) + zipSize;
}

INT64 WriteTexture(CWSZ name, RcString password, IO::RFile pakFile, const Vector<BYTE>& vData)
{
	size_t headerSize = sizeof(IO::DDSHeader);
	IO::DDSHeader header;
	memcpy(&header, vData.data(), sizeof(header));
	if (!header.Validate())
	{
		std::wcerr << _T("ERROR: Invalid DDS header: ") << name << std::endl;
		throw std::exception();
	}
	IO::DDSHeaderDXT10 header10;
	if (header.IsDXT10())
	{
		memcpy(&header10, &vData[headerSize], sizeof(header10));
		headerSize += sizeof(IO::DDSHeaderDXT10);
	}

	CSZ magicNV = reinterpret_cast<CSZ>(&header._reserved1[9]);

	if (header._mipMapCount > 0)
	{
		const UINT32 mipsEx = Math::ComputeMipLevels(header._width, header._height);
		if (mipsEx != header._mipMapCount)
		{
			std::wcerr << _T("ERROR: Wrong mipmap count: ") << name << _T(", mips=") << header._mipMapCount << std::endl;
			throw std::exception();
		}
	}

	Vector<BYTE> vPart(vData.size());
	INT64 size = 0;
	size_t dataPos = headerSize;
	const int partCount = header.GetPartCount();

	const INT64 pakStartPos = pakFile.GetPosition();
	pakFile << header;
	if (header.IsDXT10())
		pakFile << header10;
	VERUS_FOR(part, partCount)
	{
		pakFile << size;
		pakFile << size;
		pakFile << size;
	}
	size = pakFile.GetPosition() - pakStartPos; // Header and part entries (200 or 220).

	VERUS_FOR(part, partCount)
	{
		const int w = header._width >> part;
		const int h = header._height >> part;
		size_t partSize = IO::DDSHeader::ComputeBcLevelSize(w, h, header.Is4BitsBC());
		if (part == partCount - 1) // 512, 256, ...
			partSize = vData.size() - dataPos; // Copy the rest.
		memcpy(vPart.data(), &vData[dataPos], partSize);

		const INT64 partOffset = pakFile.GetPosition() - pakStartPos;
		const INT64 partSize64 = partSize;
		const INT64 partZipSize = WriteData(name, password, pakFile, vPart.data(), partSize);

		pakFile.Seek(pakStartPos + headerSize + part * sizeof(INT64) * 3, SEEK_SET);
		pakFile << partOffset;
		pakFile << partSize64;
		pakFile << partZipSize;
		pakFile.Seek(0, SEEK_END);

		size += partZipSize;
		dataPos += partSize;
	}
	VERUS_RT_ASSERT((pakFile.GetPosition() - pakStartPos) == size);
	return size;
}

void TraverseDirectory(CWSZ parentDir, CWSZ foundDir, IO::RFile pakFile)
{
	if (g_depth > 5)
	{
		std::wcerr << _T("ERROR: Too deep. ") << foundDir << std::endl;
		throw std::exception();
	}
	g_depth++;

	WCHAR currentDir[MAX_PATH] = {};
	PathAppend(currentDir, g_inputDir);
	PathAppend(currentDir, parentDir);
	PathAppend(currentDir, foundDir);
	WCHAR query[MAX_PATH] = {};
	PathAppend(query, currentDir);
	PathAppend(query, _T("*.*"));

	HANDLE hDir = FindFirstFile(query, &g_fd);
	if (hDir != INVALID_HANDLE_VALUE)
	{
		do
		{
			if (!(wcscmp(g_fd.cFileName, _T(".")) && wcscmp(g_fd.cFileName, _T(".."))))
				continue;

			if (Str::EndsWith(_C(Str::WideToUtf8(g_fd.cFileName)), ".7z", false))
				continue;

			if (g_fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			{
				TraverseDirectory(currentDir, g_fd.cFileName, pakFile);
			}
			else if (_wcsicmp(g_fd.cFileName, _T("Thumbs.db")) && _wcsicmp(g_fd.cFileName, _T("desktop.ini")))
			{
				FileEntry fe;
				WCHAR entryName[MAX_PATH] = {};
				if (g_depth > 1)
				{
					const size_t offset = wcslen(g_inputDir) + wcslen(g_filename) + 2;
					wcscpy_s(entryName, MAX_PATH, currentDir + offset);
					wcscat_s(entryName, MAX_PATH, _T("/"));
				}
				wcscat_s(entryName, MAX_PATH, g_fd.cFileName);
				WideCharToMultiByte(CP_ACP, 0, entryName, -1, fe._name, sizeof(fe._name), 0, 0);
				for (int i = 0; fe._name[i]; ++i)
				{
					if (fe._name[i] == '\\')
						fe._name[i] = '/';
				}
				if (wcslen(entryName) < sizeof(fe._name))
				{
					fe._offset = pakFile.GetPosition();

					WCHAR pathName[MAX_PATH] = {};
					PathAppend(pathName, g_inputDir);
					PathAppend(pathName, g_filename);
					PathAppend(pathName, entryName);
					INT64 size = 0;
					IO::File file;
					if (file.Open(_C(Str::WideToUtf8(pathName)), "rb"))
					{
						size = file.GetSize();
						if (!size)
						{
							std::wcerr << _T("ERROR: Empty file: ") << pathName << std::endl;
							throw std::exception();
						}
						Vector<BYTE> vData(size);
						file.Read(vData.data(), size);
						file.Close();

						String password;
						password.resize(strlen(fe._name));
						for (size_t i = 0; i < password.size(); ++i)
							password[i] = ~(((fe._name[i] >> 4) & 0x0F) | ((fe._name[i] << 4) & 0xF0));

						if (Str::EndsWith(fe._name, ".dds", false))
							fe._size = WriteTexture(entryName, password, pakFile, vData);
						else
							fe._size = WriteData(entryName, password, pakFile, vData.data(), size);
					}
					else
					{
						std::wcerr << _T("ERROR: Cannot open file: ") << entryName << std::endl;
						throw std::exception();
					}

					std::wcout << _T("D") << g_depth << _T(": \"") << fe._name << _T("\" (") << (fe._size * 100 / size) << _T("%)") << std::endl;

					g_fileEntries.push(std::move(fe));
				}
				else
				{
					std::wcerr << _T("ERROR: Very long file name: ") << entryName << std::endl;
					throw std::exception();
				}
			}
		} while (FindNextFile(hDir, &g_fd));
		FindClose(hDir);
	}
	else
	{
		std::wcerr << _T("ERROR: Directory not found: ") << foundDir << std::endl;
		throw std::exception();
	}
	g_depth--;
}

int main(VERUS_MAIN_DEFAULT_ARGS)
{
	try
	{
		Run();
	}
	catch (const std::exception& e)
	{
		std::wcerr << _T("EXCEPTION: ") << e.what() << std::endl;
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
