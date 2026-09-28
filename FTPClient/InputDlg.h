// InputDlg.h — hop thoai nhap 1 dong van ban, dung chung cho "Doi ten" va "Tao thu muc moi"
#pragma once
#include "resource.h"

class CInputDlg : public CDialogEx
{
public:
    CInputDlg(const CString& prompt, const CString& initialValue, CWnd* pParent = nullptr);

    CString m_value;

    enum { IDD = IDD_INPUT_DIALOG };

protected:
    virtual void DoDataExchange(CDataExchange* pDX);
    virtual BOOL OnInitDialog();

    DECLARE_MESSAGE_MAP()

private:
    CString m_prompt;
};
