// DVRDlg.cpp : implementation file
//

#include "stdafx.h"
#include "DVR.h"
#include "DVRDlg.h"
#include "Global.h"
#include "DVRGlobal.h"
#include "DVRManager.h"
#include "LocalSDKFun.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

CString g_strContent;
int LinkClient(HANDLE hClient, ULONG addr)
{
	BYTE byIP[4];
	byIP[0] = BYTE(addr & 0xFF);
	byIP[1] = BYTE((addr&0xFF00) >> 8);
	byIP[2] = BYTE((addr&0xFF0000) >> 16);
	byIP[3] = BYTE((addr&0xFF000000) >> 24);
	TRACE("Link Client:%d.%d.%d.%d",byIP[0],byIP[1],byIP[2],byIP[3]);
	return 1;
}

int RcvClientCmd(HANDLE hClient, PVOID pInBuffer,PVOID pOutBuffer)
{
	ASSERT(pInBuffer != NULL);
	PCMDINFO pCmd = (PCMDINFO)pInBuffer;
	TRACE(pCmd->chCmd);
	PCMDINFO pMsg = (PCMDINFO)pOutBuffer;
	if(pMsg != NULL) {
		strcpy(pMsg->chCmd,g_strContent);
	}
	return 1;
}

int ClientLeft(HANDLE hClient)
{
	TRACE("Client Left");
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
// CDVRDlg dialog

CDVRDlg::CDVRDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CDVRDlg::IDD, pParent)
{
	//{{AFX_DATA_INIT(CDVRDlg)
	m_strKeyName = _T("");
	m_strContent = _T("");
	m_strInfo = _T("");
	m_strPassword = _T("");
	//}}AFX_DATA_INIT
	// Note that LoadIcon does not require a subsequent DestroyIcon in Win32
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
	m_pManager = NULL;
}

void CDVRDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CDVRDlg)
	DDX_Text(pDX, IDC_EDT_KEYNAME, m_strKeyName);
	DDV_MaxChars(pDX, m_strKeyName, 8);
	DDX_Text(pDX, IDC_EDT_STRING, m_strContent);
	DDV_MaxChars(pDX, m_strContent, 8);
	DDX_Text(pDX, IDC_TXT_INFO, m_strInfo);
	DDX_Text(pDX, IDC_EDT_PASSWORD, m_strPassword);
	//}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CDVRDlg, CDialog)
	//{{AFX_MSG_MAP(CDVRDlg)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_BN_CLICKED(IDC_BTN_APPLY, OnBtnApply)
	ON_BN_CLICKED(IDC_BTN_TEST, OnBtnTest)
	ON_BN_CLICKED(IDC_BTN_DELETE, OnBtnDelete)
	ON_WM_DESTROY()
	ON_WM_TIMER()
	//}}AFX_MSG_MAP
	ON_MESSAGE(WM_NETS_FEEDBACK,OnNetSFeedback)
	ON_MESSAGE(WM_NETS_TUNNEL,OnNetSTunnel)
	ON_MESSAGE(WM_CLT_TUNNEL,OnCltTunnel)
	ON_MESSAGE(WM_NETS_NORESP,OnNetSNoresp)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CDVRDlg message handlers

BOOL CDVRDlg::OnInitDialog()
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
#ifdef _DEBUG
		m_strKeyName = _T("ertyy");
		m_strPassword = _T("1234");
#endif
	SetDvrCallback(LinkClient,RcvClientCmd,ClientLeft);
	
	m_pManager = new CDVRManager(FOURCC_NCAM,0,0);
	CString strTmp = GetCurrentPath();
	if(!m_pManager->StartWorking(m_hWnd,GetCurrentPath(),NETS_NAME)) {
		delete m_pManager;
		m_pManager = NULL;
	}
	
	char chHostName[32];
	if(SOCKET_ERROR != gethostname(chHostName,32)) {
		m_strContent = chHostName;
		if(m_strContent.GetLength()>8) {
			m_strContent = m_strContent.Left(8);
		}
	}
	SetTimer(1,1000,NULL);
	UpdateData(FALSE);
	return TRUE;  // return TRUE  unless you set the focus to a control
}

void CDVRDlg::OnSysCommand(UINT nID, LPARAM lParam)
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

void CDVRDlg::OnPaint() 
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

void CDVRDlg::OnCancel()
{
}

// The system calls this to obtain the cursor to display while the user drags
//  the minimized window.
HCURSOR CDVRDlg::OnQueryDragIcon()
{
	return (HCURSOR) m_hIcon;
}

void CDVRDlg::OnBtnApply() 
{
	// TODO: Add your control notification handler code here
	UpdateData();
	if(NULL == m_pManager) return;
	if(0 == m_strKeyName.GetLength()) {
		AfxMessageBox(IDS_NAMESHORT);
		return ;
	}
	m_pManager->SetGlobalName(m_strKeyName,m_strPassword);
	if(SENDLINK_SUCCESS != m_pManager->RegisterUnit()) {
		AfxMessageBox(IDS_LINKERROR);
		return;
	}
	m_strInfo.LoadString(IDS_WAITING);
	UpdateData(FALSE);
}

void CDVRDlg::OnBtnTest() 
{
	// TODO: Add your control notification handler code here
	UpdateData();
	if(NULL == m_pManager) return;
	if(0 == m_strKeyName.GetLength()) {
		AfxMessageBox(IDS_NAMESHORT);
		return ;
	}
	m_pManager->SetGlobalName(m_strKeyName,m_strPassword);
	if(SENDLINK_SUCCESS != m_pManager->ManuUpdate()) {
		AfxMessageBox(IDS_LINKERROR);
		return;
	}
	m_strInfo.LoadString(IDS_WAITING);
}

void CDVRDlg::OnBtnDelete() 
{
	// TODO: Add your control notification handler code here
	UpdateData();
	if(NULL == m_pManager) return;
	if(0 == m_strKeyName.GetLength()) {
		AfxMessageBox(IDS_NAMESHORT);
		return ;
	}
	m_pManager->SetGlobalName(m_strKeyName,m_strPassword);
	if(SENDLINK_SUCCESS != m_pManager->UnregisterUnit()) {
		AfxMessageBox(IDS_LINKERROR);
		return;
	}
	m_strInfo.LoadString(IDS_WAITING);
	UpdateData(FALSE);
}

void CDVRDlg::OnOK() 
{
	// TODO: Add extra validation here
	if(NULL != m_pManager)
		m_pManager->StopWorking();
	KillTimer(1);
	CDialog::OnOK();
}

void CDVRDlg::OnDestroy() 
{
	if(NULL != m_pManager)
		delete m_pManager;
	
	CDialog::OnDestroy();
	
	// TODO: Add your message handler code here
}

void CDVRDlg::OnNetSFeedback(WPARAM wParam,LPARAM lParam)
{
	switch(wParam) {
	case ACK_REGISTER_SUCCESS:
		m_strInfo.LoadString(IDS_REG_SUCCESS);
		break;
	case ACK_REGISTER_DUPLICATE:
		m_strInfo.LoadString(IDS_REG_DUPLICATE);
		break;
	case ACK_UPDATE_OK:
		m_strInfo.LoadString(IDS_UPDATE_OK);
		break;
	case ACK_DELETE_SUCCESS:
		m_strInfo.LoadString(IDS_DEL_SUCCESS);
		break;
	default:
		m_strInfo.LoadString(IDS_INVALID);
	}
	UpdateData(FALSE);
}

void CDVRDlg::OnNetSTunnel(WPARAM wParam,LPARAM lParam)
{
	DWORD addr = (DWORD)wParam;
	BYTE byIP[4];
	byIP[0] = BYTE(addr & 0xFF);
	byIP[1] = BYTE((addr&0xFF00) >> 8);
	byIP[2] = BYTE((addr&0xFF0000) >> 16);
	byIP[3] = BYTE((addr&0xFF000000) >> 24);
	TRACE("NetS notify tunnel to:%d.%d.%d.%d",byIP[0],byIP[1],byIP[2],byIP[3]);
}

void CDVRDlg::OnCltTunnel(WPARAM wParam,LPARAM lParam)
{
	if(TUNNEL_ACK_SUCCESS == wParam) {
		TRACE("Get clt success");
	}
	else {
		TRACE("Get clt Error");
	}
}

void CDVRDlg::OnNetSNoresp(WPARAM,LPARAM lParam)
{
	m_strInfo.LoadString(IDS_TIMEOUT);
	UpdateData(FALSE);
}

void CDVRDlg::OnTimer(UINT nIDEvent) 
{
	// TODO: Add your message handler code here and/or call default
	UpdateData();
	g_strContent = m_strContent;
	CDialog::OnTimer(nIDEvent);
}
