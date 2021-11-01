// DlgUser.cpp : implementation file
//

#include "stdafx.h"
#include "NetServer.h"
#include "DlgUser.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CDlgUser dialog


CDlgUser::CDlgUser(CWnd* pParent /*=NULL*/)
	: CDialog(CDlgUser::IDD, pParent)
{
	//{{AFX_DATA_INIT(CDlgUser)
	m_strAccount = _T("Admin");
	m_strPassword = _T("");
	//}}AFX_DATA_INIT
}


void CDlgUser::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CDlgUser)
	DDX_Text(pDX, IDC_EDT_ACCOUNT, m_strAccount);
	DDX_Text(pDX, IDC_EDT_PASSWORD, m_strPassword);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CDlgUser, CDialog)
	//{{AFX_MSG_MAP(CDlgUser)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CDlgUser message handlers

void CDlgUser::OnOK() 
{
	// TODO: Add extra validation here
	UpdateData();
	if(m_strAccount != "Admin" || m_strPassword != "1234") {
		AfxMessageBox("Account/Password Error!");
		return;
	}
	CDialog::OnOK();
}
