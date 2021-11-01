#include "stdafx.h"
#include "NTTProtocol.h"
#include "Global.h"
#include "CltGlobal.h"
#include "DvrTnel.h"
#include "CltManager.h"
#include "NTTClientFun.h"
#include "MemVicocDecFun.h"

CCltManager::CCltManager() :
m_bWorking(FALSE),
m_uNetSAddr(INADDR_NONE),
m_wNetSPort(NETSERVER_PORT),
m_sGram(INVALID_SOCKET),
m_nThreadCount(0)
{
	m_wLocalPort = CLIENT_PORT;
	memset(&m_LocalAddr,0,sizeof(SOCKADDR_IN));
	m_LocalAddr.sin_family = AF_INET;
	m_LocalAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	m_LocalAddr.sin_port = htons(m_wLocalPort);
	ASSERT(RCV_BUF_LENGTH > INTERNET_MUP_LENGTH);
	for(int i = 0; i < GRAM_BUFFER_COUNT; i++) {
		m_GramDesc[i].nLength = 0;
		m_GramDesc[i].pBuf = new char[RCV_BUF_LENGTH];
		memset(m_GramDesc[i].pBuf,0,RCV_BUF_LENGTH);
	}
	m_nReadSDataIndex = m_nWriteSDataIndex = 0;
	m_bSendBufFull = FALSE;
	for(i = 0; i < SENDDATA_BUFFER_COUNT; i++) {
		m_SendDataDesc[i].nLength = 0;
		m_SendDataDesc[i].pBuf = new char[SEND_BUF_LENGTH];
		memset(m_SendDataDesc[i].pBuf,0,SEND_BUF_LENGTH);
	}

	m_nReadGramIndex = m_nWriteGramIndex = 0;
	m_hRcvGram = WSACreateEvent();
	m_hDealwithGram = CreateEvent(NULL,FALSE,FALSE,NULL);
	m_hSendData = CreateEvent(NULL,FALSE,FALSE,NULL);
	m_hStopWorking = CreateEvent(NULL,TRUE,FALSE,NULL);
	InitializeCriticalSection(&m_csDvrArray);
	InitializeCriticalSection(&m_csRes);
	InitializeCriticalSection(&m_csSData);
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
	for(i = 0; i < GRAM_BUFFER_COUNT; i++) {
		delete m_GramDesc[i].pBuf;
	}
	for(i = 0; i < SENDDATA_BUFFER_COUNT; i++) {
		delete m_SendDataDesc[i].pBuf;
	}
	WSACloseEvent(m_hRcvGram);
	CloseHandle(m_hDealwithGram);
	CloseHandle(m_hSendData);
	CloseHandle(m_hStopWorking);
	DeleteCriticalSection(&m_csDvrArray);
	DeleteCriticalSection(&m_csRes);
	DeleteCriticalSection(&m_csSData);
}

BOOL CCltManager::StartWorking(LPCTSTR NetSName)
{
	m_strNetSName = NetSName;
	m_uNetSAddr = Name2IP(m_strNetSName);
	ASSERT(INVALID_SOCKET == m_sGram);
	int on = 1;
	m_sGram = socket(AF_INET,SOCK_DGRAM,0);
	int count=0;
	do {
		if(SOCKET_ERROR != bind(m_sGram, (SOCKADDR *FAR)&m_LocalAddr,sizeof(SOCKADDR_IN))) {
			break;
		}
		m_wLocalPort++;
		count++;
		m_LocalAddr.sin_port = htons(m_wLocalPort);
	} while (count < 100);
	
	if(count >= 100) {
		closesocket(m_sGram);
		m_sGram = INVALID_SOCKET;
		return FALSE;
	}
	if(SOCKET_ERROR == (WSAEventSelect(m_sGram,m_hRcvGram,FD_READ))) {
		closesocket(m_sGram);
		m_sGram = INVALID_SOCKET;
		return FALSE;
	}

#define SOCK_RCVBUF 65536
	//// 设置发送缓冲为64K.默认应该是8K
	int nBufSize = SOCK_RCVBUF,nOptLen = sizeof(int);
	int nRe = setsockopt(m_sGram,SOL_SOCKET,SO_RCVBUF,(CHAR*)&nBufSize,nOptLen);
	if(0 == nRe) {
		getsockopt(m_sGram,SOL_SOCKET,SO_RCVBUF,(CHAR*)&nBufSize,&nOptLen);
		if(nBufSize != SOCK_RCVBUF) {
			nBufSize = 8196; //// default value
			setsockopt(m_sGram,SOL_SOCKET,SO_RCVBUF,(CHAR*)&nBufSize,nOptLen);
		}
	}

	m_bWorking = TRUE;
	DWORD dwID;
	IncreaseThreadCount();
	CreateThread(NULL,0,(LPTHREAD_START_ROUTINE)SendGramProc,this,0,&dwID);
	IncreaseThreadCount();
	CreateThread(NULL,0,(LPTHREAD_START_ROUTINE)DistribGramProc,this,0,&dwID);
	IncreaseThreadCount();
	HANDLE hRcv = CreateThread(NULL,0,(LPTHREAD_START_ROUTINE)RcvGramProc,this,0,&dwID);
	SetThreadPriority(hRcv,THREAD_PRIORITY_HIGHEST);
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
	closesocket(m_sGram);
	m_sGram = INVALID_SOCKET;
	m_nReadGramIndex = m_nWriteGramIndex = 0;
	m_nReadSDataIndex = m_nWriteSDataIndex = 0;
	for(int i = 0; i < GRAM_BUFFER_COUNT; i++) {
		m_GramDesc[i].nLength = 0;
	}
}

HANDLE CCltManager::StartLink(LPCTSTR ShowName,int nLinkType,PCMDINFO pFirstCmd,WORD wDvrPort,BYTE &Error)
{
	if(LINK_TUNNEL == nLinkType) {
		int len = strlen(ShowName);
		if(len < MIN_NAME_LENGTH || len > MAX_NAME_LENGTH) return NULL;
	}
	
	EnterCriticalSection(&m_csDvrArray);
	int size = m_DvrTnelArray.GetSize();
	for(int i = 0; i < size; i++) {
		if(m_DvrTnelArray[i]->IsMyself(ShowName)) {
			break;
		}
	} 
	LeaveCriticalSection(&m_csDvrArray);
	TRACE("Tnel i:%d,size:%d",i,size);
	if(i < size) return NULL;  ///// 不可以使用相同的名称
	TRACE("Start Link");
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
		if(!pDvr->InitResource(pFirstCmd)) {
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
		CDvrTnel *pDvr = new CDvrTnel(ShowName,wDvrPort);
		if(!pDvr->InitResource(pFirstCmd)) {
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

void CCltManager::StopLink(LPCTSTR ShowName,BOOL bInCallback)
{
	int size,i;
	if(!bInCallback)	EnterCriticalSection(&m_csDvrArray);
	size = m_DvrTnelArray.GetSize();
	for(i = 0; i < size; i++) {
		if(m_DvrTnelArray[i]->IsMyself(ShowName)) {
			break;
		}
	}
	if(i < size) {
		m_DvrTnelArray[i]->SetExitMark(EXIT_FORCE_EXIT);
	}
	if(!bInCallback) LeaveCriticalSection(&m_csDvrArray);
}

void CCltManager::StopLink(HANDLE hDvr,BOOL bInCallback)
{
	int size,i;
	DWORD dw1;
	dw1 = (DWORD)hDvr;
	if(!bInCallback) EnterCriticalSection(&m_csDvrArray);
	size = m_DvrTnelArray.GetSize();
	for(i = 0; i < size; i++) {
		if(dw1 == (DWORD)m_DvrTnelArray[i]) {
			break;
		}
	}
	if(i < size) {
		m_DvrTnelArray[i]->SetExitMark(EXIT_FORCE_EXIT);
	}
	if(!bInCallback) LeaveCriticalSection(&m_csDvrArray);
}

BOOL CCltManager::SendCmd(LPCTSTR ShowName,PCMDINFO pCmd,BOOL bInCallback)
{
	int size,i;
	BOOL bRe = TRUE;
	CDvrTnel *pDvr = NULL;
	
	if(!bInCallback) EnterCriticalSection(&m_csDvrArray);
	size = m_DvrTnelArray.GetSize();
	for(i = 0; i < size; i++) {
		if(m_DvrTnelArray[i]->IsMyself(ShowName)) {
			m_DvrTnelArray[i]->SetAlive();
			pDvr = m_DvrTnelArray[i];
			break;
		}
	}
	if(!bInCallback) LeaveCriticalSection(&m_csDvrArray);
	if(NULL != pDvr) {
		if(CMD_SUCCESS==pDvr->SetCmdGram(pCmd))
			bRe = TRUE;
		else
			bRe = FALSE;
	}
	else {
		bRe = FALSE;
	}
	return bRe;
}

BOOL CCltManager::SendCmd(HANDLE hDvr,PCMDINFO pCmd,BOOL bInCallback)
{
	int size,i;
	DWORD dw1;
	dw1 = (DWORD)hDvr;
	BOOL bRe = TRUE;
	CDvrTnel *pDvr = NULL;
	if(!bInCallback) EnterCriticalSection(&m_csDvrArray);
	size = m_DvrTnelArray.GetSize();
	for(i = 0; i < size; i++) {
		if(dw1 == (DWORD)m_DvrTnelArray[i]) {
			m_DvrTnelArray[i]->SetAlive();
			pDvr = m_DvrTnelArray[i];
			break;
		}
	}
	if(!bInCallback) LeaveCriticalSection(&m_csDvrArray);
	if(NULL != pDvr) {
		pDvr->SetCmdGram(pCmd);
		bRe = TRUE;
	}
	else {
		bRe = FALSE;
	}

	return bRe;
}

BOOL CCltManager::CreateVideoResource(LPCTSTR ShowName,int nChanCount,BOOL bInCallback)
{
	int size;
	BOOL bRe = TRUE;
	CDvrTnel *pDvr = NULL;
	if(!bInCallback) EnterCriticalSection(&m_csDvrArray);
	size = m_DvrTnelArray.GetSize();
	for(int i = 0; i < size; i++) {
		if(m_DvrTnelArray[i]->IsMyself(ShowName)) {
			m_DvrTnelArray[i]->SetAlive();
			pDvr = m_DvrTnelArray[i];
			break;
		}
	}
	if(!bInCallback) LeaveCriticalSection(&m_csDvrArray);
	if(NULL != pDvr) {
		pDvr->CreateVideoChannel(nChanCount);
		bRe = TRUE;
	}
	else {
		bRe = FALSE;
	}

	return bRe;
}

int CCltManager::GetVideoChannelCount(LPCTSTR ShowName,BOOL bInCallback)
{
	int size,chan,i;
	CDvrTnel *pDvr = NULL;
	if(!bInCallback) EnterCriticalSection(&m_csDvrArray);
	size = m_DvrTnelArray.GetSize();
	for(i = 0; i < size; i++) {
		if(m_DvrTnelArray[i]->IsMyself(ShowName)) {
			m_DvrTnelArray[i]->SetAlive();
			pDvr = m_DvrTnelArray[i];
			break;
		}
	}
	if(!bInCallback) LeaveCriticalSection(&m_csDvrArray);
	if(NULL != pDvr) {
		chan = pDvr->GetChannelCount();
	}
	else {
		chan = -1;
	}

	return chan;
}

BOOL CCltManager::StartVideoTran(LPCTSTR ShowName,int chan,BOOL bInCallback)
{
	int size,i;
	BOOL bRe = TRUE;
	CDvrTnel *pDvr = NULL;
	if(!bInCallback) EnterCriticalSection(&m_csDvrArray);
	size = m_DvrTnelArray.GetSize();
	for(i = 0; i < size; i++) {
		if(m_DvrTnelArray[i]->IsMyself(ShowName)) {
			m_DvrTnelArray[i]->SetAlive();
			pDvr = m_DvrTnelArray[i];
			break;
		}
	}
	if(!bInCallback) LeaveCriticalSection(&m_csDvrArray);
	if(NULL != pDvr) {
		pDvr->StartVideoTran(chan);
		bRe = TRUE;
	}
	else {
		bRe = FALSE;
	}

	return bRe;
}

BOOL CCltManager::StartAllVideoTran(LPCTSTR ShowName,BOOL bInCallback)
{
	int size,i;
	BOOL bRe = TRUE;
	CDvrTnel *pDvr = NULL;
	if(!bInCallback) EnterCriticalSection(&m_csDvrArray);
	size = m_DvrTnelArray.GetSize();
	for(i = 0; i < size; i++) {
		if(m_DvrTnelArray[i]->IsMyself(ShowName)) {
			m_DvrTnelArray[i]->SetAlive();
			pDvr = m_DvrTnelArray[i];
			break;
		}
	}
	if(!bInCallback) LeaveCriticalSection(&m_csDvrArray);
	if(NULL != pDvr) {
		pDvr->StartAllVideoTran();
		bRe = TRUE;
	}
	else {
		bRe = FALSE;
	}

	return bRe;
}

BOOL CCltManager::StopVideoTran(LPCTSTR ShowName,int chan,BOOL bInCallback)
{
	int size,i;
	BOOL bRe = TRUE;
	CDvrTnel *pDvr = NULL;
	if(!bInCallback) EnterCriticalSection(&m_csDvrArray);
	size = m_DvrTnelArray.GetSize();
	for(i = 0; i < size; i++) {
		if(m_DvrTnelArray[i]->IsMyself(ShowName)) {
			m_DvrTnelArray[i]->SetAlive();
			pDvr = m_DvrTnelArray[i];
			break;
		}
	}
	if(!bInCallback) LeaveCriticalSection(&m_csDvrArray);
	if(NULL != pDvr) {
		pDvr->StopVideoTran(chan);
		bRe = TRUE;
	}
	else {
		bRe = FALSE;
	}

	return bRe;
}

BOOL CCltManager::StopAllVideoTran(LPCTSTR ShowName,BOOL bInCallback)
{
	int size,i;
	BOOL bRe = TRUE;
	CDvrTnel *pDvr = NULL;
	if(!bInCallback) EnterCriticalSection(&m_csDvrArray);
	size = m_DvrTnelArray.GetSize();
	for(i = 0; i < size; i++) {
		if(m_DvrTnelArray[i]->IsMyself(ShowName)) {
			m_DvrTnelArray[i]->SetAlive();
			pDvr = m_DvrTnelArray[i];
			break;
		}
	}
	if(!bInCallback) LeaveCriticalSection(&m_csDvrArray);
	if(NULL != pDvr) {
		pDvr->StopAllVideoTran();
		bRe = TRUE;
	}
	else {
		bRe = FALSE;
	}

	return bRe;
}

BOOL CCltManager::StartRecord(LPCTSTR ShowName,int chan,LPCTSTR FileName,BOOL bInCallback)
{
	int size,i;
	BOOL bRe = TRUE;
	CDvrTnel *pDvr = NULL;
	if(!bInCallback) EnterCriticalSection(&m_csDvrArray);
	size = m_DvrTnelArray.GetSize();
	for(i = 0; i < size; i++) {
		if(m_DvrTnelArray[i]->IsMyself(ShowName)) {
			m_DvrTnelArray[i]->SetAlive();
			pDvr = m_DvrTnelArray[i];
			break;
		}
	}
	if(!bInCallback) LeaveCriticalSection(&m_csDvrArray);
	if(NULL != pDvr) {
		bRe = pDvr->StartRecord(chan,FileName);
	}
	else {
		bRe = FALSE;
	}

	return bRe;
}

BOOL CCltManager::StopRecord(LPCTSTR ShowName,int chan,BOOL bInCallback)
{
	int size,i;
	BOOL bRe = TRUE;
	CDvrTnel *pDvr = NULL;
	if(!bInCallback) EnterCriticalSection(&m_csDvrArray);
	size = m_DvrTnelArray.GetSize();
	for(i = 0; i < size; i++) {
		if(m_DvrTnelArray[i]->IsMyself(ShowName)) {
			m_DvrTnelArray[i]->SetAlive();
			pDvr = m_DvrTnelArray[i];
			break;
		}
	}
	if(!bInCallback) LeaveCriticalSection(&m_csDvrArray);
	if(NULL != pDvr) {
		bRe = pDvr->StopRecord(chan);
	}
	else {
		bRe = FALSE;
	}

	return bRe;
}

BOOL CCltManager::Snap(LPCTSTR ShowName,int chan,LPCTSTR FileName,BOOL bInCallback)
{
	int size,i;
	BOOL bRe = TRUE;
	CDvrTnel *pDvr = NULL;
	if(!bInCallback) EnterCriticalSection(&m_csDvrArray);
	size = m_DvrTnelArray.GetSize();
	for(i = 0; i < size; i++) {
		if(m_DvrTnelArray[i]->IsMyself(ShowName)) {
			m_DvrTnelArray[i]->SetAlive();
			pDvr = m_DvrTnelArray[i];
			break;
		}
	}
	if(!bInCallback) LeaveCriticalSection(&m_csDvrArray);
	if(NULL != pDvr) {
		bRe = pDvr->Snap(chan,FileName);
	}
	else {
		bRe = FALSE;
	}

	return bRe;
}

DWORD CCltManager::GetRecFilePos(LPCTSTR ShowName,int chan,BOOL bInCallback)
{
	int size,i;
	DWORD dwRe = 0;
	CDvrTnel *pDvr = NULL;
	if(!bInCallback) EnterCriticalSection(&m_csDvrArray);
	size = m_DvrTnelArray.GetSize();
	for(i = 0; i < size; i++) {
		if(m_DvrTnelArray[i]->IsMyself(ShowName)) {
			m_DvrTnelArray[i]->SetAlive();
			pDvr = m_DvrTnelArray[i];
			break;
		}
	}
	if(!bInCallback) LeaveCriticalSection(&m_csDvrArray);
	if(NULL != pDvr) {
		dwRe = pDvr->GetRecFilePos(chan);
	}
	else {
		dwRe = 0;
	}

	return dwRe;
}

BOOL CCltManager::SetVideoPos(LPCTSTR ShowName,int chan,int left,int top,int width,int height,BOOL bInCallback)
{
	int size,i;
	BOOL bRe = TRUE;
	CDvrTnel *pDvr = NULL;
	if(!bInCallback) EnterCriticalSection(&m_csDvrArray);
	size = m_DvrTnelArray.GetSize();
	for(i = 0; i < size; i++) {
		if(m_DvrTnelArray[i]->IsMyself(ShowName)) {
			m_DvrTnelArray[i]->SetAlive();
			pDvr = m_DvrTnelArray[i];
			break;
		}
	}
	if(!bInCallback) LeaveCriticalSection(&m_csDvrArray);
	if(NULL != pDvr) {
		pDvr->SetVideoPos(chan,left,top,width,height);
		bRe = TRUE;
	}
	else {
		bRe = FALSE;
	}
	
	return bRe;
}

BOOL CCltManager::RefreshVideo(LPCTSTR ShowName,int chan,BOOL bInCallback)
{
	int size,i;
	BOOL bRe = TRUE;
	CDvrTnel *pDvr = NULL;
	if(!bInCallback) EnterCriticalSection(&m_csDvrArray);
	size = m_DvrTnelArray.GetSize();
	for(i = 0; i < size; i++) {
		if(m_DvrTnelArray[i]->IsMyself(ShowName)) {
			m_DvrTnelArray[i]->SetAlive();
			pDvr = m_DvrTnelArray[i];
			break;
		}
	}
	if(!bInCallback) LeaveCriticalSection(&m_csDvrArray);
	if(NULL != pDvr) {
		pDvr->RefreshVideo(chan);
		bRe = TRUE;
	}
	else {
		bRe = FALSE;
	}

	return bRe;
}

BOOL CCltManager::ShowVideo(LPCTSTR ShowName,int chan,BOOL bShow,BOOL bInCallback)
{
	int size,i;
	BOOL bRe = TRUE;
	CDvrTnel *pDvr = NULL;
	if(!bInCallback) EnterCriticalSection(&m_csDvrArray);
	size = m_DvrTnelArray.GetSize();
	for(i = 0; i < size; i++) {
		if(m_DvrTnelArray[i]->IsMyself(ShowName)) {
			m_DvrTnelArray[i]->SetAlive();
			pDvr = m_DvrTnelArray[i];
			break;
		}
	}
	if(!bInCallback) LeaveCriticalSection(&m_csDvrArray);
	if(NULL != pDvr) {
		pDvr->ShowVideo(chan,bShow);
		bRe = TRUE;
	}
	else {
		bRe = FALSE;
	}

	return bRe;
}

LPCTSTR CCltManager::GetDvrName(HANDLE hDvr)
{
	CDvrTnel *pDvr = (CDvrTnel*)hDvr;
	return pDvr->GetDvrName();
}

UINT CCltManager::DealwithTnelGram(PTNELMSG_GRAM pTnel)
{
	CDvrTnel *pDvr;
	char chDvrName[NAME_BUF_LENGTH];
	if(!LegalTnel(pTnel,chDvrName)) {
		return 0;
	}
	TRACE("Clt Tnel:%s",chDvrName);
	EnterCriticalSection(&m_csDvrArray);
	pDvr = GetDvr(chDvrName);
	LeaveCriticalSection(&m_csDvrArray);
	if(NULL != pDvr) {
		pDvr->DealwithTnelGram(pTnel);
	}
	return 1;
}

UINT CCltManager::DealwithCnctGram(POPENTNEL pOpen,SOCKADDR_IN IPAddr)
{
	CDvrTnel *pDvr;
	char chDvrName[NAME_BUF_LENGTH];
	memset(chDvrName,0,NAME_BUF_LENGTH);
	if(!LegalCnct(pOpen,chDvrName)) {
		return 0;
	}
	TRACE("Clt Open:%s",chDvrName);
	EnterCriticalSection(&m_csDvrArray);
	//// 这里可以判断是否有重复的出现，在这个软件里是不可能的，但是如果有，可以在这里进行处理
	pDvr = GetDvr(chDvrName);
	LeaveCriticalSection(&m_csDvrArray);
	if(NULL != pDvr) {
		pDvr->DealwithCnctGram(pOpen,IPAddr);
	}
	
	return 1;
}

UINT CCltManager::DealwithHshkACK(PHSHK_GRAM pHshk)
{
	CDvrTnel *pDvr;
	char chDvrName[NAME_BUF_LENGTH],chCltName[NAME_BUF_LENGTH];
	if(!LegalHshkACK(pHshk,chDvrName,chCltName)) {
		return 0;
	}
	//// TRACE("Clt: Msg %s:%s",chDvrName,chCltName);
	EnterCriticalSection(&m_csDvrArray);
	pDvr = GetDvr(chDvrName);
	LeaveCriticalSection(&m_csDvrArray);
	if(NULL != pDvr) {
		pDvr->DealwithHshkACK(pHshk->byAct,chCltName);
	}
	
	return 1;
}

UINT CCltManager::DealwithMsgGram(PWORK_GRAM pMsg)
{
	CDvrTnel *pDvr;
	char chDvrName[NAME_BUF_LENGTH],chCltName[NAME_BUF_LENGTH];
	if(!LegalMsg(pMsg,chDvrName,chCltName)) {
		return 0;
	}
	//// TRACE("Clt: Msg %s:%s",chDvrName,chCltName);
	EnterCriticalSection(&m_csDvrArray);
	pDvr = GetDvr(chDvrName);
	LeaveCriticalSection(&m_csDvrArray);
	if(NULL != pDvr) {
		pDvr->DealwithMsgGram(&pMsg->Info,chCltName);
	}
	
	return 1;
}

UINT CCltManager::DealwithDataGram(PVOID pDataBuf)
{	
	CDvrTnel *pDvr;
	PVIDEO_GRAM pVGram;
	PDATA_GRAM_HEADER pDataHeader;
	char chDvrName[NAME_BUF_LENGTH],chCltName[NAME_BUF_LENGTH];
	memset(chDvrName,0,NAME_BUF_LENGTH);
	memset(chCltName,0,NAME_BUF_LENGTH);
	pDataHeader = (PDATA_GRAM_HEADER)pDataBuf;
	//// TRACE("get Data Gram");
	if(!LegalData(pDataHeader,chDvrName,chCltName)) {
		return 0;
	}
	if(FOURCC_VIDE == pDataHeader->dwType) {
		EnterCriticalSection(&m_csDvrArray);
		pDvr = GetDvr(chDvrName);
		LeaveCriticalSection(&m_csDvrArray);
		if(NULL != pDvr) {
			if(pDataHeader->byAct < VIDEO_ACTVERSION) {
				pDvr->SetExitMark(EXIT_VER_OLD);
			}
			else {
				pVGram = (PVIDEO_GRAM)pDataBuf;
				pDvr->DealwithVideoGram(&pVGram->vHeader,pVGram->vData,chCltName);
			}
		}
	}
	if(FOURCC_NWID == pDataHeader->dwType) {
		EnterCriticalSection(&m_csDvrArray);
		pDvr = GetDvr(chDvrName);
		LeaveCriticalSection(&m_csDvrArray);
		if(NULL != pDvr) {
			pDvr->DealwithNaviData(chCltName);
		}
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

BOOL CCltManager::LegalHshkACK(PHSHK_GRAM pHshkACK,char *pDvrName,char *pCltName)
{
	if(pHshkACK->dwType != FOURCC_HSHK) return FALSE;
	char chTmp[NAME_BUF_LENGTH];
	int nLength = 0;
	if(!ThrowRandom(chTmp,pHshkACK->chLocalName,&nLength,NAME_BUF_LENGTH,NAME_BUF_LENGTH,RANDOM_TRANS_NAMEKEY)) {
		return FALSE;
	}
	if(nLength > MAX_NAME_LENGTH || nLength < MIN_NAME_LENGTH) {
		return FALSE;
	}
	strcpy(pDvrName,chTmp);
	if(!ThrowRandom(chTmp,pHshkACK->chDestName,&nLength,NAME_BUF_LENGTH,NAME_BUF_LENGTH,RANDOM_TRANS_NAMEKEY)) {
		return FALSE;
	}
	if(nLength > MAX_NAME_LENGTH || nLength < MIN_NAME_LENGTH) {
		return FALSE;
	}
	strcpy(pCltName,chTmp);

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

BOOL CCltManager::LegalData(PDATA_GRAM_HEADER pDataHeader,char *pDvrName,char *pCltName)
{
	if(pDataHeader->dwType != FOURCC_VIDE && 
		pDataHeader->dwType != FOURCC_AUDI && 
		pDataHeader->dwType != FOURCC_NWID) {
		return FALSE;
	}
	char chTmp[NAME_BUF_LENGTH];
	int nLength = 0;
	if(!ThrowRandom(chTmp,pDataHeader->chLocalName,&nLength,NAME_BUF_LENGTH,NAME_BUF_LENGTH,RANDOM_TRANS_NAMEKEY)) {
		return FALSE;
	}
	if(nLength > MAX_NAME_LENGTH || nLength < MIN_NAME_LENGTH) {
		return FALSE;
	}
	strcpy(pDvrName,chTmp);
	if(!ThrowRandom(chTmp,pDataHeader->chDestName,&nLength,NAME_BUF_LENGTH,NAME_BUF_LENGTH,RANDOM_TRANS_NAMEKEY)) {
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
		if(m_DvrTnelArray[i]->FindDvr(strDvrName))  {
			m_DvrTnelArray[i]->SetAlive();
			break;
		}
	}
	if(i == count) return NULL;
	else return m_DvrTnelArray[i];

}

UINT CCltManager::KeepRcving()
{
	ASSERT(m_sGram != INVALID_SOCKET);
	DWORD dwWait;
	int nRcvLength,nAddrLength;
	int nTempWriteIndex;
	BOOL bBufFull = FALSE;
	HANDLE hEv[2];
	hEv[0] = m_hRcvGram;
	hEv[1] = m_hStopWorking;
	while (m_bWorking) {
		dwWait = WSAWaitForMultipleEvents(2,hEv,FALSE,INFINITE,FALSE);
		if(WAIT_OBJECT_0 == dwWait) {
			WSAResetEvent(hEv[0]);
			if(bBufFull) {
				nTempWriteIndex = (m_nWriteGramIndex+1) % GRAM_BUFFER_COUNT;
				if(nTempWriteIndex != m_nReadGramIndex) {
					m_nWriteGramIndex = nTempWriteIndex;
					bBufFull = FALSE;
				}
				else {
					TRACE("Rcv Buf Full:WriteIndex %d,ReadIndex %d",nTempWriteIndex,m_nReadGramIndex);
				}
			}
			memset(&m_GramDesc[m_nWriteGramIndex].IPAddr,0,sizeof(SOCKADDR_IN));
			nAddrLength = sizeof(SOCKADDR_IN);
			//// EnterCriticalSection(&g_csSocket); //// 接收和发送不冲突
			nRcvLength = recvfrom(m_sGram,m_GramDesc[m_nWriteGramIndex].pBuf,RCV_BUF_LENGTH,0,
				(SOCKADDR*)&m_GramDesc[m_nWriteGramIndex].IPAddr,&nAddrLength);
			//// LeaveCriticalSection(&g_csSocket);
			if(nRcvLength <= 0) {
				continue;
			}
			m_GramDesc[m_nWriteGramIndex].nLength = nRcvLength;
			nTempWriteIndex = (m_nWriteGramIndex+1) % GRAM_BUFFER_COUNT;
			if(nTempWriteIndex == m_nReadGramIndex) {
				//// 等下一次接收前再转索引
				bBufFull = TRUE;
			}
			else {
				m_nWriteGramIndex = nTempWriteIndex;
			}
			SetEvent(m_hDealwithGram);
			/*
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
			else if(INTERNET_MUP_LENGTH == nRcvLength) {
				//// 视频数据或者音频数据这里直接处理，不做线程通知。
				memcpy(m_pDataBuffer[m_nWriteDataBufIndex],rcvBuf,INTERNET_MUP_LENGTH);
				int n = (m_nWriteDataBufIndex+1) % DATA_BUFFER_COUNT;
				if(n != m_nReadDataBufIndex) {
					m_nWriteDataBufIndex = n;
				}
				else {
					TRACE("Recv Buffer FULL");
				}
				SetEvent(m_hDataGram);
			}
			*/
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
	HANDLE hEv[2];
	
	hEv[0] = m_hDealwithGram;
	hEv[1] = m_hStopWorking;
	while (m_bWorking) {
		dwWait = WaitForMultipleObjects(2,hEv,FALSE,INFINITE); 
		if(WAIT_OBJECT_0 == dwWait) {
			while(m_nReadGramIndex != m_nWriteGramIndex) {
				if(sizeof(TNELMSG_GRAM) == m_GramDesc[m_nReadGramIndex].nLength) {
					DealwithTnelGram((PTNELMSG_GRAM)m_GramDesc[m_nReadGramIndex].pBuf);
				}
				else if(sizeof(OPENTNEL) == m_GramDesc[m_nReadGramIndex].nLength) {
					DealwithCnctGram((POPENTNEL)m_GramDesc[m_nReadGramIndex].pBuf,
						m_GramDesc[m_nReadGramIndex].IPAddr);
				}
				else if(sizeof(HSHK_GRAM) == m_GramDesc[m_nReadGramIndex].nLength) {
					DealwithHshkACK((PHSHK_GRAM)m_GramDesc[m_nReadGramIndex].pBuf);
				}
				else if(sizeof(WORK_GRAM) == m_GramDesc[m_nReadGramIndex].nLength) {
					DealwithMsgGram((PWORK_GRAM)m_GramDesc[m_nReadGramIndex].pBuf);
				}
				else if(INTERNET_MUP_LENGTH == m_GramDesc[m_nReadGramIndex].nLength) {
					DealwithDataGram(m_GramDesc[m_nReadGramIndex].pBuf);
				}
				//// 这里最好不用 ++m_nReadGramIndex. 否则计算过程中可能被另外一个线程使用到，造成错误
				m_nReadGramIndex = (m_nReadGramIndex+1) % GRAM_BUFFER_COUNT;
			}
		}
		else {
			break;
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
						TRACE("delete Tnel Dvr %d",index);
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

BOOL CCltManager::PushSData(int nLength, SOCKADDR_IN sAddr, PVOID pData)
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

UINT CCltManager::SendGram()
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

UINT CCltManager::SendGramProc(LPVOID p)
{
	UINT uRe;
	CCltManager *pClt = (CCltManager*)p;
	uRe = pClt->SendGram();
	pClt->DecreaseThreadCount();

	return uRe;
}
