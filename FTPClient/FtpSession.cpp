// FtpSession.cpp
#include "stdafx.h"
#include "FtpSession.h"

CFtpSession::CFtpSession()
    : m_ctrlSocket(INVALID_SOCKET)
{
}

CFtpSession::~CFtpSession()
{
    Disconnect();
}

// ---------------------------------------------------------------------------
// Doc mot ky tu he thong phan hoi day du (co the nhieu dong, vd "150-...","150 ...")
// tu control socket. Tra ve ma so 3 chu so (vd 220, 230, 550...) hoac -1 neu loi.
// ---------------------------------------------------------------------------
int CFtpSession::ReadReply(CString& replyText, CString& error)
{
    replyText.Empty();

    for (;;)
    {
        int nlPos = m_ctrlRecvBuf.Find(_T("\n"));
        if (nlPos < 0)
        {
            char buf[2049];
            int n = recv(m_ctrlSocket, buf, sizeof(buf) - 1, 0);
            if (n <= 0)
            {
                error = _T("Mat ket noi toi may chu (control connection).");
                return -1;
            }
            buf[n] = '\0';
            m_ctrlRecvBuf += CString(buf);
            continue;
        }

        CString line = m_ctrlRecvBuf.Left(nlPos);
        m_ctrlRecvBuf = m_ctrlRecvBuf.Mid(nlPos + 1);
        line.TrimRight(_T("\r\n"));

        Log(_T("<< ") + line);
        replyText += line + _T("\r\n");

        if (line.GetLength() >= 4 &&
            _istdigit(line[0]) && _istdigit(line[1]) && _istdigit(line[2]))
        {
            TCHAR sep = line[3];
            if (sep == _T(' '))
            {
                return _ttoi(line.Left(3));
            }
            // sep == '-' -> dong bat dau phan hoi nhieu dong, tiep tuc doc
            // cho toi khi gap dong "code ..." (co dau cach) cung ma so.
        }
        // Cac dong tiep theo cua phan hoi nhieu dong: doc tiep vong lap.
    }
}

bool CFtpSession::SendCommand(const CString& cmd, CString& error)
{
    if (m_ctrlSocket == INVALID_SOCKET)
    {
        error = _T("Chua ket noi toi may chu.");
        return false;
    }

    CString maskedLog = cmd;
    if (maskedLog.Left(5).CompareNoCase(_T("PASS ")) == 0)
        maskedLog = _T("PASS ****");
    Log(_T(">> ") + maskedLog);

    CString line = cmd + _T("\r\n");
    CStringA lineA(line);
    int len = lineA.GetLength();
    int total = 0;
    while (total < len)
    {
        int n = send(m_ctrlSocket, lineA.GetString() + total, len - total, 0);
        if (n == SOCKET_ERROR)
        {
            error = _T("Loi khi gui lenh toi may chu.");
            return false;
        }
        total += n;
    }
    return true;
}

int CFtpSession::SendCommandAndReadReply(const CString& cmd, CString& replyText, CString& error)
{
    if (!SendCommand(cmd, error))
        return -1;
    return ReadReply(replyText, error);
}

// ---------------------------------------------------------------------------
bool CFtpSession::Connect(const CString& host, int port, CString& error)
{
    Disconnect();
    m_ctrlRecvBuf.Empty();

    CStringA hostA(host);
    CString portStr; portStr.Format(_T("%d"), port);
    CStringA portA(portStr);

    struct addrinfo hints;
    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    struct addrinfo* res = nullptr;
    if (getaddrinfo(hostA, portA, &hints, &res) != 0 || res == nullptr)
    {
        error = _T("Khong phan giai duoc dia chi may chu: ") + host;
        return false;
    }

    m_ctrlSocket = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (m_ctrlSocket == INVALID_SOCKET)
    {
        error = _T("Khong tao duoc socket.");
        freeaddrinfo(res);
        return false;
    }

    if (connect(m_ctrlSocket, res->ai_addr, (int)res->ai_addrlen) == SOCKET_ERROR)
    {
        error.Format(_T("Khong ket noi duoc toi %s:%d"), host.GetString(), port);
        closesocket(m_ctrlSocket);
        m_ctrlSocket = INVALID_SOCKET;
        freeaddrinfo(res);
        return false;
    }
    freeaddrinfo(res);

    CString reply;
    int code = ReadReply(reply, error);
    if (code != 220)
    {
        error = _T("May chu tu choi ket noi:\r\n") + reply;
        Disconnect();
        return false;
    }
    return true;
}

bool CFtpSession::Login(const CString& user, const CString& pass, CString& error)
{
    CString reply, cmd;

    cmd.Format(_T("USER %s"), user.GetString());
    int code = SendCommandAndReadReply(cmd, reply, error);
    if (code == 331)
    {
        cmd.Format(_T("PASS %s"), pass.GetString());
        code = SendCommandAndReadReply(cmd, reply, error);
    }
    if (code != 230)
    {
        error = _T("Dang nhap that bai:\r\n") + reply;
        return false;
    }

    // Mac dinh dung che do nhi phan (Binary/Image) cho tat ca cac lenh truyen file.
    SendCommandAndReadReply(_T("TYPE I"), reply, error);
    return true;
}

void CFtpSession::Disconnect()
{
    if (m_ctrlSocket != INVALID_SOCKET)
    {
        CString reply, err;
        SendCommandAndReadReply(_T("QUIT"), reply, err);
        closesocket(m_ctrlSocket);
        m_ctrlSocket = INVALID_SOCKET;
    }
    m_ctrlRecvBuf.Empty();
}

// ---------------------------------------------------------------------------
bool CFtpSession::Pwd(CString& path, CString& error)
{
    CString reply;
    int code = SendCommandAndReadReply(_T("PWD"), reply, error);
    if (code != 257) { error = _T("PWD loi:\r\n") + reply; return false; }

    // Dang phan hoi: 257 "/duong/dan" la thu muc hien hanh
    int q1 = reply.Find(_T('"'));
    int q2 = reply.Find(_T('"'), q1 + 1);
    if (q1 < 0 || q2 < 0) { error = _T("Khong doc duoc duong dan tu PWD."); return false; }
    path = reply.Mid(q1 + 1, q2 - q1 - 1);
    path.Replace(_T("\"\""), _T("\""));
    return true;
}

bool CFtpSession::Cwd(const CString& path, CString& error)
{
    CString reply, cmd;
    cmd.Format(_T("CWD %s"), path.GetString());
    int code = SendCommandAndReadReply(cmd, reply, error);
    if (code != 250)
    {
        error = _T("Khong doi duoc thu muc:\r\n") + reply;
        return false;
    }
    return true;
}

bool CFtpSession::Size(const CString& remoteFile, ULONGLONG& size, CString& error)
{
    CString reply, cmd;
    cmd.Format(_T("SIZE %s"), remoteFile.GetString());
    int code = SendCommandAndReadReply(cmd, reply, error);
    if (code != 213) { error = _T("May chu khong ho tro lenh SIZE:\r\n") + reply; return false; }
    CString num = reply.Mid(4);
    num.TrimRight(_T("\r\n"));
    num.Trim();
    size = (ULONGLONG)_ttoi64(num);
    return true;
}

bool CFtpSession::Dele(const CString& remoteFile, CString& error)
{
    CString reply, cmd;
    cmd.Format(_T("DELE %s"), remoteFile.GetString());
    int code = SendCommandAndReadReply(cmd, reply, error);
    if (code != 250) { error = _T("Khong xoa duoc file:\r\n") + reply; return false; }
    return true;
}

bool CFtpSession::Rmd(const CString& remoteDir, CString& error)
{
    CString reply, cmd;
    cmd.Format(_T("RMD %s"), remoteDir.GetString());
    int code = SendCommandAndReadReply(cmd, reply, error);
    if (code != 250) { error = _T("Khong xoa duoc thu muc:\r\n") + reply; return false; }
    return true;
}

bool CFtpSession::Mkd(const CString& remoteDir, CString& error)
{
    CString reply, cmd;
    cmd.Format(_T("MKD %s"), remoteDir.GetString());
    int code = SendCommandAndReadReply(cmd, reply, error);
    if (code != 257) { error = _T("Khong tao duoc thu muc:\r\n") + reply; return false; }
    return true;
}

bool CFtpSession::Rename(const CString& oldName, const CString& newName, CString& error)
{
    CString reply, cmd;
    cmd.Format(_T("RNFR %s"), oldName.GetString());
    int code = SendCommandAndReadReply(cmd, reply, error);
    if (code != 350) { error = _T("RNFR bi tu choi:\r\n") + reply; return false; }

    cmd.Format(_T("RNTO %s"), newName.GetString());
    code = SendCommandAndReadReply(cmd, reply, error);
    if (code != 250) { error = _T("RNTO bi tu choi:\r\n") + reply; return false; }
    return true;
}

// ---------------------------------------------------------------------------
bool CFtpSession::EnterPassiveMode(SOCKET& dataSocket, CString& error)
{
    dataSocket = INVALID_SOCKET;
    CString reply;
    int code = SendCommandAndReadReply(_T("PASV"), reply, error);
    if (code != 227) { error = _T("PASV bi tu choi:\r\n") + reply; return false; }

    int start = reply.Find(_T('('));
    int end = reply.Find(_T(')'), start);
    if (start < 0 || end < 0) { error = _T("Khong doc duoc dia chi PASV."); return false; }

    CString nums = reply.Mid(start + 1, end - start - 1);
    int p1, p2, p3, p4, p5, p6;
    if (_stscanf_s(nums, _T("%d,%d,%d,%d,%d,%d"), &p1, &p2, &p3, &p4, &p5, &p6) != 6)
    {
        error = _T("Khong phan tich duoc dia chi PASV: ") + nums;
        return false;
    }

    CString ip;
    ip.Format(_T("%d.%d.%d.%d"), p1, p2, p3, p4);
    int port = p5 * 256 + p6;

    dataSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (dataSocket == INVALID_SOCKET) { error = _T("Khong tao duoc data socket."); return false; }

    sockaddr_in addr;
    ZeroMemory(&addr, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((u_short)port);
    CStringA ipA(ip);
    inet_pton(AF_INET, ipA, &addr.sin_addr);

    if (connect(dataSocket, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR)
    {
        error.Format(_T("Khong mo duoc data connection toi %s:%d"), ip.GetString(), port);
        closesocket(dataSocket);
        dataSocket = INVALID_SOCKET;
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
std::vector<CString> CFtpSession::SplitWhitespace(const CString& line)
{
    std::vector<CString> tok;
    int i = 0, len = line.GetLength();
    while (i < len)
    {
        while (i < len && line[i] == _T(' ')) i++;
        int start = i;
        while (i < len && line[i] != _T(' ')) i++;
        if (i > start) tok.push_back(line.Mid(start, i - start));
    }
    return tok;
}

bool CFtpSession::ParseListLineUnix(const CString& line, CFtpFileEntry& entry)
{
    if (line.IsEmpty()) return false;
    TCHAR t = line[0];
    if (t != _T('d') && t != _T('-') && t != _T('l')) return false;

    std::vector<CString> tok = SplitWhitespace(line);
    if (tok.size() < 9) return false;

    entry.isDirectory = (t == _T('d'));
    entry.size = (ULONGLONG)_ttoi64(tok[4]);
    entry.dateStr = tok[5] + _T(" ") + tok[6] + _T(" ") + tok[7];

    CString name = tok[8];
    for (size_t k = 9; k < tok.size(); k++)
        name += _T(" ") + tok[k];

    int arrow = name.Find(_T(" -> "));   // symlink "ten -> dich"
    if (arrow >= 0) name = name.Left(arrow);

    entry.name = name;
    return true;
}

bool CFtpSession::ParseListLineDos(const CString& line, CFtpFileEntry& entry)
{
    // Dang kieu IIS/Windows FTP: "09-15-24  10:23AM  <DIR>          ten"
    //                        hoac "09-15-24  10:23AM     1234        ten"
    std::vector<CString> tok = SplitWhitespace(line);
    if (tok.size() < 4) return false;

    if (tok[2].CompareNoCase(_T("<DIR>")) == 0)
    {
        entry.isDirectory = true;
        entry.size = 0;
    }
    else
    {
        entry.isDirectory = false;
        entry.size = (ULONGLONG)_ttoi64(tok[2]);
    }
    entry.dateStr = tok[0] + _T(" ") + tok[1];

    CString name = tok[3];
    for (size_t k = 4; k < tok.size(); k++)
        name += _T(" ") + tok[k];
    entry.name = name;
    return true;
}

bool CFtpSession::List(const CString& path, std::vector<CFtpFileEntry>& out, CString& error)
{
    out.clear();

    SOCKET dataSock;
    if (!EnterPassiveMode(dataSock, error)) return false;

    CString cmd = path.IsEmpty() ? CString(_T("LIST")) : (_T("LIST ") + path);
    if (!SendCommand(cmd, error)) { closesocket(dataSock); return false; }

    CString reply;
    int code = ReadReply(reply, error);
    if (code != 150 && code != 125)
    {
        error = _T("LIST bi tu choi:\r\n") + reply;
        closesocket(dataSock);
        return false;
    }

    CStringA raw;
    char buf[4096];
    int n;
    while ((n = recv(dataSock, buf, sizeof(buf), 0)) > 0)
        raw.Append(buf, n);
    closesocket(dataSock);

    ReadReply(reply, error);   // 226 Transfer complete

    CString text(raw);
    int pos = 0;
    int textLen = text.GetLength();
    while (pos < textLen)
    {
        int nl = text.Find(_T('\n'), pos);
        CString line = (nl < 0) ? text.Mid(pos) : text.Mid(pos, nl - pos);
        pos = (nl < 0) ? textLen : nl + 1;
        line.TrimRight(_T("\r\n"));
        if (line.IsEmpty()) continue;

        CFtpFileEntry entry;
        if (ParseListLineUnix(line, entry) || ParseListLineDos(line, entry))
        {
            if (entry.name != _T(".") && entry.name != _T(".."))
                out.push_back(entry);
        }
    }
    return true;
}

// ---------------------------------------------------------------------------
bool CFtpSession::DownloadRange(const CString& remoteFile, const CString& localFile,
                                 ULONGLONG offset, ULONGLONG length,
                                 std::atomic<ULONGLONG>* counter,
                                 std::atomic<bool>* cancelFlag,
                                 CString& error)
{
    SOCKET dataSock;
    if (!EnterPassiveMode(dataSock, error)) return false;

    if (offset > 0)
    {
        CString cmd, reply;
        cmd.Format(_T("REST %llu"), offset);
        int code = SendCommandAndReadReply(cmd, reply, error);
        if (code != 350)
        {
            error = _T("REST bi tu choi (may chu co the khong ho tro tai song song):\r\n") + reply;
            closesocket(dataSock);
            return false;
        }
    }

    CString cmd, reply;
    cmd.Format(_T("RETR %s"), remoteFile.GetString());
    if (!SendCommand(cmd, error)) { closesocket(dataSock); return false; }
    int code = ReadReply(reply, error);
    if (code != 150 && code != 125)
    {
        error = _T("RETR bi tu choi:\r\n") + reply;
        closesocket(dataSock);
        return false;
    }

    CFile file;
    CFileException fe;
    if (!file.Open(localFile, CFile::modeWrite | CFile::modeNoTruncate | CFile::shareDenyNone, &fe))
    {
        error.Format(_T("Khong mo duoc file dich (loi %d)."), fe.m_cause);
        closesocket(dataSock);
        return false;
    }
    file.Seek((LONGLONG)offset, CFile::begin);

    const bool limited = (length > 0);
    ULONGLONG remaining = length;
    char buf[8192];
    bool truncatedOnPurpose = false;

    for (;;)
    {
        if (cancelFlag && cancelFlag->load())
        {
            error = _T("Da huy thao tac.");
            file.Close();
            closesocket(dataSock);
            return false;
        }

        int want = sizeof(buf);
        if (limited)
        {
            if (remaining == 0) break;
            if (remaining < (ULONGLONG)want) want = (int)remaining;
        }

        int n = recv(dataSock, buf, want, 0);
        if (n < 0) { error = _T("Loi khi nhan du lieu."); file.Close(); closesocket(dataSock); return false; }
        if (n == 0) break;   // server dong ket noi -> het du lieu

        file.Write(buf, n);
        if (counter) *counter += (ULONGLONG)n;
        if (limited) remaining -= (ULONGLONG)n;
    }

    file.Close();

    if (limited && remaining == 0)
    {
        // Ta chu dong dong data connection som (chi lay dung 1 doan cua file)
        // nen may chu se bao 426 thay vi 226 — day la hanh vi binh thuong khi
        // tai song song nhieu doan.
        truncatedOnPurpose = true;
    }
    closesocket(dataSock);
    ReadReply(reply, error);

    if (limited && !truncatedOnPurpose && remaining > 0)
    {
        error = _T("Ket noi bi ngat truoc khi tai du du lieu.");
        return false;
    }
    return true;
}

bool CFtpSession::UploadFile(const CString& localFile, const CString& remoteFile,
                              std::atomic<ULONGLONG>* counter,
                              std::atomic<bool>* cancelFlag,
                              CString& error)
{
    CFile file;
    CFileException fe;
    if (!file.Open(localFile, CFile::modeRead | CFile::shareDenyWrite, &fe))
    {
        error.Format(_T("Khong mo duoc file nguon (loi %d)."), fe.m_cause);
        return false;
    }

    SOCKET dataSock;
    if (!EnterPassiveMode(dataSock, error)) { file.Close(); return false; }

    CString cmd, reply;
    cmd.Format(_T("STOR %s"), remoteFile.GetString());
    if (!SendCommand(cmd, error)) { file.Close(); closesocket(dataSock); return false; }
    int code = ReadReply(reply, error);
    if (code != 150 && code != 125)
    {
        error = _T("STOR bi tu choi:\r\n") + reply;
        file.Close();
        closesocket(dataSock);
        return false;
    }

    char buf[8192];
    UINT n;
    while ((n = file.Read(buf, sizeof(buf))) > 0)
    {
        if (cancelFlag && cancelFlag->load())
        {
            error = _T("Da huy thao tac.");
            file.Close();
            closesocket(dataSock);
            return false;
        }
        int total = 0;
        while (total < (int)n)
        {
            int sent = send(dataSock, buf + total, n - total, 0);
            if (sent == SOCKET_ERROR)
            {
                error = _T("Loi khi gui du lieu.");
                file.Close();
                closesocket(dataSock);
                return false;
            }
            total += sent;
        }
        if (counter) *counter += (ULONGLONG)n;
    }

    file.Close();
    closesocket(dataSock);
    ReadReply(reply, error);
    return true;
}
