// InputDlg.cpp
#include "stdafx.h"
#include "InputDlg.h"

CInputDlg::CInputDlg(const CString& prompt, const CString& initialValue, CWnd* pParent)
    : CDialogEx(IDD_INPUT_DIALOG, pParent), m_prompt(prompt), m_value(initialValue)
{
}

void CInputDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
    DDX_Text(pDX, IDC_EDIT_INPUT, m_value);
}

BOOL CInputDlg::OnInitDialog()
{
    CDialogEx::OnInitDialog();
    SetDlgItemText(IDC_STATIC_PROMPT, m_prompt);
    CEdit* pEdit = (CEdit*)GetDlgItem(IDC_EDIT_INPUT);
    if (pEdit)
    {
        pEdit->SetFocus();
        pEdit->SetSel(0, -1);
    }
    return FALSE; // da tu dat focus
}

BEGIN_MESSAGE_MAP(CInputDlg, CDialogEx)
END_MESSAGE_MAP()
