#include "stdafx.h"
#include "NTTProtocol.h"
#include "Global.h"
#include "DVRGlobal.h"
#include "ClientTnel.h"
#include "ChannelInfo.h"
#include "SendVideo.h"
#include "DVRManager.h"

int CClientTnel::m_TnelCount = 0;
CClientTnel::CClientTnel(char *Name,DWORD dwIPAddr,WORD wPort,BYTE byAct) 
{
	m_strName = Name;
	ASSERT(m_strName.GetLength() < NAME_BUF_LENGTH);
	memset(&m_OpenTnel,0,sizeof(OPENTNEL));
	memset(&m_HshkACKGram,0,sizeof(HSHK_GRAM));
	memset(&m_CmdGram,0,sizeof(WORK_GRAM));
	memset(&m_ClientAddr,0,sizeof(SOCKADDR_IN));
	m_ClientAddr.sin_family = AF_INET;
	m_ClientAddr.sin_addr.s_addr = dwIPAddr;
	m_ClientAddr.sin_port = wPort;  //// 这里是对方直接发来的Port,所以不能用htons 转换
	if(CONNECT_CMD == byAct) {
		m_bTnelOpened = TRUE;
		m_nGramType = GRAM_TYPE_MSG;
		m_nSendGramInterval = INFINITE;
		m_byMsgAct = CONNECT_MSG;  //// 这里表示双方已经联系上，所以这样设置。不是这样还会有变数，因此不设置
	}
	else {
		m_bTnelOpened = FALSE;
		m_nGramType = GRAM_TYPE_TNEL;
		m_nSendGramInterval = CREATE_TNEL_INTERVAL;
	}

	m_dwNwidPackets = DEFAULT_NWID_PACKETS; 
	m_dwNetRelayInterval = DEFAULT_NETRELAY_INTERVAL;
	m_HshkStatus = HshkStart;
	m_bForceExit = FALSE;
	m_bCanVideo = FALSE;
	m_bCanAudio = FALSE;
	m_nSendTnelCount = 0;
	m_nLastCmdJump = 0;
	m_nThreadCount = 0;
	m_hNormalGram = CreateEvent(NULL,FALSE,FALSE,NULL);
	m_hNaviData = CreateEvent(NULL,FALSE,FALSE,NULL);
	m_hStopWorking = CreateEvent(NULL,TRUE,FALSE,NULL);
	InitializeCriticalSection(&m_csGram);
	InitializeCriticalSection(&m_csCount);
	m_TnelCount++;
}

CClientTnel::~CClientTnel()
{
	//// ????
	m_bCanVideo = FALSE;
	m_bCanAudio = FALSE;
	SetEvent(m_hStopWorking);
	while (m_nThreadCount > 0) {
		Sleep(50);
	}
	TRACE("%s delete in Station",m_strName);
	while (m_SendVideoArray.GetSize() > 0) {
		delete m_SendVideoArray[0];
		m_SendVideoArray.RemoveAt(0);
	}
	DeleteCriticalSection(&m_csGram);
	DeleteCriticalSection(&m_csCount);
	CloseHandle(m_hNormalGram);
	CloseHandle(m_hStopWorking);
	m_TnelCount--;
}

// 这里的DVRName在Cmd命令里从对方报文里面取，Tnel命令里是DVR直接给
BOOL CClientTnel::InitResource(LPCTSTR DVRName)
{
	m_strDvrName = DVRName;
	ASSERT(m_strDvrName.GetLength() < NAME_BUF_LENGTH);

	if(GRAM_TYPE_TNEL == m_nGramType) {
		//// 创建开隧道报文
		m_OpenTnel.dwType = FOURCC_OPEN;
		m_OpenTnel.byAct = TUNNEL_REQUIRE;
		char chTmp[NAME_BUF_LENGTH];
		int nLength = m_strDvrName.GetLength();
		memset(chTmp,0,NAME_BUF_LENGTH);
		strcpy(chTmp,m_strDvrName);
		PushinRandom(m_OpenTnel.chFirstName,chTmp,NAME_BUF_LENGTH,nLength,RANDOM_TRANS_NAMEKEY);
		PushinRandom(m_OpenTnel.chSecondName,chTmp,NAME_BUF_LENGTH,nLength,RANDOM_TRANS_NAMEKEY);
	}
	DWORD dwID;
	IncreaseThreadCount();
	CreateThread(NULL,0,(LPTHREAD_START_ROUTINE)CClientTnel::SendGramProc,this,0,&dwID);
	return TRUE;
}

//// 用这个来标识同一个电脑的数据，不允许一个电脑连接两次（比如使用IP和名称）
BOOL CClientTnel::IsMyself(LPCTSTR CltName)
{
	//// CltName 其实是一些数字
	CString str = CltName;
	return (m_strName==str);
}

//// 使用此函数的原因是假设远端停止连接后又开始请求隧道，而这个对象还没有被删除
//// 这个参数是DVR 本身提供的
BOOL CClientTnel::RetryTnel(LPCTSTR DVRName)
{
	//// 此处必须假设：如果这里参数的名称与已经使用的Client 名称不同，则是已经使用的是直接连接的名称
	//// 这种情况下要返回已经使用其他连接的报文

	CString strNewDvrName = DVRName;
	//// 名称不同，这时确认原来使用的是直接连接
	if(!UncaseCompare(strNewDvrName,m_strDvrName)) {
		MakeTnelGram(strNewDvrName,m_strDvrName,TUNNEL_ACK_DUPLINK);
		return FALSE;
	}
	EnterCriticalSection(&m_csCount);
	if(m_bTnelOpened) {
		//// 这个时候要保持更长时间的心跳连接
		m_nLastCmdJump = 0;
	}
	else {
		//// 这个时候要保持更长时间的开通道
		m_nSendTnelCount = 0;
		if(INFINITE == m_nSendGramInterval) {
			//// 这是非常小概率事件。只有用户自己等待一段时间后又手动开始连接才有这个可能
			m_OpenTnel.dwType = FOURCC_OPEN;
			m_OpenTnel.byAct = TUNNEL_REQUIRE;
			char chTmp[NAME_BUF_LENGTH];
			int nLength = m_strDvrName.GetLength();
			memset(chTmp,0,NAME_BUF_LENGTH);
			strcpy(chTmp,m_strDvrName);
			PushinRandom(m_OpenTnel.chFirstName,chTmp,NAME_BUF_LENGTH,nLength,RANDOM_TRANS_NAMEKEY);
			PushinRandom(m_OpenTnel.chSecondName,chTmp,NAME_BUF_LENGTH,nLength,RANDOM_TRANS_NAMEKEY);
			m_nSendGramInterval = CREATE_TNEL_INTERVAL;
			SetEvent(m_hNormalGram);  //// 激活线程
		}
	}
	LeaveCriticalSection(&m_csCount);

	return TRUE;
}

//// 自检。如果返回FALSE,说明没有存在意义，需要删除
BOOL CClientTnel::SelfChecking()
{
	if(m_bForceExit) return FALSE;  //// 强制删除标志。由用户设置
	BOOL bExist = TRUE;
	EnterCriticalSection(&m_csCount);
	if(m_bTnelOpened) {
		if(m_nLastCmdJump > MAX_SILENCE_INTERVAL) {
			bExist = FALSE;
		}
		else {
			m_nLastCmdJump++;
		}
	}
	else {
		if(m_nSendTnelCount > MAX_TNEL_SEND_COUNT) {
			bExist = FALSE;
		}
		else {
			m_nSendTnelCount++;
		}
	}
	LeaveCriticalSection(&m_csCount);
	//// TRACE("DVR: Clt exit:%d",bExist);
	return bExist;
}

void CClientTnel::MakeTnelGram(LPCTSTR FirstName,LPCTSTR SecondName,BYTE byAct)
{
	CString strFirstName = FirstName;
	CString strSecondName = SecondName;
	ASSERT(strFirstName.GetLength()<=MAX_NAME_LENGTH && strSecondName.GetLength()<=MAX_NAME_LENGTH &&
		strFirstName.GetLength()>=MIN_NAME_LENGTH && strSecondName.GetLength()>=MIN_NAME_LENGTH);
	char chTmp[NAME_BUF_LENGTH];
	int nLength;
	EnterCriticalSection(&m_csGram);
	m_OpenTnel.dwType = FOURCC_OPEN;
	m_OpenTnel.byAct = byAct;
	memset(chTmp,0,NAME_BUF_LENGTH);
	nLength = strFirstName.GetLength();
	strcpy(chTmp,strFirstName);
	PushinRandom(m_OpenTnel.chFirstName,chTmp,NAME_BUF_LENGTH,nLength,RANDOM_TRANS_NAMEKEY);
	memset(chTmp,0,NAME_BUF_LENGTH);
	nLength = strSecondName.GetLength();
	strcpy(chTmp,strSecondName);
	PushinRandom(m_OpenTnel.chSecondName,chTmp,NAME_BUF_LENGTH,nLength,RANDOM_TRANS_NAMEKEY);
	g_pManager->PushSData(sizeof(OPENTNEL),m_ClientAddr,&m_OpenTnel);
	LeaveCriticalSection(&m_csGram);
	return ;
}

//// 对方发过来的隧道信息。如果地址不一致，返回已经连接；如果已经连接，则直接抛弃。
//// 这里必须确认的是，发送隧道报文时，当客户端名称一致的时候，设想地址肯定是一致的
BYTE CClientTnel::DealwithCnctGram(PCNCTDESC pTnelInfo)
{
	CString strTmpName;
	char chTmp[NAME_BUF_LENGTH];
	int nLength = 0;
	memset(chTmp,0,NAME_BUF_LENGTH);
	if(!ThrowRandom(chTmp,pTnelInfo->OpenGram.chFirstName,&nLength,NAME_BUF_LENGTH,NAME_BUF_LENGTH,RANDOM_TRANS_NAMEKEY)) {
		return NTT_UNKNOW_ERROR;
	}
	if(nLength > MAX_NAME_LENGTH || nLength < MIN_NAME_LENGTH) {
		return NTT_UNKNOW_ERROR;
	}
	//// strTmpName = chTmp;
	if(m_strName != chTmp) return NTT_UNKNOW_ERROR;
	//// 正常情况下，地址不同和连接的名称不同是同时出现的
	//// 以下判断的优先顺序要注意
	memset(chTmp,0,NAME_BUF_LENGTH);
	if(!ThrowRandom(chTmp,pTnelInfo->OpenGram.chSecondName,&nLength,NAME_BUF_LENGTH,NAME_BUF_LENGTH,RANDOM_TRANS_NAMEKEY)) {
		return NTT_UNKNOW_ERROR;
	}
	if(nLength > MAX_NAME_LENGTH || nLength < MIN_NAME_LENGTH) {
		return NTT_UNKNOW_ERROR;
	}
	//// strTmpName = chTmp;
	if(!UncaseCompare(chTmp,m_strDvrName))  {
		//// 和最早的连接名称不同，可以认为同一个客户端的第二次连接
		MakeTnelGram(chTmp,m_strDvrName,TUNNEL_ACK_DUPLINK);
		return TUNNEL_ACK_DUPLINK;
	}
	/*
	if((pTnelInfo->addr.sin_addr.s_addr!=m_ClientAddr.sin_addr.s_addr) ||
		pTnelInfo->addr.sin_port!=m_ClientAddr.sin_port) {
		//// 和最早的连接地址不同，可以认为同一个客户端的第二次连接
		MakeTnelGram(strTmpName,m_strDvrName,TUNNEL_ACK_DUPLINK);
		return TUNNEL_ACK_DUPLINK;
	}
	*/
	if(TUNNEL_REQUIRE==pTnelInfo->OpenGram.byAct) {
		if(!m_bTnelOpened) {
			//// 为了确保对方的地址可以收到，这里更换地址为对方发来的地址
			EnterCriticalSection(&m_csGram);
			memcpy(&m_ClientAddr,&pTnelInfo->addr,sizeof(SOCKADDR_IN));
			LeaveCriticalSection(&m_csGram);
			m_bTnelOpened = TRUE;
			m_byMsgAct = TUNNEL_MSG;
		}
		MakeTnelGram(m_strDvrName,m_strDvrName,TUNNEL_ACK_SUCCESS);
		return TUNNEL_ACK_SUCCESS;
	}
	//// 以下说明对方发来的是回应信息
	if(m_bTnelOpened) {
		return TUNNEL_ACK_SUCCESS;  
	}
	else {
		m_bTnelOpened = TRUE;
		m_byMsgAct = TUNNEL_MSG;
		return TUNNEL_ACK_SUCCESS;
	}

}

BOOL CClientTnel::DealwithHshkGram(PHSHKDESC pHshkInfo)
{
	char chTmp[NAME_BUF_LENGTH];
	int nLength = 0;
	//// TRACE("Get Client Cmd");
	if(pHshkInfo->HshkGram.dwType != FOURCC_HSHK) return FALSE;
	if(!ThrowRandom(chTmp,pHshkInfo->HshkGram.chLocalName,&nLength,NAME_BUF_LENGTH,NAME_BUF_LENGTH,RANDOM_TRANS_NAMEKEY)) {
		return FALSE;
	}
	ASSERT(m_strName == chTmp);  //// 因为在上一级已经判断过了
	if(!ThrowRandom(chTmp,pHshkInfo->HshkGram.chDestName,&nLength,NAME_BUF_LENGTH,NAME_BUF_LENGTH,RANDOM_TRANS_NAMEKEY)) {
		return FALSE;
	}
	if(nLength > MAX_NAME_LENGTH || nLength < MIN_NAME_LENGTH) {
		return FALSE;
	}
	
	//// 命令格式正确,处理报文
	if(m_bTnelOpened && (!UncaseCompare(m_strDvrName,chTmp))) return FALSE;  //// 一个客户使用两个以上的方式连接DVR
	
	//// 命令格式正确
	EnterCriticalSection(&m_csCount);
	m_nLastCmdJump = 0;
	LeaveCriticalSection(&m_csCount);
	if(!m_bTnelOpened) {
		//// 判断是否是通过直接连接的途径来的。这里有两种可能性：
		//// 1、对方已经收到这边的打洞报文，确认隧道开通，然后发送报文，而此时本地还没有收到对方的回复报文；
		//// 2、对方通过直接连接的方式处理
		EnterCriticalSection(&m_csGram);
		if((pHshkInfo->addr.sin_addr.s_addr!=m_ClientAddr.sin_addr.s_addr) ||
			(pHshkInfo->addr.sin_port!=m_ClientAddr.sin_port)) {
			//// 说明这个命令是直接连接过来的
			m_ClientAddr = pHshkInfo->addr;
			m_byMsgAct = CONNECT_MSG;
			m_strDvrName = chTmp; 
		}
		else {
			//// 对方已经收到打洞报文，但是回复报文因某种原因没有收到。并且发送了命令回来
			m_byMsgAct = TUNNEL_MSG;
			//// 为保险起见这里再做一次DVRName赋值。有这样一个可能性导致直接连接的地址和隧道地址相同：
			//// 即Client端直接连在Internet上。而DVR端通过端口映射或者直接连接外网方式连接，
			//// 此时Client发出的DVR名称可能和隧道名称不同。
			m_strDvrName = chTmp;
			
		}
		m_nLastCmdJump = 0;
		m_nSendTnelCount = 0;
		m_bTnelOpened = TRUE;
		LeaveCriticalSection(&m_csGram);
	}
	SetHshkACK(&pHshkInfo->HshkGram);
	return TRUE;
}

//// 有可能直接连接的。这个时候隧道开通变量置真，不用开辟隧道了，返回后由管理线程处理并发送命令
//// 如果这时候隧道还没有开通，则判断是否是另外的一个连接。同时发送隧道重复的命令
BOOL CClientTnel::CheckCmdGram(PCMDDESC pWorkInfo)
{
	char chTmp[NAME_BUF_LENGTH];
	int nLength = 0;
	//// TRACE("Get Client Cmd");
	if(pWorkInfo->WorkGram.dwType != FOURCC_PUSH) return FALSE;
	if(!ThrowRandom(chTmp,pWorkInfo->WorkGram.chLocalName,&nLength,NAME_BUF_LENGTH,NAME_BUF_LENGTH,RANDOM_TRANS_NAMEKEY)) {
		return FALSE;
	}
	ASSERT(m_strName == chTmp);  //// 因为在上一级已经判断过了
	if(!ThrowRandom(chTmp,pWorkInfo->WorkGram.chDestName,&nLength,NAME_BUF_LENGTH,NAME_BUF_LENGTH,RANDOM_TRANS_NAMEKEY)) {
		return FALSE;
	}
	if(nLength > MAX_NAME_LENGTH || nLength < MIN_NAME_LENGTH) {
		return FALSE;
	}
	if(!m_bTnelOpened) return FALSE; //// 不和以前的版本兼容，单独处理
	if(m_bTnelOpened && (!UncaseCompare(m_strDvrName,chTmp))) return FALSE;  //// 一个客户使用两个以上的方式连接DVR
	
	//// 命令格式正确
	EnterCriticalSection(&m_csCount);
	m_nLastCmdJump = 0;
	LeaveCriticalSection(&m_csCount);

	return TRUE;
}

BOOL CClientTnel::CheckVResdGram(PVRESDPACKET pVResdPackt,DWORD dwIPAddr,LPCTSTR DvrName)
{
	if(m_ClientAddr.sin_addr.s_addr != dwIPAddr) {
		//// 这里不判断端口号，因为以后也许会通过不同端口发送回传消息
		return FALSE;
	}
	if(!UncaseCompare(DvrName,m_strDvrName)) {
		return FALSE;
	}
	if(pVResdPackt->byChanNum >= m_SendVideoArray.GetSize()) {
		//// Error
		return FALSE;
	}
	//// dealwith pVresdpacket
	m_nLastCmdJump = 0; //// 说明对方还在活动。有时候会因为发送数据太多造成命令发送失败
	m_SendVideoArray[pVResdPackt->byChanNum]->AskForResend(pVResdPackt);
	return TRUE;
}

BOOL CClientTnel::SetMsgGram(PCMDINFO pWorkInfo)
{
	char chTmp[NAME_BUF_LENGTH];
	int nLength;
	EnterCriticalSection(&m_csGram);
	memcpy(&m_CmdGram.Info,pWorkInfo,sizeof(CMDINFO));
	m_CmdGram.dwType = FOURCC_PUSH;
	memset(chTmp,0,NAME_BUF_LENGTH);
	nLength = m_strDvrName.GetLength();
	strcpy(chTmp,m_strDvrName);
	PushinRandom(m_CmdGram.chLocalName,chTmp,NAME_BUF_LENGTH,nLength,RANDOM_TRANS_NAMEKEY);
	memset(chTmp,0,NAME_BUF_LENGTH);
	nLength = m_strName.GetLength();
	strcpy(chTmp,m_strName);
	PushinRandom(m_CmdGram.chDestName,chTmp,NAME_BUF_LENGTH,nLength,RANDOM_TRANS_NAMEKEY);
	m_CmdGram.byAct = m_byMsgAct;
	m_nGramType = GRAM_TYPE_MSG;
	LeaveCriticalSection(&m_csGram);
	SetEvent(m_hNormalGram);

	return TRUE;
}

BOOL CClientTnel::SetHshkACK(PHSHK_GRAM pHshkGram)
{
	char chTmp[NAME_BUF_LENGTH];
	int nLength;
	BOOL bSetACK = FALSE;
	BOOL bDataNavi = FALSE;
	if((HSHK_STARTING==pHshkGram->byAct) && (HshkStart==m_HshkStatus)) {
		bSetACK = TRUE;
		m_HshkACKGram.byAct = HSHK_ACK_STARTING;
	}
	if((HSHK_NAVIGATING==pHshkGram->byAct) && (HshkStart==m_HshkStatus)) {
		bDataNavi = TRUE;
		m_HshkStatus = HshkNavi;
	}
	if((HSHK_NETSTATUS==pHshkGram->byAct) && ((HshkNavi==m_HshkStatus)||(HshkFinish==m_HshkStatus))) {
		if(HshkNavi == m_HshkStatus) {
			m_dwNwidPackets = pHshkGram->dwParam1;
			m_dwNetRelayInterval = pHshkGram->dwParam2;
			m_HshkStatus = HshkFinish;
			TRACE("Dvr Get Nwid:%d,Interval:%d",m_dwNwidPackets,m_dwNetRelayInterval);
		}
		bSetACK = TRUE;
		m_HshkACKGram.byAct = HSHK_ACK_NETSTATUS;
	}
	if(bSetACK) {
		EnterCriticalSection(&m_csGram);
		m_HshkACKGram.dwType = FOURCC_HSHK;
		memset(chTmp,0,NAME_BUF_LENGTH);
		nLength = m_strDvrName.GetLength();
		strcpy(chTmp,m_strDvrName);
		PushinRandom(m_HshkACKGram.chLocalName,chTmp,NAME_BUF_LENGTH,nLength,RANDOM_TRANS_NAMEKEY);
		memset(chTmp,0,NAME_BUF_LENGTH);
		nLength = m_strName.GetLength();
		strcpy(chTmp,m_strName);
		PushinRandom(m_HshkACKGram.chDestName,chTmp,NAME_BUF_LENGTH,nLength,RANDOM_TRANS_NAMEKEY);
		m_nGramType = GRAM_TYPE_HSHK;
		LeaveCriticalSection(&m_csGram);
		SetEvent(m_hNormalGram);	
		TRACE("Set HandShk ACK:%x",m_HshkACKGram.byAct);
	}
	if(bDataNavi) {
		SetEvent(m_hNaviData);
	}

	return (bSetACK||bDataNavi);
}

void CClientTnel::SetExit()
{
	if(!m_bTnelOpened) return;
	CMDINFO ExitInfo;
	memset(&ExitInfo,0,sizeof(CMDINFO));
	strcpy(ExitInfo.chCmd,MSG_EXIT);
	SetMsgGram(&ExitInfo);
}

void CClientTnel::EnableVideo(BOOL bEnable)
{
	if(bEnable && 0==m_SendVideoArray.GetSize()) {
		for(int i = 0; i < g_ChanInfoArray.GetSize(); i++) {
			CSendVideo* pSV = new CSendVideo(i);
			pSV->InitResource(m_strDvrName,m_strName,m_ClientAddr);
			m_SendVideoArray.Add(pSV);
		}
	}
	m_bCanVideo = bEnable;
}

void CClientTnel::EnableAudio(BOOL bEnable)
{
	//// ....当带宽小于一定的值的时候，就不传音频。
	m_bCanAudio = bEnable;
}

BOOL CClientTnel::CheckVideoMsg(PCMDINFO pVideoInfo)
{
////	ASSERT(g_ChanInfoArray.GetSize() <= CMD_PARAM2_LENGTH);
	if(!m_bCanVideo) return FALSE;
	int i;
	ASSERT(0 == strcmp(pVideoInfo->chCmd,MSG_VIDEO));
	SetMsgGram(pVideoInfo);
	if(0 == strcmp(pVideoInfo->chSubCmd,SUBMSG_DISABLE)) {
		return FALSE;
	}
	int nChannelCount = g_ChanInfoArray.GetSize() > CMD_PARAM2_LENGTH ? CMD_PARAM2_LENGTH : g_ChanInfoArray.GetSize();
	int nSendCount = 0;
	for(i = 0; i < nChannelCount; i++) {
		if(pVideoInfo->chParam2[i] & ENABLE_VIDEO_BIT) {
			nSendCount ++;
		}
	}
	TRACE("Get Video Cmd,nSendCOunt:%d",nSendCount);
	DWORD dwTotalPackets = m_dwNwidPackets+60; //// nvcd 2010-06-07s
	DWORD dwNWidPerVChannel = (0 == nSendCount) ? dwTotalPackets : (dwTotalPackets/nSendCount)+1;
	for(i = 0; i < nChannelCount; i++) {
		if(pVideoInfo->chParam2[i] & ENABLE_VIDEO_BIT) {
			m_SendVideoArray[i]->SetNetStatus(dwNWidPerVChannel,m_dwNetRelayInterval);  //// 设置带宽和网络延时
			m_SendVideoArray[i]->StartSending();
			TRACE("DVR Video Start: %d",i);
		}
		else {
			if(m_SendVideoArray[i]->IsSending()) {
				m_SendVideoArray[i]->StopSending();
				TRACE("DVR Video Stop: %d",i);
			}

		}
	}
	return TRUE;
}

BOOL CClientTnel::CheckAudioMsg(PCMDINFO pAudioInfo)
{
////	ASSERT(g_ChanInfoArray.GetSize() <= CMD_PARAM2_LENGTH);
	//// .... 
	if(!m_bCanAudio) return FALSE;
	SetMsgGram(pAudioInfo);

	return TRUE;
}

BOOL CClientTnel::SendGram()
{
	DWORD wait;
	HANDLE hEv[3];
	hEv[0] = m_hNormalGram;
	hEv[1] = m_hNaviData;
	hEv[2] = m_hStopWorking;
	while (TRUE) {
		wait = WaitForMultipleObjects(3,hEv,FALSE,m_nSendGramInterval);
		if(WAIT_OBJECT_0 == wait) {
			if(GRAM_TYPE_MSG == m_nGramType) {
				EnterCriticalSection(&m_csGram);
				g_pManager->PushSData(sizeof(WORK_GRAM),m_ClientAddr,&m_CmdGram);
				m_nSendGramInterval = INFINITE; //// 以防万一
				LeaveCriticalSection(&m_csGram);
			}
			else if(GRAM_TYPE_HSHK == m_nGramType) {
				EnterCriticalSection(&m_csGram);
				//// 发送HandShake ACK
				g_pManager->PushSData(sizeof(HSHK_GRAM),m_ClientAddr,&m_HshkACKGram);
				m_nSendGramInterval = INFINITE;  //// 以防万一
				LeaveCriticalSection(&m_csGram);
				TRACE("Set Hand Shake Gram");
			}
			else if(GRAM_TYPE_TNEL == m_nGramType) {  
				EnterCriticalSection(&m_csGram);
				g_pManager->PushSData(sizeof(OPENTNEL),m_ClientAddr,&m_OpenTnel);
				LeaveCriticalSection(&m_csGram);
				EnterCriticalSection(&m_csCount);
				m_nSendTnelCount++;
				LeaveCriticalSection(&m_csCount);
			}
		}
		else if((WAIT_OBJECT_0+1) == wait) {
			int nLength;
			char chTmp[NAME_BUF_LENGTH];
			char chNaviData[INTERNET_MUP_LENGTH];
			PDATA_GRAM_HEADER pDataHeader = (PDATA_GRAM_HEADER)chNaviData;
			pDataHeader->dwType = FOURCC_NWID;
			pDataHeader->byAct = 0x45; //// 暂时没有用，以后观察
			memset(chTmp,0,NAME_BUF_LENGTH);
			nLength = m_strDvrName.GetLength();
			strcpy(chTmp,m_strDvrName);
			PushinRandom(pDataHeader->chLocalName,chTmp,NAME_BUF_LENGTH,nLength,RANDOM_TRANS_NAMEKEY);
			memset(chTmp,0,NAME_BUF_LENGTH);
			nLength = m_strName.GetLength();
			strcpy(chTmp,m_strName);
			PushinRandom(pDataHeader->chDestName,chTmp,NAME_BUF_LENGTH,nLength,RANDOM_TRANS_NAMEKEY);
			DWORD dwCur = GetTickCount();
			//// 只发送1秒的数据
			int count = 0;
			do {
				g_pManager->PushSData(INTERNET_MUP_LENGTH,m_ClientAddr,chNaviData);
				if(1000 == count) {  //// 这样做的目的是降低网络流量，因为目前网络负载不可能达到完全发送都能够同时接收到
					count = 0;
					Sleep(1);
				}
			} while((GetTickCount()-dwCur) < 1000);
			TRACE("Send NetWid Navigating Count");
		}
		else if(WAIT_TIMEOUT == wait) { //// 打洞
			ASSERT(GRAM_TYPE_TNEL == m_nGramType);
			if(m_bTnelOpened || (m_nSendTnelCount>MAX_TNEL_SEND_COUNT)) {
				m_nSendGramInterval = INFINITE; 
				continue;
			}
			EnterCriticalSection(&m_csGram);
			g_pManager->PushSData(sizeof(OPENTNEL),m_ClientAddr,&m_OpenTnel);
			LeaveCriticalSection(&m_csGram);
			EnterCriticalSection(&m_csCount);
			m_nSendTnelCount++;
			LeaveCriticalSection(&m_csCount);
		}
		else {
			break;
		}
	}
	return TRUE;
}

UINT CClientTnel::SendGramProc(LPVOID p)
{
	CClientTnel *pClt = (CClientTnel*)p;
	pClt->SendGram();
	pClt->DecreaseThreadCount();
	return 1;
}
