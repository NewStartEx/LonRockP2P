#include "stdafx.h"
#include "NetSDVR.h"
#include "Global.h"

extern CRITICAL_SECTION g_csSocket;  
UINT CNetSDVR::m_CertainKindCount = 0;

CNetSDVR::CNetSDVR(SOCKET sFeedback,APPLY_GRAM DvrInfo,DWORD dwIPAddr,WORD wPort,SYSTEMTIME stLastLink)
{
	m_DvrInfo = DvrInfo;
	m_dwIPAddr = dwIPAddr;
	m_wPort = wPort;
	m_bErrFeedback = FALSE;
	m_dwErrIPAddr = 0;
	m_wErrPort = 0;
	m_sFeedback = sFeedback;
	m_bStopWorking = FALSE;
	m_bPositionChanged = FALSE;
	memset(&m_curGram,0,sizeof(APPLY_GRAM));
	m_stLastLink = stLastLink;  
	m_byStatus = m_DvrInfo.byStatus;
	m_nOnlineTimeout = DVR_Online == GETACTIVE(m_byStatus) ? 0 : DVR_ONLINE_TIMEOUT;
	//// 对某一类上线与否的关注
	if(DVR_Online == GETACTIVE(m_byStatus)) {
		//// 如果发现某个产品在线数量已经多过限制，这个地方应该为Offline
		m_CertainKindCount ++;  
	}
	m_bStatusChanged = FALSE;
	m_bPositionChanged = FALSE;
	m_nThreadCount = 0;
	InitializeCriticalSection(&m_csSending);
}

CNetSDVR::~CNetSDVR()
{
	EnterCriticalSection(&m_csSending);
	m_bStopWorking = TRUE;
	LeaveCriticalSection(&m_csSending);
	while (m_nThreadCount > 0) {
		Sleep(50);
	}
	if(DVR_Online == GETACTIVE(m_byStatus)) {
		m_CertainKindCount --;
	}
	DeleteCriticalSection(&m_csSending);
}

BOOL CNetSDVR::FetchSavingInfo(PDVR_ITEM pItem)
{
	ASSERT(NULL != pItem);
	int nLength = strlen(m_DvrInfo.chName);
	if(nLength < MIN_NAME_LENGTH || nLength > MAX_NAME_LENGTH) 
		return FALSE;
#ifdef _DEBUG
	CString strtmp = m_DvrInfo.chName;
	strtmp.MakeUpper();
	/*
	if(strtmp == _T("CL4588")) {
		return FALSE;
	}
	*/
#endif
	memset(pItem,0,sizeof(DVR_ITEM));
	PushinRandom(pItem->chName,m_DvrInfo.chName,NAME_BUF_LENGTH,nLength,RANDOM_TRANS_NAMEKEY);
	//// memcpy(&pItem->chName,&m_DvrInfo.chName,NAME_BUF_LENGTH);
	//// memcpy(&pItem->chPassword,m_DvrInfo.chPassword,NAME_BUF_LENGTH);
	PushinRandom(pItem->chPassword,m_DvrInfo.chPassword,NAME_BUF_LENGTH,MD5_CODE_LENGTH,RANDOM_TRANS_NAMEKEY);
	pItem->dwType = m_DvrInfo.dwType;
	pItem->byAct1 = DVR_ACT_REGISTER;
#if 0 //// 不清楚为什么有段时间这个值变成了0x7F
	SETACTIVE(pItem->byAct2,DVR_Offline);  //// 存盘的时候这个必须为这个值
#else
	pItem->byAct2 = DVR_Offline; //// nvcd
#endif
	pItem->dwDevID[0] = m_DvrInfo.dwDevID[0];
	pItem->dwDevID[1] = m_DvrInfo.dwDevID[1];
	pItem->dwIPaddr = m_dwIPAddr;
	pItem->wPort = m_wPort;
	pItem->wLastLinkYear = m_stLastLink.wYear;
	pItem->wLastLinkMonth = m_stLastLink.wMonth;
	pItem->wLastLinkDay = m_stLastLink.wDay;
	pItem->wLastLinkHour = m_stLastLink.wHour;
	pItem->wLastLinkMinute = m_stLastLink.wMinute;
	pItem->wLastLinkSecond = m_stLastLink.wSecond;
	pItem->wReserved = 0;
	pItem->dwReserved = 0;

	m_bPositionChanged = FALSE; //// 这里确认是保存了的，下次不用保存了
	return TRUE;
}

BOOL CNetSDVR::FetchCltTunnelInfo(PTNELMSG_GRAM pGram)
{
	ASSERT(NULL != pGram);
	pGram->dwType = FOURCC_NETS;
	pGram->byAct = DVR_Online == GETACTIVE(m_byStatus) ? TUNNEL_ACK_SUCCESS : TUNNEL_ACK_OFFLINE;
	int nLength = strlen(m_DvrInfo.chName);
	PushinRandom(pGram->chDstName,m_DvrInfo.chName,NAME_BUF_LENGTH,nLength,RANDOM_TRANS_NAMEKEY);
	//// memcpy(pGram->chDstName,m_DvrInfo.chName,NAME_BUF_LENGTH);
	pGram->dwDstIPaddr = m_dwIPAddr;
	pGram->wDstPort = m_wPort;
	pGram->wReserved = 0;
	pGram->dwReserved = m_DvrInfo.dwType;  //// 对IE里不能做NVR连接的暂时性屏蔽所作的工作

	if(DVR_Online == GETACTIVE(m_byStatus)) {
		GetSystemTime(&m_stLastLink);
	}
	return TRUE;
}

BOOL CNetSDVR::IsMyself(LPCTSTR Name)
{
	return UncaseCompare(Name,m_DvrInfo.chName);
}

BOOL CNetSDVR::FetchDVRInfo(PDVRMSG pDvrMsg)
{
	ASSERT(NULL != pDvrMsg);
	pDvrMsg->dwType = m_DvrInfo.dwType;
	pDvrMsg->dwDevID[0] = m_DvrInfo.dwDevID[0];
	pDvrMsg->dwDevID[1] = m_DvrInfo.dwDevID[1];
	memcpy(pDvrMsg->chName,m_DvrInfo.chName,NAME_BUF_LENGTH);
	pDvrMsg->byAct = m_DvrInfo.byAct;
	pDvrMsg->dwIPaddr = m_dwIPAddr;

	return TRUE;
}

//// 下面函数返回就是ACK的值。
//// 如果是返回删除成功，则由调用线程发送数据报，同时删除该DVR元素
//// 如果此时正在发送数据，返回Busy,什么都不处理
UINT CNetSDVR::CheckDVRGram(PAPPLY_GRAM pGram,char *pPassword,DWORD dwNewIPAddr,WORD wNewPort)
{
	ASSERT(NULL != pGram);
	CString str1,str2;
	UINT uResult = 0;
	int nLength = strlen(m_DvrInfo.chName);
	pGram->dwType = FOURCC_NETS;
	PushinRandom(pGram->chName,m_DvrInfo.chName,NAME_BUF_LENGTH,nLength,RANDOM_FEEDBACK_NAMEKEY);
	//// 高级用户强制休眠检查
	if(DVR_ACT_SLEEPDOWN == pGram->byAct) {
		SYSTEMTIME stime;
		GetSystemTime(&stime);
		char chCheck[NAME_BUF_LENGTH];
		sprintf(chCheck,"%d,%d,%d:peelS",stime.wYear,stime.wDay,stime.wHour);
		if(0 != strcmp(chCheck,pPassword)) return ACK_ILLEGAL_INFO;
		nLength = strlen(chCheck);
		PushinRandom(pGram->chPassword,chCheck,NAME_BUF_LENGTH,nLength,RANDOM_FEEDBACK_NAMEKEY);
		if(DVR_Online == GETACTIVE(m_byStatus)) {
			m_CertainKindCount--;
			m_bStatusChanged = TRUE;
		}
		SETACTIVE(m_byStatus,DVR_Offline);
		pGram->byAct = ACK_SLEEP_SUCCESS;
		m_nOnlineTimeout = DVR_ONLINE_TIMEOUT;
		return ACK_SLEEP_SUCCESS;
	}
	PushinRandom(pGram->chPassword,m_DvrInfo.chPassword,NAME_BUF_LENGTH,MD5_CODE_LENGTH,RANDOM_FEEDBACK_PASSKEY);
	memcpy(&m_curGram,pGram,sizeof(APPLY_GRAM));

	m_bErrFeedback = FALSE; 
	if(pGram->dwType != m_DvrInfo.dwType) {
		; //// 类型不一样，保留
	}
	if(pGram->dwDevID[0] != m_DvrInfo.dwDevID[0] || 
		pGram->dwDevID[1] != m_DvrInfo.dwDevID[1]) {
		; //// 标志不一样，保留
	}
	//// 在这之前应该确认名称一致了，这里不用管Gram的名称和密码项。这两项是加密的
	BOOL bPassOK = TRUE;
	for(int i = 0; i < MD5_CODE_LENGTH; i++) {
		if(m_DvrInfo.chPassword[i] != pPassword[i]) {
			bPassOK = FALSE;
			break;
		}
	}
	if(!bPassOK) {
		//// 这个时候要还原该请求地址本来的密码，不能把原来的密码给出去
		PushinRandom(pGram->chPassword,pPassword,NAME_BUF_LENGTH,MD5_CODE_LENGTH,RANDOM_FEEDBACK_PASSKEY);
		memcpy(m_curGram.chPassword,pGram->chPassword,NAME_BUF_LENGTH);
		m_curGram.byAct = DVR_ACT_REGISTER == pGram->byAct ? ACK_REGISTER_DUPLICATE : ACK_PASSWORD_INVALID;
		m_bErrFeedback = TRUE;
	}
	//// 注册
	else if(DVR_ACT_REGISTER == pGram->byAct) {
		if(dwNewIPAddr==m_dwIPAddr && wNewPort==m_wPort) {
			//// 同一个地址发送的多个请求
			m_nOnlineTimeout = 0;
			m_curGram.byAct = ACK_REGISTER_SUCCESS;
			if(DVR_Offline == GETACTIVE(m_byStatus)) {
				SETACTIVE(m_byStatus,DVR_Online);
				m_bStatusChanged = TRUE;
				m_CertainKindCount++;
			}
			GetSystemTime(&m_stLastLink);
		}
		else {
			//// 已经被注册了
			m_curGram.byAct = ACK_REGISTER_DUPLICATE;
			m_bErrFeedback = TRUE;
		}
	}
	else if(DVR_ACT_MANUALUPDATE == pGram->byAct) {
		if(DVR_SLEEPING != GETSLEEPING(m_byStatus)) {
			if((m_dwIPAddr!=dwNewIPAddr) || (m_wPort!=wNewPort)) {
				m_bPositionChanged = TRUE;
				m_dwIPAddr = dwNewIPAddr;
				m_wPort = wNewPort;
			}
			////m_dwIPAddr = dwNewIPAddr;
			////m_wPort = wNewPort;
			
			//// 如果发现某个产品在线数量已经多过限制，这个地方应该为Offline
			if(DVR_Offline == GETACTIVE(m_byStatus)) {
				m_bStatusChanged = TRUE;
				m_CertainKindCount++; 
				SETACTIVE(m_byStatus,DVR_Online);
			}
			m_nOnlineTimeout = 0;
			m_curGram.byAct = ACK_UPDATE_OK;
		}
		else {
			m_curGram.byAct = ACK_DEVICE_SLEEPING;
			m_bErrFeedback = TRUE;
		}
	}
	else if(DVR_ACT_AUTOUPDATE == pGram->byAct) {
		//// 自动更新不需要回复
		if((m_dwIPAddr!=dwNewIPAddr) || (m_wPort!=wNewPort)) {
			m_bPositionChanged = TRUE;
			m_dwIPAddr = dwNewIPAddr;
			m_wPort = wNewPort;
		}
		pGram->byAct = ACK_UPDATE_OK; //// 可以不用改
		
		//// 如果发现某个产品在线数量已经多过限制，这个地方应该为Offline
		if(DVR_Offline == GETACTIVE(m_byStatus)) {
			m_bStatusChanged = TRUE;
			m_CertainKindCount++;
			SETACTIVE(m_byStatus,DVR_Online);
		}
		m_nOnlineTimeout = 0;
		return ACK_UPDATE_OK;
	}
	else if(DVR_ACT_WAKEUP == pGram->byAct) {
		if(DVR_Online == GETACTIVE(m_byStatus)) {
			m_bErrFeedback = TRUE;
			m_curGram.byAct = ACK_ACCOUNT_ALIVE;
		}
		else {
			//// 如果发现某个产品在线数量已经多过限制，这个地方应该为Offline
			m_bStatusChanged = TRUE;
			m_CertainKindCount++;
			SETACTIVE(m_byStatus,DVR_Online);
			if((m_dwIPAddr!=dwNewIPAddr) || (m_wPort!=wNewPort)) {
				m_bPositionChanged = TRUE;
				m_dwIPAddr = dwNewIPAddr;
				m_wPort = wNewPort;
			}
			SETSLEEPING(m_byStatus,0);
			m_nOnlineTimeout = 0;
			GetSystemTime(&m_stLastLink); //// 这个时候的时间算做最后连接时间
			m_curGram.byAct = ACK_WAKEUP_SUCCESS; //// 可以不用改
		}
	}
	else if(DVR_ACT_DELETE == pGram->byAct) {
		//// 这种情况下管理线程需要回复
		m_bStopWorking = TRUE;
		pGram->byAct = ACK_DELETE_SUCCESS;
		return ACK_DELETE_SUCCESS;
	}
	else if(DVR_ACT_EXIT == pGram->byAct) {
		if(DVR_Online == GETACTIVE(m_byStatus)) {
			m_CertainKindCount--;
			m_bStatusChanged = TRUE;
		}
		SETACTIVE(m_byStatus,DVR_Offline);
		m_nOnlineTimeout = DVR_ONLINE_TIMEOUT;
		return DVR_ACT_EXIT;
	}
	else {
		return ACK_ILLEGAL_INFO; //// 错误信息
	}
	if(m_bErrFeedback) {
		m_dwErrIPAddr = dwNewIPAddr;
		m_wErrPort = wNewPort;
	}
	uResult = m_curGram.byAct;
	DWORD dwID;
	IncreaseThreadCount();
	CreateThread(NULL,0,(LPTHREAD_START_ROUTINE)CNetSDVR::SendFeedBackProc,this,0,&dwID);
	return uResult;
}

BOOL CNetSDVR::CheckSleepDown()
{
	if(DVR_SLEEPING != GETSLEEPING(m_byStatus)) {
		; //// 这里做判断
	}
	if(DVR_SLEEPING == GETSLEEPING(m_byStatus)) {
		return TRUE;
	}
	return FALSE;
}

BOOL CNetSDVR::SelfChanged(BOOL *pStatusChanged)
{
	if(CheckSleepDown()) {
		*pStatusChanged = m_bStatusChanged;
		m_bStatusChanged = FALSE;
		return FALSE;
	}
	BOOL bResult = m_bPositionChanged;
	m_bPositionChanged = FALSE;  ///// 地址变化了
////	TRACE("OnlineTimeout:%d",m_nOnlineTimeout);
	if(m_nOnlineTimeout >= DVR_ONLINE_TIMEOUT) {
		if(DVR_Online == GETACTIVE(m_byStatus)) {
			m_CertainKindCount--;
			m_bStatusChanged = TRUE;
		}
		SETACTIVE(m_byStatus,DVR_Offline);
	}
	else {
		m_nOnlineTimeout++;
	}
	*pStatusChanged = m_bStatusChanged;
	m_bStatusChanged = FALSE;
	TRACE("NetSDVR:%s, bResult:%d,Status:%d,Timeout:%d",m_DvrInfo.chName,bResult,m_byStatus,m_nOnlineTimeout);
	return bResult;
}

BOOL CNetSDVR::SendFeedBack()
{
	CSelfLock SelfLock(&m_csSending);
	if(m_bStopWorking) return FALSE;
	
	ASSERT(INVALID_SOCKET != m_sFeedback);
	
	SOCKADDR_IN DestAddr;
	memset(&DestAddr,0,sizeof(SOCKADDR_IN));
	
	if(m_bErrFeedback) {
		DestAddr.sin_family = AF_INET;
		DestAddr.sin_addr.s_addr = (ULONG)m_dwErrIPAddr;
		DestAddr.sin_port = m_wErrPort; //// 这里是对方直接发来的Port,所以不能用htons 转换
	}
	else {
		DestAddr.sin_family = AF_INET;
		DestAddr.sin_addr.s_addr = (ULONG)m_dwIPAddr;
		DestAddr.sin_port = m_wPort; //// 这里是对方直接发来的Port,所以不能用htons 转换
	}

	EnterCriticalSection(&g_csSocket);
	int nRe = sendto(m_sFeedback,(char*)&m_curGram,sizeof(APPLY_GRAM),0,(SOCKADDR*)&DestAddr,sizeof(SOCKADDR_IN));
	LeaveCriticalSection(&g_csSocket);
////	nRe = WaitForSockWroten(m_sFeedback,5);
	TRACE("DVR Feedback Select:%d",nRe);

	//// 可以写，说明数据已经发送完毕。 待测试-- nvc
	return TRUE;
}

UINT CNetSDVR::SendFeedBackProc(LPVOID p)
{
	CNetSDVR* pDVR = (CNetSDVR*)p;
	BOOL Re;
	Re = pDVR->SendFeedBack();
	pDVR->DecreaseThreadCount();
	return Re;
}
