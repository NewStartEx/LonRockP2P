// NetServerDlg.cpp : implementation file
//

#include "stdafx.h"
#include "NetServer.h"
#include "NetServerDlg.h"
#include "Global.h"
#include "HttpHandle.h"
#include "NetSGlobal.h"
#include "NetSManager.h"
#include "NTTProtocol.h"
#include "DlgUser.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

#define TIME_FORMAT "%Y-%m-%d %H:%M:%S "
extern int g_nAccountNum;

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
// CNetServerDlg dialog

CNetServerDlg::CNetServerDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CNetServerDlg::IDD, pParent)
{
	//{{AFX_DATA_INIT(CNetServerDlg)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
	// Note that LoadIcon does not require a subsequent DestroyIcon in Win32
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
	m_pManager = NULL;
	m_bSysClosed = FALSE;
	
	m_rcAccountInfo.left = 182;
	m_rcAccountInfo.top = 18;
	m_rcAccountInfo.right = 400;
	m_rcAccountInfo.bottom = 70;

	m_nOnlineNum = 0;
}

void CNetServerDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CNetServerDlg)
	DDX_Control(pDX, IDC_LST_WORKRECORD, m_LbxWorkRecord);
	DDX_Control(pDX, IDC_LST_ONLINEDVR, m_lstActiveDVR);
	//}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CNetServerDlg, CDialog)
	//{{AFX_MSG_MAP(CNetServerDlg)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_WM_DESTROY()
	ON_BN_CLICKED(IDC_BTN_CLEAR, OnBtnClear)
	ON_WM_CLOSE()
	ON_WM_TIMER()
	ON_WM_NCHITTEST()
	//}}AFX_MSG_MAP
	ON_MESSAGE(WM_DVR_CHANGED,OnDvrChanged)
	ON_MESSAGE(WM_CLIENT_REQUEST,OnClientRequest)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CNetServerDlg message handlers

BOOL CNetServerDlg::OnInitDialog()
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
	ListView_SetExtendedListViewStyle(m_lstActiveDVR.m_hWnd,LVS_EX_FULLROWSELECT|
		LVS_EX_GRIDLINES|LVS_EX_UNDERLINEHOT);  //// LVS_EX_ONECLICKACTIVATE|
	
////	m_lstActiveDVR.SetBkColor(GetApp()->m_clrBkg);
	CString strTitle;
	strTitle.LoadString(IDS_DVRNAME);
	m_lstActiveDVR.InsertColumn(0,strTitle,LVCFMT_LEFT,130);
	strTitle.LoadString(IDS_DVRTYPE);
	m_lstActiveDVR.InsertColumn(1,strTitle,LVCFMT_CENTER,75);
	strTitle.LoadString(IDS_DVRADDR);
	m_lstActiveDVR.InsertColumn(2,strTitle,LVCFMT_CENTER,120);
	strTitle.LoadString(IDS_DVRBEHAVIOR);
	m_lstActiveDVR.InsertColumn(3,strTitle,LVCFMT_CENTER,50);
	CTime st = CTime::GetCurrentTime();
	CString strTmp;
	strTmp.LoadString(IDS_START);
	strTmp = st.Format(TIME_FORMAT) + strTmp;
	m_LbxWorkRecord.AddString(strTmp);

	m_pManager = new CNetSManager;
	if(!m_pManager->StartWorking(m_hWnd,GetCurrentPath())) {
		AfxMessageBox("Init Resource Error!");
		delete m_pManager;
		m_pManager = NULL;
		return FALSE;
	}
	DEVMODE DevMode;
	memset(&DevMode,0,sizeof(DEVMODE));
	DevMode.dmSize = sizeof(DEVMODE);
	EnumDisplaySettings(NULL,ENUM_CURRENT_SETTINGS,	&DevMode );
	
	CHandleHttp::m_hChgPort = CreateEvent(NULL,TRUE,FALSE,NULL);
	CHandleHttp::m_hFinish = CreateEvent(NULL,TRUE,FALSE,NULL);
	CHandleHttp::m_wWebPort = HTTP_PORT;
	DWORD dwID;
	CHandleHttp::IncreaseHttpThreadCount();
	CreateThread(NULL,0,(LPTHREAD_START_ROUTINE)CHandleHttp::HttpServerProc,NULL,0,&dwID);

	int x = 10, y = 40;
	int width = 410,height = DevMode.dmPelsHeight-100;
	GetDlgItem(IDC_LST_ONLINEDVR)->SetWindowPos(NULL,x,y,width,height,SWP_SHOWWINDOW);
	GetDlgItem(IDC_TXT_DVRLST)->SetWindowPos(NULL,x,y-20,width,height,SWP_NOSIZE);
	x = x+width+15;
	width = DevMode.dmPelsWidth-x-15;
	GetDlgItem(IDC_LST_WORKRECORD)->SetWindowPos(NULL,x,y,width,height,SWP_SHOWWINDOW);
	GetDlgItem(IDC_TXT_CNCTLST)->SetWindowPos(NULL,x,y-20,width,height,SWP_NOSIZE);
	GetDlgItem(IDC_BTN_CLEAR)->SetWindowPos(NULL,DevMode.dmPelsWidth-90,y-25,width,height,SWP_NOSIZE);

	SetTimer(ClEAR_DVRLIST_TIMER,60000,NULL);
	ShowWindow(SW_SHOWMAXIMIZED);
	return TRUE;  // return TRUE  unless you set the focus to a control
}

void CNetServerDlg::OnSysCommand(UINT nID, LPARAM lParam)
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

void CNetServerDlg::OnPaint() 
{
	CPaintDC dc(this); // device context for painting

	if (IsIconic())
	{
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
		CFont fontTitle;
		LOGFONT lgf;
		memset(&lgf,0,sizeof(LOGFONT));
		strcpy(lgf.lfFaceName,"Tahoma");
		lgf.lfHeight=14;
		lgf.lfWeight=400;
		fontTitle.CreateFontIndirect(&lgf);
		CFont * poldFont = dc.SelectObject(&fontTitle);
		dc.SetTextColor(0);
		dc.SetBkMode(TRANSPARENT);
		CString strInfo;
		strInfo.Format("注册用户: %d,  在线用户: %d",g_nAccountNum,m_nOnlineNum);
		dc.TextOut(m_rcAccountInfo.left,m_rcAccountInfo.top,strInfo);
		
		CDialog::OnPaint();
	}
}

// The system calls this to obtain the cursor to display while the user drags
//  the minimized window.
HCURSOR CNetServerDlg::OnQueryDragIcon()
{
	return (HCURSOR) m_hIcon;
}

void CNetServerDlg::OnCancel()
{
	if(m_bSysClosed) {
		CDialog::OnOK();
	}
}

void CNetServerDlg::OnDvrChanged(WPARAM wParam, LPARAM lParam)
{
	//// 注意这里必须是 SendMessage. 如果是PostMessage,lParam可能变化或者丢失
	PDVRMSG pDvrMsg = (PDVRMSG)lParam;
	CString strType,strName,strAddr,strDesc;
	switch(pDvrMsg->dwType) {
	case FOURCC_CARD:
		strType.LoadString(IDS_TYPE_CARD);
		break;
	case FOURCC_MDVR:
		strType.LoadString(IDS_TYPE_MDVR);
		break;
	case FOURCC_EDVR:
		strType.LoadString(IDS_TYPE_EDVR);
		break;
	case FOURCC_NCAM:
		strType.LoadString(IDS_TYPE_NCAM);
		break;
	case FOURCC_NDVR:
		strType.LoadString(IDS_TYPE_NDVR);
		break;
	default:
		strType.LoadString(IDS_TYPE_UNKNOW);
		break;
	}
	TRACE("NetS get TYPE:%x",pDvrMsg->dwType);
	strName = pDvrMsg->chName;
	BYTE byIP[4];
	byIP[0] = BYTE(pDvrMsg->dwIPaddr & 0xFF);
	byIP[1] = BYTE((pDvrMsg->dwIPaddr&0xFF00) >> 8);
	byIP[2] = BYTE((pDvrMsg->dwIPaddr&0xFF0000) >> 16);
	byIP[3] = BYTE((pDvrMsg->dwIPaddr&0xFF000000) >> 24);
	strAddr.Format("%d.%d.%d.%d",byIP[0],byIP[1],byIP[2],byIP[3]);
	switch(wParam) {
	case WPARAM_DVR_ONLINE:
		strDesc.LoadString(IDS_ONLINE);
		break;
	case WPARAM_DVR_OFFLINE:
		strDesc.LoadString(IDS_OFFLINE);
		break;
	case WPARAM_DVR_DELETED:
		strDesc.LoadString(IDS_DELETE);
		break;
	case WPARAM_DVR_ADDED:
		strDesc.LoadString(IDS_REGISTER);
		break;
	default:
		break;
	}

	int nIndex = SearchDvrList(strName);
	if(-1 == nIndex) {
		nIndex = ArrangDvrPos(strName);
		m_lstActiveDVR.InsertItem(nIndex,strName);
	}
	m_lstActiveDVR.SetItemText(nIndex,1,strType);
	m_lstActiveDVR.SetItemText(nIndex,2,strAddr);
	m_lstActiveDVR.SetItemText(nIndex,3,strDesc);

	m_nOnlineNum = m_lstActiveDVR.GetItemCount();
	InvalidateRect(&m_rcAccountInfo);
}

int CNetServerDlg::ArrangDvrPos(LPCTSTR DvrName)
{
	CString strName1,strName2;
	int count = m_lstActiveDVR.GetItemCount();
	if(0 == count) return 0;
	strName1 = DvrName;
	strName1.MakeUpper();
	if(1 == count) {
		strName2 = m_lstActiveDVR.GetItemText(0,0);
		strName2.MakeUpper();
		ASSERT(strName1 != strName2);
		if(strName1 < strName2) {
			return 0;
		}
		else {
			return 1;
		}
	}
	else {
		return OrderMakeDvrPos(strName1,0,count);
	}
}

int CNetServerDlg::OrderMakeDvrPos(LPCTSTR DvrName,int startIndex, int stopIndex)
{
	CString strName1,strName2;
	strName1 = DvrName;
	strName1.MakeUpper();
	if(stopIndex-startIndex < 10) {
		for(int i = startIndex; i < stopIndex; i++) {
			strName2 = m_lstActiveDVR.GetItemText(i,0);
			strName2.MakeUpper();
			ASSERT(strName1 != strName2);
			if(strName1 < strName2) {
				break;
			}
		}
		return i;
	}
	int newPos = (startIndex+stopIndex) >> 1;
	strName2 = m_lstActiveDVR.GetItemText(newPos,0);
	strName2.MakeUpper();
	ASSERT(strName1 != strName2);
	if(strName1 < strName2) {
		return OrderMakeDvrPos(strName1,startIndex,newPos);
	}
	else {
		return OrderMakeDvrPos(strName1,newPos,stopIndex);
	}
}

int CNetServerDlg::SearchDvrList(LPCTSTR DvrName)
{
	CString strName1,strName2;
	int count = m_lstActiveDVR.GetItemCount();
	if(0 == count) {
		return -1;
	}
	strName1 = DvrName;
	strName1.MakeUpper();
	if(1 == count) {
		strName2 = m_lstActiveDVR.GetItemText(0,0);
		strName2.MakeUpper();
		if(strName1 == strName2) {
			return 0;
		}
		else {
			return -1;
		}
	}
	else {
		return OrderFindDvrList(strName1,0,count);
	}
}

int CNetServerDlg::OrderFindDvrList(LPCTSTR DvrName,int startIndex,int stopIndex)
{
	CString strName1,strName2;
	strName1 = DvrName;
	strName1.MakeUpper();
	if((stopIndex-startIndex) < 10) {
		int Index = -1;
		for(int i = startIndex; i < stopIndex; i++) {
			strName2 = m_lstActiveDVR.GetItemText(i,0);
			strName2.MakeUpper();
			if(strName1 == strName2) {
				Index = i;
				break;
			}
		}
		return Index;
	}
	int newPos = (startIndex+stopIndex) >> 1;
	strName2 = m_lstActiveDVR.GetItemText(newPos,0);
	strName2.MakeUpper();
	if(strName1 < strName2) {
		return OrderFindDvrList(strName1,startIndex,newPos);
	}
	else {
		return OrderFindDvrList(strName1,newPos,stopIndex);
	}
}

void CNetServerDlg::OnClientRequest(WPARAM wParam, LPARAM lParam)
{
	CString strTmp,strAddr;
	PCLTMSG pCltMsg = (PCLTMSG)lParam;
	CTime st = CTime::GetCurrentTime();
	BYTE byIP[4];
	byIP[0] = BYTE(pCltMsg->dwCltAddr & 0xFF);
	byIP[1] = BYTE((pCltMsg->dwCltAddr&0xFF00) >> 8);
	byIP[2] = BYTE((pCltMsg->dwCltAddr&0xFF0000) >> 16);
	byIP[3] = BYTE((pCltMsg->dwCltAddr&0xFF000000) >> 24);
	strAddr.Format("%d.%d.%d.%d",byIP[0],byIP[1],byIP[2],byIP[3]);
	strTmp.Format(IDS_WORKITEM,strAddr,pCltMsg->chDvrName);
	strTmp = st.Format(TIME_FORMAT) + strTmp;
	CString strStatus;
	switch(wParam) {
	case WPARAM_CLT_WORKING:
		strStatus.Format(IDS_ONLINE);
		break;
	case WPARAM_CLT_OFFDEV:
		strStatus.Format(IDS_OFFLINE);
		break;
	default:
		strStatus.Format(IDS_FAILED);
	}
	strTmp += strStatus;
	m_LbxWorkRecord.AddString(strTmp);
	int nCount = m_LbxWorkRecord.GetCount();
	if(nCount > 600) {
		m_LbxWorkRecord.DeleteString(0);
	}
}

void CNetServerDlg::OnDestroy() 
{
	if(NULL != m_pManager)
		delete m_pManager;
	CDialog::OnDestroy();
	
	// TODO: Add your message handler code here
}

void CNetServerDlg::OnOK() 
{
	// TODO: Add extra validation here
	int k = 0;
}

void CNetServerDlg::OnBtnClear() 
{
	// TODO: Add your control notification handler code here
	m_LbxWorkRecord.ResetContent();
}

void CNetServerDlg::OnClose() 
{
	// TODO: Add your message handler code here and/or call default
	CDlgUser dlg;
	if(IDCANCEL == dlg.DoModal()) 
		return;

	if(NULL != m_pManager)
		m_pManager->StopWorking();
	SetEvent(CHandleHttp::m_hFinish);
	while (CHandleHttp::IsWorking()) {
		Sleep(50);
	}
	CloseHandle(CHandleHttp::m_hChgPort);
	CloseHandle(CHandleHttp::m_hFinish);
	
	m_bSysClosed = TRUE;
	CDialog::OnClose();
}

void CNetServerDlg::OnTimer(UINT nIDEvent) 
{
	// TODO: Add your message handler code here and/or call default
	if(nIDEvent == ClEAR_DVRLIST_TIMER) {
		int lstCount = m_lstActiveDVR.GetItemCount();
		int Index = 0;
		CString strStatus,strOffline,strDelete;
		strOffline.LoadString(IDS_OFFLINE);
		strDelete.LoadString(IDS_DELETE);
		if(lstCount > 0) {
			do {
				strStatus = m_lstActiveDVR.GetItemText(Index,3);
				if(strStatus == strOffline || strStatus == strDelete) {
					m_lstActiveDVR.DeleteItem(Index);
					lstCount = m_lstActiveDVR.GetItemCount();
				}
				else {
					Index++;
				}
			} while(Index < lstCount);
		}
		m_nOnlineNum = m_lstActiveDVR.GetItemCount();
		InvalidateRect(&m_rcAccountInfo);
	}
	
	CDialog::OnTimer(nIDEvent);
}

//重载OnNcHitTest 如果鼠标在标题 返回鼠标在客户区 这样无法拖拽 无法双击 
UINT CNetServerDlg::OnNcHitTest(CPoint point){
	UINT nHitTest = CWnd::OnNcHitTest(point);
	if(nHitTest==HTCAPTION)
		return HTCLIENT;
	return nHitTest; 
}
