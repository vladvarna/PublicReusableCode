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

#include <string>
#include <memory>
#include <vector>
#include <tchar.h>
#include <stdarg.h>
#include <stdio.h>
#include <handleapi.h>
//#include <assert.h>
//#include <crtdbg.h>

//TODO: criticality


//Error Level:
#define E_USER_INFO				0x1	//
#define E_USER_NOTIFICATION	    0x2	//
#define E_USER_STATUS			0x3	//

#define E_WARNING 				0x5	//

#define E_ERROR   				0x8	//e 

#define E_TRACE		 			0xa	//message to trace program

#define E_TOHANDLE				0xd	//temporary used
#define E_FATAL					0xe	//abort
#define E_EXCEPTION				0xe	//exception
#define E_LEVEL_MASK			0xF	//severity level mask


namespace FlexibleLog
	{

	class FLog
		{
		private:
			CRITICAL_SECTION section;
			HANDLE hLogFile;
			char sep = '\t';
			bool encrypted;	
			bool writeThrough; //https://learn.microsoft.com/en-us/windows/win32/fileio/file-caching
			//TODO: investigate using no buffering, but must write 512/4K aligned: https://learn.microsoft.com/en-us/windows/win32/fileio/file-buffering

		public:
			const enum LogFormat
				{
				Text,
				CSV,
				JSON,
				JSON5, // TODO: https://json5.org/
				YAML,
				//XML, // reserved
				} format;

			std::basic_string<TCHAR> logPath;
			unsigned maxSizeB;


			//..........................................................................
			FLog(LogFormat fileFormat = LogFormat::JSON, unsigned maxSizeMB = 10, bool shouldEncrypt = true, bool shouldWriteThrough = true):
				format(fileFormat),
				maxSizeB(maxSizeMB << 20),
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
			~FLog()
				{
				CloseFile();
				}


			//..........................................................................
			int Log(int type_,int line_,const char*func_,const char*file_,int fail_,LPCTSTR formstr,...)
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
				//EnterCriticalSection(&section);
				ToDisk(type_,fail_,buffer, last, func_, line_, file_, st);
				//LeaveCriticalSection(&section);
				return 0;
				}



			void Path(LPCTSTR subFolder1 = _T("NLOK"), LPCTSTR subFolder2 = _T("Firewall"));
			void Rotate();
			bool OpenFile();
			void CloseFile();
			bool ToDisk(int ltype,int lfail,TCHAR*lmsg, DWORD winLast, const char* lfunc, int line,const char* file, SYSTEMTIME &st);
		};

	
    extern std::unique_ptr<FLog> GlobalLog;

	//switch to an external logger ---------------------------------------------------------------------------------------
	inline void UseExternalLogger(FLog*extLogger)	{	GlobalLog=std::unique_ptr<FLog>(extLogger);	}


// output (to log)
	#define LOG(type,fail,msg,...) GlobalLog->Log(type,__LINE__,__FUNCTION__,__FILE__,fail,msg,__VA_ARGS__)

	void _TESTS_();

	}