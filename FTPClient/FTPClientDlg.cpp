// FTPClientDlg.cpp
#include "stdafx.h"
#include "FTPClient.h"
#include "FTPClientDlg.h"
#include "InputDlg.h"

#include <algorithm>
#include <memory>
#include <atomic>
#include <thread>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// ===========================================================================
// Cac struct noi bo dung de truyen tham so / ket qua giua luong nen va UI thread
// ===========================================================================

// Tham so cho SimpleOpThreadProc (Connect/Disconnect/Cwd/Delete/Rename/Mkdir)
struct CBgOpParam
{
    HWND         hwnd;
    CFtpSession* session;
    EOpType      type;
    CString      host; int port; CString user; CString pass;  // chi dung cho Connect
    CString      arg1;
    CString      arg2;
    bool         arg1IsDir = false;
};

// Ket qua tra ve chung cho moi thao tac (post kem WM_APP_OPDONE)
struct COpDoneResult
{
    EOpType type;
    bool    success;
    CString error;
    CString itemName;
};

// Ket qua Pwd + List (post kem WM_APP_LISTDONE)
struct CListResult
{
    bool success = false;
    CString path;
    CString error;
    std::vector<CFtpFileEntry> entries;
};

// Tham so cho DownloadThreadProc
struct CDownloadParam
{
    HWND    hwnd;
    CString host; int port; CString user; CString pass;
    CString remoteFile;
    CString localFile;
    int     numConnections;
};

// Tham so cho UploadThreadProc
struct CUploadParam
{
    HWND    hwnd;
    CString host; int port; CString user; CString pass;
    CString localFile;
    CString remoteFile;
};

// ===========================================================================

CFTPClientDlg::CFTPClientDlg(CWnd* pParent)
    : CDialogEx(IDD_FTPCLIENT_DIALOG, pParent)
{
}

void CFTPClientDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_LIST_REMOTE, m_listRemote);
    DDX_Text(pDX, IDC_EDIT_HOST, m_host);
    DDX_Text(pDX, IDC_EDIT_PORT, m_port);
    DDX_Text(pDX, IDC_EDIT_USER, m_user);
    DDX_Text(pDX, IDC_EDIT_PASS, m_pass);
    DDX_Text(pDX, IDC_EDIT_THREADS, m_numConnections);
    DDV_MinMaxInt(pDX, m_numConnections, 1, 16);
}

BEGIN_MESSAGE_MAP(CFTPClientDlg, CDialogEx)
    ON_BN_CLICKED(IDC_BTN_CONNECT, &CFTPClientDlg::OnBnClickedBtnConnect)
    ON_BN_CLICKED(IDC_BTN_REFRESH, &CFTPClientDlg::OnBnClickedBtnRefresh)
    ON_BN_CLICKED(IDC_BTN_UP, &CFTPClientDlg::OnBnClickedBtnUp)
    ON_BN_CLICKED(IDC_BTN_DOWNLOAD, &CFTPClientDlg::OnBnClickedBtnDownload)
    ON_BN_CLICKED(IDC_BTN_UPLOAD, &CFTPClientDlg::OnBnClickedBtnUpload)
    ON_BN_CLICKED(IDC_BTN_DELETE, &CFTPClientDlg::OnBnClickedBtnDelete)
    ON_BN_CLICKED(IDC_BTN_RENAME, &CFTPClientDlg::OnBnClickedBtnRename)
    ON_BN_CLICKED(IDC_BTN_MKDIR, &CFTPClientDlg::OnBnClickedBtnMkdir)
    ON_NOTIFY(NM_DBLCLK, IDC_LIST_REMOTE, &CFTPClientDlg::OnNMDblclkListRemote)
    ON_MESSAGE(WM_APP_LOG, &CFTPClientDlg::OnAppLog)
    ON_MESSAGE(WM_APP_PROGRESS, &CFTPClientDlg::OnAppProgress)
    ON_MESSAGE(WM_APP_LISTDONE, &CFTPClientDlg::OnAppListDone)
    ON_MESSAGE(WM_APP_OPDONE, &CFTPClientDlg::OnAppOpDone)
END_MESSAGE_MAP()

// ---------------------------------------------------------------------------
BOOL CFTPClientDlg::OnInitDialog()
{
    CDialogEx::OnInitDialog();

    SetDlgItemText(IDC_EDIT_PORT, _T("21"));
    SetDlgItemText(IDC_EDIT_THREADS, _T("4"));
    m_port = 21;
    m_numConnections = 4;

    CSpinButtonCtrl* pSpin = (CSpinButtonCtrl*)GetDlgItem(IDC_SPIN_THREADS);
    if (pSpin) pSpin->SetRange(1, 16);

    m_listRemote.SetExtendedStyle(m_listRemote.GetExtendedStyle() | LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
    m_listRemote.InsertColumn(0, _T("Ten"), LVCFMT_LEFT, 230);
    m_listRemote.InsertColumn(1, _T("Loai"), LVCFMT_LEFT, 70);
    m_listRemote.InsertColumn(2, _T("Kich thuoc"), LVCFMT_RIGHT, 90);
    m_listRemote.InsertColumn(3, _T("Ngay"), LVCFMT_LEFT, 90);

    HWND hwnd = GetSafeHwnd();
    m_session.SetLogFunc([hwnd](const CString& s)
    {
        CString* pMsg = new CString(s);
        ::PostMessage(hwnd, WM_APP_LOG, 0, (LPARAM)pMsg);
    });

    SetDlgItemText(IDC_STATIC_PATH, _T("/"));
    SetUiEnabled(true);

    AppendLog(_T("San sang. Nhap thong tin may chu FTP roi bam 'Ket noi'."));

    return TRUE;
}

void CFTPClientDlg::OnCancel()
{
    if (m_busy)
    {
        AfxMessageBox(_T("Dang co thao tac thuc hien, vui long doi hoan tat truoc khi dong chuong trinh."));
        return;
    }
    if (m_connected)
        m_session.Disconnect();
    CDialogEx::OnCancel();
}

// ---------------------------------------------------------------------------
void CFTPClientDlg::AppendLog(const CString& s)
{
    CEdit* pEdit = (CEdit*)GetDlgItem(IDC_EDIT_LOG);
    if (!pEdit) return;
    int len = pEdit->GetWindowTextLength();
    pEdit->SetSel(len, len);
    pEdit->ReplaceSel(s + _T("\r\n"));
}

void CFTPClientDlg::SetUiEnabled(bool enabled)
{
    bool connectedUi = enabled && m_connected;

    GetDlgItem(IDC_BTN_CONNECT)->EnableWindow(enabled);
    GetDlgItem(IDC_EDIT_HOST)->EnableWindow(enabled && !m_connected);
    GetDlgItem(IDC_EDIT_PORT)->EnableWindow(enabled && !m_connected);
    GetDlgItem(IDC_EDIT_USER)->EnableWindow(enabled && !m_connected);
    GetDlgItem(IDC_EDIT_PASS)->EnableWindow(enabled && !m_connected);

    GetDlgItem(IDC_BTN_REFRESH)->EnableWindow(connectedUi);
    GetDlgItem(IDC_BTN_UP)->EnableWindow(connectedUi);
    GetDlgItem(IDC_BTN_DOWNLOAD)->EnableWindow(connectedUi);
    GetDlgItem(IDC_BTN_UPLOAD)->EnableWindow(connectedUi);
    GetDlgItem(IDC_BTN_DELETE)->EnableWindow(connectedUi);
    GetDlgItem(IDC_BTN_RENAME)->EnableWindow(connectedUi);
    GetDlgItem(IDC_BTN_MKDIR)->EnableWindow(connectedUi);
    GetDlgItem(IDC_EDIT_THREADS)->EnableWindow(enabled);
}

CString CFTPClientDlg::FormatSize(ULONGLONG size)
{
    static const TCHAR* units[] = { _T("B"), _T("KB"), _T("MB"), _T("GB"), _T("TB") };
    double val = (double)size;
    int unit = 0;
    while (val >= 1024.0 && unit < 4) { val /= 1024.0; unit++; }

    CString s;
    if (unit == 0) s.Format(_T("%llu %s"), size, units[0]);
    else s.Format(_T("%.1f %s"), val, units[unit]);
    return s;
}

void CFTPClientDlg::PopulateList(const std::vector<CFtpFileEntry>& entries)
{
    m_entries = entries;
    std::stable_sort(m_entries.begin(), m_entries.end(),
        [](const CFtpFileEntry& a, const CFtpFileEntry& b)
        {
            if (a.isDirectory != b.isDirectory) return a.isDirectory > b.isDirectory;
            return a.name.CompareNoCase(b.name) < 0;
        });

    m_listRemote.DeleteAllItems();
    for (size_t i = 0; i < m_entries.size(); i++)
    {
        const CFtpFileEntry& e = m_entries[i];
        int idx = m_listRemote.InsertItem((int)i, e.name);
        m_listRemote.SetItemText(idx, 1, e.isDirectory ? _T("Thu muc") : _T("File"));
        m_listRemote.SetItemText(idx, 2, e.isDirectory ? CString() : FormatSize(e.size));
        m_listRemote.SetItemText(idx, 3, e.dateStr);
    }
}

CString CFTPClientDlg::GetSelectedName(bool* pIsDir) const
{
    int idx = m_listRemote.GetNextItem(-1, LVNI_SELECTED);
    if (idx < 0 || idx >= (int)m_entries.size()) return CString();
    if (pIsDir) *pIsDir = m_entries[idx].isDirectory;
    return m_entries[idx].name;
}

std::vector<CString> CFTPClientDlg::GetSelectedNames(bool onlyFiles) const
{
    std::vector<CString> result;
    POSITION pos = m_listRemote.GetFirstSelectedItemPosition();
    while (pos)
    {
        int idx = m_listRemote.GetNextSelectedItem(pos);
        if (idx >= 0 && idx < (int)m_entries.size())
        {
            const CFtpFileEntry& e = m_entries[idx];
            if (!onlyFiles || !e.isDirectory)
                result.push_back(e.name);
        }
    }
    return result;
}

// ---------------------------------------------------------------------------
void CFTPClientDlg::EnqueueJob(const COpJob& job)
{
    if (job.type == EOpType::Upload)
        m_needRefreshAfterQueue = true;

    m_jobQueue.push_back(job);
    if (!m_queueRunning)
        StartNextJob();
}

void CFTPClientDlg::StartNextJob()
{
    if (m_jobQueue.empty())
    {
        m_queueRunning = false;

        if (m_needRefreshAfterQueue && m_connected)
        {
            m_needRefreshAfterQueue = false;
            m_busy = true;
            SetUiEnabled(false);

            CBgOpParam* param = new CBgOpParam();
            param->hwnd = GetSafeHwnd();
            param->session = &m_session;
            param->type = EOpType::Cwd;
            param->arg1 = _T(".");
            AfxBeginThread(SimpleOpThreadProc, param);
        }
        else
        {
            m_busy = false;
            SetUiEnabled(true);
        }
        return;
    }

    m_queueRunning = true;
    m_busy = true;
    SetUiEnabled(false);

    COpJob job = m_jobQueue.front();
    m_jobQueue.pop_front();

    if (job.type == EOpType::Download)
    {
        AppendLog(_T("Bat dau tai xuong: ") + job.remoteName);
        CDownloadParam* param = new CDownloadParam();
        param->hwnd = GetSafeHwnd();
        param->host = m_connHost; param->port = m_connPort;
        param->user = m_connUser; param->pass = m_connPass;
        param->remoteFile = job.remoteName;
        param->localFile = job.extra;
        param->numConnections = m_numConnections;
        AfxBeginThread(DownloadThreadProc, param);
    }
    else if (job.type == EOpType::Upload)
    {
        AppendLog(_T("Bat dau tai len: ") + job.remoteName);
        CUploadParam* param = new CUploadParam();
        param->hwnd = GetSafeHwnd();
        param->host = m_connHost; param->port = m_connPort;
        param->user = m_connUser; param->pass = m_connPass;
        param->localFile = job.extra;
        param->remoteFile = job.remoteName;
        AfxBeginThread(UploadThreadProc, param);
    }
}

// ---------------------------------------------------------------------------
// Cac trinh xu ly nut bam
// ---------------------------------------------------------------------------
void CFTPClientDlg::OnBnClickedBtnConnect()
{
    if (m_busy) return;

    if (!m_connected)
    {
        UpdateData(TRUE);
        if (m_host.IsEmpty())
        {
            AfxMessageBox(_T("Vui long nhap dia chi may chu FTP."));
            return;
        }
        if (m_port == 0) m_port = 21;

        m_connHost = m_host;
        m_connPort = (int)m_port;
        m_connUser = m_user;
        m_connPass = m_pass;

        m_busy = true;
        SetUiEnabled(false);
        AppendLog(_T("Dang ket noi toi ") + m_host + _T("..."));

        CBgOpParam* param = new CBgOpParam();
        param->hwnd = GetSafeHwnd();
        param->session = &m_session;
        param->type = EOpType::Connect;
        param->host = m_connHost; param->port = m_connPort;
        param->user = m_connUser; param->pass = m_connPass;
        AfxBeginThread(SimpleOpThreadProc, param);
    }
    else
    {
        m_busy = true;
        SetUiEnabled(false);
        AppendLog(_T("Dang ngat ket noi..."));

        CBgOpParam* param = new CBgOpParam();
        param->hwnd = GetSafeHwnd();
        param->session = &m_session;
        param->type = EOpType::Disconnect;
        AfxBeginThread(SimpleOpThreadProc, param);
    }
}

void CFTPClientDlg::OnBnClickedBtnRefresh()
{
    if (!m_connected || m_busy) return;
    m_busy = true;
    SetUiEnabled(false);

    CBgOpParam* param = new CBgOpParam();
    param->hwnd = GetSafeHwnd();
    param->session = &m_session;
    param->type = EOpType::Cwd;
    param->arg1 = _T(".");
    AfxBeginThread(SimpleOpThreadProc, param);
}

void CFTPClientDlg::OnBnClickedBtnUp()
{
    if (!m_connected || m_busy) return;
    m_busy = true;
    SetUiEnabled(false);

    CBgOpParam* param = new CBgOpParam();
    param->hwnd = GetSafeHwnd();
    param->session = &m_session;
    param->type = EOpType::Cwd;
    param->arg1 = _T("..");
    AfxBeginThread(SimpleOpThreadProc, param);
}

void CFTPClientDlg::OnNMDblclkListRemote(NMHDR* pNMHDR, LRESULT* pResult)
{
    *pResult = 0;
    if (!m_connected || m_busy) return;

    LPNMITEMACTIVATE p = reinterpret_cast<LPNMITEMACTIVATE>(pNMHDR);
    int idx = p->iItem;
    if (idx < 0 || idx >= (int)m_entries.size()) return;

    const CFtpFileEntry& e = m_entries[idx];
    if (!e.isDirectory) return;

    m_busy = true;
    SetUiEnabled(false);

    CBgOpParam* param = new CBgOpParam();
    param->hwnd = GetSafeHwnd();
    param->session = &m_session;
    param->type = EOpType::Cwd;
    param->arg1 = e.name;
    AfxBeginThread(SimpleOpThreadProc, param);
}

void CFTPClientDlg::OnBnClickedBtnDownload()
{
    if (!m_connected || m_busy) return;

    std::vector<CString> files = GetSelectedNames(true);
    if (files.empty())
    {
        AfxMessageBox(_T("Vui long chon it nhat mot FILE de tai xuong (khong ho tro tai ca thu muc)."));
        return;
    }

    UpdateData(TRUE);
    int conns = m_numConnections;
    if (conns < 1) conns = 1;
    if (conns > 16) conns = 16;

    CFolderPickerDialog dlg(nullptr, OFN_PATHMUSTEXIST, this);
    dlg.m_ofn.lpstrTitle = _T("Chon thu muc luu file tai ve");
    if (dlg.DoModal() != IDOK) return;
    CString destFolder = dlg.GetPathName();

    for (const CString& name : files)
    {
        COpJob job;
        job.type = EOpType::Download;
        job.remoteName = name;
        job.extra = destFolder + _T("\\") + name;
        EnqueueJob(job);
    }
}

void CFTPClientDlg::OnBnClickedBtnUpload()
{
    if (!m_connected || m_busy) return;

    CString filter = _T("Tat ca cac file (*.*)|*.*||");
    CFileDialog dlg(TRUE, nullptr, nullptr,
        OFN_HIDEREADONLY | OFN_FILEMUSTEXIST | OFN_ALLOWMULTISELECT | OFN_EXPLORER,
        filter, this);

    const DWORD bufSize = 1024 * 16;
    std::vector<TCHAR> buf(bufSize, 0);
    dlg.m_ofn.lpstrFile = buf.data();
    dlg.m_ofn.nMaxFile = bufSize;

    if (dlg.DoModal() != IDOK) return;

    POSITION pos = dlg.GetStartPosition();
    while (pos)
    {
        CString path = dlg.GetNextPathName(pos);
        CString fname = path;
        int slash = fname.ReverseFind(_T('\\'));
        if (slash >= 0) fname = fname.Mid(slash + 1);

        COpJob job;
        job.type = EOpType::Upload;
        job.remoteName = fname;
        job.extra = path;
        EnqueueJob(job);
    }
}

void CFTPClientDlg::OnBnClickedBtnDelete()
{
    if (!m_connected || m_busy) return;

    bool isDir = false;
    CString name = GetSelectedName(&isDir);
    if (name.IsEmpty())
    {
        AfxMessageBox(_T("Vui long chon mot file hoac thu muc."));
        return;
    }

    CString msg;
    msg.Format(_T("Ban co chac muon xoa \"%s\"?"), name.GetString());
    if (AfxMessageBox(msg, MB_YESNO | MB_ICONQUESTION) != IDYES) return;

    m_busy = true;
    SetUiEnabled(false);

    CBgOpParam* param = new CBgOpParam();
    param->hwnd = GetSafeHwnd();
    param->session = &m_session;
    param->type = EOpType::Delete;
    param->arg1 = name;
    param->arg1IsDir = isDir;
    AfxBeginThread(SimpleOpThreadProc, param);
}

void CFTPClientDlg::OnBnClickedBtnRename()
{
    if (!m_connected || m_busy) return;

    CString name = GetSelectedName();
    if (name.IsEmpty())
    {
        AfxMessageBox(_T("Vui long chon mot file hoac thu muc."));
        return;
    }

    CInputDlg dlg(_T("Nhap ten moi:"), name, this);
    if (dlg.DoModal() != IDOK) return;

    CString newName = dlg.m_value;
    newName.Trim();
    if (newName.IsEmpty() || newName == name) return;

    m_busy = true;
    SetUiEnabled(false);

    CBgOpParam* param = new CBgOpParam();
    param->hwnd = GetSafeHwnd();
    param->session = &m_session;
    param->type = EOpType::Rename;
    param->arg1 = name;
    param->arg2 = newName;
    AfxBeginThread(SimpleOpThreadProc, param);
}

void CFTPClientDlg::OnBnClickedBtnMkdir()
{
    if (!m_connected || m_busy) return;

    CInputDlg dlg(_T("Ten thu muc moi:"), CString(), this);
    if (dlg.DoModal() != IDOK) return;

    CString name = dlg.m_value;
    name.Trim();
    if (name.IsEmpty()) return;

    m_busy = true;
    SetUiEnabled(false);

    CBgOpParam* param = new CBgOpParam();
    param->hwnd = GetSafeHwnd();
    param->session = &m_session;
    param->type = EOpType::Mkdir;
    param->arg1 = name;
    AfxBeginThread(SimpleOpThreadProc, param);
}

// ---------------------------------------------------------------------------
// Xu ly thong diep tu luong nen
// ---------------------------------------------------------------------------
LRESULT CFTPClientDlg::OnAppLog(WPARAM, LPARAM lParam)
{
    std::unique_ptr<CString> msg((CString*)lParam);
    AppendLog(*msg);
    return 0;
}

LRESULT CFTPClientDlg::OnAppProgress(WPARAM wParam, LPARAM)
{
    int percent = (int)wParam;
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;

    CProgressCtrl* pProgress = (CProgressCtrl*)GetDlgItem(IDC_PROGRESS1);
    if (pProgress) pProgress->SetPos(percent);
    return 0;
}

LRESULT CFTPClientDlg::OnAppListDone(WPARAM, LPARAM lParam)
{
    std::unique_ptr<CListResult> lr((CListResult*)lParam);

    if (lr->success)
    {
        m_currentPath = lr->path;
        SetDlgItemText(IDC_STATIC_PATH, m_currentPath.IsEmpty() ? CString(_T("/")) : m_currentPath);
        PopulateList(lr->entries);
    }
    else
    {
        AppendLog(_T("Loi khi lay danh sach thu muc: ") + lr->error);
    }

    m_busy = false;
    SetUiEnabled(true);
    return 0;
}

LRESULT CFTPClientDlg::OnAppOpDone(WPARAM, LPARAM lParam)
{
    std::unique_ptr<COpDoneResult> res((COpDoneResult*)lParam);

    switch (res->type)
    {
    case EOpType::Connect:
        if (res->success)
        {
            m_connected = true;
            AppendLog(_T("Da ket noi va dang nhap thanh cong."));
            SetDlgItemText(IDC_BTN_CONNECT, _T("Ngat ket noi"));
        }
        else
        {
            m_connected = false;
            AppendLog(_T("Loi ket noi: ") + res->error);
            AfxMessageBox(res->error, MB_ICONERROR);
            SetDlgItemText(IDC_BTN_CONNECT, _T("Ket noi"));
            m_busy = false;
            SetUiEnabled(true);
        }
        break;

    case EOpType::Disconnect:
        m_connected = false;
        m_currentPath.Empty();
        m_entries.clear();
        m_listRemote.DeleteAllItems();
        SetDlgItemText(IDC_STATIC_PATH, _T("/"));
        SetDlgItemText(IDC_BTN_CONNECT, _T("Ket noi"));
        AppendLog(_T("Da ngat ket noi."));
        m_busy = false;
        SetUiEnabled(true);
        break;

    case EOpType::Cwd:
    case EOpType::Delete:
    case EOpType::Rename:
    case EOpType::Mkdir:
        if (!res->success)
        {
            AppendLog(_T("Loi: ") + res->error);
            AfxMessageBox(res->error, MB_ICONERROR);
            m_busy = false;
            SetUiEnabled(true);
        }
        // Neu thanh cong: WM_APP_LISTDONE se den ngay sau do va tu mo lai UI.
        break;

    case EOpType::Download:
    case EOpType::Upload:
    {
        CString label = (res->type == EOpType::Download) ? _T("Tai xuong") : _T("Tai len");
        if (res->success)
            AppendLog(label + _T(" thanh cong: ") + res->itemName);
        else
            AppendLog(label + _T(" THAT BAI (") + res->itemName + _T("): ") + res->error);

        StartNextJob();
        break;
    }
    }
    return 0;
}

// ---------------------------------------------------------------------------
// Cac ham chay tren luong nen
// ---------------------------------------------------------------------------
UINT AFX_CDECL CFTPClientDlg::SimpleOpThreadProc(LPVOID pParam)
{
    std::unique_ptr<CBgOpParam> p((CBgOpParam*)pParam);
    CString error;
    bool success = true;

    switch (p->type)
    {
    case EOpType::Connect:
        success = p->session->Connect(p->host, p->port, error);
        if (success) success = p->session->Login(p->user, p->pass, error);
        break;

    case EOpType::Disconnect:
        p->session->Disconnect();
        success = true;
        break;

    case EOpType::Cwd:
        success = p->session->Cwd(p->arg1, error);
        break;

    case EOpType::Delete:
        success = p->arg1IsDir ? p->session->Rmd(p->arg1, error) : p->session->Dele(p->arg1, error);
        break;

    case EOpType::Rename:
        success = p->session->Rename(p->arg1, p->arg2, error);
        break;

    case EOpType::Mkdir:
        success = p->session->Mkd(p->arg1, error);
        break;

    default:
        break;
    }

    COpDoneResult* res = new COpDoneResult();
    res->type = p->type;
    res->success = success;
    res->error = error;
    ::PostMessage(p->hwnd, WM_APP_OPDONE, 0, (LPARAM)res);

    bool shouldRelist = success &&
        (p->type == EOpType::Connect || p->type == EOpType::Cwd ||
         p->type == EOpType::Delete  || p->type == EOpType::Rename ||
         p->type == EOpType::Mkdir);

    if (shouldRelist)
    {
        CListResult* lr = new CListResult();
        lr->success = p->session->Pwd(lr->path, lr->error);
        if (lr->success)
            lr->success = p->session->List(CString(), lr->entries, lr->error);
        ::PostMessage(p->hwnd, WM_APP_LISTDONE, 0, (LPARAM)lr);
    }

    return 0;
}

UINT AFX_CDECL CFTPClientDlg::DownloadThreadProc(LPVOID pParam)
{
    std::unique_ptr<CDownloadParam> p((CDownloadParam*)pParam);
    CString error;
    bool success = false;

    CFtpSession probe;
    do
    {
        if (!probe.Connect(p->host, p->port, error)) break;
        if (!probe.Login(p->user, p->pass, error)) break;

        ULONGLONG totalSize = 0;
        bool haveSize = probe.Size(p->remoteFile, totalSize, error);

        // Tao san file dich (voi dung kich thuoc neu biet truoc) de cac luong con
        // co the ghi truc tiep vao dung vi tri (offset) cua minh.
        {
            CFile f;
            CFileException fe;
            if (!f.Open(p->localFile, CFile::modeCreate | CFile::modeWrite | CFile::shareDenyWrite, &fe))
            {
                error.Format(_T("Khong tao duoc file dich (loi %d)."), fe.m_cause);
                break;
            }
            if (haveSize && totalSize > 0)
                f.SetLength(totalSize);
            f.Close();
        }

        std::atomic<ULONGLONG> doneBytes(0);
        std::atomic<bool> cancelFlag(false);

        if (!haveSize || totalSize == 0)
        {
            // Khong biet truoc kich thuoc (hoac file rong): tai bang 1 ket noi duy nhat,
            // khong the bao phan tram chinh xac.
            error.Empty();
            success = probe.DownloadRange(p->remoteFile, p->localFile, 0, 0, &doneBytes, &cancelFlag, error);
        }
        else
        {
            int conns = p->numConnections;
            if (conns < 1) conns = 1;
            if ((ULONGLONG)conns > totalSize) conns = (int)totalSize; // moi ket noi it nhat 1 byte

            ULONGLONG chunk = totalSize / conns;
            std::vector<unsigned char> okFlags(conns, 0);
            std::vector<CString> errors(conns);
            std::atomic<int> remaining(conns);
            std::vector<std::thread> workers;

            for (int i = 0; i < conns; i++)
            {
                ULONGLONG offset = chunk * (ULONGLONG)i;
                ULONGLONG len = (i == conns - 1) ? (totalSize - offset) : chunk;

                workers.emplace_back([&, i, offset, len]()
                {
                    CFtpSession sess;
                    CString err;
                    bool ok = false;
                    if (sess.Connect(p->host, p->port, err) && sess.Login(p->user, p->pass, err))
                        ok = sess.DownloadRange(p->remoteFile, p->localFile, offset, len, &doneBytes, &cancelFlag, err);
                    sess.Disconnect();
                    okFlags[i] = ok ? 1 : 0;
                    if (!ok) errors[i] = err;
                    remaining.fetch_sub(1);
                });
            }

            while (remaining.load() > 0)
            {
                ::Sleep(200);
                ULONGLONG done = doneBytes.load();
                int percent = (int)((done * 100) / totalSize);
                ::PostMessage(p->hwnd, WM_APP_PROGRESS, (WPARAM)percent, 0);
            }
            for (auto& t : workers) t.join();

            ::PostMessage(p->hwnd, WM_APP_PROGRESS, (WPARAM)100, 0);

            success = true;
            for (int i = 0; i < conns; i++)
            {
                if (!okFlags[i])
                {
                    success = false;
                    if (!errors[i].IsEmpty())
                        error += errors[i] + _T("  ");
                }
            }
        }
    } while (false);

    probe.Disconnect();

    COpDoneResult* res = new COpDoneResult();
    res->type = EOpType::Download;
    res->success = success;
    res->error = error;
    res->itemName = p->remoteFile;
    ::PostMessage(p->hwnd, WM_APP_OPDONE, 0, (LPARAM)res);
    ::PostMessage(p->hwnd, WM_APP_PROGRESS, (WPARAM)0, 0);

    return 0;
}

UINT AFX_CDECL CFTPClientDlg::UploadThreadProc(LPVOID pParam)
{
    std::unique_ptr<CUploadParam> p((CUploadParam*)pParam);
    CString error;
    bool success = false;

    ULONGLONG totalSize = 0;
    CFileStatus fs;
    if (CFile::GetStatus(p->localFile, fs))
        totalSize = (ULONGLONG)fs.m_size;

    CFtpSession sess;
    if (sess.Connect(p->host, p->port, error) && sess.Login(p->user, p->pass, error))
    {
        std::atomic<ULONGLONG> doneBytes(0);
        std::atomic<bool> cancelFlag(false);
        std::atomic<bool> finished(false);

        std::thread worker([&]()
        {
            success = sess.UploadFile(p->localFile, p->remoteFile, &doneBytes, &cancelFlag, error);
            finished = true;
        });

        while (!finished.load())
        {
            ::Sleep(200);
            ULONGLONG done = doneBytes.load();
            int percent = (totalSize > 0) ? (int)((done * 100) / totalSize) : 0;
            ::PostMessage(p->hwnd, WM_APP_PROGRESS, (WPARAM)percent, 0);
        }
        worker.join();
        ::PostMessage(p->hwnd, WM_APP_PROGRESS, (WPARAM)100, 0);
    }
    sess.Disconnect();

    COpDoneResult* res = new COpDoneResult();
    res->type = EOpType::Upload;
    res->success = success;
    res->error = error;
    res->itemName = p->remoteFile;
    ::PostMessage(p->hwnd, WM_APP_OPDONE, 0, (LPARAM)res);
    ::PostMessage(p->hwnd, WM_APP_PROGRESS, (WPARAM)0, 0);

    return 0;
}
