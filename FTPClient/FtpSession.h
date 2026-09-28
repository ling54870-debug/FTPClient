// FtpSession.h
//
// Cai dat mot phien lam viec FTP client "tu tay" tren nen WinSock2 (khong dung
// CInternetSession/CFtpConnection cua MFC, khong dung WinInet) — dung cho
// De tai 402: Xay dung chuong trinh FTP Client.
//
// Moi doi tuong CFtpSession quan ly MOT ket noi dieu khien (control connection).
// Cac lenh LIST/RETR/STOR mo them mot data connection rieng (che do PASV)
// theo dung RFC 959.
//
// De tai xuong mot file bang NHIEU ket noi song song, ta tao NHIEU doi tuong
// CFtpSession doc lap (moi cai tu dang nhap rieng), moi doi tuong tai mot
// doan (byte range) cua file bang lenh REST + RETR roi ghi vao dung vi tri
// (offset) trong file dich — xem CFtpSession::DownloadRange().

#pragma once

#include <vector>
#include <atomic>
#include <functional>

// Mo ta mot phan tu (file/thu muc) trong danh sach LIST tra ve tu server
struct CFtpFileEntry
{
    CString     name;
    bool        isDirectory = false;
    ULONGLONG   size = 0;
    CString     dateStr;
};

class CFtpSession
{
public:
    typedef std::function<void(const CString&)> LogFunc;

    CFtpSession();
    ~CFtpSession();

    // Khong cho phep sao chep (moi doi tuong so huu 1 socket)
    CFtpSession(const CFtpSession&) = delete;
    CFtpSession& operator=(const CFtpSession&) = delete;

    void SetLogFunc(LogFunc fn) { m_logFn = fn; }

    bool IsConnected() const { return m_ctrlSocket != INVALID_SOCKET; }

    // --- Ket noi / dang nhap -------------------------------------------------
    bool Connect(const CString& host, int port, CString& error);
    bool Login(const CString& user, const CString& pass, CString& error);
    void Disconnect();

    // --- Duyet thu muc ---------------------------------------------------------
    bool Pwd(CString& path, CString& error);
    bool Cwd(const CString& path, CString& error);
    bool List(const CString& path, std::vector<CFtpFileEntry>& out, CString& error);
    bool Size(const CString& remoteFile, ULONGLONG& size, CString& error);

    // --- Thao tac tren file/thu muc --------------------------------------------
    bool Dele(const CString& remoteFile, CString& error);
    bool Rmd(const CString& remoteDir, CString& error);
    bool Mkd(const CString& remoteDir, CString& error);
    bool Rename(const CString& oldName, const CString& newName, CString& error);

    // --- Truyen du lieu ---------------------------------------------------------
    // Tai mot doan [offset, offset+length) cua remoteFile ve localFile (ghi dung vi tri offset).
    // Neu length == 0 nghia la "tai toi het file" (dung cho tai 1 ket noi duy nhat,
    // hoac cho doan cuoi cung khi khong biet chac kich thuoc).
    bool DownloadRange(const CString& remoteFile, const CString& localFile,
                        ULONGLONG offset, ULONGLONG length,
                        std::atomic<ULONGLONG>* bytesDoneCounter,
                        std::atomic<bool>* cancelFlag,
                        CString& error);

    bool UploadFile(const CString& localFile, const CString& remoteFile,
                     std::atomic<ULONGLONG>* bytesDoneCounter,
                     std::atomic<bool>* cancelFlag,
                     CString& error);

private:
    SOCKET   m_ctrlSocket;
    CString  m_ctrlRecvBuf;   // bo dem cho phan hoi dieu khien (co the den lam nhieu lan)
    LogFunc  m_logFn;

    void  Log(const CString& s) const { if (m_logFn) m_logFn(s); }

    bool  SendCommand(const CString& cmd, CString& error);
    int   ReadReply(CString& replyText, CString& error);
    int   SendCommandAndReadReply(const CString& cmd, CString& replyText, CString& error);

    bool  EnterPassiveMode(SOCKET& dataSocket, CString& error);

    static bool ParseListLineUnix(const CString& line, CFtpFileEntry& entry);
    static bool ParseListLineDos(const CString& line, CFtpFileEntry& entry);
    static std::vector<CString> SplitWhitespace(const CString& line);
};
