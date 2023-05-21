//Author: Vlad VARNA
//License: Public domain

#pragma once

#include <string>
#include <Windows.h>

class UProgress
{
protected:
	HANDLE			m_hThread;
	HANDLE			m_hEvent;
	unsigned int	m_ThreadId;
	HWND			m_hParentWnd;
	HWND			m_hDlgWnd;			
	HWND			m_hProgressWnd;		
	HWND			m_hAnimWnd;		
	std::wstring	m_Caption;			
	std::wstring	m_ProgressMsg;	//TODO: add critical section	
	int				m_W,m_H,m_X,m_Y;
	RECT			m_TextBox;
	//RECT			m_IconBox;
	HCURSOR			m_hAnimCur;
	HFONT			m_hFont;
	bool			m_TopMost;

public:
	UProgress(HWND hParent=NULL, WCHAR*text=NULL, int w=400, int h=140, HCURSOR hCursor=NULL, int x=-1, int y=-1,bool topmost=false);
	~UProgress();														

	UProgress(const UProgress&) = delete;
	UProgress& operator=(const UProgress&) = delete;

	bool Start();
	void Stop();
	void SetRange(UINT rangeA=0,UINT rangeB=100);
	void SetPos(UINT procent=0);
	void SetMarquee(bool on=true,int T=100);
	void SetIcon(HICON hico=NULL);
	void SetCaption(LPCWSTR windowName);
	void SetMessage(LPCWSTR text);
	
protected:
	static LRESULT CALLBACK WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
	static unsigned int __stdcall ThreadProc(void* lpParameter);
};

