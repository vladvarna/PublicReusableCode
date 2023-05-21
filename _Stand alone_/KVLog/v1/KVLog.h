#pragma once

#if !defined(_STDAFX_H_) && !defined(PCH_H)
	#define WIN32_LEAN_AND_MEAN
//	#define _CRT_SECURE_NO_WARNINGS
	#ifndef VC_EXTRALEAN
		#define VC_EXTRALEAN            // Exclude rarely-used stuff from Windows headers
	#endif
	#include <windows.h>

	#ifndef _CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES
	#define _CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES 1
	#endif
	#ifndef _CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES_COUNT 
	#define _CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES_COUNT  1
	#endif
	
	#define _ATL_CSTRING_EXPLICIT_CONSTRUCTORS      // some CString constructors will be explicit

#endif

#if !_HAS_CXX17
    #error Must use at least C++17 to compile this
#endif


#include <string>
#include <sstream>
#include <vector>
#include <variant>
#include <map>
#include <unordered_map>
#include <iomanip>

#include <memory>

#include <tchar.h>
#include <stdarg.h>
//#include <stdio.h>
//#include <handleapi.h>
//#include <assert.h>
//#include <crtdbg.h>

//TODO: criticality, mem size + time limit, unordered_map


namespace KVLog
	{
	//Error Level:
	enum Level
		{
		USER_Info=				0x1,	//
		USER_Notification=		0x2,	//
		USER_Status=			0x3,	//

		Warning= 				0x7,	//
		Error=   				0x8,	// 
		Verbose=				0x9,	//
		Trace=		 			0xa,	//

		Exception=				0xe,	//exception
		};


	using LogValue = std::variant<
        std::string, 
        std::wstring, 
        int, 
        unsigned, 
        double, 
        float 
#if _HAS_CXX20
        ,bool 
#endif
#ifdef LOG_CONST_STR
        ,const char* 
        ,const wchar_t*
#endif
        >;

    template<typename T> std::string JHex(T i);    // output HEX value in JSON
    std::basic_string<TCHAR> Win32ErrorMessage(DWORD errCode);

    // https://stackoverflow.com/questions/17016175/c-unordered-map-using-a-custom-class-type-as-the-key

	using KV = std::pair<std::string, LogValue>;

	using LogVector = std::vector< KV >;

    std::string LogVectorToJSON(const LogVector& vector);

    struct LogHasher
        {
        std::size_t operator()(const LogVector& k) const
            {
            return std::hash<std::string>()(LogVectorToJSON(k));
            }
        };


	using LogMap = std::unordered_map< LogVector, std::pair<int, LogVector>, LogHasher >;


	class Log
		{
		private:
			CRITICAL_SECTION section;
			HANDLE hLogFile;
			bool encrypted;	
			bool writeThrough; //https://learn.microsoft.com/en-us/windows/win32/fileio/file-caching
			//TODO: investigate using no buffering, but must write 512/4K aligned: https://learn.microsoft.com/en-us/windows/win32/fileio/file-buffering

			LogMap events;
		public:

			std::basic_string<TCHAR> logPath;
			unsigned maxSizeB;
            unsigned maxFiles;
            unsigned maxItems;
            LPCTSTR subFolder1;
            LPCTSTR subFolder2;

			//..........................................................................
            Log(LPCTSTR subFolder2_ = _T("Logs"),
                LPCTSTR subFolder1_ = _T("GEN"),
                unsigned maxSizeMB = 10,
                unsigned maxFilesToKeep = 3, 
                unsigned maxItemsInMem = 10000000, 
                bool shouldEncrypt = true, 
                bool shouldWriteThrough = true):
			        subFolder1(subFolder1_),
                    subFolder2(subFolder2_),
                    maxSizeB(maxSizeMB << 20),
			        maxFiles(maxFilesToKeep),
                    maxItems(maxItemsInMem),
                    encrypted(shouldEncrypt),
			        writeThrough(shouldWriteThrough),
			        hLogFile(INVALID_HANDLE_VALUE)
				{
				InitializeCriticalSection(&section);
				Path();
#ifndef LOG_TO_DISK_OPEN_CLOSE
				OpenFile();
#endif
				};


			//..........................................................................
			~Log()
				{
				CloseFile();
				}


			bool Add(LogVector& logKeys, LogVector& logValues)
				{
                EnterCriticalSection(&section);

                if(events.find(logKeys) == events.end()) //key not in map
					{
                    if (events.size() > maxItems) // if too many in memory, write to disk and start over
                        {
                        FlushMap();
                        events.clear();
                        }
                    SYSTEMTIME st;
                    GetLocalTime(&st);
                    std::stringstream ss;
                    ss << std::setfill('0') << std::setw(4);
                    ss << st.wYear << "-" << std::setw(2) << st.wMonth << "-" << std::setw(2) << st.wDay;
                    ss << " " << std::setw(2) << st.wHour << ":" << std::setw(2) << st.wMinute << ":" << std::setw(2) << st.wSecond;
                    ss << "." << std::setw(3) << st.wMilliseconds;
                    logValues.emplace_back(KV{ "_TIME_", ss.str() });

                    events[logKeys] = std::pair(1 , logValues);
                    LeaveCriticalSection(&section);
                    return true;
					}
				else
					{
					events[logKeys].first ++;
					//TODO: merge values ?
                    LeaveCriticalSection(&section);
                    return false;
					}

                }


			bool LogBasic(Level level, LogVector logKeys = LogVector(), LogVector logValues = LogVector())
                {
                logKeys.emplace_back(KV{ "_LEVEL_", level });
                logKeys.emplace_back(KV{ "_THREAD_", (unsigned)GetCurrentThreadId() });

                return Add(logKeys, logValues);
                }


			bool LogMessage(Level level, LPCTSTR message, LogVector logKeys = LogVector(), LogVector logValues = LogVector())
                {
                logKeys.emplace_back(KV{ "_MSG_", message });
                return LogBasic(level, logKeys, logValues);
                }


			bool LogLastError(Level level, LPCTSTR message, LogVector logKeys = LogVector(), LogVector logValues = LogVector())
                {
                DWORD lastError = GetLastError();
                if (lastError == ERROR_SUCCESS)
                    return false;
                logKeys.emplace_back(KV{ "_GetLastError_", JHex(lastError) });
                logKeys.emplace_back(KV{ "_GetLastErrorText_", Win32ErrorMessage(lastError) });
                logKeys.emplace_back(KV{ "_MSG_", message });

                return LogBasic(level, logKeys, logValues);
                }


            //..........................................................................
			int LogFormat(Level level, LPCTSTR formstr,...)
				{
				DWORD last=GetLastError();
				SYSTEMTIME st;
				GetLocalTime(&st);
				TCHAR buffer[0x10000]={0};
				if(formstr)
					{
					va_list vparam;
					va_start(vparam,formstr);
					_vstprintf_s(buffer,sizeof(buffer)/sizeof(buffer[0]),formstr,vparam);
					va_end(vparam);
					}
				LogMessage(level, buffer); 
                return 0;
				}


			//..........................................................................
			int LogSource(Level level,int line,const char*func,const char*file,int errorCode,LPCTSTR formstr,...)
				{
				TCHAR buffer[0x10000]={0};
				if(formstr)
					{
					va_list vparam;
					va_start(vparam,formstr);
					_vstprintf_s(buffer,sizeof(buffer)/sizeof(buffer[0]),formstr,vparam);
					va_end(vparam);
					}
                LogVector logKeys;
                logKeys.emplace_back(KV{ "__FUNCTION__", std::string(func) } );
                logKeys.emplace_back(KV{ "__LINE__", line });
            #ifdef _DEBUG // file paths are a privacy concern
                logKeys.emplace_back(KV{ "__FILE__", std::string(file) });
            #endif
                logKeys.emplace_back(KV{ "_ERRORCODE_", errorCode });
                logKeys.emplace_back(KV{ "_GetLastError_", JHex(GetLastError()) });

                LogMessage(level, buffer, logKeys);
                return 0;
				}


            bool ItemToDisk(LogMap::iterator);
			bool FlushMap();
			void Path();
			void Rotate();
			bool OpenFile();
			void CloseFile();
		};

	
    extern std::unique_ptr<Log> GlobalLog;


// output (to log)
	#define LOG(type,fail,msg,...) GlobalLog->Log(type,__LINE__,__FUNCTION__,__FILE__,fail,msg,__VA_ARGS__)

	void _TESTS_();

	}