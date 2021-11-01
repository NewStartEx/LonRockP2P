// NETCLIDlg.cpp : implementation file
//

#include "stdafx.h"
#include "NETCLI.h"
#include "NETCLIDlg.h"
#include "Global.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CNETCLIDlg dialog

CNETCLIDlg::CNETCLIDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CNETCLIDlg::IDD, pParent)
{
	//{{AFX_DATA_INIT(CNETCLIDlg)
	m_strCs2User = _T("");
	m_nPort = 0;
	m_nVAPort = 0;
	//}}AFX_DATA_INIT
	// Note that LoadIcon does not require a subsequent DestroyIcon in Win32
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
	m_hExitVAThr = CreateEvent(NULL, TRUE, TRUE, NULL);
	m_hExitVAThrOK = CreateEvent(NULL, TRUE, TRUE, NULL);
	m_hExitTCPThr = CreateEvent(NULL, FALSE, TRUE, NULL);
	m_bInitOK = FALSE;
}

void CNETCLIDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CNETCLIDlg)
	DDX_Control(pDX, IDC_BTNSHOW, m_btnShowVio);
	DDX_Control(pDX, IDC_BTNSTOPSHOW, m_btnStopVio);
	DDX_Control(pDX, IDC_CMCHANNEL, m_comboChannel);
	DDX_Text(pDX, IDC_EDCS2USER, m_strCs2User);
	DDX_Text(pDX, IDC_EDCOMMANDPORT, m_nPort);
	DDX_Text(pDX, IDC_EDWORKPORT, m_nVAPort);
	//}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CNETCLIDlg, CDialog)
	//{{AFX_MSG_MAP(CNETCLIDlg)
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_WM_MOVE()
	ON_BN_CLICKED(IDC_BTNCON, OnBtncon)
	ON_WM_CLOSE()
	ON_BN_CLICKED(IDC_BTNSHOW, OnBtnshow)
	ON_BN_CLICKED(IDC_BTNCUT, OnBtncut)
	ON_BN_CLICKED(IDC_BTNSTOPSHOW, OnBtnstopshow)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CNETCLIDlg message handlers

BOOL CNETCLIDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	// Set the icon for this dialog.  The framework does this automatically
	//  when the application's main window is not a dialog
	SetIcon(m_hIcon, TRUE);			// Set big icon
	SetIcon(m_hIcon, FALSE);		// Set small icon
	
	// TODO: Add extra initialization here
	g_SetDialogStrings(this, IDD);
	m_cSubDlg.Create(IDD_VIDEO_DIALOG, this);

	m_btnStopVio.EnableWindow(FALSE);

	InitNet();
	HI_PLAYER_Initialize(&m_hViode);
	HI_PLAYER_SetDrawWnd(m_hViode, m_cSubDlg.GetSafeHwnd());

	m_bInitOK = TRUE;
	SetWindowPos(NULL, 100, 100, 570, 430, NULL);
	return TRUE;  // return TRUE  unless you set the focus to a control
}

// If you add a minimize button to your dialog, you will need the code below
//  to draw the icon.  For MFC applications using the document/view model,
//  this is automatically done for you by the framework.

void CNETCLIDlg::OnPaint() 
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
HCURSOR CNETCLIDlg::OnQueryDragIcon()
{
	return (HCURSOR) m_hIcon;
}

void CNETCLIDlg::OnOK() 
{
	// TODO: Add extra validation here
	
// 	CDialog::OnOK();
}

void CNETCLIDlg::OnMove(int x, int y) 
{
	CDialog::OnMove(x, y);
	
	// TODO: Add your message handler code here
	if(FALSE == m_bInitOK)
		return;
	POINT pt = {0,0};
	ClientToScreen(&pt);
	m_cSubDlg.SetWindowPos(NULL, pt.x, pt.y, 420, 280, SWP_SHOWWINDOW);
	SetFocus();
}

//�ͻ������������ѯ����վIP��ַ�Ĳ���
void CNETCLIDlg::OnBtncon() 
{
	// TODO: Add your control notification handler code here
	UpdateData(TRUE);
	if(m_strCs2User.IsEmpty())
		return;
	m_sockSrc = socket(AF_INET, SOCK_DGRAM, 0);
	CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)UdpRecvThread, (LPVOID)this, 0, NULL);
	//���ú͵�ַ��ת���������ӵ� �˿� Э�� IP��ַ
	SOCKADDR_IN destaddr;
	//����ַ���IP��ַ
	hostent* pHost = gethostbyname("CS2.lonrock.com");
	if(NULL == pHost){
		AfxMessageBox("Net Error");
		return;
	}
	SetSocketInfo(destaddr, AF_INET, 6911, *pHost->h_addr_list);
	//���÷�����Ϣ
	UDPSendFormat Msg;
	Msg.type = SENDMSGTYPE;
	Msg.content = 0x30;
	int len = m_strCs2User.GetLength();
	char* p = m_strCs2User.GetBuffer(LENCH);
	Encrypt(p, len);
	memmove(Msg.SerUser, p, LENCH);
	m_strCs2User.ReleaseBuffer();

	char strtemp[LENCH];
	len = sprintf(strtemp, "%d", time(NULL));
	Encrypt(strtemp, len);
	memmove(Msg.CliID, strtemp, LENCH);

	int flag = 60;
	m_nComThrState = 0;
	while(flag-- && 0==m_nComThrState){
		sendto(m_sockSrc, (char*)&Msg, sizeof Msg, 0, (SOCKADDR*)&destaddr, sizeof destaddr);
		Sleep(1000);
	}
	closesocket(m_sockSrc);
	m_sockSrc = NULL;
	if(flag < 0){
		AfxMessageBox("TIMEOUT");
		return;
	}

	switch(m_nComThrState){
	case NOTEXIST:
		AfxMessageBox("Not Exist User");
		return;
	case NOTONLINE:
		AfxMessageBox("Not OnLine");
		return;
	case BUSY:
		AfxMessageBox("It Busy");
		return;
	case REPEATLINE:
		AfxMessageBox("Repeat Line");
		return;
	case ONLINE:
		CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)TCPCOMThread, (LPVOID)this, 0, NULL);
		AfxMessageBox("Connection is successful");
		return;
	default:
		AfxMessageBox("Connection is fail");
		return;
	}

}

DWORD CNETCLIDlg::UdpRecvThread(LPVOID s){
	CNETCLIDlg* pObj = (CNETCLIDlg*)s;
	UDPRecvFormat Msg={0};
	int MsgLen = sizeof Msg;
	while(RECVMSGTYPE!=Msg.type){
		if(NULL == pObj->m_sockSrc)
			return NETERROR;
		recvfrom(pObj->m_sockSrc, (char*)&Msg, MsgLen, 0, NULL, NULL);
	}

	if(ONLINE != Msg.content){
		pObj->m_nComThrState = Msg.content;
		return NETERROR;
	}
	pObj->m_dwSerIP = Msg.SevIP;
	int i;
	char chKey = Msg.SerUser[2];
	for(i=0; i<LENCH; ++i)
		pObj->m_chSerUser[i] = ((Msg.SerUser[i]|0x100)-chKey)&0xFF;
	int len = pObj->m_chSerUser[LENCH-1];
	memmove(pObj->m_chSerUser+2, pObj->m_chSerUser+3, LENCH-2);
	pObj->m_chSerUser[len]='\0';
	//��������״̬ ���ͻ����Ѿ��յ�����վ�Ļظ� ��ֹͣ���߳�������վ���͵�½�����ѭ��
	pObj->m_nComThrState = Msg.content;
	return 0;
}
//�˳�����ʱ �Ͽ����� �ͷ���Դ
void CNETCLIDlg::OnClose() 
{
	// TODO: Add your message handler code here and/or call default
	OnBtncut();
	WSACleanup();
	HI_PLAYER_Stop(m_hViode);
	HI_PLAYER_Uninitialize(m_hViode);
	CDialog::OnClose();
}

DWORD CNETCLIDlg::TCPCOMThread(LPVOID s){
	CNETCLIDlg *pObj = (CNETCLIDlg*)s;
	//�̻߳�ʹ�õ��ĸ�������
	char* ConStrArr[]={"CHAN_QUAN", "success", "STATUS", "channel"};
	//�����ͻ��˵�����վ������
	SOCKET sockCom = socket(AF_INET, SOCK_STREAM, 0);
	SOCKADDR_IN destAddr;
	destAddr.sin_family = AF_INET;
	destAddr.sin_port = htons(pObj->m_nPort);
	destAddr.sin_addr.s_addr = pObj->m_dwSerIP;
	if(SOCKET_ERROR == connect(sockCom, (SOCKADDR*)&destAddr, sizeof destAddr)){
		closesocket(sockCom);
		return NETERROR;
	}
	
	TCPComFormat sendMsg, recvMsg;
	//���͵�¼����վ����
	int len = strlen(ConStrArr[0]);
	strcpy(sendMsg.mainMsg, ConStrArr[0]);
	sendMsg.subMsg[0] = '\0';
	strcpy(sendMsg.shparameter, "Admin");
	sendMsg.longparameter[0] = '\0';
	int ss = sizeof sendMsg;
	send(sockCom, (char*)&sendMsg, ss, 0);
	//���յ�¼����վ����Ļظ�[������ �ȴ�����]
	recv(sockCom, (char*)&recvMsg, ss, 0);
	if(strcmp(recvMsg.mainMsg, ConStrArr[0]) || strcmp(recvMsg.subMsg, ConStrArr[1])){
		closesocket(sockCom);
		return NETERROR;
	}
	int num;
	//���������б�������
	sscanf(recvMsg.shparameter, "%d", &num);
	pObj->SetChannelCombo(num);
	//����������������� ע�� �������������������ԭ���ĵ�¼����վ�������������һ��
	strcpy(sendMsg.mainMsg, ConStrArr[2]);
	strcpy(sendMsg.subMsg, ConStrArr[3]);
	//5�뷢��һ����������
	while(WAIT_TIMEOUT == WaitForSingleObject(pObj->m_hExitTCPThr, 0)){
		send(sockCom, (char*)&sendMsg, ss, 0);
		Sleep(5000);
	}
	closesocket(sockCom);
	return 0;
}

void CNETCLIDlg::SetChannelCombo(int nCount){
	char chtemp[10];
	m_comboChannel.Clear();
	for(int i=0; i<nCount; ++i){
		sprintf(chtemp, "V%02d", i+1);
		m_comboChannel.InsertString(i, chtemp);
	}
	m_comboChannel.SetCurSel(0);
}
//�ͻ�������վ������Ƶ���ݼ�����վ�Ļظ� 
void CNETCLIDlg::OnBtnshow() 
{
	// TODO: Add your control notification handler code here
	char* ArrStr[] = {"FORMAT", "DATA"};
	CString strChannel;
	m_comboChannel.GetWindowText(strChannel);
	if(strChannel.IsEmpty())
		return;
	//�����ͻ��˵�����վ������
	m_soVAData = socket(AF_INET, SOCK_STREAM, 0);
	SOCKADDR_IN destAddr;
	destAddr.sin_family = AF_INET;
	destAddr.sin_port = htons(m_nVAPort);
	destAddr.sin_addr.s_addr = m_dwSerIP;
	if(SOCKET_ERROR == connect(m_soVAData, (SOCKADDR*)&destAddr, sizeof destAddr)){
		closesocket(m_soVAData);
		return;
	}
	//������Ƶ��ʽ[��������Ƶ����֮ǰ ����������վ������Ƶ���ݸ�ʽ]
	VioFSend SendMsg;
	strcpy(SendMsg.content, ArrStr[0]);
	strChannel.Format("%d", m_comboChannel.GetCurSel());
	strcpy(SendMsg.parameter, strChannel);
	send(m_soVAData, (char*)&SendMsg, sizeof SendMsg, 0);
	//������Ƶ��ʽ
	VioFRecv RecvMsg;
	recv(m_soVAData, (char*)&RecvMsg, sizeof RecvMsg, 0);
	if(strcmp(RecvMsg.title, ArrStr[0])){
		closesocket(m_soVAData);
		return;
	}
	VAFormat VAstyle;
	memmove(&VAstyle, RecvMsg.parameter, sizeof VAstyle);
	if(RECVVATITLE != VAstyle.dwTitle){
		closesocket(m_soVAData);
		return;
	}
	HI_S_SysHeader hi_head;
	hi_head.u32SysFlag = VAstyle.dwTitle;
	hi_head.struVHeader.u32Height = VAstyle.dwHight;
	hi_head.struVHeader.u32Width = VAstyle.dwWidth;
	hi_head.struAHeader.u32Format = VAstyle.dwAudio;
	HI_PLAYER_OpenStream(m_hViode, (HI_U8*)&hi_head, sizeof hi_head);
	HI_PLAYER_Play(m_hViode);

	//��������������Ƶ����
	strcpy(SendMsg.content, ArrStr[1]);
	strChannel.Format("%d", m_comboChannel.GetCurSel());
	strcpy(SendMsg.parameter, strChannel);
	send(m_soVAData, (char*)&SendMsg, sizeof SendMsg, 0);
	CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)RecvVADateTh, this, 0, NULL);	
	m_btnStopVio.EnableWindow(TRUE);
	m_btnShowVio.EnableWindow(FALSE);
}

DWORD CNETCLIDlg::RecvVADateTh(LPVOID s){
	CNETCLIDlg* pObj = (CNETCLIDlg*)s;
	ResetEvent(pObj->m_hExitVAThr);
	ResetEvent(pObj->m_hExitVAThrOK);
	//֪ͨ����վֹͣ������Ƶ���ݵ���Ϣ
 	VioFSend Msg;
 	strcpy(Msg.content, "DISCONNECT");
 	char str[10];
 	sprintf(str, "%d", pObj->m_comboChannel.GetCurSel());
 	strcpy(Msg.parameter, str);

	char* pBuff = (char*)malloc(VICO_BUFFER_LENGTH);
	FILE* fp = fopen("Test", "w");
	if(NULL == fp)
		return NETERROR;
	int nLen;
	while(WAIT_TIMEOUT == WaitForSingleObject(pObj->m_hExitVAThr, 0)){
		if(HI_PLAYER_GetBufferValue(pObj->m_hViode)>4500)//ʵʱ�����6000 ����4500��ʱ�����ͣ�������
			continue;

		nLen = recv(pObj->m_soVAData, pBuff, VICO_BUFFER_LENGTH, 0);
		if(SOCKET_ERROR == nLen)
			break;
		HI_PLAYER_InputData(pObj->m_hViode, (HI_U8*)pBuff, nLen);
		fwrite(pBuff, 1, nLen, fp);
	}
	free(pBuff);
	fclose(fp);
	HI_PLAYER_ResetSourceBuffer(pObj->m_hViode);
	//������Ϣ֪ͨ����վֹͣ������Ƶ����
 	send(pObj->m_soVAData, (char*)&Msg, sizeof Msg, 0);
	//�߳�����˳�
	SetEvent(pObj->m_hExitVAThrOK);
	return 0;
}

//�Ͽ��ͻ�������վ�������������
void CNETCLIDlg::OnBtncut() 
{
	// TODO: Add your control notification handler code here
	OnBtnstopshow();
	m_comboChannel.Clear();
	SetEvent(m_hExitTCPThr);
	m_btnShowVio.EnableWindow(TRUE);
	m_btnStopVio.EnableWindow(FALSE);
}

//�Ͽ�����վ��ͻ��˷�����Ƶ���ݵ�����
void CNETCLIDlg::OnBtnstopshow() 
{
	// TODO: Add your control notification handler code here
	//�Ͽ����ӵĲ����Ѿ�д���߳��ڲ� ֱ�ӷ�����Ϣ�˳��߳̾Ϳ��ԶϿ�����
	SetEvent(m_hExitVAThr);
	if(WAIT_TIMEOUT == WaitForSingleObject(m_hExitVAThrOK, 1000))
		AfxMessageBox("Error Thr");
	m_btnShowVio.EnableWindow(TRUE);
	m_btnStopVio.EnableWindow(FALSE);
}
