#include "stdafx.h"
#include "NTTProtocol.h"
#include "Global.h"
#include "CltGlobal.h"
#include "CltSDKFun.h"
#include "DVRTnel.h"

extern CRITICAL_SECTION g_csSocket;

extern CONNECTEDDVR g_ConnectedDVR;
extern RECVFROMDVR g_RecvFromDVR;
extern DISCONNECTEDDVR g_DisconnectedDVR;

void EncodeCopyName(char *pDstBuf,LPCTSTR pSrcBuf)
{
	char chTmp[NAME_BUF_LENGTH];
	memset(chTmp,0,NAME_BUF_LENGTH);
	int nLength = strlen(pSrcBuf);
	ASSERT(nLength <= MAX_NAME_LENGTH && nLength >= MIN_NAME_LENGTH);
	strcpy(chTmp,pSrcBuf);
	PushinRandom(pDstBuf,chTmp,NAME_BUF_LENGTH,nLength,RANDOM_TRANS_NAMEKEY);
}

CString DecodeName(char *pName)
{
	CString strName;
	char chTmp[NAME_BUF_LENGTH];
	int nLength = 0;
	if(!ThrowRandom(chTmp,pName,&nLength,NAME_BUF_LENGTH,NAME_BUF_LENGTH,RANDOM_TRANS_NAMEKEY)) {
		return strName;
	}
	strName = chTmp;
	return strName;
}

int CDvrTnel::m_TotalCount = 0;
CDvrTnel::CDvrTnel(LPCTSTR DvrGlobalName, ULONG NetSAddr, WORD wNetSPort)
{
	m_strDvrName = DvrGlobalName;
	m_strShowName = DvrGlobalName;
	ASSERT(m_strDvrName.GetLength() < NAME_BUF_LENGTH);
	memset(&m_ReqGram,0,sizeof(CLTREQ_GRAM));
	memset(&m_TnelGram,0,sizeof(TNELMSG_GRAM));
	memset(&m_OpenGram,0,sizeof(OPENTNEL));
	memset(&m_CmdGram,0,sizeof(WORK_GRAM));
	memset(&m_NetSAddr,0,sizeof(SOCKADDR_IN));
	strcpy(m_LiveInfo.chCmd,"STATUS");
	m_NetSAddr.sin_family = AF_INET;
	m_NetSAddr.sin_addr.s_addr = NetSAddr;
	m_NetSAddr.sin_port = htons(wNetSPort);
	m_LinkStatus = Link_Applying;
	m_bFound = FALSE;
	m_bHopeless = FALSE;
	m_bReplied = FALSE;
	m_nSendReqCount = 0;
	m_nSendTnelCount = 0;
	m_nLastRcvTick = 0;
	m_nKeepliveTick = 0;
	m_nThreadCount = 0;
	m_uExitReason = 0;
	m_sDVR = INVALID_SOCKET;
	m_hSendGram = CreateEvent(NULL,FALSE,FALSE,NULL);
	m_hStopWorking = CreateEvent(NULL,TRUE,FALSE,NULL);
	InitializeCriticalSection(&m_csGram);
	InitializeCriticalSection(&m_csCount);
	m_TotalCount++;
	m_nIndex = m_TotalCount;
}

CDvrTnel::CDvrTnel(LPCTSTR DvrIPName,WORD wDvrPort)
{
	m_strShowName = DvrIPName;
	int nLength = m_strShowName.GetLength();
	//// ����ͨѶʹ�����ƣ���Ҫ�Ǹĵ�".",����������
	//// ���ʹ��һ���ܳ����������п��ܳ����������
	nLength = nLength > MAX_NAME_LENGTH ? MAX_NAME_LENGTH : nLength;
	m_strDvrName.Empty();
	srand(GetTickCount());
	for(int i = 0; i < nLength; i++) {
		if(m_strShowName[i] != _T('.')) {
			m_strDvrName += m_strShowName[i];
		}
		else {
			char kk = (rand()&0xFF);
			m_strDvrName += kk;
		}
	}
	ASSERT(m_strDvrName.GetLength() < NAME_BUF_LENGTH);
	memset(&m_ReqGram,0,sizeof(CLTREQ_GRAM));
	memset(&m_TnelGram,0,sizeof(TNELMSG_GRAM));
	memset(&m_OpenGram,0,sizeof(OPENTNEL));
	memset(&m_CmdGram,0,sizeof(WORK_GRAM));
	memset(&m_DvrAddr,0,sizeof(SOCKADDR_IN));
	m_DvrAddr.sin_family = AF_INET;
	m_DvrAddr.sin_addr.s_addr = Name2IP(DvrIPName);
	m_DvrAddr.sin_port = htons(wDvrPort);
	m_LinkStatus = Link_Found;
	m_bFound = FALSE;
	m_bHopeless = FALSE;
	m_nSendReqCount = 0;
	m_nSendTnelCount = 0;
	m_nLastRcvTick = 0;
	m_nKeepliveTick = 0;
	m_nThreadCount = 0;
	m_uExitReason = 0;
	m_sDVR = INVALID_SOCKET;
	m_hSendGram = CreateEvent(NULL,FALSE,FALSE,NULL);
	m_hStopWorking = CreateEvent(NULL,TRUE,FALSE,NULL);
	InitializeCriticalSection(&m_csGram);
	InitializeCriticalSection(&m_csCount);
	m_TotalCount++;
	m_nIndex = m_TotalCount;
}

CDvrTnel::~CDvrTnel()
{
	SetEvent(m_hStopWorking);
	while (m_nThreadCount > 0) {
		Sleep(50);
	}
	if(g_DisconnectedDVR != NULL) {
		g_DisconnectedDVR(m_strShowName,m_uExitReason);
	}
	DeleteCriticalSection(&m_csGram);
	DeleteCriticalSection(&m_csCount);
	CloseHandle(m_hSendGram);
	CloseHandle(m_hStopWorking);
	m_TotalCount--;
}

BOOL CDvrTnel::InitResource(SOCKET sDVR,PCMDINFO pFirstCmd)
{
	//// ����Ҫ�ж���ʲô��ʽ���ɵ���
	//// ���������һ������
	ASSERT(INVALID_SOCKET == m_sDVR);
	m_sDVR = sDVR;
	MakeName();
	m_CmdGram.dwType = FOURCC_PUSH;
	//// strcpy(m_CmdGram.chDestName,m_strDvrName);  ���������ʱ������޸�
	//// strcpy(m_CmdGram.chLocalName,m_chName);
	memcpy(&m_CmdGram.Info,pFirstCmd,sizeof(CMDINFO));
	if(Link_Applying == m_LinkStatus) {
		m_CmdGram.byAct = TUNNEL_CMD;
		m_ReqGram.dwType = FOURCC_HOLE;
		/*
		strcpy(m_ReqGram.chDstName,m_strDvrName);
		strcpy(m_ReqGram.chName,m_chName);
		*/
		EncodeCopyName(m_ReqGram.chDstName,m_strDvrName);
		EncodeCopyName(m_ReqGram.chName,m_chName);
		m_ReqGram.byAct = TUNNEL_REQUIRE;
		EnterCriticalSection(&g_csSocket);
		sendto(m_sDVR,(char*)&m_ReqGram,sizeof(CLTREQ_GRAM),0,(SOCKADDR*)&m_NetSAddr,sizeof(SOCKADDR_IN));
		LeaveCriticalSection(&g_csSocket);
	}
	else {
		ASSERT(Link_Found == m_LinkStatus);
		m_CmdGram.byAct = CONNECT_CMD;
		EncodeCopyName(m_CmdGram.chDestName,m_strDvrName);
		EncodeCopyName(m_CmdGram.chLocalName,m_chName);
		EnterCriticalSection(&g_csSocket);
		sendto(m_sDVR,(char*)&m_CmdGram,sizeof(WORK_GRAM),0,(SOCKADDR*)&m_DvrAddr,sizeof(SOCKADDR_IN));
		LeaveCriticalSection(&g_csSocket);
	}
	DWORD dwID;
	IncreaseThreadCount();
	CreateThread(NULL,0,(LPTHREAD_START_ROUTINE)CDvrTnel::SendGramProc,this,0,&dwID);
	
	return TRUE;
}

BOOL CDvrTnel::IsMyself(LPCTSTR ShowName)
{
	return UncaseCompare(m_strShowName,ShowName);
}

BOOL CDvrTnel::FindDvr(LPCTSTR DvrName) 
{
	return UncaseCompare(m_strDvrName,DvrName);
}

//// �Լ졣�������FALSE,˵��û�д������壬��Ҫɾ��
BOOL CDvrTnel::SelfChecking()
{
	BOOL bExit = TRUE;
	if(m_bHopeless) return FALSE;
	switch(m_LinkStatus) {
	case Link_Applying:
		if(m_nSendReqCount > MAX_REQ_COUNT) {
			m_uExitReason = EXIT_NETS_LOST;
			if(g_ConnectedDVR != NULL) {
				g_ConnectedDVR(m_strShowName,0,FALSE);
			}
			bExit = FALSE;
		}
		else {
			m_nSendReqCount ++;
			EnterCriticalSection(&m_csGram);
			EncodeCopyName(m_ReqGram.chDstName,m_strDvrName);
			EncodeCopyName(m_ReqGram.chName,m_chName);
			LeaveCriticalSection(&m_csGram);
			SetEvent(m_hSendGram);
		}
		break;
	case Link_Creating:
		if(m_nSendTnelCount > MAX_TNEL_COUNT) {
			m_uExitReason = EXIT_DVR_LOST;
			if(g_ConnectedDVR != NULL) {
				g_ConnectedDVR(m_strShowName,m_DvrAddr.sin_addr.s_addr,FALSE);
			}
			bExit = FALSE;
		}
		else {
			m_nSendTnelCount ++;
			EnterCriticalSection(&m_csGram);
			EncodeCopyName(m_OpenGram.chFirstName,m_chName);
			EncodeCopyName(m_OpenGram.chSecondName,m_strDvrName);
			LeaveCriticalSection(&m_csGram);
			SetEvent(m_hSendGram);
		}
		break;
	case Link_Found:
		TRACE("Clt Selfcheck link:%d",m_nLastRcvTick);
		if(m_nLastRcvTick > MAX_REPLY_INTERVAL) {
			m_uExitReason = EXIT_DVR_LOST;
			bExit = FALSE;
		}
		else {
			m_nLastRcvTick ++;
			if(m_nKeepliveTick >= KEEPLIVE_INTERVAL) {
				EnterCriticalSection(&m_csGram);
				EncodeCopyName(m_CmdGram.chDestName,m_strDvrName);
				EncodeCopyName(m_CmdGram.chLocalName,m_chName);
				LeaveCriticalSection(&m_csGram);
				SetEvent(m_hSendGram);
				m_nKeepliveTick = 0;
			}
			else {
				m_nKeepliveTick++;
			}
		}
		break;
	}
	return bExit;
}

UINT CDvrTnel::SetCmdGram(PCMDINFO pCmd)
{
	if(INVALID_SOCKET == m_sDVR) return CMD_SOCK_FAIL;
	if(Link_Found != m_LinkStatus) return CMD_LINKING;
	EnterCriticalSection(&m_csGram);
	EncodeCopyName(m_CmdGram.chDestName,m_strDvrName);
	EncodeCopyName(m_CmdGram.chLocalName,m_chName);
	memcpy(&m_CmdGram.Info,pCmd,sizeof(CMDINFO));
	LeaveCriticalSection(&m_csGram);
	SetEvent(m_hSendGram);

	return CMD_SUCCESS;
}

BOOL CDvrTnel::DealwithTnelGram(PTNELMSG_GRAM pTnel)
{
	if(Link_Applying != m_LinkStatus) {
		return TRUE;
	}
	if(TUNNEL_ACK_SUCCESS != pTnel->byAct) {
		if(TUNNEL_ACK_NODEVICE == pTnel->byAct) {
			m_uExitReason = EXIT_DVR_NONAME;
		}
		else if(TUNNEL_ACK_OFFLINE == pTnel->byAct) {
			m_uExitReason = EXIT_DVR_OFFLINE;
		}
		else {
			m_uExitReason = EXIT_DVR_BUSY;
		}
		m_bHopeless = TRUE;
		return FALSE;
	}
	//// ����Ҫһ��,�������һ����������������DVR
	CString strDestName = DecodeName(pTnel->chDstName);
	if(!UncaseCompare(m_strDvrName,strDestName)) {
		m_uExitReason = EXIT_UNKNOW;
		return FALSE;
	}

	EnterCriticalSection(&m_csGram);
	m_DvrAddr.sin_family = AF_INET;
	m_DvrAddr.sin_addr.s_addr = pTnel->dwDstIPaddr;
	m_DvrAddr.sin_port = pTnel->wDstPort;  //// ֱ�ӷ����ģ��Ͳ���htons��ת����
	m_OpenGram.dwType = FOURCC_OPEN;
	m_OpenGram.byAct = TUNNEL_REQUIRE;
	EncodeCopyName(m_OpenGram.chFirstName,m_chName);
	EncodeCopyName(m_OpenGram.chSecondName,m_strDvrName);
	//// strcpy(m_OpenGram.chFirstName,m_chName);
	//// strcpy(m_OpenGram.chSecondName,m_strDvrName);
	m_LinkStatus = Link_Creating;
	LeaveCriticalSection(&m_csGram);
	SetEvent(m_hSendGram);

	return TRUE;
}

BYTE CDvrTnel::DealwithCnctGram(PCNCTDESC pTnelInfo)
{
	if(Link_Found == m_LinkStatus) return TUNNEL_ACK_SUCCESS;
	//// �п��ܻ���Link_Applying��ʱ�򣬶Է��ͷ���������ݰ���
	//// ����ǿ���ȷ��.byAct��TUNNEL_REQUIRE������� TUNNEL_ACK_DUPLINK�����ڹ������д����ˣ����ý�������
	//// ��û�б�Ҫ�ٷ�ȷ���ˣ�ֱ�ӷ�������Ϳ���
	if(TUNNEL_REQUIRE!=pTnelInfo->OpenGram.byAct && TUNNEL_ACK_SUCCESS!=pTnelInfo->OpenGram.byAct) {
		//// ����DVR�������Ѿ����ˣ������ϻ��п���ͬһ���ͻ����ֵĵڶ�������
		m_bHopeless = TRUE;
		m_uExitReason = EXIT_DVR_BUSY;
		return pTnelInfo->OpenGram.byAct;
	}
	EnterCriticalSection(&m_csGram);
	m_DvrAddr = pTnelInfo->addr;
	m_LinkStatus = Link_Found;
	EncodeCopyName(m_CmdGram.chDestName,m_strDvrName);
	EncodeCopyName(m_CmdGram.chLocalName,m_chName);
	LeaveCriticalSection(&m_csGram);
	SetEvent(m_hSendGram);
	//// ��Ȼ�����Ѿ������ϣ�����������Щֱ�����ӵģ�������MSG����ȷ���Ƿ����ӵĻص�����

	return TUNNEL_ACK_SUCCESS;
}

BOOL CDvrTnel::DealwithMsgGram(PCMDINFO pWorkInfo,LPCTSTR pCltName)
{
	if(Link_Found != m_LinkStatus) {
		//// �����Ƕ��⹥��
		return FALSE;
	}
	if(0 != strcmp(m_chName,pCltName)) {
		//// �����Ƕ��⹥��
		return FALSE;
	}
	if(!m_bFound) {
		if(g_ConnectedDVR != NULL) {
			g_ConnectedDVR(m_strShowName,m_DvrAddr.sin_addr.s_addr,TRUE);
		}
		m_bFound = TRUE;		
	}
	m_nLastRcvTick = 0;
	if(g_RecvFromDVR != NULL) {
		g_RecvFromDVR(m_strShowName,pWorkInfo);
	}
	MakeLiveGram();//// ���ʱ����Է������������ˣ�����һֱ�������һ������
	return TRUE;
}

void CDvrTnel::MakeName()
{
	SYSTEMTIME st;
	srand(GetTickCount());
	int nRand = rand();
	GetSystemTime(&st);
	memset(m_chName,0,sizeof(NAME_BUF_LENGTH));
	sprintf(m_chName,"%d%d%d%d%d",st.wMilliseconds,st.wSecond,st.wHour,st.wMinute,m_nIndex);
	int len = strlen(m_chName);
	memcpy(&m_chName[len],&nRand,sizeof(DWORD));
}

void CDvrTnel::MakeLiveGram()
{
	EnterCriticalSection(&m_csGram);
	m_CmdGram.Info = m_LiveInfo;
	LeaveCriticalSection(&m_csGram);
	m_nKeepliveTick = 0;
}

BOOL CDvrTnel::SendGram()
{
	DWORD dwWait;
	HANDLE hEv[2];
	hEv[0] = m_hSendGram;
	hEv[1] = m_hStopWorking;
	while (TRUE) {
		dwWait = WaitForMultipleObjects(2,hEv,FALSE,INFINITE);
		if(WAIT_OBJECT_0 == dwWait) {
			EnterCriticalSection(&m_csGram);
			EnterCriticalSection(&g_csSocket);
			if(Link_Applying == m_LinkStatus) {
				sendto(m_sDVR,(char*)&m_ReqGram,sizeof(CLTREQ_GRAM),0,(SOCKADDR*)&m_NetSAddr,sizeof(SOCKADDR_IN));
			}
			else if(Link_Creating == m_LinkStatus) {
				sendto(m_sDVR,(char*)&m_OpenGram,sizeof(OPENTNEL),0,(SOCKADDR*)&m_DvrAddr,sizeof(SOCKADDR_IN));
			}
			else {
				sendto(m_sDVR,(char*)&m_CmdGram,sizeof(WORK_GRAM),0,(SOCKADDR*)&m_DvrAddr,sizeof(SOCKADDR_IN));
			}
			LeaveCriticalSection(&g_csSocket);
			LeaveCriticalSection(&m_csGram);
		}
		else {
			break;
		}
	}

	return TRUE;
}

UINT CDvrTnel::SendGramProc(LPVOID p)
{
	CDvrTnel* pDvr = (CDvrTnel*)p;
	pDvr->SendGram();
	pDvr->DecreaseThreadCount();

	return 1;
}
