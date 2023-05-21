#include "FlexibleLog.h"
#include <sstream>
#include <system_error> // for  std::system_category().message()
#include <comdef.h> // for _com_error
#include <filesystem> // C++17 for filename from path
//#include <AtlConv.h>
#include <codecvt>
#include <Shlobj.h>
#include <psapi.h> //for GetProcessMemoryInfo
#pragma comment(lib,"psapi.lib")

namespace FlexibleLog
	{

	std::unique_ptr<FLog> GlobalLog(new FLog());

	// JSON doesn't allow hex values so we use a string
	template< typename T >
	std::string JHex(T i)
		{
		std::stringstream stream;
		stream << "\"0x"
			//<< std::setfill('0') << std::setw(sizeof(T) * 2)
			<< std::hex << i <<"\"";
		return stream.str();
		}

	template< typename T >
	std::string YHex(T i)
		{
		std::stringstream stream;
		stream << "0x"
			//<< std::setfill('0') << std::setw(sizeof(T) * 2)
			<< std::hex << i << "";
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


	std::wstring LogEscapeYAML(const std::wstring& input)
		{
		std::wstring output;
		output.reserve(input.length());

		for(std::string::size_type i = 0; i < input.length(); ++i)
			{
			switch(input[i]) {
					case ':':
						output += L"：";
						break;
					case '-':
						output += L"‒";
						break;
					default:
						output += input[i];
						break;
				}

			}

		return output;
		}

	DWORD GlobalMemoryUsage()
		{
		MEMORYSTATUSEX statex;
		statex.dwLength = sizeof(statex);
		GlobalMemoryStatusEx(&statex);
		return statex.dwMemoryLoad;
		}


	// 
	ULONG ProcessMemoryUsage()
		{
		PROCESS_MEMORY_COUNTERS_EX pmc;
		GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc));
		return pmc.PrivateUsage;
		}


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



	void FLog::Path(LPCTSTR subFolder1, LPCTSTR subFolder2)
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
		if(format == LogFormat::Text)
			ss << _T(".log");
		else if(format == LogFormat::CSV)
			ss << _T(".csv");
		else if(format == LogFormat::JSON || format == LogFormat::JSON5)
			ss << _T(".json");
		else if(format == LogFormat::YAML)
			ss << _T(".yml");
		//else if(format == LogFormat::XML)
		//	ss << _T(".xml");

		logPath = ss.str();
		}


	void FLog::Rotate()
		{
		if(0==maxSizeB || hLogFile == INVALID_HANDLE_VALUE)
			return;
		LONGLONG fileSize=0;
		if(!GetFileSizeEx(hLogFile, (PLARGE_INTEGER)&fileSize))
			return;
		if(fileSize > maxSizeB)
			{
			DWORD writtenB = 0;
			std::basic_string<TCHAR> buf(_T("Max log size reached"));
			WriteFile(hLogFile, buf.c_str(), buf.length() * sizeof(TCHAR), &writtenB, NULL);
			CloseFile();
			Path();
			OpenFile();
			}
		}


	//..........................................................................
	bool FLog::OpenFile()
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
	bool FLog::ToDisk(int ltype, int lfail, TCHAR* lmsg, DWORD winLast, const char* lfunc, int line,const char* file, SYSTEMTIME& st)
		{
		DWORD writtenB=0;
#ifdef LOG_TO_DISK_OPEN_CLOSE
		OpenFile();
#endif
		Rotate();
		EnterCriticalSection(&section);
		if(hLogFile != INVALID_HANDLE_VALUE)
			{
			std::stringstream ss;

			if(format == LogFormat::Text || format == LogFormat::CSV)
				{
				ss << wstring_to_utf8(lmsg) << sep << lfail << sep << lfunc << sep << GetCurrentThreadId() << "\r\n";
				}
			else if(format == LogFormat::JSON)
				{
				ss << "{\t\"message\": \"" << LogEscapeJSON( wstring_to_utf8(lmsg)) << "\",\r\n";
				ss << "\t\"result\": " << JHex(lfail) << ",\r\n";
				ss << "\t\"time\": \"" << st.wYear << "-" << st.wMonth << "-" << st.wDay;
				ss << " " << st.wHour << ":" << st.wMinute << ":" << st.wSecond;
				ss << "." << st.wMilliseconds << "\",\r\n";
				ss << "\t\"type\": " << JHex(ltype) << ",\r\n";
				ss << "\t\"memusage%\": " << GlobalMemoryUsage() << ",\r\n";
				ss << "\t\"memprivateMB\": " << (ProcessMemoryUsage() >> 20) << ",\r\n";
				ss << "\t\"source\": {\r\n";
				ss << "\t\t\"thread\": " << GetCurrentThreadId() << ", \r\n";
				//ss << "\t\t\"pid\":" << GetCurrentProcessId() << ",\r\n";
				ss << "\t\t\"func\": \"" << lfunc << "\",\r\n";
#ifdef _DEBUG
				ss << "\t\t\"filepath\": \"" << LogEscapeJSON(std::filesystem::path(file).parent_path().generic_u8string()) << "\",\r\n"; //it's a full path with backslashes
#endif
				ss << "\t\t\"filename\": \"" << LogEscapeJSON(std::filesystem::path(file).filename().generic_u8string()) << "\",\r\n";
				ss << "\t\t\"line\": " << line << " }\r\n";
				ss << "},\r\n";
				}
			else if(format == LogFormat::YAML)
				{
				ss << "- entry: \r\n";
				ss << "   message: \r\n";
				ss << "     " << wstring_to_utf8(LogEscapeYAML(lmsg)) << "\r\n";
				ss << "   result: \r\n";
				ss << "     -" << YHex(lfail) << "\r\n";
				ss << "     -" << lfail << "\r\n";
				//ss << "     -" << wstring_to_utf8(Win32ErrorMessage(lfail)); // has CRLF
				ss << "     -" << wstring_to_utf8(ComErrorMessage(lfail)) << "\r\n";
				//ss << "     -" << StdSystemErrorMessage(lfail) << "\r\n";
				ss << "   time: \r\n";
				ss << "     " << st.wYear << "-" << st.wMonth << "-" << st.wDay;
				ss << " " << st.wHour << ":" << st.wMinute << ":" << st.wSecond;
				ss << "." << st.wMilliseconds << "\r\n";
				if(winLast)
					{
				ss << "   last: \r\n";
				ss << "     -" << YHex(winLast) << "\r\n";
				ss << "     -" << wstring_to_utf8(Win32ErrorMessage(winLast)); // has CRLF;
				//ss << "     -" << wstring_to_utf8(ComErrorMessage(winLast)) << "\r\n";
				//ss << "     -" << StdSystemErrorMessage(winLast) << "\r\n";
					}
				ss << "   type: \r\n";
				ss << "     " << YHex(ltype) << "\r\n";
				ss << "   memusage%: \r\n";
				ss << "     " << GlobalMemoryUsage() << "\r\n";
				ss << "   memprivateKB: \r\n";
				ss << "     " << (ProcessMemoryUsage() >> 10) << "\r\n";
				ss << "   source: \r\n";
				ss << "     thread: \r\n";
				ss << "       " << GetCurrentThreadId() << "\r\n";
				//ss << "     pid: \r\n";
				//ss << "       " << GetCurrentProcessId() << "\r\n";
				ss << "     func: \r\n";
				ss << "       " << lfunc << "\r\n";
#ifdef _DEBUG
				ss << "     filepath: \r\n";
				ss << "       " << std::filesystem::path(file).parent_path().generic_u8string() << "\r\n"; //it's a full path with colon, but we don't need to escape it, we also can't escape it
#endif
				ss << "     filename: \r\n";
				ss << "       " << std::filesystem::path(file).filename().generic_u8string() << "\r\n"; 
				ss << "     line: \r\n";
				ss << "       " << line << "\r\n";
				ss << "\r\n";
				}


			std::string buffer(ss.str());
			if (buffer.length() > 0)
				WriteFile(hLogFile, buffer.c_str(), buffer.size(), &writtenB, NULL);
			}
		LeaveCriticalSection(&section);
#ifdef LOG_TO_DISK_OPEN_CLOSE
		CloseFile();
#endif
		return writtenB>0;
		}


	//..........................................................................
	void FLog::CloseFile()
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


	//________________________________________________________________________________________
	void _TESTS_()
		{
		LOG(E_ERROR,2,_T("%s %x"),_T("beta"),0x12345678);
		LOG(E_WARNING,1,_T("%s %x"),_T("alpha"),0x1234);
		LOG(E_ERROR,3,_T("%s %x"),_T("gamma"),0xabcd);
		SetLastError(ERROR_IPSEC_QM_POLICY_IN_USE);
		LOG(E_ERROR,3,_T("%s %x"),_T("GetLastError"),0xabcd);
		SetLastError(0);
		LOG(E_WARNING,7,_T("%s %x"),_T("gamma"),0xabcd);
		LOG(E_WARNING, ERROR_API_UNAVAILABLE,_T("Win32 error") );
		LOG(E_USER_INFO, E_NOINTERFACE, _T("COM HRESULT"));
		LOG(E_TRACE,7,_T("%s %x"),_T("gamma"),0xabcd);
		LOG(E_TRACE, -1, _T("- \\ / \" ' : "));
		LOG(E_TRACE, -1, _T("%s"), _T("Ḽơᶉëᶆ ȋṕšᶙṁ ḍỡḽǭᵳ ʂǐť ӓṁệẗ, ĉṓɲṩḙċťᶒțûɾ ấɖḯƥĭṩčįɳġ ḝłįʈ, șếᶑ ᶁⱺ ẽḭŭŝḿꝋď ṫĕᶆᶈṓɍ ỉñḉīḑȋᵭṵńť ṷŧ ḹẩḇőꝛế éȶ đꝍꞎôꝛȇ ᵯáꞡᶇā ąⱡîɋṹẵ"));
		LOG(0, -1, _T("𝘈Ḇ𝖢𝕯٤ḞԍНǏ𝙅ƘԸⲘ𝙉০Ρ𝗤Ɍ𝓢ȚЦ𝒱Ѡ𝓧ƳȤѧᖯć𝗱ễ𝑓𝙜Ⴙ𝞲𝑗𝒌ļṃŉо𝞎𝒒ᵲꜱ𝙩ừ𝗏ŵ𝒙𝒚ź"));
		LOG(0,4,_T("%s %x"),_T("delta"),0xabcd);
		}

	}