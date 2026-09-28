// FTPClient.h — file header chinh cua ung dung
#pragma once

#ifndef __AFXWIN_H__
    #error "Include 'stdafx.h' truoc file nay de dung PCH"
#endif

#include "resource.h"

class CFTPClientApp : public CWinApp
{
public:
    CFTPClientApp();

    virtual BOOL InitInstance();
    virtual int ExitInstance();

    DECLARE_MESSAGE_MAP()

private:
    bool m_wsaInitialized = false;
};

extern CFTPClientApp theApp;
