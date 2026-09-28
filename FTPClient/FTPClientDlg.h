// FTPClientDlg.h — hop thoai chinh cua chuong trinh FTP Client
#pragma once

#include "resource.h"
#include "FtpSession.h"
#include <deque>

// Cac message tu dinh nghia de luong nen (background thread) bao ket qua ve cho UI thread
#define WM_APP_LOG        (WM_APP + 1)   // lParam = CString* (cap phat dong, handler tu xoa)
#define WM_APP_PROGRESS   (WM_APP + 2)   // wParam = phan tram (0-100)
#define WM_APP_LISTDONE   (WM_APP + 3)   // lParam = CListResult* (cap phat dong)
#define WM_APP_OPDONE     (WM_APP + 4)   // lParam = COpDoneResult* (cap phat dong)

enum class EOpType
{
    Connect,
    Disconnect,
    Cwd,        // doi thu muc (dung ca cho vao thu muc con, len thu muc cha "..", lam moi ".")
    Delete,
    Rename,
    Mkdir,
    Download,
    Upload,
};

// Mot cong viec cho vao hang doi (tai xuong / tai len nhieu file lien tiep)
struct COpJob
{
    EOpType type;
    CString remoteName;   // ten file tren server
    CString extra;        // download: duong dan file dich tren may local
                           // upload:   duong dan file nguon tren may local
};

class CFTPClientDlg : public CDialogEx
{
public:
    CFTPClientDlg(CWnd* pParent = nullptr);

    enum { IDD = IDD_FTPCLIENT_DIALOG };

protected:
    virtual void DoDataExchange(CDataExchange* pDX);
    virtual BOOL OnInitDialog();
    virtual void OnCancel();

    afx_msg void OnBnClickedBtnConnect();
    afx_msg void OnBnClickedBtnRefresh();
    afx_msg void OnBnClickedBtnUp();
    afx_msg void OnBnClickedBtnDownload();
    afx_msg void OnBnClickedBtnUpload();
    afx_msg void OnBnClickedBtnDelete();
    afx_msg void OnBnClickedBtnRename();
    afx_msg void OnBnClickedBtnMkdir();
    afx_msg void OnNMDblclkListRemote(NMHDR* pNMHDR, LRESULT* pResult);

    afx_msg LRESULT OnAppLog(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnAppProgress(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnAppListDone(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnAppOpDone(WPARAM wParam, LPARAM lParam);

    DECLARE_MESSAGE_MAP()

private:
    CFtpSession   m_session;          // ket noi "duyet" chinh, giu suot phien lam viec
    CListCtrl     m_listRemote;
    std::vector<CFtpFileEntry> m_entries;   // danh sach tuong ung voi cac dong trong m_listRemote
    CString       m_currentPath;
    bool          m_connected = false;
    bool          m_busy = false;     // dang co thao tac chay nen (khoa UI)

    // Du lieu DDX cho cac o nhap
    CString m_host, m_user, m_pass;
    UINT    m_port = 21;
    int     m_numConnections = 4;

    // Luu lai thong tin dang nhap de cac luong tai/upload tu tao ket noi rieng
    CString m_connHost, m_connUser, m_connPass;
    int     m_connPort = 21;

    std::deque<COpJob> m_jobQueue;
    bool m_queueRunning = false;
    bool m_needRefreshAfterQueue = false;

    void AppendLog(const CString& s);
    void SetUiEnabled(bool enabled);
    void PopulateList(const std::vector<CFtpFileEntry>& entries);
    CString GetSelectedName(bool* pIsDir = nullptr) const;
    std::vector<CString> GetSelectedNames(bool onlyFiles) const;
    static CString FormatSize(ULONGLONG size);

    void EnqueueJob(const COpJob& job);
    void StartNextJob();

    static UINT AFX_CDECL SimpleOpThreadProc(LPVOID pParam);   // Connect/Disconnect/Cwd/Delete/Rename/Mkdir
    static UINT AFX_CDECL DownloadThreadProc(LPVOID pParam);
    static UINT AFX_CDECL UploadThreadProc(LPVOID pParam);
};
