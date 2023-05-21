//Author: Vlad VARNA
//License: Public domain

#pragma once

#include <string>
#include <Windows.h>

class UProgress
{
protected:
	HANDLE				m_hThread;
	HANDLE				m_hEvent;
	unsigned int	m_ThreadId;
	HWND					m_hParentWnd;
	HWND					m_hDlgWnd;			
	HWND					m_hProgressWnd;		
	HWND					m_hAnimWnd;		
	std::wstring	m_Caption;			
	std::wstring	m_ProgressMsg;		
	int						m_W,m_H;
	RECT					m_TextBox;
	//RECT					m_IconBox;
	HCURSOR				m_hAnimCur;
	HFONT					m_hFont;

public:
	UProgress(HWND hParent=NULL, WCHAR*text=NULL, int w=400, int h=140, HCURSOR hCursor=NULL);
	~UProgress();														

	bool Start();
	void Stop();
  void SetPos(UINT procent=0);
	void SetPos(UINT procent, const wchar_t* msg );
	void SetPos(UINT procent, UINT nResourceID = 0, HMODULE hModule = NULL );
	void SetMarquee(bool on=true,int T=100);
	void SetIcon(HICON hico=NULL);
	void SetCaption(LPCWSTR windowName);

protected:
	static LRESULT CALLBACK WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
	static unsigned int __stdcall ThreadProc(void* lpParameter);
};

