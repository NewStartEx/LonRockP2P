// ClientDlg.cpp : implementation file
//

#include "stdafx.h"
#include "Client.h"
#include "ClientDlg.h"
#include "CltManager.h"
#include "CltSDKFun.h"
#include "NTTProtocol.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


CClientDlg* g_GUI = NULL;
CString g_strStatus;
CString g_strContent;
int LinkDvr(LPCTSTR strDVR, ULONG addr, BOOL bWork)
{
	CString strStatus,strTmp;
	strStatus = strDVR;
	strStatus += ":";
	if(bWork) {
		BYTE byIP[4];
		byIP[0] = BYTE(addr & 0xFF);
		byIP[1] = BYTE((addr&0xFF00) >> 8);
		byIP[2] = BYTE((addr&0xFF0000) >> 16);
		byIP[3] = BYTE((addr&0xFF000000) >> 24);
		strTmp.Format("%d.%d.%d.%d",byIP[0],byIP[1],byIP[2],byIP[3]);
	}
	else {
		strTmp.LoadString(IDS_NOTFOUND);
	}
	strStatus += strTmp;
	g_strStatus = strStatus;
	if(NULL != g_GUI) {
		//// g_GUI->SetStatus(strStatus);
		g_GUI->PostMessage(WM_DVR_STATUS);
	}
	return 1;
}

int RcvDvrMsg(LPCTSTR strDVR, PVOID pInBuffer)
{
	CString strContent;
	PCMDINFO pMsg = (PCMDINFO)pInBuffer;
	strContent = pMsg->chCmd;
	g_strContent = strContent;
	if(NULL != g_GUI) {
		//// g_GUI->SetContent(strContent);
		g_GUI->PostMessage(WM_DVR_CONTENT);
	}
	return 1;
}

int DvrLeft(LPCTSTR strDVR,UINT uReason)
{
	CString strStatus,strTmp;
	strStatus = strDVR;
	strStatus += ":";
	strTmp.LoadString(IDS_DISMISS);
	strStatus += strTmp;
	g_strStatus = strStatus;
	if(NULL != g_GUI) {
		//// g_GUI->SetStatus(strStatus);
		g_GUI->PostMessage(WM_DVR_STATUS);
	}
	return 1;
}
/////////////////////////////////////////////////////////////////////////////
// CAboutDlg dialog used for App About

class CAboutDlg : public CDialog
{
public:
	CAboutDlg();

// Dialog Data
	//{{AFX_DATA(CAboutDlg)
	enum { IDD = IDD_ABOUTBOX };
	//}}AFX_DATA

	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CAboutDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	//{{AFX_MSG(CAboutDlg)
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialog(CAboutDlg::IDD)
{
	//{{AFX_DATA_INIT(CAboutDlg)
	//}}AFX_DATA_INIT
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CAboutDlg)
	//}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialog)
	//{{AFX_MSG_MAP(CAboutDlg)
		// No message handlers
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CClientDlg dialog

CClientDlg::CClientDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CClientDlg::IDD, pParent)
{
	//{{AFX_DATA_INIT(CClientDlg)
	m_strKeyName = _T("");
	m_strStatus = _T("");
	m_strContent = _T("");
	//}}AFX_DATA_INIT
	// Note that LoadIcon does not require a subsequent DestroyIcon in Win32
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
	m_pManager = NULL;
}

void CClientDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CClientDlg)
	DDX_Text(pDX, IDC_EDT_KEYNAME, m_strKeyName);
	DDV_MaxChars(pDX, m_strKeyName, 8);
	DDX_Text(pDX, IDC_TXT_STATUS, m_strStatus);
	DDX_Text(pDX, IDC_TXT_STRING, m_strContent);
	//}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CClientDlg, CDialog)
	//{{AFX_MSG_MAP(CClientDlg)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_BN_CLICKED(IDC_BTN_SERCH, OnBtnSerch)
	ON_WM_DESTROY()
	//}}AFX_MSG_MAP
	ON_MESSAGE(WM_DVR_STATUS,OnDvrStatus)
	ON_MESSAGE(WM_DVR_CONTENT,OnDvrContent)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CClientDlg message handlers

BOOL CClientDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	// Add "About..." menu item to system menu.

	// IDM_ABOUTBOX must be in the system command range.
	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);

	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu != NULL)
	{
		CString strAboutMenu;
		strAboutMenu.LoadString(IDS_ABOUTBOX);
		if (!strAboutMenu.IsEmpty())
		{
			pSysMenu->AppendMenu(MF_SEPARATOR);
			pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
		}
	}

	// Set the icon for this dialog.  The framework does this automatically
	//  when the application's main window is not a dialog
	SetIcon(m_hIcon, TRUE);			// Set big icon
	SetIcon(m_hIcon, FALSE);		// Set small icon
	
	// TODO: Add extra initialization here
	SetCltCallback(LinkDvr,RcvDvrMsg,DvrLeft);
	m_pManager = new CCltManager;
	if(!m_pManager->StartWorking(m_hWnd,NETS_NAME)) {
		delete m_pManager;
		m_pManager = NULL;
	}
	g_GUI = this;
	return TRUE;  // return TRUE  unless you set the focus to a control
}

void CClientDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	if ((nID & 0xFFF0) == IDM_ABOUTBOX)
	{
		CAboutDlg dlgAbout;
		dlgAbout.DoModal();
	}
	else
	{
		CDialog::OnSysCommand(nID, lParam);
	}
}

// If you add a minimize button to your dialog, you will need the code below
//  to draw the icon.  For MFC applications using the document/view model,
//  this is automatically done for you by the framework.

void CClientDlg::OnPaint() 
{
	if (IsIconic())
	{
		CPaintDC dc(this); // device context for painting

		SendMessage(WM_ICONERASEBKGND, (WPARAM) dc.GetSafeHdc(), 0);

		// Center icon in client rectangle
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// Draw the icon
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialog::OnPaint();
	}
}

// The system calls this to obtain the cursor to display while the user drags
//  the minimized window.
HCURSOR CClientDlg::OnQueryDragIcon()
{
	return (HCURSOR) m_hIcon;
}

void CClientDlg::OnCancel()
{
}

void CClientDlg::OnBtnSerch() 
{
	// TODO: Add your control notification handler code here
	UpdateData();
	if(0 == m_strKeyName.GetLength()) {
		AfxMessageBox(IDS_KEYERR);
		return;
	}
	CMDINFO CmdInfo;
	memset(&CmdInfo,0,sizeof(CMDINFO));
	strcpy(CmdInfo.chCmd,"Test");
	BYTE byErr;
	if(NULL != m_pManager) {
		if(!m_strOldName.IsEmpty())
			m_pManager->StopLink(m_strOldName);
		if(NULL == m_pManager->StartLink(m_strKeyName,LINK_TUNNEL,&CmdInfo,byErr)) {
			AfxMessageBox("IDS_NETERR");
			return;
		}
		m_strOldName = m_strKeyName;
	}
}

void CClientDlg::SetStatus(LPCTSTR status)
{
	m_strStatus = status;
	UpdateData(FALSE);
}

void CClientDlg::SetContent(LPCTSTR content)
{
	m_strContent = content;
	UpdateData(FALSE);
}

void CClientDlg::OnOK() 
{
	// TODO: Add extra validation here
	if(NULL != m_pManager)
		m_pManager->StopWorking();
	CDialog::OnOK();
}

void CClientDlg::OnDestroy() 
{
	CDialog::OnDestroy();
	
	// TODO: Add your message handler code here
	if(NULL != m_pManager)
		delete m_pManager;
}

void CClientDlg::OnDvrStatus(WPARAM wParam,LPARAM lParam)
{
	UpdateData(); //// 保证其他参数不被改变
	m_strStatus = g_strStatus;
	UpdateData(FALSE);
}

void CClientDlg::OnDvrContent(WPARAM wParam,LPARAM lParam)
{
	UpdateData();  //// 保证其他参数不被改变
	m_strContent = g_strContent;
	UpdateData(FALSE);
}
