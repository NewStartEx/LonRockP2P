#include "stdafx.h"
#include "NTTProtocol.h"
#include "Global.h"
#include "CltGlobal.h"
#include "DvrTnel.h"
#include "CltManager.h"

CRITICAL_SECTION g_csSocket;

CCltManager::CCltManager() :
m_bWorking(FALSE),
m_hMsgWnd(NULL),
m_uNetSAddr(INADDR_NONE),
m_wNetSPort(NETSERVER_PORT),
m_wDvrPort(DVR_PORT),
m_sRcvGram(INVALID_SOCKET),
m_nThreadCount(0)
{
	InitializeCriticalSection(&g_csSocket);
	m_wLocalPort = CLIENT_PORT;
	memset(&m_LocalAddr,0,sizeof(SOCKADDR_IN));
	m_LocalAddr.sin_family = AF_INET;
	m_LocalAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	m_LocalAddr.sin_port = htons(m_wLocalPort);

	m_hRcvGram = WSACreateEvent();
	m_hTnelGram = CreateEvent(NULL,FALSE,FALSE,NULL);
	m_hCnctGram = CreateEvent(NULL,FALSE,FALSE,NULL);
	m_hMsgGram = CreateEvent(NULL,FALSE,FALSE,NULL);
	m_hStopWorking = CreateEvent(NULL,TRUE,FALSE,NULL);
	InitializeCriticalSection(&m_csGram);
	InitializeCriticalSection(&m_csDvrArray);
	InitializeCriticalSection(&m_csRes);
}

CCltManager::~CCltManager()
{
	if(m_bWorking) {
		StopWorking();
	}
	int size = m_DvrTnelArray.GetSize();
	for(int i = 0; i < size; i++) {
		delete m_DvrTnelArray[i];
	}
	m_DvrTnelArray.RemoveAll();
	WSACloseEvent(m_hRcvGram);
	CloseHandle(m_hTnelGram);
	CloseHandle(m_hCnctGram);
	CloseHandle(m_hMsgGram);
	CloseHandle(m_hStopWorking);
	DeleteCriticalSection(&m_csGram);
	DeleteCriticalSection(&m_csDvrArray);
	DeleteCriticalSection(&m_csRes);
	DeleteCriticalSection(&g_csSocket);
}

BOOL CCltManager::StartWorking(HWND hWnd,LPCTSTR NetSName)
{
	m_hMsgWnd = hWnd;
	m_strNetSName = NetSName;
	m_uNetSAddr = Name2IP(m_strNetSName);
	ASSERT(INVALID_SOCKET == m_sRcvGram);
	int on = 1;
	m_sRcvGram = socket(AF_INET,SOCK_DGRAM,0);
	int count=0;
	do {
		if(SOCKET_ERROR != bind(m_sRcvGram, (SOCKADDR *FAR)&m_LocalAddr,sizeof(SOCKADDR_IN))) {
			break;
		}
		m_wLocalPort++;
		count++;
		m_LocalAddr.sin_port = htons(m_wLocalPort);

	} while (count < 100);
	if(count >= 100) {
		closesocket(m_sRcvGram);
		m_sRcvGram = INVALID_SOCKET;
		return FALSE;
	}
	if(SOCKET_ERROR == (WSAEventSelect(m_sRcvGram,m_hRcvGram,FD_READ))) {
		closesocket(m_sRcvGram);
		m_sRcvGram = INVALID_SOCKET;
		return FALSE;
	}

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

void CCltManager::StopWorking()
{
	m_bWorking = FALSE;
	SetEvent(m_hStopWorking);
	while (m_nThreadCount > 0) {
		Sleep(50);
	}
	closesocket(m_sRcvGram);
	m_sRcvGram = INVALID_SOCKET;
}

HANDLE CCltManager::StartLink(LPCTSTR ShowName,int nLinkType,PCMDINFO pFirstCmd,BYTE &Error)
{
	int len = strlen(ShowName);
	if(len < MIN_NAME_LENGTH) return NULL;
	EnterCriticalSection(&m_csDvrArray);
	int size = m_DvrTnelArray.GetSize();
	for(int i = 0; i < size; i++) {
		if(m_DvrTnelArray[i]->IsMyself(ShowName)) {
			break;
		}
	}
	LeaveCriticalSection(&m_csDvrArray);
	if(i < size) return NULL;  ///// 不可以使用相同的名称
	if(LINK_TUNNEL == nLinkType) {
		//// 每次都检查一次，避免因NetS更改而造成的错误
		//// 由于更改很少，所以不用互斥量保护
		m_uNetSAddr = Name2IP(m_strNetSName);
		if(INADDR_NONE == m_uNetSAddr) {
			Error = OBJECT_NETS_ERR;
			return NULL;
		}
		//// 这里要判断该名称是否已经存在。如果存在则不能再使用
		CDvrTnel *pDvr = new CDvrTnel(ShowName,m_uNetSAddr,m_wNetSPort);
		if(!pDvr->InitResource(m_sRcvGram,pFirstCmd)) {
			Error = OBJECT_DVR_ERR;
			delete pDvr;
			return NULL;
		}
		EnterCriticalSection(&m_csDvrArray);
		m_DvrTnelArray.Add(pDvr);
		LeaveCriticalSection(&m_csDvrArray);
		return pDvr;
	}
	else if(LINK_CMD == nLinkType) {
		CDvrTnel *pDvr = new CDvrTnel(ShowName,m_wDvrPort);
		if(!pDvr->InitResource(m_sRcvGram,pFirstCmd)) {
			Error = OBJECT_DVR_ERR;
			delete pDvr;
			return NULL;
		}
		EnterCriticalSection(&m_csDvrArray);
		m_DvrTnelArray.Add(pDvr);
		LeaveCriticalSection(&m_csDvrArray);
		return pDvr;
	}
	else {
		return NULL;
	}
}

void CCltManager::StopLink(LPCTSTR ShowName)
{
	int size,i;
	EnterCriticalSection(&m_csDvrArray);
	size = m_DvrTnelArray.GetSize();
	for(i = 0; i < size; i++) {
		if(m_DvrTnelArray[i]->IsMyself(ShowName)) {
			break;
		}
	}
	if(i < size) {
		delete m_DvrTnelArray[i];
		m_DvrTnelArray.RemoveAt(i);
	}
	LeaveCriticalSection(&m_csDvrArray);
}

void CCltManager::StopLink(HANDLE hDvr)
{
	int size,i;
	DWORD dw1;
	dw1 = (DWORD)hDvr;
	EnterCriticalSection(&m_csDvrArray);
	size = m_DvrTnelArray.GetSize();
	for(i = 0; i < size; i++) {
		if(dw1 == (DWORD)m_DvrTnelArray[i]) {
			break;
		}
	}
	if(i < size) {
		delete m_DvrTnelArray[i];
		m_DvrTnelArray.RemoveAt(i);
	}
	LeaveCriticalSection(&m_csDvrArray);
}

BOOL CCltManager::SendCmd(LPCTSTR ShowName,PCMDINFO pCmd)
{
	int size,i;
	BOOL bRe = TRUE;
	EnterCriticalSection(&m_csDvrArray);
	size = m_DvrTnelArray.GetSize();
	for(i = 0; i < size; i++) {
		if(m_DvrTnelArray[i]->IsMyself(ShowName)) {
			break;
		}
	}
	if(i < size) {
		m_DvrTnelArray[i]->SetCmdGram(pCmd);
		bRe = TRUE;
	}
	else {
		bRe = FALSE;
	}
	LeaveCriticalSection(&m_csDvrArray);
	return bRe;
}

BOOL CCltManager::SendCmd(HANDLE hDvr,PCMDINFO pCmd)
{
	int size,i;
	DWORD dw1;
	dw1 = (DWORD)hDvr;
	BOOL bRe = TRUE;
	EnterCriticalSection(&m_csDvrArray);
	size = m_DvrTnelArray.GetSize();
	for(i = 0; i < size; i++) {
		if(dw1 == (DWORD)m_DvrTnelArray[i]) {
			break;
		}
	}
	if(i < size) {
		m_DvrTnelArray[i]->SetCmdGram(pCmd);
		bRe = TRUE;
	}
	else {
		bRe = FALSE;
	}
	LeaveCriticalSection(&m_csDvrArray);

	return bRe;
}

LPCTSTR CCltManager::GetDvrName(HANDLE hDvr)
{
	CDvrTnel *pDvr = (CDvrTnel*)hDvr;
	return pDvr->GetDvrName();
}

UINT CCltManager::DealwithTnelGram()
{
	TNELMSG_GRAM TnelMsgGram;
	CDvrTnel *pDvr;
	int count = m_TnelMsgArray.GetSize();
	char chDvrName[NAME_BUF_LENGTH];
	while (count > 0) {
		EnterCriticalSection(&m_csGram);
		TnelMsgGram = m_TnelMsgArray[0];
		m_TnelMsgArray.RemoveAt(0);
		LeaveCriticalSection(&m_csGram);
		count = m_TnelMsgArray.GetSize();
		if(!LegalTnel(&TnelMsgGram,chDvrName)) {
			continue;
		}
		TRACE("Clt Tnel:%s",chDvrName);
		EnterCriticalSection(&m_csDvrArray);
		pDvr = GetDvr(chDvrName);
		if(NULL != pDvr) {
			pDvr->DealwithTnelGram(&TnelMsgGram);
		}
		LeaveCriticalSection(&m_csDvrArray);
	}
	return 1;
}

UINT CCltManager::DealwithCnctGram()
{
	CNCTDESC CnctDesc;
	CDvrTnel *pDvr;
	int count = m_CnctDescArray.GetSize();
	char chDvrName[NAME_BUF_LENGTH];
	while (count > 0) {
		EnterCriticalSection(&m_csGram);
		CnctDesc = m_CnctDescArray[0];
		m_CnctDescArray.RemoveAt(0);
		LeaveCriticalSection(&m_csGram);
		count = m_CnctDescArray.GetSize();
		memset(chDvrName,0,NAME_BUF_LENGTH);
		if(!LegalCnct(&CnctDesc.OpenGram,chDvrName)) {
			continue;
		}
		TRACE("Clt Open:%s",chDvrName);
		EnterCriticalSection(&m_csDvrArray);
		//// 这里可以判断是否有重复的出现，在这个软件里是不可能的，但是如果有，可以在这里进行处理
		pDvr = GetDvr(chDvrName);
		if(NULL != pDvr) {
			pDvr->DealwithCnctGram(&CnctDesc);
		}
		LeaveCriticalSection(&m_csDvrArray);
	}
	return 1;
}

UINT CCltManager::DealwithMsgGram()
{
	WORK_GRAM WorkMsgGram;
	CDvrTnel *pDvr;
	int count = m_MsgGramArray.GetSize();
	char chDvrName[NAME_BUF_LENGTH],chCltName[NAME_BUF_LENGTH];
	while (count > 0) {
		EnterCriticalSection(&m_csGram);
		WorkMsgGram = m_MsgGramArray[0];
		m_MsgGramArray.RemoveAt(0);
		LeaveCriticalSection(&m_csGram);
		count = m_MsgGramArray.GetSize();
		if(!LegalMsg(&WorkMsgGram,chDvrName,chCltName)) {
			continue;
		}
		TRACE("Clt: Msg %s:%s",chDvrName,chCltName);
		EnterCriticalSection(&m_csDvrArray);
		pDvr = GetDvr(chDvrName);
		if(NULL != pDvr) {
			pDvr->DealwithMsgGram(&WorkMsgGram.Info,chCltName);
		}
		LeaveCriticalSection(&m_csDvrArray);
	}
	return 1;
}

BOOL CCltManager::LegalTnel(PTNELMSG_GRAM pTnel,char *pDvrName)
{
	if(FOURCC_NETS != pTnel->dwType) {
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
	strcpy(pDvrName,chTmp);
	
	return TRUE;
}

BOOL CCltManager::LegalCnct(POPENTNEL pOpen,char *pDvrName)
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
	if(!ThrowRandom(chTmp,pOpen->chFirstName,&nLength,NAME_BUF_LENGTH,NAME_BUF_LENGTH,RANDOM_TRANS_NAMEKEY)) {
		return FALSE;
	}
	if(nLength > MAX_NAME_LENGTH || nLength < MIN_NAME_LENGTH) {
		return FALSE;
	}
	strcpy(pDvrName,chTmp);

	return TRUE;
}

BOOL CCltManager::LegalMsg(PWORK_GRAM pMsg,char *pDvrName,char *pCltName)
{
	if(pMsg->dwType != FOURCC_PUSH) return FALSE;
	char chTmp[NAME_BUF_LENGTH];
	int nLength = 0;
	if(!ThrowRandom(chTmp,pMsg->chLocalName,&nLength,NAME_BUF_LENGTH,NAME_BUF_LENGTH,RANDOM_TRANS_NAMEKEY)) {
		return FALSE;
	}
	if(nLength > MAX_NAME_LENGTH || nLength < MIN_NAME_LENGTH) {
		return FALSE;
	}
	strcpy(pDvrName,chTmp);
	if(!ThrowRandom(chTmp,pMsg->chDestName,&nLength,NAME_BUF_LENGTH,NAME_BUF_LENGTH,RANDOM_TRANS_NAMEKEY)) {
		return FALSE;
	}
	if(nLength > MAX_NAME_LENGTH || nLength < MIN_NAME_LENGTH) {
		return FALSE;
	}
	strcpy(pCltName,chTmp);

	return TRUE;
}

CDvrTnel* CCltManager::GetDvr(char* Name)
{
	int count = m_DvrTnelArray.GetSize();
	CString strDvrName = Name;
	for(int i = 0; i < count; i++) {
		if(m_DvrTnelArray[i]->FindDvr(strDvrName)) 
			break;
	}
	if(i == count) return NULL;
	else return m_DvrTnelArray[i];

}

UINT CCltManager::KeepRcving()
{
	ASSERT(m_sRcvGram != INVALID_SOCKET);
	SOCKADDR_IN DestAddr;
	DWORD dwWait;
	TNELMSG_GRAM TnelMsgGram;
	CNCTDESC CnctDesc;
	WORK_GRAM  MsgGram;
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
			//// EnterCriticalSection(&g_csSocket);接收和发送不冲突
			nRcvLength = recvfrom(m_sRcvGram,rcvBuf,DVR_RCV_BUF_LENGTH,0,(SOCKADDR*)&DestAddr,&len);
			//// LeaveCriticalSection(&g_csSocket);
			if(sizeof(TNELMSG_GRAM) == nRcvLength) {
				if(DestAddr.sin_addr.s_addr != m_uNetSAddr) continue; //// 端口可能因为NAT而不同
				//// Tunnel applying Gram
				memcpy(&TnelMsgGram,rcvBuf,sizeof(TNELMSG_GRAM));
				EnterCriticalSection(&m_csGram);
				m_TnelMsgArray.Add(TnelMsgGram);
				LeaveCriticalSection(&m_csGram);
				SetEvent(m_hTnelGram);
			}
			else if(sizeof(OPENTNEL) == nRcvLength) {
				//// Client's Open Tunnel Gram
				memcpy(&CnctDesc.OpenGram,rcvBuf,sizeof(OPENTNEL));
				CnctDesc.addr = DestAddr;
				EnterCriticalSection(&m_csGram);
				m_CnctDescArray.Add(CnctDesc);
				LeaveCriticalSection(&m_csGram);
				SetEvent(m_hCnctGram);
			}
			else if(sizeof(WORK_GRAM) == nRcvLength) {
				//// Dvr's Message
				memcpy(&MsgGram,rcvBuf,sizeof(WORK_GRAM));
				EnterCriticalSection(&m_csGram);
				m_MsgGramArray.Add(MsgGram);
				LeaveCriticalSection(&m_csGram);
				SetEvent(m_hMsgGram);
			}
			else {
				//// 莫名数据,这里不处理重传数据。
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

UINT CCltManager::DistribGram()
{
	DWORD dwWait;
	HANDLE hEv[4];
	
	hEv[0] = m_hTnelGram;
	hEv[1] = m_hCnctGram;
	hEv[2] = m_hMsgGram;
	hEv[3] = m_hStopWorking;
	while (m_bWorking) {
		dwWait = WaitForMultipleObjects(4,hEv,FALSE,INFINITE); 
		if(WAIT_OBJECT_0 == dwWait) {
			DealwithTnelGram();
		}
		else if((WAIT_OBJECT_0+1) == dwWait) {
			DealwithCnctGram();
		}
		else if((WAIT_OBJECT_0+2) == dwWait) {
			DealwithMsgGram();
		}
		else if((WAIT_OBJECT_0+3) == dwWait) {
			break;
		}
		else {
			TRACE("Handle Gram Thread Impossible Happen");
		}
	}
	return 1;
}

UINT CCltManager::CheckOver()
{
	DWORD dwWait;
	while (m_bWorking) {
		dwWait = WaitForSingleObject(m_hStopWorking,CHECK_OVER_INTERVAL);
		if(WAIT_TIMEOUT == dwWait) {
			///// 其实是一个函数体
			{
				EnterCriticalSection(&m_csDvrArray);
				BOOL bExist;
				int index = 0;
				int count = m_DvrTnelArray.GetSize();
				while (index < count) {
					bExist = m_DvrTnelArray[index]->SelfChecking();
					if(bExist) {
						index ++;
					}
					else {
						delete m_DvrTnelArray[index];
						m_DvrTnelArray.RemoveAt(index);
						count--;
					}
				}
				LeaveCriticalSection(&m_csDvrArray);
			}
		}
		else {
			break;
		}
	}
	return 1;
}

UINT CCltManager::RcvGramProc(LPVOID p)
{
	UINT uRe;
	CCltManager* pManager = (CCltManager*)p;
	uRe = pManager->KeepRcving();
	pManager->DecreaseThreadCount();

	return uRe;
}

UINT CCltManager::DistribGramProc(LPVOID p)
{
	UINT uRe;
	CCltManager* pManager = (CCltManager*)p;
	uRe = pManager->DistribGram();
	pManager->DecreaseThreadCount();

	return uRe;
}

UINT CCltManager::CheckOverProc(LPVOID p)
{
	UINT uRe;
	CCltManager* pManager = (CCltManager*)p;
	uRe = pManager->CheckOver();
	pManager->DecreaseThreadCount();

	return uRe;
}
