#include "KVLog.h"
#include <system_error> // for  std::system_category().message()
#include <comdef.h> // for _com_error
#include <filesystem> // C++17 for filename from path
//#include <AtlConv.h>
#include <codecvt>
#include <Shlobj.h> // for special paths
//#include <psapi.h> //for GetProcessMemoryInfo
//#pragma comment(lib,"psapi.lib")

namespace KVLog
	{

	std::unique_ptr<Log> GlobalLog(new Log());

    template<class T> 
    void hash_combine(std::size_t& seed, const T& v)
    {
        std::hash<T> hasher;
        const std::size_t kMul = 0x9ddfea08eb382d69ULL;
        std::size_t a = (hasher(v) ^ seed) * kMul;
        a ^= (a >> 47);
        std::size_t b = (seed ^ a) * kMul;
        b ^= (b >> 47);
        seed = b * kMul;
    }

    // JSON doesn't allow hex values so we use a string
	template< typename T >
	std::string JHex(T i)
		{
		std::stringstream stream;
		stream << "0x"
			//<< std::setfill('0') << std::setw(sizeof(T) * 2)
			<< std::hex << i ;
		return stream.str();
		}

	// convert UTF-8 string to wstring
	std::wstring utf8_to_wstring(const std::string& str)
		{
		if(str.empty()) return std::wstring();
		int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
		std::wstring wstrTo(size_needed, 0);
		MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed);
		return wstrTo;
		}

	// convert wstring to UTF-8 string
	std::string wstring_to_utf8(const std::wstring& wstr)
		{
		if(wstr.empty()) return std::string();
		int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
		std::string strTo(size_needed, 0);
		WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);
		return strTo;
		}

	//https://stackoverflow.com/questions/7724448/simple-json-string-escape-for-c
	std::string LogEscapeJSON(const std::string& input)
		{
		std::string output;
		output.reserve(input.length());

		for(std::string::size_type i = 0; i < input.length(); ++i)
			{
			switch(input[i]) {
					case '"':
						output += "\\\"";
						break;
					case '/':
						output += "\\/";
						break;
					case '\b':
						output += "\\b";
						break;
					case '\f':
						output += "\\f";
						break;
					case '\n':
						output += "\\n";
						break;
					case '\r':
						output += "\\r";
						break;
					case '\t':
						output += "\\t";
						break;
					case '\\':
						output += "\\\\";
						break;
					default:
						output += input[i];
						break;
				}

			}

		return output;
		}


	std::string LogVectorToJSON(const LogVector& vector)
		{
		std::stringstream ss;
		bool comma = false;
		for(const auto& kv : vector)
			{
			// add a comma only starting with 2nd element
			if(comma) 
				ss << ",";
			else
				comma = true;
			ss << "\"" << kv.first << "\":";
			try 
				{
				switch(kv.second.index())
					{
					case 0: //string
						ss << "\"" << LogEscapeJSON(std::get<std::string>(kv.second)) << "\"";
						break;
					case 1: //wstring
						ss << "\"" << LogEscapeJSON(wstring_to_utf8(std::get<std::wstring>(kv.second))) << "\"";
						break;
					case 2: //int
						ss << std::get<int>(kv.second);
						break;
					case 3: //unsigned
						ss << std::get<unsigned>(kv.second);
						break;
					case 4: //double
						ss << std::get<double>(kv.second); //TODO: use a precision ?
						break;
					case 5: //float
						ss << std::get<float>(kv.second); //TODO: use a precision ?
						break;
#if _HAS_CXX20
                    case 6: //bool
						ss << (std::get<bool>(kv.second) ? "true" : "false");
						break;
#endif
#ifdef LOG_CONST_STR
                    case 7:    // char*
						ss << "\"" << LogEscapeJSON(std::get<const char*>(kv.second)) << "\"";
						break;
					case 8: //wchar_t*
						ss << "\"" << LogEscapeJSON(wstring_to_utf8(std::get<const wchar_t*>(kv.second))) << "\"";
						break;
#endif
                    }
				}
			catch(std::bad_variant_access&)
				{
				return ss.str() + "\"LogValue variant mismatched index\"";
				}
			}
		//remove last comma  
		//ss.seekp(-1, std::ios_base::end);
		//ss << "\0";
		return ss.str();
		}


    std::vector<WIN32_FIND_DATA> Glob(std::wstring folder, LPCWSTR pattern = L"*.*")
        {
        std::vector<WIN32_FIND_DATA> files;
        std::wstring search_path = folder + L"/" + pattern;
        WIN32_FIND_DATA fd;
        HANDLE hFind = ::FindFirstFile(search_path.c_str(), &fd);
        if (hFind != INVALID_HANDLE_VALUE)
            {
            do {
				if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
					{
					files.push_back(fd);
					}
            } while (::FindNextFile(hFind, &fd));
            ::FindClose(hFind);
            }
        return files;
        }

    void DeleteOldFiles(unsigned maxFilesKept, LPCTSTR folder, LPCTSTR pattern = _T("*.json"))
        {
        std::vector<WIN32_FIND_DATA> fileList = Glob(folder, pattern);
        std::sort(fileList.begin(), fileList.end(), [](WIN32_FIND_DATA t1, WIN32_FIND_DATA t2) {
            // true if the first argument is ordered before second
            return CompareFileTime(&t1.ftCreationTime, &t2.ftCreationTime) > 0;
        });    // descending = newest to oldest
        for (int f = maxFilesKept; f < fileList.size(); ++f)
            {
            std::basic_string<TCHAR> path = folder;
            path += _T("/");
            path += fileList[f].cFileName;
#ifdef _DEBUG
            SetFileAttributes(path.c_str(), fileList[f].dwFileAttributes | FILE_ATTRIBUTE_HIDDEN);
#else
            DeleteFile(path.c_str());
#endif
            }
        }


	DWORD GlobalMemoryUsage()
		{
		MEMORYSTATUSEX statex;
		statex.dwLength = sizeof(statex);
		GlobalMemoryStatusEx(&statex);
		return statex.dwMemoryLoad;
		}


	//// 
	//ULONG ProcessMemoryUsage()
	//	{
	//	PROCESS_MEMORY_COUNTERS_EX pmc;
	//	GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc));
	//	return pmc.PrivateUsage;
	//	}


	std::basic_string<TCHAR> Win32ErrorMessage(DWORD errCode)
		{
		TCHAR* errbuf = NULL;
		std::basic_string<TCHAR> ret;
		if(FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
						  NULL, errCode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (TCHAR*)&errbuf, 0, NULL))
			{
			ret = errbuf;
			LocalFree(errbuf);
			}
		else
			{
#ifdef _UNICODE
			ret = std::to_wstring(errCode) + _T("?\r\n");
#else		
			ret = std::to_string(errCode) + _T("?\r\n");
#endif 
			}
		return ret;
		}

	// Needs <comdef.h>
	std::basic_string<TCHAR> ComErrorMessage(DWORD hResult)
		{
		_com_error err(hResult);
		return err.ErrorMessage();
		// TODO: see if _com_error::Description is more useful
		}

	// C++ 11
	// Needs <system_error>
	// no wide version
	std::string StdSystemErrorMessage(DWORD hResult)
		{
		return std::system_category().message(hResult);
		}



	void Log::Path()
		{
		TCHAR specialPath[MAX_PATH + 1] = { _T("\0") };
		if(!SHGetSpecialFolderPath(NULL, specialPath, CSIDL_COMMON_APPDATA, TRUE))
			{
			GetTempPath(MAX_PATH, specialPath);
			}

		SYSTEMTIME st;
		GetLocalTime(&st);

		std::basic_stringstream<TCHAR> ss;

		ss << specialPath << _T("\\");
		if(subFolder1)
			{
			ss << subFolder1 << _T("\\");
			CreateDirectory(ss.str().c_str(), NULL);
			}
		if(subFolder2)
			{
			ss << subFolder2 << _T("\\");
			CreateDirectory(ss.str().c_str(), NULL);
			}
		ss << st.wYear << _T("-") << st.wMonth << _T("-") << st.wDay;
		ss << _T("_") << st.wHour << _T("_") << st.wMinute << _T("_") << st.wSecond;
		ss << _T(".") << st.wMilliseconds;
		ss << _T(".json");
		logPath = ss.str();
		}


	void Log::Rotate()
		{
		if(0==maxSizeB || hLogFile == INVALID_HANDLE_VALUE)
			return;
		LONGLONG fileSize=0;
		if(!GetFileSizeEx(hLogFile, (PLARGE_INTEGER)&fileSize))
			return;
		if(fileSize > maxSizeB)
			{
			//DWORD writtenB = 0;
			//std::basic_string<TCHAR> buf(_T("Max log size reached"));
			//WriteFile(hLogFile, buf.c_str(), buf.length() * sizeof(TCHAR), &writtenB, NULL);
            DeleteOldFiles(maxFiles-1, std::filesystem::path(logPath).parent_path().generic_wstring().c_str(), _T("*.json"));
            CloseFile();
			Path();
			OpenFile();
			}
		}


	//..........................................................................
	bool Log::OpenFile()
		{
		EnterCriticalSection(&section);
		DWORD attributes = FILE_ATTRIBUTE_ARCHIVE | FILE_ATTRIBUTE_NOT_CONTENT_INDEXED; //FILE_ATTRIBUTE_TEMPORARY - avoids writing to disk
		if(encrypted)
			attributes |= FILE_ATTRIBUTE_ENCRYPTED;
		if(writeThrough)
			attributes |= FILE_FLAG_WRITE_THROUGH;
		hLogFile = CreateFile(logPath.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ, 0, OPEN_ALWAYS, attributes, 0);
		if(hLogFile == INVALID_HANDLE_VALUE) 
			{
			LeaveCriticalSection(&section);
			return false;
			}
		LeaveCriticalSection(&section);
		return true;
		}


	//..........................................................................
	void Log::CloseFile()
		{
		EnterCriticalSection(&section);
		if(hLogFile)
			{
			//FlushFileBuffers(hLogFile);
			CloseHandle(hLogFile);
			hLogFile = INVALID_HANDLE_VALUE;
			}
		LeaveCriticalSection(&section);
		}


	bool Log::ItemToDisk(LogMap::iterator it)
		{
		DWORD writtenB = 0;
		Rotate();
		if(hLogFile != INVALID_HANDLE_VALUE)
			{
			std::stringstream ss;

			LONGLONG fileSize = 0;
			if(GetFileSizeEx(hLogFile, (PLARGE_INTEGER)&fileSize))
				{
				if(fileSize>0)
					ss << ",\r\n";
				}
			else
				ss << ",\r\n";

			ss << "{";
			ss << "\"_COUNT_\":" << it->second.first;
			ss << ",";
			ss << LogVectorToJSON(it->first);
			//ss << "\r\n";
			ss << ",\"_EXTRA_\": {" << LogVectorToJSON(it->second.second) << "}";
			ss << "}";

			std::string buffer(ss.str());
			if(buffer.length() > 0)
				WriteFile(hLogFile, buffer.c_str(), buffer.size(), &writtenB, NULL);
			}
		return writtenB > 0;
		}

	bool Log::FlushMap()
		{
#ifdef LOG_TO_DISK_OPEN_CLOSE
		OpenFile();
#endif
        EnterCriticalSection(&section);
        //TODO: write [ and ] to make it a full JSON
        std::map<std::string, LogMap::iterator> chronologicalMap;
        int u = 0; //this will prevent time aliasing under 1 ms
		for(LogMap::iterator it = events.begin(); it!= events.end(); ++ it)
			{
			//ItemToDisk(it);
            auto LogValues = it->second.second; //map second value and pair also second value
            auto timeIt = std::find_if(LogValues.begin(), LogValues.end(),
                [](KV& element) 
                    {
                    return element.first == "_TIME_";
                    });
            if (timeIt == LogValues.end()) // if no time found write directly
                {
                ItemToDisk(it);
                }
            else //insert in chronological list
                {
                try
                    {
                    std::string timeStamp = std::get<std::string>(timeIt->second);
                    chronologicalMap[timeStamp + JHex(u)] = it;
                    u++;
                    }
                catch (std::bad_variant_access&)
                    {
                    ItemToDisk(it);
                    }
                }
			}
        // we cannot leave the critical section yet, as we hold pointers in the chronologicalMap, not values
        for (std::map<std::string, LogMap::iterator>::iterator itc = chronologicalMap.begin(); itc != chronologicalMap.end(); ++itc)
            {
            ItemToDisk(itc->second);
            }
        LeaveCriticalSection(&section);
#ifdef LOG_TO_DISK_OPEN_CLOSE
		CloseFile();
#endif
		return false;
		}


	//________________________________________________________________________________________
	void _TESTS_()
		{
		LogVector log;
		log.emplace_back(KV{ "port", 22 });
		log.emplace_back(KV{ "host", L"example.com" });
		log.emplace_back(KV{ "open", true });
		log.emplace_back(KV{ "proto", "tcp" });
		log.emplace_back(KV{ "duration", 1.3});

		LogVector log2{ KV{ "port", 22 },
						KV{ "host", L"example.com" },
						KV{ "open", true },
						KV{ "proto", "tcp" },
						KV{ "duration", 1.3 }
						};

		LogVector extra{KV{ "context", "firewall" },
						KV{ "rule", "learn" },
						KV{ "timeout", 4.5 }
			};

		GlobalLog->LogBasic(Level::USER_Status,log,extra);
		GlobalLog->LogBasic(Level::USER_Status,log2, extra); //dupe
		GlobalLog->LogMessage(Level::Error, L"hi!" , LogVector { KV {"test", 1}, KV {"_LEVEL_", Level::USER_Status} }, extra);

		unsigned start = GetTickCount64();

		for(int k = 0; k < 100000; k++)
			{
			LogVector log3{ KV{ "port", rand() },
							KV{ "host", L"example.com" },
							KV{ "open", true },
							KV{ "proto", u8"tcp" },
							KV{ "str", std::string{"123"} },
							KV{ "wstr", std::wstring{L"1234"} },
							KV{ "duration", 1.3 }
							};

			LogVector info{ KV {"#", k},
							KV {"GlobalMem[%]", (int)GlobalMemoryUsage()},
							//KV {"ProcMem[MB]", (int)ProcessMemoryUsage()>>20},
							KV {"Tick", (int)GetTickCount64()},
							};
			
			GlobalLog->Add(log3, info);

			}

		unsigned stop = GetTickCount64();

		GlobalLog->LogMessage(Level::USER_Info, L"Loop logging finished" ,LogVector{ KV {"duration[ms]", (int)(stop-start)}, KV {"LEVEL", Level::USER_Status} });

		SetLastError(ERROR_IPSEC_QM_POLICY_IN_USE);
		GlobalLog->LogSource(Level::Trace, __LINE__, __FUNCTION__, __FILE__, 0xbaadf00d, L"The current PID is %u", GetCurrentProcessId());
        GlobalLog->LogLastError(Level::Error,L"Windows error");
        SetLastError(0);
        GlobalLog->LogSource(Level::Trace, __LINE__, __FUNCTION__, __FILE__, -1, L"The current PID is %u", GetCurrentProcessId());
        
        GlobalLog->LogFormat(Level::Trace, L"The current CPU is %u", GetCurrentProcessorNumber());
		
        GlobalLog->FlushMap();
		}

	}