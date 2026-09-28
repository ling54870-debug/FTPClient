// FTPClient.cpp — dinh nghia hanh vi khoi dong cua ung dung
#include "stdafx.h"
#include "FTPClient.h"
#include "FTPClientDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

BEGIN_MESSAGE_MAP(CFTPClientApp, CWinApp)
END_MESSAGE_MAP()

CFTPClientApp::CFTPClientApp()
{
}

CFTPClientApp theApp;

BOOL CFTPClientApp::InitInstance()
{
    CWinApp::InitInstance();

    // Khoi tao WinSock2 mot lan duy nhat cho toan bo tien trinh.
    WSADATA wsaData;
    int wsaErr = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (wsaErr != 0)
    {
        AfxMessageBox(_T("Khong khoi tao duoc Winsock (WSAStartup that bai)."), MB_ICONERROR);
        return FALSE;
    }
    m_wsaInitialized = true;

    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(icex);
    icex.dwICC = ICC_LISTVIEW_CLASSES | ICC_PROGRESS_CLASS | ICC_UPDOWN_CLASS | ICC_BAR_CLASSES;
    InitCommonControlsEx(&icex);

    AfxEnableControlContainer();

    CFTPClientDlg dlg;
    m_pMainWnd = &dlg;
    dlg.DoModal();

    return FALSE; // ung dung chi la 1 hop thoai, thoat sau khi dong
}

int CFTPClientApp::ExitInstance()
{
    if (m_wsaInitialized)
        WSACleanup();
    return CWinApp::ExitInstance();
}
