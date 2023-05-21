//Author: Vlad VARNA
//License: Public domain

#include "stdafx.h"
#include "UProgress.h"
#include <CommCtrl.h>
#include <process.h>


//CTOR................................................................................................................................................
UProgress::UProgress(HWND hParent, WCHAR*text, int w, int h, HCURSOR hCursor, int x, int y, bool topmost):
					 m_hParentWnd(hParent),
                     m_W(w),m_H(h),m_X(x),m_Y(y),
                     m_TopMost(topmost),
					 m_hThread(NULL),
                     m_hDlgWnd(NULL),
                     m_ThreadId(0),
                     m_hProgressWnd(NULL),
                     m_hEvent(NULL),
                     m_hAnimWnd(NULL)
	{
	ZeroMemory(&m_TextBox,sizeof(RECT));
	//ZeroMemory(&m_IconBox,sizeof(RECT));
	if(hCursor)
		m_hAnimCur=hCursor;
	else
		{
		TCHAR windir[MAX_PATH];
		GetWindowsDirectory(windir,MAX_PATH);
		_tcscat_s(windir, MAX_PATH, _T("/Cursors/aero_busy.ani"));
		m_hAnimCur=LoadCursorFromFile(windir);
		}
	if(!m_hAnimCur)
		m_hAnimCur=LoadCursor(NULL,IDC_WAIT);
	if(text)
		{
		m_Caption=text;
		m_ProgressMsg=text;
		}
	//adjust DPI
	HDC hldc=::GetDC(NULL);
	double xdpi=GetDeviceCaps(hldc,LOGPIXELSX)/96.;	//minimum is 96DPI
	double ydpi=GetDeviceCaps(hldc,LOGPIXELSY)/96.;	
	m_W=(int)(m_W*xdpi);
	m_H=(int)(m_H*ydpi);
	int cellh=MulDiv(-9,GetDeviceCaps(hldc,LOGPIXELSY),72);
	ReleaseDC(NULL,hldc);
	
	m_hFont=CreateFont(cellh,0,0,0,FW_SEMIBOLD,0,0,0,DEFAULT_CHARSET,OUT_TT_PRECIS,CLIP_DEFAULT_PRECIS,PROOF_QUALITY,FF_DONTCARE|DEFAULT_PITCH,_T("MS Shell Dlg"));
	}

//DTOR................................................................................................................................................
UProgress::~UProgress()
	{
	Stop();
	DeleteObject(m_hFont);
	}

//................................................................................................................................................
bool UProgress::Start()
	{
	if(m_hThread==NULL)
		{
		m_hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
		m_hThread = (HANDLE)_beginthreadex(NULL, 0, ThreadProc, static_cast<LPVOID>(this), 0, &m_ThreadId);
		SetThreadPriority(m_hThread,THREAD_PRIORITY_ABOVE_NORMAL);
		if(WaitForSingleObject(m_hEvent, 3*1000)!=WAIT_OBJECT_0)
			{
			return false;
			}
		}
	else
		{
		PostThreadMessage( m_ThreadId, WM_ACTIVATE, WA_CLICKACTIVE, 0 );
		}
	return true;
	}

//................................................................................................................................................
void UProgress::Stop()
	{
	if(m_hThread)
		{
		PostThreadMessage(m_ThreadId, WM_QUIT, 0, 0);
		if(WaitForSingleObject(m_hThread, 20*1000) == WAIT_TIMEOUT)
			{
			::TerminateThread(m_hThread, -7);
			}
		CloseHandle(m_hThread);
		CloseHandle(m_hEvent);
		}
	m_hThread = NULL;
	}

//................................................................................................................................................
unsigned int __stdcall UProgress::ThreadProc( void* lpParameter )
	{
	UProgress* pThis = static_cast<UProgress*>(lpParameter);

	WNDCLASSEX wndcls = {0};
	wndcls.cbSize=sizeof(WNDCLASSEX);
	wndcls.style = 0;
	wndcls.lpfnWndProc = WndProc; 
	wndcls.hInstance = GetModuleHandle(NULL);
	wndcls.hCursor = LoadCursor(NULL, IDC_APPSTARTING);
	wndcls.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
	wndcls.lpszClassName = L"WC_UNIVERSAL_PROGRESS_BAR_V";
	//wndcls.hIcon = LoadIcon(wndcls.hInstance, MAKEINTRESOURCE(128));
	//wndcls.hIconSm = LoadIcon(wndcls.hInstance, MAKEINTRESOURCE(128));

	TCHAR thisExe[MAX_PATH];
	GetModuleFileName(wndcls.hInstance,thisExe,MAX_PATH);
	ExtractIconEx(thisExe,0,&wndcls.hIcon,&wndcls.hIconSm,1); //get main icon
	
	RegisterClassEx(&wndcls);

	RECT rcDesktop = { 0 };
	SystemParametersInfo(SPI_GETWORKAREA, NULL, &rcDesktop, NULL);
	if(pThis->m_X>=0)
		rcDesktop.left = pThis->m_X;
	else
		rcDesktop.left = (rcDesktop.right + rcDesktop.left - pThis->m_W)/2;
	if(pThis->m_Y>=0)
		rcDesktop.top = pThis->m_Y;
	else
		rcDesktop.top = (rcDesktop.top + rcDesktop.bottom - pThis->m_H)/2;

	
	//
	pThis->m_hDlgWnd = CreateWindowEx(WS_EX_LAYERED|(pThis->m_TopMost?WS_EX_TOPMOST:0), wndcls.lpszClassName, pThis->m_Caption.c_str(), 
		WS_CLIPCHILDREN|WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU, rcDesktop.left, rcDesktop.top, pThis->m_W, pThis->m_H, pThis->m_hParentWnd, NULL, wndcls.hInstance, NULL);
	if(!pThis->m_hDlgWnd)
		return 0;

	SetWindowLongPtr(pThis->m_hDlgWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis) );
	SetLayeredWindowAttributes(pThis->m_hDlgWnd,0,240,LWA_ALPHA);

	RECT client;
	GetClientRect(pThis->m_hDlgWnd, &client);

	pThis->m_TextBox.left=client.left+8;
	pThis->m_TextBox.right=client.right-8;
	pThis->m_TextBox.bottom=client.bottom-8-24-8;
	pThis->m_TextBox.top=pThis->m_TextBox.bottom-24;

	//pThis->m_IconBox.left=client.left+8;
	//pThis->m_IconBox.right=client.right-8;
	//pThis->m_IconBox.top=client.top+8;
	//pThis->m_IconBox.bottom=pThis->m_TextBox.top-8;

	pThis->m_hProgressWnd = CreateWindow(PROGRESS_CLASS, NULL, WS_CHILD|WS_VISIBLE,	8, client.bottom-8-24, client.right-16, 24, pThis->m_hDlgWnd, NULL, GetModuleHandle(NULL), NULL);
	pThis->m_hAnimWnd = CreateWindow(WC_STATIC, NULL, WS_CHILD|WS_VISIBLE|SS_ICON|SS_CENTERIMAGE,	8, 8, client.right-16, pThis->m_TextBox.top-16, pThis->m_hDlgWnd, NULL, GetModuleHandle(NULL), NULL);
	//SendMessage(pThis->m_hProgressWnd, PBM_SETRANGE, 0, MAKELPARAM(0, 100));
	
	SendMessage(pThis->m_hAnimWnd, STM_SETIMAGE, IMAGE_CURSOR, (LPARAM)pThis->m_hAnimCur);
	
	ShowWindow(pThis->m_hDlgWnd, SW_SHOWNOACTIVATE);

	MSG msg;

	PeekMessage(&msg,NULL,0,0,PM_NOREMOVE); // create message queue

	SetEvent(pThis->m_hEvent); //ready

	while(GetMessage(&msg,NULL,0,0)>0)
		{ 
		if(msg.message == WM_QUIT)
			break;
		else if(msg.message == PBM_SETPOS)
			{
			SendMessage(pThis->m_hProgressWnd, msg.message, msg.wParam, msg.lParam);
			}
		else if(msg.message == PBM_SETRANGE32)
			{
			SendMessage(pThis->m_hProgressWnd, msg.message, msg.wParam, msg.lParam);
			}
		else if(msg.message == PBM_SETMARQUEE)
			{
			SendMessage(pThis->m_hProgressWnd, msg.message, msg.wParam, msg.lParam);
			}
		else if(msg.message == STM_SETIMAGE)
			{
			SendMessage(pThis->m_hAnimWnd, msg.message, msg.wParam, msg.lParam);
			}
		else
			{
			TranslateMessage(&msg); 
			DispatchMessage(&msg);
			}
		}
	DestroyWindow(pThis->m_hDlgWnd);

	return 0;
	}

//................................................................................................................................................
LRESULT CALLBACK UProgress::WndProc( HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam )
	{
	UProgress* pThis = reinterpret_cast<UProgress*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
	if(pThis == NULL)
		{
		return DefWindowProc(hwnd, uMsg, wParam, lParam);
		}
	HDC hdc;
	HFONT hPrevFont;
	switch (uMsg)
		{
		case WM_PAINT:
			hdc=GetDC(hwnd);
			DefWindowProc(hwnd, WM_ERASEBKGND, (WPARAM)hdc, 0);
			//DrawIcon(hdc,0,0,pThis->m_hIcon);
			//DrawIconEx(hdc,pThis->m_IconBox.left,pThis->m_IconBox.top,pThis->m_hAnimCur,pThis->m_IconBox.right-pThis->m_IconBox.left,pThis->m_IconBox.bottom-pThis->m_IconBox.top,0,(HBRUSH)(COLOR_WINDOW + 1),DI_NORMAL);
			hPrevFont=(HFONT)SelectObject(hdc,pThis->m_hFont);
			SetBkColor(hdc,GetSysColor(COLOR_BTNFACE));
			SetTextColor(hdc,GetSysColor(COLOR_BTNTEXT));
			DrawText(hdc,pThis->m_ProgressMsg.c_str(),(int)pThis->m_ProgressMsg.length(),&pThis->m_TextBox,DT_CENTER|DT_WORD_ELLIPSIS|DT_SINGLELINE|DT_VCENTER);
			SelectObject(hdc,hPrevFont);
			ReleaseDC(hwnd,hdc);
			ValidateRect(hwnd,NULL);
			return 0;
		}

	return DefWindowProc(hwnd, uMsg, wParam, lParam);
	}

//................................................................................................................................................
void UProgress::SetCaption( LPCWSTR text )
	{
	m_Caption = text;
	SetWindowText(m_hDlgWnd,text);
	//PostThreadMessage( m_ThreadId, WM_SETTEXT, 0, reinterpret_cast<LPARAM>(text) );
	}

//................................................................................................................................................
void UProgress::SetMessage( LPCWSTR text )
	{
	m_ProgressMsg = text;
	InvalidateRect(m_hDlgWnd,NULL,FALSE);
	//PostThreadMessage( m_ThreadId, WM_SETTEXT, 0, reinterpret_cast<LPARAM>(text) );
	}

//................................................................................................................................................
void UProgress::SetRange( UINT rangeA,UINT rangeB )
	{
	PostThreadMessage( m_ThreadId, PBM_SETRANGE32, rangeA, rangeB );
	}

//................................................................................................................................................
void UProgress::SetPos( UINT procent )
	{
	PostThreadMessage( m_ThreadId, PBM_SETPOS , procent, 0);
	}

//................................................................................................................................................
void UProgress::SetMarquee(bool on,int T)
	{
	DWORD style=(DWORD)GetWindowLongPtr(m_hProgressWnd,GWL_STYLE);
	if(on)
		SetWindowLongPtr(m_hProgressWnd,GWL_STYLE,style|PBS_MARQUEE);
	else
		SetWindowLongPtr(m_hProgressWnd,GWL_STYLE,style&~PBS_MARQUEE);
	PostThreadMessage(m_ThreadId, PBM_SETMARQUEE, on, T );
	}

//................................................................................................................................................
void UProgress::SetIcon(HICON hico)
	{
	PostThreadMessage(m_ThreadId, STM_SETIMAGE, IMAGE_ICON, (LPARAM)hico );
	}

