#include "stdafx.h"
#include "NTTProtocol.h"
#include "Global.h"
#include "DVRGlobal.h"
#include "LinkNetS.h"
#include "DVRManager.h"

CLinkNetS::CLinkNetS(LPCTSTR NetSName,WORD Port) :
m_nThreadCount(0)
{
	m_bWaitForAck = FALSE;
	m_bTimeout = FALSE;
	m_uSuspendTick = AUTOUPDATE_INTERVAL;  ////  每N*600秒一次自动更新
	m_uCurSuspendTick = 0; 
	m_bStopWorking = FALSE;
	m_nSendCount = 0;
	m_nSendInterval = INFINITE;
	m_strNetSName = NetSName;
	memset(&m_NetSAddr,0,sizeof(SOCKADDR_IN));
	m_NetSAddr.sin_family = AF_INET;
	m_NetSAddr.sin_addr.s_addr = Name2IP(NetSName);
	m_NetSAddr.sin_port = htons(Port);
	m_hSendReq = CreateEvent(NULL,FALSE,FALSE,NULL);
	m_hStopWorking = CreateEvent(NULL,TRUE,FALSE,NULL);
	InitializeCriticalSection(&m_csReqInfo);
}

CLinkNetS::~CLinkNetS()
{
	SetEvent(m_hStopWorking);
	//// ????
	while (m_nThreadCount > 0) {
		Sleep(50);
	}
	DeleteCriticalSection(&m_csReqInfo);
	CloseHandle(m_hSendReq);
	CloseHandle(m_hStopWorking);
}

// 在这里初始化SOCKET
BOOL CLinkNetS::InitSending()
{
	if(m_bStopWorking) return FALSE;

	DWORD dwID;
	IncreaseThreadCount();
	CreateThread(NULL,0,(LPTHREAD_START_ROUTINE)CLinkNetS::SendReqProc,this,0,&dwID);
	return TRUE;
}

BOOL CLinkNetS::FillReqInfo(PAPPLY_GRAM pInfo)
{
	if(m_bWaitForAck) return FALSE;
	EnterCriticalSection(&m_csReqInfo);
	memcpy(&m_ReqInfo,pInfo,sizeof(APPLY_GRAM));
	m_nSendInterval = REQ_INTERVAL;
	m_bWaitForAck = TRUE;
	m_bTimeout = FALSE;
	LeaveCriticalSection(&m_csReqInfo);
	m_nSendCount = 0;
	SetEvent(m_hSendReq);
	return TRUE;
}

ULONG CLinkNetS::GetNetSAddr(BOOL bRefresh)
{
	if(INADDR_NONE == m_NetSAddr.sin_addr.s_addr || bRefresh) {
		m_NetSAddr.sin_addr.s_addr = Name2IP(m_strNetSName);
	}
	return m_NetSAddr.sin_addr.s_addr;
}
//// 等待返回信息，同时停止本次请求发送.注意自动更新是不被回复的
BYTE CLinkNetS::CheckFeedback(PAPPLY_GRAM pFeedback,LPCTSTR ChkName,char *pChkPass)
{
	if(!m_bWaitForAck) return ACK_OTHER_ERROR; //// 这个时候不需要考虑返回的内容
	if(FOURCC_NETS != pFeedback->dwType) {
		return ACK_ILLEGAL_INFO;
	}
	char chTmp[NAME_BUF_LENGTH];
	int nLength = 0;
	if(!ThrowRandom(chTmp,pFeedback->chName,&nLength,NAME_BUF_LENGTH,NAME_BUF_LENGTH,RANDOM_FEEDBACK_NAMEKEY)) {
		return ACK_ILLEGAL_INFO;
	}
	//// TRACE("DVR Feedback Name: %s",chTmp);
	if(nLength < MIN_NAME_LENGTH || nLength > MAX_NAME_LENGTH) {
		return ACK_ILLEGAL_INFO;
	}
	if(!UncaseCompare(ChkName,chTmp)) {
		return ACK_ILLEGAL_INFO;
	}
	if(!ThrowRandom(chTmp,pFeedback->chPassword,&nLength,NAME_BUF_LENGTH,NAME_BUF_LENGTH,RANDOM_FEEDBACK_PASSKEY)) {
		return ACK_ILLEGAL_INFO;
	}
	BYTE byTmp[NAME_BUF_LENGTH];
	memcpy(byTmp,chTmp,MD5_CODE_LENGTH);
	/*
	TRACE("DVR Feedback Pass: %02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
		byTmp[0],byTmp[1],byTmp[2],byTmp[3],byTmp[4],byTmp[5],byTmp[6],byTmp[7],byTmp[8],byTmp[9],byTmp[10],
		byTmp[11],byTmp[12],byTmp[13],byTmp[14],byTmp[15]);
	*/
	if(MD5_CODE_LENGTH != nLength) {
		return ACK_ILLEGAL_INFO;
	}
	for(int i = 0; i < MD5_CODE_LENGTH; i++) {
		if(pChkPass[i] != chTmp[i]) return ACK_ILLEGAL_INFO;
	}
	m_bWaitForAck = FALSE;
	//// TRACE("DVR : Feedback Act:%x",pFeedback->byAct);
	return pFeedback->byAct; 
}

// 如果正在等待回复，则等待计数器清零，
// 否则等待计数器增加，到自动更新的时候，发送自动更新命令
BOOL CLinkNetS::SelfChecking(PAPPLY_GRAM pLinkInfo)
{
	//// 注意判断的顺序，不能颠倒
	CSelfLock SelfLock(&m_csReqInfo);
	if(m_bWaitForAck) {
		m_uCurSuspendTick = 0;
		return TRUE;
	}
	if(m_bTimeout) {
		m_bTimeout = FALSE;
		return FALSE;
	}

	if(++m_uCurSuspendTick < m_uSuspendTick)
		return TRUE;
	m_NetSAddr.sin_addr.s_addr = Name2IP(m_strNetSName);
	memcpy(&m_ReqInfo,pLinkInfo,sizeof(APPLY_GRAM));
	SetEvent(m_hSendReq);
	m_uCurSuspendTick = 0;
	return TRUE;
}

UINT CLinkNetS::SendReq()
{
	HANDLE hEv[2];
	hEv[0] = m_hSendReq;
	hEv[1] = m_hStopWorking;
	int size = sizeof(SOCKADDR_IN);
	while (!m_bStopWorking) {
		DWORD wait = WaitForMultipleObjects(2,hEv,FALSE,m_nSendInterval);
		if(WAIT_OBJECT_0 == wait) {
			EnterCriticalSection(&m_csReqInfo);
			g_pManager->PushSData(sizeof(APPLY_GRAM),m_NetSAddr,&m_ReqInfo);			
			LeaveCriticalSection(&m_csReqInfo);
			TRACE("DVR Send Applying ,type:%x",m_ReqInfo.dwType);
			m_nSendCount++;
		}
		else if(WAIT_TIMEOUT == wait) {
			if(!m_bWaitForAck) {
				m_nSendInterval = INFINITE;
				m_nSendCount = 0;
				continue;
			}
			if(MAX_REQSEND_COUNT == m_nSendCount) {
				//// 没有等到NetS的返回
				m_bWaitForAck = FALSE;
				m_bTimeout = TRUE;
				m_nSendInterval = INFINITE;
				continue;
			}
			EnterCriticalSection(&m_csReqInfo);
			g_pManager->PushSData(sizeof(APPLY_GRAM),m_NetSAddr,&m_ReqInfo);
			LeaveCriticalSection(&m_csReqInfo);
			TRACE("DVR keep Sending Applying,type:%x",m_ReqInfo.dwType);
			m_nSendCount++;
		}
		else {
			break;
		}
	}
	return 1;
}

UINT CLinkNetS::SendReqProc(LPVOID p)
{
	CLinkNetS* pLinkNetS = (CLinkNetS*)p;
	pLinkNetS->SendReq();
	pLinkNetS->DecreaseThreadCount();

	return 1;
}
