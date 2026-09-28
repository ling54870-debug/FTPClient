// stdafx.h : file include cho cac file include he thong chuan,
// hoac cac file include duoc dung thuong xuyen nhung it thay doi

#pragma once

#ifndef VC_EXTRALEAN
#define VC_EXTRALEAN
#endif

#define WIN32_LEAN_AND_MEAN

#include "targetver.h"

#define _AFX_ALL_WARNINGS

// MFC
#include <afxwin.h>
#include <afxext.h>
#include <afxdialogex.h>

#ifndef _AFX_NO_OLE_SUPPORT
#include <afxdtctl.h>
#endif

#ifndef _AFX_NO_AFXCMN_SUPPORT
#include <afxcmn.h>
#endif

#include <afxdlgs.h>

// Winsock2
#include <winsock2.h>
#include <ws2tcpip.h>

#include <vector>
#include <deque>
#include <atomic>
#include <thread>
#include <functional>
#include <algorithm>
#include <memory>

#pragma comment(lib, "Ws2_32.lib")