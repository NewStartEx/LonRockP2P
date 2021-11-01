#include "stdafx.h"
#include "NTTProtocol.h"
#include "NTTStationFun.h"
#include "DVRManager.h"
#include "ChannelInfo.h"
#include "Global.h"
#include "md5.h"

extern CONNECTEDFROMCLIENT g_ConnectedFromClient; //// 登录命令使用时调用
extern RECVFROMCLIENT g_RecvFromClient; //// 接收其他命令的时候调用
extern DISCONNECTEDFROMCLIENT g_DisconnectedFromClient; //// 删除ClientTnel类之前调用

CDVRManager::CDVRManager(DWORD dwType, DWORD* dwID) :
m_bWorking(FALSE),
m_bSleeping(FALSE),
m_bGotGlobalName(FALSE),
m_bNameSet(FALSE),
m_bApplying(FALSE),
m_uRegisterStatus(NTT_NOTREGISTER),
m_nThreadCount(0)
{
	m_hMsgWnd = NULL;
	m_sGram = INVALID_SOCKET;
	m_wNetSPort = NETSERVER_PORT;
	m_wDVRPort = DVR_PORT;
	m_pLinkNetS = NULL;
	memset(m_chPassword,0,NAME_BUF_LENGTH);
	memset(m_chCheckingPassword,0,NAME_BUF_LENGTH);
	memset(&m_DVRInfo,0,sizeof(APPLY_GRAM));
	m_DVRInfo.dwType = dwType;
	m_DVRInfo.dwDevID[0] = dwID[0];
	m_DVRInfo.dwDevID[1] = dwID[1];
	m_DVRInfo.dwDevID[2] = dwID[2];
	m_DVRInfo.dwDevID[3] = dwID[3];
	memset(&m_LocalAddr,0,sizeof(SOCKADDR_IN));
	m_LocalAddr.sin_family = AF_INET;
	m_LocalAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	m_LocalAddr.sin_port = htons(m_wDVRPort);

	m_nReadSDataIndex = m_nWriteSDataIndex = 0;
	m_bSendBufFull = FALSE;
	for(int i = 0; i < SENDDATA_BUFFER_COUNT; i++) {
		m_SendDataDesc[i].nLength = 0;
		m_SendDataDesc[i].pBuf = new char[SEND_BUF_LENGTH];
		memset(m_SendDataDesc[i].pBuf,0,SEND_BUF_LENGTH);
	}
	
	m_hRcvGram = WSACreateEvent();
	m_hNetSFBGram = CreateEvent(NULL,FALSE,FALSE,NULL);
	m_hTnelGram = CreateEvent(NULL,FALSE,FALSE,NULL);
	m_hCnctGram = CreateEvent(NULL,FALSE,FALSE,NULL);
	m_hHshkGram = CreateEvent(NULL,FALSE,FALSE,NULL);
	m_hCmdGram = CreateEvent(NULL,FALSE,FALSE,NULL);
	m_hVResdGram = CreateEvent(NULL,FALSE,FALSE,NULL);
////	m_hFResdGram = (NULL,FALSE,FALSE,NULL);
	m_hSendData = CreateEvent(NULL,FALSE,FALSE,NULL);
	m_hStopWorking = CreateEvent(NULL,TRUE,FALSE,NULL);
	InitializeCriticalSection(&m_csGram);
	InitializeCriticalSection(&m_csCltArray); 
	InitializeCriticalSection(&m_csRes); //// 对一些其他资源的保护
	InitializeCriticalSection(&m_csSData);
}

CDVRManager::~CDVRManager()
{
	if(m_bWorking) {
		StopWorking();
	}
	if(NULL != m_pLinkNetS) {
		delete m_pLinkNetS;
	}
	int size = m_CltTnelArray.GetSize();
	for(int i = 0; i < size; i++) {
		if(g_DisconnectedFromClient != NULL) {
			g_DisconnectedFromClient(m_CltTnelArray[i]);
		}
		delete m_CltTnelArray[i];
	}
	m_CltTnelArray.RemoveAll();
	WSACloseEvent(m_hRcvGram);
	m_NetSFBArray.RemoveAll();
	m_TnelMsgArray.RemoveAll();
	m_CnctDescArray.RemoveAll();
	m_CmdDescArray.RemoveAll();
	m_VResdDescArray.RemoveAll();
	for(i = 0; i < SENDDATA_BUFFER_COUNT; i++) {
		delete m_SendDataDesc[i].pBuf;
	}
	CloseHandle(m_hNetSFBGram);
	CloseHandle(m_hTnelGram);
	CloseHandle(m_hCnctGram);
	CloseHandle(m_hHshkGram);
	CloseHandle(m_hCmdGram);
	CloseHandle(m_hVResdGram);
////CloseHandle(m_hFResdGram);
	CloseHandle(m_hSendData);
	CloseHandle(m_hStopWorking);
	DeleteCriticalSection(&m_csGram);
	DeleteCriticalSection(&m_csCltArray);
	DeleteCriticalSection(&m_csRes);
	DeleteCriticalSection(&m_csSData);
}

BOOL CDVRManager::StartWorking(LPCTSTR Path,LPCTSTR NetSName)
{
	ASSERT(INVALID_SOCKET == m_sGram);
	int on = 1;
	m_sGram = socket(AF_INET,SOCK_DGRAM,0);
	int count=0;
	do {
		if(SOCKET_ERROR != bind(m_sGram, (SOCKADDR *FAR)&m_LocalAddr,sizeof(SOCKADDR_IN))) {
////		AfxMessageBox("绑定网络");
			break;
		}
		m_wDVRPort++;
		count++;
		m_LocalAddr.sin_port = htons(m_wDVRPort);

	} while(count < 100);
	if(count >= 100) {
		//// bind Failed
	////	AfxMessageBox("绑定网络 错误");
		closesocket(m_sGram);
		m_sGram = INVALID_SOCKET;
		return FALSE;
	}
	if(SOCKET_ERROR == (WSAEventSelect(m_sGram,m_hRcvGram,FD_READ))) {
////		AfxMessageBox("读网络 错误");
		closesocket(m_sGram);
		m_sGram = INVALID_SOCKET;
		return FALSE;
	}
////	AfxMessageBox("启动网络");
#define SOCK_SNDBUF 65536
	//// 设置发送缓冲为64K.默认应该是8K
	int nBufSize = SOCK_SNDBUF,nOptLen = sizeof(int);
	int nRe = setsockopt(m_sGram,SOL_SOCKET,SO_SNDBUF,(CHAR*)&nBufSize,nOptLen);
	if(0 == nRe) {
		getsockopt(m_sGram,SOL_SOCKET,SO_SNDBUF,(CHAR*)&nBufSize,&nOptLen);
		if(nBufSize != SOCK_SNDBUF) {
			nBufSize = 8196; //// default value
			setsockopt(m_sGram,SOL_SOCKET,SO_SNDBUF,(CHAR*)&nBufSize,nOptLen);
		}
	}
	//// m_strInfoFileName is absolete 
	m_strInfoFileName = Path;
	m_strInfoFileName += DVRINFO_FILE_NAME;
	m_strNetSName = NetSName;
	m_bGotGlobalName = ReadDVRInfo();
	m_uRegisterStatus = m_bGotGlobalName ? NTT_REGISTERED : NTT_NOTREGISTER;
	
	m_bWorking = TRUE;
	DWORD dwID;
	IncreaseThreadCount();
	HANDLE hRcv = CreateThread(NULL,0,(LPTHREAD_START_ROUTINE)SendGramProc,this,0,&dwID);
	SetThreadPriority(hRcv,THREAD_PRIORITY_HIGHEST);

	m_pLinkNetS = new CLinkNetS(m_strNetSName,m_wNetSPort);
	m_pLinkNetS->InitSending();
	if(m_bGotGlobalName) {
		ManuUpdate();
	}

	TRACE("DVR start working");
	IncreaseThreadCount();
	CreateThread(NULL,0,(LPTHREAD_START_ROUTINE)DistribGramProc,this,0,&dwID);
	IncreaseThreadCount();
	CreateThread(NULL,0,(LPTHREAD_START_ROUTINE)RcvGramProc,this,0,&dwID);
	IncreaseThreadCount();
	CreateThread(NULL,0,(LPTHREAD_START_ROUTINE)CheckOverProc,this,0,&dwID);
	
	return TRUE;
}

void CDVRManager::StopWorking()
{
	if(m_bGotGlobalName) {
		UnitExit();
	}
	m_bWorking = FALSE;
	SetEvent(m_hStopWorking);
	while (m_nThreadCount > 0) {
		Sleep(50);
	}
	WriteDVRInfo();
	m_nReadSDataIndex = m_nWriteSDataIndex = 0;
	closesocket(m_sGram);
	m_sGram = INVALID_SOCKET;
}

BOOL CDVRManager::SetGlobalName(LPCTSTR Name,LPCTSTR Password)
{
	CString strName,strPassword;
	strName = Name;
	int nLength = strName.GetLength();
	if(nLength > MAX_NAME_LENGTH || nLength < MIN_NAME_LENGTH) return FALSE;
	for(int i = 0; i < nLength; i++) {
		if(' ' == strName[i] || '*' == strName[i] || '.' == strName[i]) {
			return FALSE;
		}
	}

	strPassword = Password;
	nLength = strPassword.GetLength();
	if(nLength > MAX_NAME_LENGTH || nLength < MIN_NAME_LENGTH) return FALSE;
	char chMd5[NAME_BUF_LENGTH];
	memset(chMd5,0,NAME_BUF_LENGTH);
	strcpy(chMd5,strPassword);
	MD5 md5(chMd5,nLength);
	const BYTE *pFinal = md5.digest();
	TRACE(md5.toString());
	EnterCriticalSection(&m_csRes);
	m_strCheckingName = strName;
	memset(m_chCheckingPassword,0,NAME_BUF_LENGTH);
	memcpy(m_chCheckingPassword,pFinal,MD5_CODE_LENGTH);
	LeaveCriticalSection(&m_csRes);
	m_bNameSet = TRUE;
	return TRUE;
}

//// 如果用户设置名称不对，可以通过这个来找回最后一次可以的名称
BOOL CDVRManager::ResetLastGlobalName(CString &strGlobalName)
{
	if(m_strGlobalName.IsEmpty()) return FALSE;
	strGlobalName = m_strGlobalName;
	m_strCheckingName = m_strGlobalName;
	memcpy(m_chCheckingPassword,m_chPassword,MD5_CODE_LENGTH);
	m_bGotGlobalName = TRUE;
	return TRUE;
}

BYTE CDVRManager::RegisterUnit()
{
	if(INADDR_NONE == m_pLinkNetS->GetNetSAddr(TRUE)) return SENDLINK_NONETS;
	ASSERT(m_pLinkNetS != NULL); //// 只要地址找到，就一定生成这个类
	if(!m_bNameSet) return SENDLINK_FAILED;
	//// ？？？？
	//// 这时候可以确认CLinkNetS是空闲状态
	int nLength = m_strCheckingName.GetLength();
	ASSERT(nLength>=MIN_NAME_LENGTH && nLength<=MAX_NAME_LENGTH);
	if(m_bGotGlobalName) {
		
		return SENDLINK_EXIST; //// nvcd 这个以后使用，避免总是申请名称
	}

	CSelfLock CS(&m_csRes);
	m_DVRInfo.byAct = DVR_ACT_REGISTER;
	SecretEncode2Apply(m_DVRInfo.chName,m_DVRInfo.chPassword);

	if(m_pLinkNetS->FillReqInfo(&m_DVRInfo)) {
		m_bApplying = TRUE;
		return SENDLINK_SUCCESS;
	}
	else {
		return SENDLINK_FAILED;
	}
	
}

BYTE CDVRManager::UnregisterUnit()
{
	if(INADDR_NONE == m_pLinkNetS->GetNetSAddr(TRUE)) return SENDLINK_NONETS;
	ASSERT(m_pLinkNetS != NULL);
	if(!m_bNameSet) return SENDLINK_FAILED;
	
	int nLength = m_strCheckingName.GetLength();
	ASSERT(nLength>=MIN_NAME_LENGTH && nLength<=MAX_NAME_LENGTH);

	CSelfLock CS(&m_csRes);
	m_DVRInfo.byAct = DVR_ACT_DELETE;

	SecretEncode2Apply(m_DVRInfo.chName,m_DVRInfo.chPassword);
	if(m_pLinkNetS->FillReqInfo(&m_DVRInfo)) {
		m_bApplying = TRUE;
		return SENDLINK_SUCCESS;
	}
	else {
		return SENDLINK_FAILED;
	}
}

BYTE CDVRManager::ManuUpdate()
{
	if(INADDR_NONE == m_pLinkNetS->GetNetSAddr(TRUE)) {
////		AfxMessageBox("没有检测到服务器");
		m_uRegisterStatus = NTT_NETWORKERR;
		return SENDLINK_NONETS;
	}
	ASSERT(m_pLinkNetS != NULL);
	if(!m_bNameSet) return SENDLINK_FAILED;
	
	int nLength = m_strCheckingName.GetLength();
	ASSERT(nLength>=MIN_NAME_LENGTH && nLength<=MAX_NAME_LENGTH);

	CSelfLock CS(&m_csRes);
	m_DVRInfo.byAct = DVR_ACT_MANUALUPDATE;

	SecretEncode2Apply(m_DVRInfo.chName,m_DVRInfo.chPassword);
	if(m_pLinkNetS->FillReqInfo(&m_DVRInfo)) {
		m_bApplying = TRUE;
		return SENDLINK_SUCCESS;
	}
	else {
////		AfxMessageBox("发送命令失败");
		m_uRegisterStatus = NTT_NETWORKERR;   //// 也许还可以细分成其他原因
		return SENDLINK_FAILED;
	}
}

BYTE CDVRManager::WakeupUnit()
{
	if(INADDR_NONE == m_pLinkNetS->GetNetSAddr(TRUE)) return SENDLINK_NONETS;
	ASSERT(m_pLinkNetS != NULL);
	if(!m_bNameSet) return SENDLINK_FAILED;
	
////	if(!m_bGotGlobalName) return SENDLINK_NONAME;  可以激活别的帐号
////	m_strCheckingName = m_strGlobalName;
////	memcpy(m_chCheckingPassword,m_chPassword,MD5_CODE_LENGTH);
	int nLength = m_strCheckingName.GetLength();
	ASSERT(nLength>=MIN_NAME_LENGTH && nLength<=MAX_NAME_LENGTH);
	CSelfLock CS(&m_csRes);
	m_DVRInfo.byAct = DVR_ACT_WAKEUP;

	SecretEncode2Apply(m_DVRInfo.chName,m_DVRInfo.chPassword);
	if(m_pLinkNetS->FillReqInfo(&m_DVRInfo)) {
		m_bApplying = TRUE;
		return SENDLINK_SUCCESS;
	}
	else {
		return SENDLINK_FAILED;
	}
}

BYTE CDVRManager::UnitExit()
{
	if(INADDR_NONE == m_pLinkNetS->GetNetSAddr(TRUE)) return SENDLINK_NONETS;
	ASSERT(m_pLinkNetS != NULL);
	if(!m_bNameSet) return SENDLINK_FAILED;
	
	int nLength = m_strCheckingName.GetLength();
	ASSERT(nLength>=MIN_NAME_LENGTH && nLength<=MAX_NAME_LENGTH);
	CSelfLock CS(&m_csRes);
	m_DVRInfo.byAct = DVR_ACT_EXIT;

	SecretEncode2Apply(m_DVRInfo.chName,m_DVRInfo.chPassword);
	if(m_pLinkNetS->FillReqInfo(&m_DVRInfo)) {
		Sleep(50);
		m_bApplying = TRUE;
		return SENDLINK_SUCCESS;
	}
	else {
		return SENDLINK_FAILED;
	}
}

void CDVRManager::EnableVideoTransfer(HANDLE hClt, BOOL bEnable,BOOL bInCallback)
{
	CClientTnel* pClt = (CClientTnel*)hClt;
	if(!bInCallback) EnterCriticalSection(&m_csCltArray);
	int count = m_CltTnelArray.GetSize();
	for(int i = 0; i < count; i++) {
		if(pClt == m_CltTnelArray[i]) {
			break;
		}
	}
	if(i < count) {
		m_CltTnelArray[i]->EnableVideo(bEnable);
	}
	if(!bInCallback) LeaveCriticalSection(&m_csCltArray);
}

void CDVRManager::EnableAudioTransfer(HANDLE hClt, BOOL bEnable,BOOL bInCallback)
{
	CClientTnel* pClt = (CClientTnel*)hClt;
	if(!bInCallback) EnterCriticalSection(&m_csCltArray);
	int count = m_CltTnelArray.GetSize();
	for(int i = 0; i < count; i++) {
		if(pClt == m_CltTnelArray[i]) {
			break;
		}
	}
	if(i < count) {
		m_CltTnelArray[i]->EnableAudio(bEnable);
	}
	if(!bInCallback) LeaveCriticalSection(&m_csCltArray);
}

void CDVRManager::SetRemoveMark(HANDLE hClt,BOOL bInCallback)
{
	CClientTnel* pClt = (CClientTnel*)hClt;
	if(!bInCallback) EnterCriticalSection(&m_csCltArray);
	int count = m_CltTnelArray.GetSize();
	for(int i = 0; i < count; i++) {
		if(pClt == m_CltTnelArray[i]) {
			break;
		}
	}
	if(i < count) {
		m_CltTnelArray[i]->SetExitMark();
	}
	if(!bInCallback) LeaveCriticalSection(&m_csCltArray);
}

BOOL CDVRManager::ReadDVRInfo()
{
	LONG lRe;
	HKEY hkey;
	lRe = RegOpenKeyEx(HKEY_LOCAL_MACHINE,REG_NTTSTATION,0,KEY_READ,&hkey);
	if(lRe != ERROR_SUCCESS) {
		return FALSE;
	}
	DWORD dwType = REG_BINARY;
	DWORD dwVer;
	DWORD dwSize;
	dwSize = sizeof(DWORD);
	lRe = RegQueryValueEx(hkey,NULL,NULL,&dwType,(BYTE*)&dwVer,&dwSize);
	if(lRe != ERROR_SUCCESS) {
		TRACE("Read Value Version Error");
		RegCloseKey(hkey);
		return FALSE;
	}
	if(dwVer != INFO_VERSION) {
		TRACE("Get Error Ntt Version");
		RegCloseKey(hkey);
		return FALSE;
	}
	dwType = REG_BINARY;
	dwSize = NAME_BUF_LENGTH;
	lRe = RegQueryValueEx(hkey,REG_NAMEKEY,NULL,&dwType,(BYTE*)m_DVRInfo.chName,&dwSize);
	if(lRe != ERROR_SUCCESS) {
		TRACE("Read Value Name Error");
		RegCloseKey(hkey);
		return FALSE;
	}
	lRe = RegQueryValueEx(hkey,REG_PASSKEY,NULL,&dwType,(BYTE*)m_DVRInfo.chPassword,&dwSize);
	if(lRe != ERROR_SUCCESS) {
		TRACE("Read Value Name Error");
		RegCloseKey(hkey);
		return FALSE;
	}
	RegCloseKey(hkey);
	if(!SecretDecode3File(m_DVRInfo.chName,m_DVRInfo.chPassword)) {
		return FALSE;
	}
	m_strGlobalName = m_strCheckingName;
	memcpy(m_chPassword,m_chCheckingPassword,MD5_CODE_LENGTH);
	m_bNameSet = TRUE;
	return TRUE;

	/*
	FILE *hf;
	int size;
	hf = fopen(m_strInfoFileName,"rb");
	if(NULL == hf) return FALSE;
	DWORD dwVer;
	size = fread(&dwVer,1,sizeof(DWORD),hf);
	if(size != sizeof(DWORD)) goto BADFILE;
	if(dwVer != INFO_VERSION) goto BADFILE;
	size = fread(&m_DVRInfo.chName,1,NAME_BUF_LENGTH,hf);
	if(size != NAME_BUF_LENGTH) goto BADFILE;
	size = fread(&m_DVRInfo.chPassword,1,NAME_BUF_LENGTH,hf);
	if(size != NAME_BUF_LENGTH) goto BADFILE;
	if(!SecretDecode3File(m_DVRInfo.chName,m_DVRInfo.chPassword)) {
		goto BADFILE;
	}
	fclose(hf);
	//// 这个时候给GlobalName赋值，避免Write的时候这两个值不对
	m_strGlobalName = m_strCheckingName;
	memcpy(m_chPassword,m_chCheckingPassword,MD5_CODE_LENGTH);
	m_bNameSet = TRUE;
	return TRUE;
BADFILE:
	fclose(hf);
	DeleteFile(m_strInfoFileName);
	return FALSE;
	*/
}

BOOL CDVRManager::WriteDVRInfo()
{
	if(!m_bGotGlobalName) {
		//// m_bGotGlobalName只有该名称被NetS删除或者重新注册的时候才是FALSE
		//// RegDeleteKey(HKEY_LOCAL_MACHINE,REG_NTTSTATION);
		DeleteFile(m_strInfoFileName);  //// 这个时候消除本地原来的名称
		return TRUE; //// 这个时候没有必要存盘
	}
	SecretEncode2File(m_DVRInfo.chName,m_DVRInfo.chPassword);
	
	LONG lRe;
	HKEY hkey;
	DWORD dwDip;
	lRe = RegCreateKeyEx(HKEY_LOCAL_MACHINE,REG_NTTSTATION,0,
		NULL,REG_OPTION_NON_VOLATILE,KEY_WRITE,NULL,&hkey,&dwDip);
	if(lRe != ERROR_SUCCESS) {
		return FALSE;
	}
	DWORD dwVer = INFO_VERSION;
	lRe = RegSetValueEx(hkey,NULL,0,REG_DWORD,(BYTE*)&dwVer,sizeof(DWORD));
	if(lRe != ERROR_SUCCESS) {
		TRACE("SetValue VERSION Error");
		RegCloseKey(hkey);
		return FALSE;
	}
	lRe = RegSetValueEx(hkey,REG_NAMEKEY,0,REG_BINARY,(BYTE*)m_DVRInfo.chName,NAME_BUF_LENGTH);
	if(lRe != ERROR_SUCCESS) {
		TRACE("SetValue Account Error");
		RegCloseKey(hkey);
		return FALSE;
	}
	lRe = RegSetValueEx(hkey,REG_PASSKEY,0,REG_BINARY,(BYTE*)m_DVRInfo.chPassword,NAME_BUF_LENGTH);
	if(lRe != ERROR_SUCCESS) {
		TRACE("SetValue Password Error");
		RegCloseKey(hkey);
		return FALSE;
	}
	RegCloseKey(hkey);
	
	/*
	FILE *hf;
	hf = fopen(m_strInfoFileName,"wb");
	if(NULL == hf) return FALSE;
	DWORD dwVer = INFO_VERSION;
	fwrite(&dwVer,sizeof(DWORD),1,hf);
	fwrite(m_DVRInfo.chName,NAME_BUF_LENGTH,1,hf);
	fwrite(m_DVRInfo.chPassword,NAME_BUF_LENGTH,1,hf);
	fclose(hf);
	SetFileAttributes(m_strInfoFileName,FILE_ATTRIBUTE_HIDDEN|FILE_ATTRIBUTE_SYSTEM);
	*/
	return TRUE;
}

UINT CDVRManager::KeepRcving()
{
	ASSERT(m_sGram != INVALID_SOCKET);
	SOCKADDR_IN DestAddr;
	DWORD dwWait;
	APPLY_GRAM NetSFBGram;
	TNELMSG_GRAM TnelMsgGram;
	CNCTDESC CnctDesc;
	HSHKDESC HshkDesc;
	CMDDESC  CmdDesc;
	VRESDDESC VResdDesc;
	char rcvBuf[DVR_RCV_BUF_LENGTH];
	int nRcvLength;
	HANDLE hEv[2];
	memset(rcvBuf,0,DVR_RCV_BUF_LENGTH);
	hEv[0] = m_hRcvGram;
	hEv[1] = m_hStopWorking;
	while (m_bWorking) {
		dwWait = WSAWaitForMultipleEvents(2,hEv,FALSE,INFINITE,FALSE);
		if(WAIT_OBJECT_0 == dwWait) {
			WSAResetEvent(hEv[0]);
			memset(&DestAddr,0,sizeof(SOCKADDR_IN));
			int len = sizeof(SOCKADDR_IN);
			//// EnterCriticalSection(&g_csSocket); //// 接收和发送没有冲突
			nRcvLength = recvfrom(m_sGram,rcvBuf,DVR_RCV_BUF_LENGTH,0,(SOCKADDR*)&DestAddr,&len);
			//// LeaveCriticalSection(&g_csSocket);
			if(sizeof(APPLY_GRAM) == nRcvLength) {
				//// 避免其他服务器冒充我司服务器发送数据
				if(DestAddr.sin_addr.s_addr != m_pLinkNetS->GetNetSAddr()) continue; //// 端口可能因为NAT而不同
				//// NetS's Feedback Gram
				memcpy(&NetSFBGram,rcvBuf,sizeof(APPLY_GRAM));
				EnterCriticalSection(&m_csGram);
				m_NetSFBArray.Add(NetSFBGram);
				LeaveCriticalSection(&m_csGram);
				SetEvent(m_hNetSFBGram);
			}
			else if(sizeof(TNELMSG_GRAM) == nRcvLength) {
				//// 避免其他服务器冒充我司服务器发送数据
				if(DestAddr.sin_addr.s_addr != m_pLinkNetS->GetNetSAddr()) continue; //// 端口可能因为NAT而不同
				//// Tunnel applying Gram
				memcpy(&TnelMsgGram,rcvBuf,sizeof(TNELMSG_GRAM));
				EnterCriticalSection(&m_csGram);
				m_TnelMsgArray.Add(TnelMsgGram);
				LeaveCriticalSection(&m_csGram);
			////	TRACE("DVR Get NetS-Dest name:%s",TnelMsgGram.chDstName);
				SetEvent(m_hTnelGram);
			}
			else if(sizeof(OPENTNEL) == nRcvLength) {
				//// Client's Open Tunnel Gram
				memcpy(&CnctDesc.OpenGram,rcvBuf,sizeof(OPENTNEL));
				CnctDesc.addr = DestAddr;
				EnterCriticalSection(&m_csGram);
				m_CnctDescArray.Add(CnctDesc);
				LeaveCriticalSection(&m_csGram);
			////	TRACE("DVR Get TNEL name:%s-to-%s",CnctDesc.OpenGram.chFirstName,CnctDesc.OpenGram.chSecondName);
				SetEvent(m_hCnctGram);
			}
			else if(sizeof(HSHK_GRAM) == nRcvLength) {
				memcpy(&HshkDesc.HshkGram,rcvBuf,sizeof(HSHK_GRAM));
				HshkDesc.addr = DestAddr;
				EnterCriticalSection(&m_csGram);
				m_HshkDescArray.Add(HshkDesc);
				LeaveCriticalSection(&m_csGram);
				SetEvent(m_hHshkGram);
			}
			else if(sizeof(WORK_GRAM) == nRcvLength) {
				//// Client's Command
				memcpy(&CmdDesc.WorkGram,rcvBuf,sizeof(WORK_GRAM));
				CmdDesc.addr = DestAddr;
				EnterCriticalSection(&m_csGram);
				m_CmdDescArray.Add(CmdDesc);
				LeaveCriticalSection(&m_csGram);
				SetEvent(m_hCmdGram);
			}
			else if(sizeof(VRESD_GRAM) == nRcvLength) {
				//// Video Resend Gram
				memcpy(&VResdDesc.VResdGram,rcvBuf,sizeof(VRESD_GRAM));
				VResdDesc.addr = DestAddr;
				EnterCriticalSection(&m_csGram);
				m_VResdDescArray.Add(VResdDesc);
				LeaveCriticalSection(&m_csGram);
				SetEvent(m_hVResdGram);
			}
			else {
				//// 莫名数据.以后可能会有文件的重传数据
				;
			}
		}
		else if((WAIT_OBJECT_0+1) == dwWait) {
			break; //// 退出循环
		}
		else {
			//// 这个是不可能的，这里做一个保护
			TRACE("Recv Thread Impossible Happen");
		}
	}
	return 1;
}

UINT CDVRManager::DistribGram()
{
	DWORD dwWait;
	HANDLE hEv[7];
	
	hEv[0] = m_hNetSFBGram;
	hEv[1] = m_hTnelGram;
	hEv[2] = m_hCnctGram;
	hEv[3] = m_hHshkGram;
	hEv[4] = m_hCmdGram;
	hEv[5] = m_hVResdGram;
	hEv[6] = m_hStopWorking;
	while (m_bWorking) {
		dwWait = WaitForMultipleObjects(7,hEv,FALSE,INFINITE); 
		if(WAIT_OBJECT_0 == dwWait) {
			DealWithNetSFBGram();
		}
		else if((WAIT_OBJECT_0+1) == dwWait) {
			DealWithTnelGram();
		}
		else if((WAIT_OBJECT_0+2) == dwWait) {
			DealWithCnctGram();
		}
		else if((WAIT_OBJECT_0+3) == dwWait) {
			DealWithHshkGram();
		}
		else if((WAIT_OBJECT_0+4) == dwWait) {
			DealWithCmdGram();
		}
		else if((WAIT_OBJECT_0+5) == dwWait) {
			DealWithVResdGram();
		}
		else if(WAIT_OBJECT_0+6) {
			break;
		}
		else {
			TRACE("Handle Gram Thread Impossible Happen");
		}
	}
	return 1;
}

UINT CDVRManager::CheckOver()
{
	DWORD dwWait;
	while (m_bWorking) {
		dwWait = WaitForSingleObject(m_hStopWorking,CHECK_OVER_INTERVAL);
		if(WAIT_TIMEOUT == dwWait) {
			NormalNetSChecking();
			NormalCltChecking();
		}
		else {
			DvrExitNotify();
			break;
		}
	}
	return 1;
}

UINT CDVRManager::DealWithNetSFBGram()
{
	ASSERT(m_pLinkNetS != NULL);
	APPLY_GRAM NetSFBGram;
	int count = m_NetSFBArray.GetSize();
	BYTE byResult;
	WPARAM wParam;
	while (count > 0) {
		EnterCriticalSection(&m_csGram);
		NetSFBGram = m_NetSFBArray[0];
		m_NetSFBArray.RemoveAt(0);
		LeaveCriticalSection(&m_csGram);
		count = m_NetSFBArray.GetSize();
		byResult = m_pLinkNetS->CheckFeedback(&NetSFBGram,m_strCheckingName,m_chCheckingPassword);
		if(ACK_OTHER_ERROR == byResult || ACK_ILLEGAL_INFO == byResult) {
			continue;
		}
		if(ACK_REGISTER_SUCCESS == byResult || ACK_UPDATE_OK == byResult) {
			m_bGotGlobalName = TRUE;
			m_strGlobalName = m_strCheckingName;
			memcpy(m_chPassword,m_chCheckingPassword,MD5_CODE_LENGTH);
			m_uRegisterStatus = NTT_REGISTERED;
			WriteDVRInfo();
		}
		else if(ACK_DELETE_SUCCESS==byResult || ACK_DEVICE_NOTEXIST==byResult) {
			m_bGotGlobalName = FALSE;
			m_uRegisterStatus = NTT_NOTREGISTER;
			WriteDVRInfo();
		}
		else if(ACK_DEVICE_SLEEPING == byResult) {
			m_bSleeping = TRUE;
			m_uRegisterStatus = NTT_SLEEPING;
		}
		else if(ACK_WAKEUP_SUCCESS == byResult) {
			m_bGotGlobalName = TRUE;
			m_bSleeping = FALSE;
			m_strGlobalName = m_strCheckingName;
			memcpy(m_chPassword,m_chCheckingPassword,MD5_CODE_LENGTH);
			m_uRegisterStatus = NTT_REGISTERED;
			WriteDVRInfo();
		}
		else if(ACK_ACCOUNT_ALIVE == byResult) {
			m_bGotGlobalName = FALSE;
			m_uRegisterStatus = NTT_ACCOUNTALIVE;
			WriteDVRInfo();
		}
		m_bApplying = FALSE; //// 不用手工发送请求了
		wParam = byResult;
		if(NULL != m_hMsgWnd)
			::PostMessage(m_hMsgWnd,WM_NETS_FEEDBACK,wParam,0);
	}
	return 1;
}

UINT CDVRManager::DealWithTnelGram()
{
	ASSERT(m_pLinkNetS != NULL);
	TNELMSG_GRAM TnelMsgGram;
	CClientTnel *pClt;
	int count = m_TnelMsgArray.GetSize();
	char chCltName[NAME_BUF_LENGTH];
	while (count > 0) {
		EnterCriticalSection(&m_csGram);
		TnelMsgGram = m_TnelMsgArray[0];
		m_TnelMsgArray.RemoveAt(0);
		LeaveCriticalSection(&m_csGram);
		count = m_TnelMsgArray.GetSize();
		memset(chCltName,0,NAME_BUF_LENGTH);
		if(!LegalTnel(&TnelMsgGram,chCltName)) {
			continue;
		}
		TRACE("DVR Tnel:%s",chCltName);
		EnterCriticalSection(&m_csCltArray);
		pClt = GetClt(chCltName);
		if(NULL == pClt) {
			pClt = new CClientTnel(chCltName,
				TnelMsgGram.dwDstIPaddr,TnelMsgGram.wDstPort,TUNNEL_CMD);
			if(!pClt->InitResource(m_strGlobalName)) {
				delete pClt;
				continue;
			}
			m_CltTnelArray.Add(pClt);
			if(g_ConnectedFromClient != NULL) {
				g_ConnectedFromClient(pClt,TnelMsgGram.dwDstIPaddr);
			}
			WPARAM wParam;
			wParam = TnelMsgGram.dwDstIPaddr;
			if(NULL != m_hMsgWnd)
				::PostMessage(m_hMsgWnd,WM_NETS_TUNNEL,wParam,0);
		}
		else {
			pClt->RetryTnel(m_strGlobalName);
		}
		LeaveCriticalSection(&m_csCltArray);
	}
	return 1;
}

UINT CDVRManager::DealWithCnctGram()
{
	CNCTDESC CnctDesc;
	CClientTnel *pClt;
	int count = m_CnctDescArray.GetSize();
	WPARAM wParam;
	char chCltName[NAME_BUF_LENGTH];
	while (count > 0) {
		EnterCriticalSection(&m_csGram);
		CnctDesc = m_CnctDescArray[0];
		m_CnctDescArray.RemoveAt(0);
		LeaveCriticalSection(&m_csGram);
		count = m_CnctDescArray.GetSize();
		memset(chCltName,0,NAME_BUF_LENGTH);
		if(!LegalCnct(&CnctDesc.OpenGram,chCltName)) {
			//// 这里要判断所用的名称是否和本机名称相同，避免恶意攻击
			continue;
		}
		TRACE("DVR Cnct:%s",chCltName);
		EnterCriticalSection(&m_csCltArray);
		pClt = GetClt(chCltName);
		//// 这里pClt 有可能等于 NULL，原因是NetS发给DVR的TUNNEL消息还没有收到
		if(NULL == pClt) {   
			pClt = new CClientTnel(chCltName,
				CnctDesc.addr.sin_addr.s_addr,CnctDesc.addr.sin_port,TUNNEL_CMD);
			pClt->InitResource(m_strGlobalName);
			m_CltTnelArray.Add(pClt);
			if(g_ConnectedFromClient != NULL) {
				g_ConnectedFromClient(pClt,CnctDesc.addr.sin_addr.s_addr);
			}
		}
		else {
			wParam = pClt->DealwithCnctGram(&CnctDesc);
			if(NULL != m_hMsgWnd)
				::PostMessage(m_hMsgWnd,WM_CLT_TUNNEL,wParam,0);
		}
		LeaveCriticalSection(&m_csCltArray);
	}
	return 1;
}

UINT CDVRManager::DealWithHshkGram()
{
	HSHKDESC HshkDesc;
	CClientTnel *pClt;
	int count = m_HshkDescArray.GetSize();
	char chCltName[NAME_BUF_LENGTH],chDvrName[NAME_BUF_LENGTH];
	while (count > 0) {
		EnterCriticalSection(&m_csGram);
		HshkDesc = m_HshkDescArray[0];
		m_HshkDescArray.RemoveAt(0);
		LeaveCriticalSection(&m_csGram);
		count = m_HshkDescArray.GetSize();
		memset(chCltName,0,NAME_BUF_LENGTH);
		if(!LegalHshk(&HshkDesc.HshkGram,chCltName,chDvrName)) {
			//// 这里要判断所用的名称是否和本机名称相同，避免恶意攻击
			continue;
		}
		TRACE("DVR Hshk:%s",chCltName);
		EnterCriticalSection(&m_csCltArray);
		pClt = GetClt(chCltName);
		if(NULL == pClt) {   
			//// 说明是直接连接的，没有通过唯一名称
			pClt = new CClientTnel(chCltName,
				HshkDesc.addr.sin_addr.s_addr,HshkDesc.addr.sin_port,CONNECT_CMD);
			pClt->InitResource(chDvrName);
			m_CltTnelArray.Add(pClt);
			if(g_ConnectedFromClient != NULL) {
				g_ConnectedFromClient(pClt,HshkDesc.addr.sin_addr.s_addr);
			}
		}
		pClt->DealwithHshkGram(&HshkDesc);
		LeaveCriticalSection(&m_csCltArray);
	}
	return 1;
}

UINT CDVRManager::DealWithCmdGram()
{
	CMDDESC CmdDesc;
	CMDINFO MsgInfo;
	CClientTnel *pClt;
	CString strGiveName;
	BOOL bParseCmd = FALSE;
	int count = m_CmdDescArray.GetSize();
	char chCltName[NAME_BUF_LENGTH],chDvrName[NAME_BUF_LENGTH];
	while (count > 0) {
		EnterCriticalSection(&m_csGram);
		CmdDesc = m_CmdDescArray[0];
		m_CmdDescArray.RemoveAt(0);
		LeaveCriticalSection(&m_csGram);
		count = m_CmdDescArray.GetSize();
		memset(chCltName,0,NAME_BUF_LENGTH);
		memset(chDvrName,0,NAME_BUF_LENGTH);
		if(!LegalCmd(&CmdDesc.WorkGram,chCltName,chDvrName)) {
			continue;
		}
		//// TRACE("DVR Cmd:Clt:%s,DVR:%s",chCltName,chDvrName);
		EnterCriticalSection(&m_csCltArray);
		pClt = GetClt(chCltName);
		//// 与老版本不同，以前在这里做New Client的判断，现在移到握手中去了
		if(NULL != pClt) {
			bParseCmd = pClt->CheckCmdGram(&CmdDesc);
		}
		if(bParseCmd && NULL != g_RecvFromClient) {
			//// 将来这里要判断音视频传输的命令
			memset(&MsgInfo,0,sizeof(CMDINFO));
			//// 使用这个回调函数要求必须有对应的消息响应
			g_RecvFromClient(pClt,(PNETPACKET)&CmdDesc.WorkGram.Info,(PNETPACKET)&MsgInfo);
			//// 判断是否是VIDEO和AUDIO传输
			if(0 == strcmp(MsgInfo.chCmd,MSG_VIDEO)) {
				pClt->CheckVideoMsg(&MsgInfo); 
			}
			else if(0 == strcmp(MsgInfo.chCmd,MSG_AUDIO)) {
				pClt->CheckAudioMsg(&MsgInfo);
			}
			else
				pClt->SetMsgGram(&MsgInfo);
		}
		LeaveCriticalSection(&m_csCltArray);
	}
	return 1;
}

UINT CDVRManager::DealWithVResdGram()
{
	VRESDDESC VResdDesc;
	CClientTnel *pClt;
	int count = m_VResdDescArray.GetSize();
	char chCltName[NAME_BUF_LENGTH],chDvrName[NAME_BUF_LENGTH];
	while (count > 0) {
		EnterCriticalSection(&m_csGram);
		VResdDesc = m_VResdDescArray[0];
		m_VResdDescArray.RemoveAt(0);
		LeaveCriticalSection(&m_csGram);
		count = m_VResdDescArray.GetSize();
		memset(chCltName,0,NAME_BUF_LENGTH);
		memset(chDvrName,0,NAME_BUF_LENGTH);
		if(!LegalVResd(&VResdDesc.VResdGram,chCltName,chDvrName)) {
			continue;
		}
		//// TRACE("DVR Resd:Clt:%s,DVR:%s",chCltName,chDvrName);
		EnterCriticalSection(&m_csCltArray);
		pClt = GetClt(chCltName);
		if(NULL != pClt) {
			pClt->CheckVResdGram(&VResdDesc.VResdGram.vResdPacket,VResdDesc.addr.sin_addr.s_addr,chDvrName);
		}
		LeaveCriticalSection(&m_csCltArray);
	}
	return 1;
}

//// Check if Tunnel Gram is Legal. Should be modified later...
BOOL CDVRManager::LegalTnel(PTNELMSG_GRAM pTnel,char *pCltName)
{
	if(FOURCC_HOLE != pTnel->dwType) {
		return FALSE;
	}
	char chTmp[NAME_BUF_LENGTH];
	int nLength = 0;
	if(!ThrowRandom(chTmp,pTnel->chDstName,&nLength,NAME_BUF_LENGTH,NAME_BUF_LENGTH,RANDOM_TRANS_NAMEKEY)) {
		return FALSE;
	}
	if(nLength > MAX_NAME_LENGTH || nLength < MIN_NAME_LENGTH) {
		return FALSE;
	}
	if(pTnel->byAct != TUNNEL_REQUIRE) {
		return FALSE;
	}
	strcpy(pCltName,chTmp);

	return TRUE;
}

BOOL CDVRManager::LegalCnct(POPENTNEL pOpen,char *pCltName)
{
	if(FOURCC_OPEN != pOpen->dwType) {
		return FALSE;
	}
	char chTmp[NAME_BUF_LENGTH];
	int nLength = 0;
	if(!ThrowRandom(chTmp,pOpen->chSecondName,&nLength,NAME_BUF_LENGTH,NAME_BUF_LENGTH,RANDOM_TRANS_NAMEKEY)) {
		return FALSE;
	}
	if(nLength > MAX_NAME_LENGTH || nLength < MIN_NAME_LENGTH) {
		return FALSE;
	}
	if(!UncaseCompare(m_strGlobalName,chTmp)) {
		return FALSE;
	}
	if(!ThrowRandom(chTmp,pOpen->chFirstName,&nLength,NAME_BUF_LENGTH,NAME_BUF_LENGTH,RANDOM_TRANS_NAMEKEY)) {
		return FALSE;
	}
	if(nLength > MAX_NAME_LENGTH || nLength < MIN_NAME_LENGTH) {
		return FALSE;
	}
	strcpy(pCltName,chTmp);

	return TRUE;
}

BOOL CDVRManager::LegalHshk(PHSHK_GRAM pHshk,char *pCltName,char *pDvrName)
{
	if(pHshk->dwType != FOURCC_HSHK) return FALSE;
	char chTmp[NAME_BUF_LENGTH];
	int nLength = 0;
	if(!ThrowRandom(chTmp,pHshk->chLocalName,&nLength,NAME_BUF_LENGTH,NAME_BUF_LENGTH,RANDOM_TRANS_NAMEKEY)) {
		return FALSE;
	}
	if(nLength > MAX_NAME_LENGTH || nLength < MIN_NAME_LENGTH) {
		return FALSE;
	}
	strcpy(pCltName,chTmp);
	if(!ThrowRandom(chTmp,pHshk->chDestName,&nLength,NAME_BUF_LENGTH,NAME_BUF_LENGTH,RANDOM_TRANS_NAMEKEY)) {
		return FALSE;
	}
	if(nLength > MAX_NAME_LENGTH || nLength < MIN_NAME_LENGTH) {
		return FALSE;
	}
	strcpy(pDvrName,chTmp);
	//// 这里的DestName不一定是GlobalName.有可能是直接连接方式的
	return TRUE;
}

BOOL CDVRManager::LegalCmd(PWORK_GRAM pCmd,char *pCltName,char *pDvrName)
{
	if(pCmd->dwType != FOURCC_PUSH) return FALSE;
	char chTmp[NAME_BUF_LENGTH];
	int nLength = 0;
	if(!ThrowRandom(chTmp,pCmd->chLocalName,&nLength,NAME_BUF_LENGTH,NAME_BUF_LENGTH,RANDOM_TRANS_NAMEKEY)) {
		return FALSE;
	}
	if(nLength > MAX_NAME_LENGTH || nLength < MIN_NAME_LENGTH) {
		return FALSE;
	}
	strcpy(pCltName,chTmp);
	if(!ThrowRandom(chTmp,pCmd->chDestName,&nLength,NAME_BUF_LENGTH,NAME_BUF_LENGTH,RANDOM_TRANS_NAMEKEY)) {
		return FALSE;
	}
	if(nLength > MAX_NAME_LENGTH || nLength < MIN_NAME_LENGTH) {
		return FALSE;
	}
	strcpy(pDvrName,chTmp);
	//// 这里的DestName不一定是GlobalName.有可能是直接连接方式的
	return TRUE;
}

BOOL CDVRManager::LegalVResd(PVRESD_GRAM pVResd,char *pCltName,char *pDvrName)
{
	if(pVResd->gramHeader.dwType != FOURCC_RESD) return FALSE;
	char chTmp[NAME_BUF_LENGTH];
	int nLength = 0;
	if(!ThrowRandom(chTmp,pVResd->gramHeader.chLocalName,&nLength,NAME_BUF_LENGTH,NAME_BUF_LENGTH,RANDOM_TRANS_NAMEKEY)) {
		return FALSE;
	}
	if(nLength > MAX_NAME_LENGTH || nLength < MIN_NAME_LENGTH) {
		return FALSE;
	}
	strcpy(pCltName,chTmp);
	if(!ThrowRandom(chTmp,pVResd->gramHeader.chDestName,&nLength,NAME_BUF_LENGTH,NAME_BUF_LENGTH,RANDOM_TRANS_NAMEKEY)) {
		return FALSE;
	}
	if(nLength > MAX_NAME_LENGTH || nLength < MIN_NAME_LENGTH) {
		return FALSE;
	}
	strcpy(pDvrName,chTmp);
	if(pVResd->vResdPacket.byChanNum >= g_ChanInfoArray.GetSize()) {
		//// 不应该大于实际通道数量
		return FALSE;
	}

	return TRUE;
}

UINT CDVRManager::NormalNetSChecking()
{
	if(!m_bApplying&&(m_bSleeping||!m_bGotGlobalName)) return 1;
	CSelfLock CS(&m_csRes);
	m_DVRInfo.byAct = DVR_ACT_AUTOUPDATE;
	SecretEncode2Apply(m_DVRInfo.chName,m_DVRInfo.chPassword);
	if(!m_pLinkNetS->SelfChecking(&m_DVRInfo)) {
		if(m_bApplying)
			m_bApplying = FALSE;
		//// Time out
		if(NULL != m_hMsgWnd)
			::PostMessage(m_hMsgWnd,WM_NETS_NORESP,0,0);
		m_uRegisterStatus = NTT_SERVERBUSY;
	}
	
	return 0;
}

UINT CDVRManager::NormalCltChecking()
{
	CSelfLock CS(&m_csCltArray);
	BOOL bExist;
	int index = 0;
	int count = m_CltTnelArray.GetSize();
	while (index < count) {
		bExist = m_CltTnelArray[index]->SelfChecking();
		if(bExist) {
			index ++;
		}
		else {
			if(g_DisconnectedFromClient != NULL) {
				g_DisconnectedFromClient(m_CltTnelArray[index]);
			}
			delete m_CltTnelArray[index];
			m_CltTnelArray.RemoveAt(index);
			count--;
		}
	}
	return 1;
}

void CDVRManager::DvrExitNotify()
{
	CSelfLock CS(&m_csCltArray);
	for(int i = 0; i < m_CltTnelArray.GetSize(); i++) {
		m_CltTnelArray[i]->SetExit();
	}
}

CClientTnel* CDVRManager::GetClt(char *Name)
{
	int count = m_CltTnelArray.GetSize();
	for(int i = 0; i < count; i++) {
		if(m_CltTnelArray[i]->IsMyself(Name)) 
			break;
	}
	if(i == count) return NULL;
	else return m_CltTnelArray[i];
}

UINT CDVRManager::RcvGramProc(LPVOID p)
{
	UINT uRe;
	CDVRManager *pDVR = (CDVRManager*)p;
	uRe = pDVR->KeepRcving();
	pDVR->DecreaseThreadCount();

	return uRe; 
}

UINT CDVRManager::DistribGramProc(LPVOID p)
{
	UINT uRe;
	CDVRManager *pDVR = (CDVRManager*)p;
	uRe = pDVR->DistribGram();
	pDVR->DecreaseThreadCount();

	return uRe;
}

UINT CDVRManager::CheckOverProc(LPVOID p)
{
	UINT uRe;
	CDVRManager *pDVR = (CDVRManager*)p;
	uRe = pDVR->CheckOver();
	pDVR->DecreaseThreadCount();

	return uRe;
}

BOOL CDVRManager::PushSData(int nLength, SOCKADDR_IN sAddr, PVOID pData)
{
	int nTmpWriteIndex;
	CSelfLock CS(&m_csSData);
	if(m_bSendBufFull) {
		nTmpWriteIndex = (m_nWriteSDataIndex+1) % SENDDATA_BUFFER_COUNT;
		if(nTmpWriteIndex != m_nReadSDataIndex) {
			m_nWriteSDataIndex = nTmpWriteIndex;
			m_bSendBufFull = FALSE;
		}
		else {
			SetEvent(m_hSendData);
			return FALSE;
		}
	}
	m_SendDataDesc[m_nWriteSDataIndex].nLength = nLength;
	m_SendDataDesc[m_nWriteSDataIndex].IPAddr = sAddr;
	memcpy(m_SendDataDesc[m_nWriteSDataIndex].pBuf,pData,nLength);
	nTmpWriteIndex = (m_nWriteSDataIndex+1)  % SENDDATA_BUFFER_COUNT;
	if(nTmpWriteIndex == m_nReadSDataIndex) {
		m_bSendBufFull = TRUE;
	}
	else {
		m_nWriteSDataIndex = nTmpWriteIndex;
	}
	SetEvent(m_hSendData);

	return TRUE;
}

UINT CDVRManager::SendGram()
{
	DWORD dwWait;
	HANDLE hEv[2];
	int size = sizeof(SOCKADDR_IN);
	hEv[0] = m_hSendData;
	hEv[1] = m_hStopWorking;
	while (m_bWorking) {
		dwWait = WaitForMultipleObjects(2,hEv,FALSE,INFINITE);
		if(WAIT_OBJECT_0 == dwWait) {
			while(m_nReadSDataIndex != m_nWriteSDataIndex) {
				sendto(m_sGram,m_SendDataDesc[m_nReadSDataIndex].pBuf,
					m_SendDataDesc[m_nReadSDataIndex].nLength,0,
					(SOCKADDR*)&m_SendDataDesc[m_nReadSDataIndex].IPAddr,size);
				m_nReadSDataIndex = (m_nReadSDataIndex+1) % SENDDATA_BUFFER_COUNT;
				WaitForSockWroten(m_sGram,2);
			}
		}
		else {
			TRACE("DVR SendGram Thread Exit");
			break;
		}
	}

	return 1;
}

UINT CDVRManager::SendGramProc(LPVOID p)
{
	UINT uRe;
	CDVRManager *pDVR = (CDVRManager*)p;
	uRe = pDVR->SendGram();
	pDVR->DecreaseThreadCount();

	return uRe;
}

//// 编解码函数集合。使用函数前必须确认参数的数组长度 >= NAME_BUF_LENGTH
BOOL CDVRManager::SecretEncode2File(/*out*/char *pName,/*out*/char *pPassword)
{
	//// 这个时候必须确认已经获得GlobalName
	char chTmp[NAME_BUF_LENGTH];
	int nNameLength = m_strGlobalName.GetLength();
	if(nNameLength < MIN_NAME_LENGTH || nNameLength > MAX_NAME_LENGTH) return FALSE;

	memset(chTmp,0,NAME_BUF_LENGTH);
	strcpy(chTmp,m_strGlobalName);
	PushinRandom(pName,chTmp,NAME_BUF_LENGTH,nNameLength,RANDOM_FILE_NAMEKEY);
	PushinRandom(pPassword,m_chPassword,NAME_BUF_LENGTH,MD5_CODE_LENGTH,RANDOM_FILE_PASSKEY);
	
	return TRUE;
}

BOOL CDVRManager::SecretDecode3File(/*in*/char *pName,/*in*/char *pPassword)
{
	char chTmp[NAME_BUF_LENGTH];
	int nLength = 0;
	if(!ThrowRandom(chTmp,pName,&nLength,NAME_BUF_LENGTH,NAME_BUF_LENGTH,RANDOM_FILE_NAMEKEY)) {
		return FALSE;
	}
	if(nLength > MAX_NAME_LENGTH || nLength < MIN_NAME_LENGTH) return FALSE;
	m_strCheckingName = chTmp;
	if(!ThrowRandom(m_chCheckingPassword,pPassword,&nLength,NAME_BUF_LENGTH,NAME_BUF_LENGTH,RANDOM_FILE_PASSKEY)) {
		memset(m_chCheckingPassword,0,NAME_BUF_LENGTH);
		return FALSE;
	}

	return TRUE;
}

BOOL CDVRManager::SecretEncode2Apply(/*out*/char *pName,/*out*/char *pPassword)
{
	char chTmp[NAME_BUF_LENGTH];
	int nNameLength = m_strCheckingName.GetLength();
	if(nNameLength < MIN_NAME_LENGTH || nNameLength > MAX_NAME_LENGTH) return FALSE;

	memset(chTmp,0,NAME_BUF_LENGTH);
	strcpy(chTmp,m_strCheckingName);
	PushinRandom(pName,chTmp,NAME_BUF_LENGTH,nNameLength,RANDOM_APPLY_NAMEKEY);
	PushinRandom(pPassword,m_chCheckingPassword,NAME_BUF_LENGTH,MD5_CODE_LENGTH,RANDOM_APPLY_PASSKEY);

	return TRUE;
}
