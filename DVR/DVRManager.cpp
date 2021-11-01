#include "stdafx.h"
#include "NTTProtocol.h"
#include "LocalSDKFun.h"
#include "DVRManager.h"
#include "Global.h"
#include "md5.h"

extern CONNECTEDFROMCLIENT g_ConnectedFromClient; //// ��¼����ʹ��ʱ����
extern RECVFROMCLIENT g_RecvFromClient; //// �������������ʱ�����
extern DISCONNECTEDFROMCLIENT g_DisconnectedFromClient; //// ɾ��ClientTnel��֮ǰ����

CRITICAL_SECTION g_csSocket;
CDVRManager::CDVRManager(DWORD dwType, DWORD dwID0, DWORD dwID1) :
m_bWorking(FALSE),
m_bGotGlobalName(FALSE),
m_bNameSet(FALSE),
m_nThreadCount(0)
{
	InitializeCriticalSection(&g_csSocket);
	m_hMsgWnd = NULL;
	m_sRcvGram = INVALID_SOCKET;
	m_wNetSPort = NETSERVER_PORT;
	m_wDVRPort = DVR_PORT;
	m_pLinkNetS = NULL;
	memset(m_chPassword,0,NAME_BUF_LENGTH);
	memset(m_chCheckingPassword,0,NAME_BUF_LENGTH);
	memset(&m_DVRInfo,0,sizeof(APPLY_GRAM));
	m_DVRInfo.dwType = dwType;
	m_DVRInfo.dwDevID[0] = dwID0;
	m_DVRInfo.dwDevID[1] = dwID1;
	memset(&m_LocalAddr,0,sizeof(SOCKADDR_IN));
	m_LocalAddr.sin_family = AF_INET;
	m_LocalAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	m_LocalAddr.sin_port = htons(m_wDVRPort);

	m_hRcvGram = WSACreateEvent();
	m_hNetSFBGram = CreateEvent(NULL,FALSE,FALSE,NULL);
	m_hTnelGram = CreateEvent(NULL,FALSE,FALSE,NULL);
	m_hCnctGram = CreateEvent(NULL,FALSE,FALSE,NULL);
	m_hCmdGram = CreateEvent(NULL,FALSE,FALSE,NULL);
	m_hStopWorking = CreateEvent(NULL,TRUE,FALSE,NULL);
	InitializeCriticalSection(&m_csGram);
	InitializeCriticalSection(&m_csCltArray); 
	InitializeCriticalSection(&m_csRes); //// ��һЩ������Դ�ı���
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
		delete m_CltTnelArray[i];
	}
	m_CltTnelArray.RemoveAll();
	WSACloseEvent(m_hRcvGram);
	CloseHandle(m_hNetSFBGram);
	CloseHandle(m_hTnelGram);
	CloseHandle(m_hCnctGram);
	CloseHandle(m_hCmdGram);
	CloseHandle(m_hStopWorking);
	DeleteCriticalSection(&m_csGram);
	DeleteCriticalSection(&m_csCltArray);
	DeleteCriticalSection(&m_csRes);
	DeleteCriticalSection(&g_csSocket);
}

BOOL CDVRManager::StartWorking(HWND hWnd,LPCTSTR Path,LPCTSTR NetSName)
{
	ASSERT(INVALID_SOCKET == m_sRcvGram);
	int on = 1;
	m_sRcvGram = socket(AF_INET,SOCK_DGRAM,0);
	int count=0;
	do {
		if(SOCKET_ERROR != bind(m_sRcvGram, (SOCKADDR *FAR)&m_LocalAddr,sizeof(SOCKADDR_IN))) {
			break;
		}
		m_wDVRPort++;
		count++;
		m_LocalAddr.sin_port = htons(m_wDVRPort);

	} while(count < 100);
	if(count >= 100) {
		//// bind Failed
		closesocket(m_sRcvGram);
		m_sRcvGram = INVALID_SOCKET;
		return FALSE;
	}
	if(SOCKET_ERROR == (WSAEventSelect(m_sRcvGram,m_hRcvGram,FD_READ))) {
		closesocket(m_sRcvGram);
		m_sRcvGram = INVALID_SOCKET;
		return FALSE;
	}
	
	m_hMsgWnd = hWnd;
	m_strInfoFileName = Path;
	m_strInfoFileName += DVRINFO_FILE_NAME;
	m_strNetSName = NetSName;
	m_bGotGlobalName = ReadDVRInfo();
	
	m_pLinkNetS = new CLinkNetS(m_strNetSName,m_wNetSPort);
	m_pLinkNetS->InitSendingSocket(m_sRcvGram);
	if(m_bGotGlobalName) {
		ManuUpdate();
	}

	TRACE("DVR start working");
	m_bWorking = TRUE;
	DWORD dwID;
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
	closesocket(m_sRcvGram);
	m_sRcvGram = INVALID_SOCKET;
}

BOOL CDVRManager::SetGlobalName(LPCTSTR Name,LPCTSTR Password)
{
	CString strName,strPassword;
	strName = Name;
	int nLength = strName.GetLength();
	if(nLength > MAX_NAME_LENGTH || nLength < MIN_NAME_LENGTH) return FALSE;
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

//// ����û��������Ʋ��ԣ�����ͨ��������һ����һ�ο��Ե�����
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
	if(INADDR_NONE == m_pLinkNetS->GetNetSAddr()) return SENDLINK_NONETS;
	ASSERT(m_pLinkNetS != NULL); //// ֻҪ��ַ�ҵ�����һ�����������
	if(!m_bNameSet) return SENDLINK_FAILED;
	//// ��������
	//// ��ʱ�����ȷ��CLinkNetS�ǿ���״̬
	int nLength = m_strCheckingName.GetLength();
	ASSERT(nLength>=MIN_NAME_LENGTH && nLength<=MAX_NAME_LENGTH);
	if(m_bGotGlobalName) {
		m_bGotGlobalName = FALSE;
		//// return SENDLINK_EXIST; //// nvcd ����Ժ�ʹ�ã�����������������
	}

	CSelfLock CS(&m_csRes);
	m_DVRInfo.byAct = DVR_ACT_REGISTER;
	SecretEncode2Apply(m_DVRInfo.chName,m_DVRInfo.chPassword);

	if(m_pLinkNetS->FillReqInfo(&m_DVRInfo)) {
		return SENDLINK_SUCCESS;
	}
	else {
		return SENDLINK_FAILED;
	}
	
}

BYTE CDVRManager::UnregisterUnit()
{
	if(INADDR_NONE == m_pLinkNetS->GetNetSAddr()) return SENDLINK_NONETS;
	ASSERT(m_pLinkNetS != NULL);
	if(!m_bNameSet) return SENDLINK_FAILED;
	
	int nLength = m_strCheckingName.GetLength();
	ASSERT(nLength>=MIN_NAME_LENGTH && nLength<=MAX_NAME_LENGTH);
	CSelfLock CS(&m_csRes);
	m_DVRInfo.byAct = DVR_ACT_DELETE;

	SecretEncode2Apply(m_DVRInfo.chName,m_DVRInfo.chPassword);
	if(m_pLinkNetS->FillReqInfo(&m_DVRInfo)) {
		return SENDLINK_SUCCESS;
	}
	else {
		return SENDLINK_FAILED;
	}
}

BYTE CDVRManager::ManuUpdate()
{
	if(INADDR_NONE == m_pLinkNetS->GetNetSAddr()) return SENDLINK_NONETS;
	ASSERT(m_pLinkNetS != NULL);
	if(!m_bNameSet) return SENDLINK_FAILED;
	
	int nLength = m_strCheckingName.GetLength();
	ASSERT(nLength>=MIN_NAME_LENGTH && nLength<=MAX_NAME_LENGTH);
	CSelfLock CS(&m_csRes);
	m_DVRInfo.byAct = DVR_ACT_MANUALUPDATE;

	SecretEncode2Apply(m_DVRInfo.chName,m_DVRInfo.chPassword);
	if(m_pLinkNetS->FillReqInfo(&m_DVRInfo)) {
		return SENDLINK_SUCCESS;
	}
	else {
		return SENDLINK_FAILED;
	}
}

BYTE CDVRManager::UnitExit()
{
	if(INADDR_NONE == m_pLinkNetS->GetNetSAddr()) return SENDLINK_NONETS;
	ASSERT(m_pLinkNetS != NULL);
	if(!m_bNameSet) return SENDLINK_FAILED;
	
	int nLength = m_strCheckingName.GetLength();
	ASSERT(nLength>=MIN_NAME_LENGTH && nLength<=MAX_NAME_LENGTH);
	CSelfLock CS(&m_csRes);
	m_DVRInfo.byAct = DVR_ACT_EXIT;

	SecretEncode2Apply(m_DVRInfo.chName,m_DVRInfo.chPassword);
	if(m_pLinkNetS->FillReqInfo(&m_DVRInfo)) {
		return SENDLINK_SUCCESS;
	}
	else {
		return SENDLINK_FAILED;
	}
}

BOOL CDVRManager::ReadDVRInfo()
{
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
	//// ���ʱ���GlobalName��ֵ������Write��ʱ��������ֵ����
	m_strGlobalName = m_strCheckingName;
	memcpy(m_chPassword,m_chCheckingPassword,MD5_CODE_LENGTH);
	m_bNameSet = TRUE;
	return TRUE;
BADFILE:
	fclose(hf);
	DeleteFile(m_strInfoFileName);
	return FALSE;
}

BOOL CDVRManager::WriteDVRInfo()
{
	FILE *hf;
	if(!m_bGotGlobalName) {
		//// m_bGotGlobalNameֻ�и����Ʊ�NetSɾ����������ע���ʱ�����FALSE
		DeleteFile(m_strInfoFileName);  //// ���ʱ����������ԭ��������
		return FALSE; //// ���ʱ��û�б�Ҫ����
	}
	SecretEncode2File(m_DVRInfo.chName,m_DVRInfo.chPassword);
	hf = fopen(m_strInfoFileName,"wb");
	if(NULL == hf) return FALSE;
	DWORD dwVer = INFO_VERSION;
	fwrite(&dwVer,sizeof(DWORD),1,hf);
	fwrite(&m_DVRInfo.chName,NAME_BUF_LENGTH,1,hf);
	fwrite(&m_DVRInfo.chPassword,NAME_BUF_LENGTH,1,hf);
	fclose(hf);
	return TRUE;
}

UINT CDVRManager::KeepRcving()
{
	ASSERT(m_sRcvGram != INVALID_SOCKET);
	SOCKADDR_IN DestAddr;
	DWORD dwWait;
	APPLY_GRAM NetSFBGram;
	TNELMSG_GRAM TnelMsgGram;
	CNCTDESC CnctDesc;
	CMDDESC  CmdDesc;
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
			//// EnterCriticalSection(&g_csSocket); ���պͷ���û�г�ͻ
			nRcvLength = recvfrom(m_sRcvGram,rcvBuf,DVR_RCV_BUF_LENGTH,0,(SOCKADDR*)&DestAddr,&len);
			//// LeaveCriticalSection(&g_csSocket);
			if(sizeof(APPLY_GRAM) == nRcvLength) {
				//// ��������������ð����˾��������������
				if(DestAddr.sin_addr.s_addr != m_pLinkNetS->GetNetSAddr()) continue; //// �˿ڿ�����ΪNAT����ͬ
				//// NetS's Feedback Gram
				memcpy(&NetSFBGram,rcvBuf,sizeof(APPLY_GRAM));
				EnterCriticalSection(&m_csGram);
				m_NetSFBArray.Add(NetSFBGram);
				LeaveCriticalSection(&m_csGram);
				SetEvent(m_hNetSFBGram);
			}
			else if(sizeof(TNELMSG_GRAM) == nRcvLength) {
				//// ��������������ð����˾��������������
				if(DestAddr.sin_addr.s_addr != m_pLinkNetS->GetNetSAddr()) continue; //// �˿ڿ�����ΪNAT����ͬ
				//// Tunnel applying Gram
				memcpy(&TnelMsgGram,rcvBuf,sizeof(TNELMSG_GRAM));
				EnterCriticalSection(&m_csGram);
				m_TnelMsgArray.Add(TnelMsgGram);
				LeaveCriticalSection(&m_csGram);
				TRACE("DVR Get NetS-Dest name:%s",TnelMsgGram.chDstName);
				SetEvent(m_hTnelGram);
			}
			else if(sizeof(OPENTNEL) == nRcvLength) {
				//// Client's Open Tunnel Gram
				memcpy(&CnctDesc.OpenGram,rcvBuf,sizeof(OPENTNEL));
				CnctDesc.addr = DestAddr;
				EnterCriticalSection(&m_csGram);
				m_CnctDescArray.Add(CnctDesc);
				LeaveCriticalSection(&m_csGram);
				TRACE("DVR Get TNEL name:%s-to-%s",CnctDesc.OpenGram.chFirstName,CnctDesc.OpenGram.chSecondName);
				SetEvent(m_hCnctGram);
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
			else {
				//// Ī������,���ﲻ�����ش����ݡ�
				;
			}
		}
		else if((WAIT_OBJECT_0+1) == dwWait) {
			break; //// �˳�ѭ��
		}
		else {
			//// ����ǲ����ܵģ�������һ������
			TRACE("Recv Thread Impossible Happen");
		}
	}
	return 1;
}

UINT CDVRManager::DistribGram()
{
	DWORD dwWait;
	HANDLE hEv[5];
	
	hEv[0] = m_hNetSFBGram;
	hEv[1] = m_hTnelGram;
	hEv[2] = m_hCnctGram;
	hEv[3] = m_hCmdGram;
	hEv[4] = m_hStopWorking;
	while (m_bWorking) {
		dwWait = WaitForMultipleObjects(5,hEv,FALSE,INFINITE); 
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
			DealWithCmdGram();
		}
		else if((WAIT_OBJECT_0+4) == dwWait) {
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
		}
		else if(ACK_DELETE_SUCCESS==byResult || ACK_DEVICE_NOTEXIST==byResult) {
			m_bGotGlobalName = FALSE;
		}
		wParam = byResult;
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
	WPARAM wParam;
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
			if(!pClt->InitResource(m_strGlobalName,m_sRcvGram)) {
				delete pClt;
				continue;
			}
			m_CltTnelArray.Add(pClt);
			if(g_ConnectedFromClient != NULL) {
				g_ConnectedFromClient(pClt,TnelMsgGram.dwDstIPaddr);
			}
			wParam = TnelMsgGram.dwDstIPaddr;
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
			//// ����Ҫ�ж����õ������Ƿ�ͱ���������ͬ��������⹥��
			continue;
		}
		TRACE("DVR Cnct:%s",chCltName);
		EnterCriticalSection(&m_csCltArray);
		pClt = GetClt(chCltName);
		//// ����pClt �п��ܵ��� NULL��ԭ����NetS����DVR��TUNNEL��Ϣ��û���յ�
		if(NULL == pClt) {   
			pClt = new CClientTnel(chCltName,
				CnctDesc.addr.sin_addr.s_addr,CnctDesc.addr.sin_port,TUNNEL_CMD);
			pClt->InitResource(m_strGlobalName,m_sRcvGram);
			m_CltTnelArray.Add(pClt);
			if(g_ConnectedFromClient != NULL) {
				g_ConnectedFromClient(pClt,CnctDesc.addr.sin_addr.s_addr);
			}
		}
		else {
			wParam = pClt->DealwithCnctGram(&CnctDesc);
			::PostMessage(m_hMsgWnd,WM_CLT_TUNNEL,wParam,0);
		}
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
		TRACE("DVR Cmd:Clt:%s,DVR:%s",chCltName,chDvrName);
		EnterCriticalSection(&m_csCltArray);
		pClt = GetClt(chCltName);
		if(NULL == pClt) {
			//// ˵����ֱ�����ӵģ�û��ͨ��Ψһ����
			pClt = new CClientTnel(chCltName,
				CmdDesc.addr.sin_addr.s_addr,CmdDesc.addr.sin_port,CONNECT_CMD);
			pClt->InitResource(chDvrName,m_sRcvGram);
			m_CltTnelArray.Add(pClt);
			bParseCmd = TRUE;
			if(g_ConnectedFromClient != NULL) {
				g_ConnectedFromClient(pClt,CmdDesc.addr.sin_addr.s_addr);
			}
		}
		else {
			bParseCmd = pClt->CheckCmdGram(&CmdDesc);
		}
		if(bParseCmd && NULL != g_RecvFromClient) {
			memset(&MsgInfo,0,sizeof(CMDINFO));
			//// ʹ������ص�����Ҫ������ж�Ӧ����Ϣ��Ӧ
			g_RecvFromClient(pClt,&CmdDesc.WorkGram.Info,&MsgInfo);
			pClt->SetMsgGram(&MsgInfo);
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
	//// �����DestName��һ����GlobalName.�п�����ֱ�����ӷ�ʽ��
	return TRUE;
}

UINT CDVRManager::NormalNetSChecking()
{
	CSelfLock CS(&m_csRes);
	m_DVRInfo.byAct = DVR_ACT_AUTOUPDATE;
	SecretEncode2Apply(m_DVRInfo.chName,m_DVRInfo.chPassword);
	if(!m_pLinkNetS->SelfChecking(&m_DVRInfo,m_bGotGlobalName)) {
		//// Time out
		::PostMessage(m_hMsgWnd,WM_NETS_NORESP,0,0);
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

//// ����뺯�����ϡ�ʹ�ú���ǰ����ȷ�ϲ��������鳤�� >= NAME_BUF_LENGTH
BOOL CDVRManager::SecretEncode2File(/*out*/char *pName,/*out*/char *pPassword)
{
	//// ���ʱ�����ȷ���Ѿ����GlobalName
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

