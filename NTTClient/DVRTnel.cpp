#include "stdafx.h"
#include "NTTProtocol.h"
#include "Global.h"
#include "CltGlobal.h"
#include "NTTClientFun.h"
#include "DVRTnel.h"
#include "CltManager.h"

extern STATIONLINKED g_StationLinked;
extern RECVFROMSTATION g_RecvFromStation;
extern STATIONLEFT g_StationLeft;

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
	memset(&m_HshkGram,0,sizeof(HSHK_GRAM));
	memset(&m_CmdGram,0,sizeof(WORK_GRAM));
	memset(&m_VideoMsg,0,sizeof(CMDINFO));
	memset(&m_AudioMsg,0,sizeof(CMDINFO));
	memset(&m_LiveInfo,0,sizeof(CMDINFO));
	memset(&m_VideoInfo,0,sizeof(CMDINFO));
	memset(&m_AudioInfo,0,sizeof(CMDINFO));
	memset(&m_NetSAddr,0,sizeof(SOCKADDR_IN));

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
	m_dwBandWidth = DEFAULT_NWID_PACKETS;
	m_dwNaviPackCount = 0;
	m_dwRelayInterval = DEFAULT_NETRELAY_INTERVAL;
	m_dwRelayStart = 0;
	m_bNaviDataArrived = FALSE;
	m_bOnlyNeedAddr = FALSE;
	m_bOnline = FALSE;
	m_hSendGram = CreateEvent(NULL,FALSE,FALSE,NULL);
	m_hNaviData = CreateEvent(NULL,FALSE,FALSE,NULL);
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
	//// 更新通讯使用名称，主要是改掉".",避免恶意解码
	//// 如果使用一个很长的域名，有可能出现这种情况
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
	memset(&m_HshkGram,0,sizeof(HSHK_GRAM));
	memset(&m_CmdGram,0,sizeof(WORK_GRAM));
	memset(&m_VideoMsg,0,sizeof(CMDINFO));
	memset(&m_AudioMsg,0,sizeof(CMDINFO));
	memset(&m_LiveInfo,0,sizeof(CMDINFO));
	memset(&m_VideoInfo,0,sizeof(CMDINFO));
	memset(&m_AudioInfo,0,sizeof(CMDINFO));
	memset(&m_DvrAddr,0,sizeof(SOCKADDR_IN));
	m_DvrAddr.sin_family = AF_INET;
	m_DvrAddr.sin_addr.s_addr = Name2IP(DvrIPName);
	m_DvrAddr.sin_port = htons(wDvrPort);
	m_LinkStatus = Link_HandShaking;
	m_bFound = FALSE;
	m_bHopeless = FALSE;
	m_nSendReqCount = 0;
	m_nSendTnelCount = 0;
	m_nLastRcvTick = 0;
	m_nKeepliveTick = 0;
	m_nThreadCount = 0;
	m_uExitReason = 0;
	m_dwBandWidth = DEFAULT_NWID_PACKETS;
	m_dwNaviPackCount = 0;
	m_dwRelayInterval = DEFAULT_NETRELAY_INTERVAL;
	m_dwRelayStart = 0;
	m_bNaviDataArrived = FALSE;
	m_bOnlyNeedAddr = FALSE;
	m_bOnline = FALSE;
	m_hSendGram = CreateEvent(NULL,FALSE,FALSE,NULL);
	m_hNaviData = CreateEvent(NULL,FALSE,FALSE,NULL);
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
	while(m_FillVideoArray.GetSize() > 0) {
		delete m_FillVideoArray[0];
		m_FillVideoArray.RemoveAt(0);
	}
	if(g_StationLeft != NULL) {
		//// 如果是用户强制，用户使用消息和回调函数里的消息可能造成死锁
		g_StationLeft(m_strShowName,m_uExitReason);
	}
	DeleteCriticalSection(&m_csGram);
	DeleteCriticalSection(&m_csCount);
	CloseHandle(m_hSendGram);
	CloseHandle(m_hNaviData);
	CloseHandle(m_hStopWorking);
	m_TotalCount--;
}

BOOL CDvrTnel::InitResource(PCMDINFO pFirstCmd)
{
	//// 这里要判断是什么方式生成的类
	//// 发送请求第一次请求
	MakeName();
	m_HshkGram.dwType = FOURCC_HSHK;
	m_CmdGram.dwType = FOURCC_PUSH;
	//// strcpy(m_CmdGram.chDestName,m_strDvrName);  发送命令的时候随机修改
	//// strcpy(m_CmdGram.chLocalName,m_chName);
	if(NULL == pFirstCmd) {
		m_bOnlyNeedAddr = TRUE;
		if(Link_Applying != m_LinkStatus) {
			return FALSE; //// 如果不是用CS2方式，则没有必要
		}
	}
	else {
		memcpy(&m_LiveInfo,pFirstCmd,sizeof(CMDINFO));
		memcpy(&m_CmdGram.Info,pFirstCmd,sizeof(CMDINFO));
	}
	if(Link_Applying == m_LinkStatus) {
		m_CmdGram.byAct = TUNNEL_CMD;
		m_ReqGram.dwType = FOURCC_HOLE;
		/*
		strcpy(m_ReqGram.chDstName,m_strDvrName);
		strcpy(m_ReqGram.chName,m_chName);
		*/
		EncodeCopyName(m_ReqGram.chDstName,m_strDvrName);
		EncodeCopyName(m_ReqGram.chName,m_chName);
		if(NULL == pFirstCmd) {
			m_ReqGram.byAct = TUNNEL_ASKIPADDR;
		}
		else {
			m_ReqGram.byAct = TUNNEL_REQUIRE;
		}
		g_pManager->PushSData(sizeof(CLTREQ_GRAM),m_NetSAddr,&m_ReqGram);
	}
	else { ///// 先发送传输请求 nvcd
		ASSERT(Link_HandShaking == m_LinkStatus);
		m_CmdGram.byAct = CONNECT_CMD;
		m_HshkGram.byAct = HSHK_STARTING;
		EncodeCopyName(m_HshkGram.chDestName,m_strDvrName);
		EncodeCopyName(m_HshkGram.chLocalName,m_chName);
		g_pManager->PushSData(sizeof(HSHK_GRAM),m_DvrAddr,&m_HshkGram);
		/* nvcd 要放到Handshaking处理中
		EncodeCopyName(m_CmdGram.chDestName,m_strDvrName);
		EncodeCopyName(m_CmdGram.chLocalName,m_chName);
		g_pManager->PushSData(sizeof(WORK_GRAM),m_DvrAddr,&m_CmdGram);
		*/
	}
	DWORD dwID;
	IncreaseThreadCount();
	CreateThread(NULL,0,(LPTHREAD_START_ROUTINE)CDvrTnel::SendGramProc,this,0,&dwID);
	IncreaseThreadCount();
	CreateThread(NULL,0,(LPTHREAD_START_ROUTINE)CDvrTnel::CalcBandWidthProc,this,0,&dwID);
	
	return TRUE;
}

BOOL CDvrTnel::CreateVideoChannel(int nChanCount)
{
	if(0 != m_FillVideoArray.GetSize()) {
		return FALSE;
	}
	int count = nChanCount > CMD_PARAM2_LENGTH ? CMD_PARAM2_LENGTH : nChanCount;
	for(int i = 0; i < nChanCount; i++) {
		CFillVideo * pFillVideo = new CFillVideo(i);
		pFillVideo->InitResource(m_strDvrName,m_chName,m_DvrAddr);
		m_FillVideoArray.Add(pFillVideo);
	}
	
	return TRUE;
}

int CDvrTnel::GetChannelCount()
{
	return m_FillVideoArray.GetSize();
}

BOOL CDvrTnel::IsMyself(LPCTSTR ShowName)
{
	return UncaseCompare(m_strShowName,ShowName);
}

BOOL CDvrTnel::FindDvr(LPCTSTR DvrName) 
{
	return UncaseCompare(m_strDvrName,DvrName);
}

void CDvrTnel::SetAlive()
{
	//// 因为传输数据的时候要尽量保证速度，因此使用互斥量要尽量快，所以用这个函数来保证该类是可以使用的
	m_nSendReqCount = 0;
	m_nSendTnelCount = 0;
	m_nLastRcvTick = 0;
}

void CDvrTnel::SetExitMark(UINT uExitReason)
{
	m_uExitReason = uExitReason;
	m_bHopeless = TRUE;
}

//// 自检。如果返回FALSE,说明没有存在意义，需要删除
BOOL CDvrTnel::SelfChecking()
{
	BOOL bExit = TRUE;
	if(m_bHopeless) return FALSE;
	switch(m_LinkStatus) {
	case Link_Applying:
		if(m_nSendReqCount > MAX_REQ_COUNT) {
			m_uExitReason = EXIT_NETS_LOST;
			if(g_StationLinked != NULL) {
				g_StationLinked(m_strShowName,0,FALSE);
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
			if(g_StationLinked != NULL) {
				g_StationLinked(m_strShowName,m_DvrAddr.sin_addr.s_addr,FALSE);
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
	case Link_HandShaking:
		if(m_nLastRcvTick > MAX_REPLY_INTERVAL) {
			if(m_bOnline)
				m_uExitReason = EXIT_VER_OLD;
			else
				m_uExitReason = EXIT_DVR_LOST;
			bExit = FALSE;
		}
		else {
			m_nLastRcvTick ++;
			EnterCriticalSection(&m_csGram);
			EncodeCopyName(m_HshkGram.chDestName,m_strDvrName);
			EncodeCopyName(m_HshkGram.chLocalName,m_chName);
			LeaveCriticalSection(&m_csGram);
			SetEvent(m_hSendGram);
		}
		break;
	case Link_Working:
		//// TRACE("Clt Selfcheck link:%d",m_nLastRcvTick);
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
				m_CmdGram.Info = m_LiveInfo;
				LeaveCriticalSection(&m_csGram);
				//// TRACE("Clt Set Live :%s",m_LiveInfo.chCmd);
				SetEvent(m_hSendGram);
				m_nKeepliveTick = 0;
			}
			else {
				m_nKeepliveTick++;
				EnterCriticalSection(&m_csGram);
				if(!AllVideoNotified()) {
					TRACE("not all video notified");
					EncodeCopyName(m_CmdGram.chDestName,m_strDvrName);
					EncodeCopyName(m_CmdGram.chLocalName,m_chName);
					m_CmdGram.Info = m_VideoInfo;
					SetEvent(m_hSendGram);
				}
				LeaveCriticalSection(&m_csGram);
			}
		}
		break;
	}
	return bExit;
}

BOOL CDvrTnel::AllVideoNotified()
{
	BYTE byReq,byBck;
	for(int i = 0; i < m_FillVideoArray.GetSize(); i++) {
		byReq = BYTE(m_VideoInfo.chParam2[i] & ENABLE_VIDEO_BIT);
		byBck = BYTE(m_VideoMsg.chParam2[i] & ENABLE_VIDEO_BIT);
		if(byReq != byBck) {
			TRACE("CLT:chan %d need change:req:%d,back:%d",i,byReq,byBck);
			return FALSE;
		}
	}
	return TRUE;
}

BOOL CDvrTnel::AllAudioNotified()
{
	return TRUE;
}

void CDvrTnel::StartVideoTran(int chan)
{
	if(Link_Working != m_LinkStatus) return;
	if(chan >= m_FillVideoArray.GetSize()) 
		return;

	m_FillVideoArray[chan]->SetRelayInterval(m_dwRelayInterval);
	m_FillVideoArray[chan]->FillStart();
	EnterCriticalSection(&m_csGram);
	EncodeCopyName(m_CmdGram.chDestName,m_strDvrName);
	EncodeCopyName(m_CmdGram.chLocalName,m_chName);
	strcpy(m_VideoInfo.chCmd,CMD_VIDEO);
	m_VideoInfo.chParam2[chan] |= ENABLE_VIDEO_BIT;
	TRACE("Clt Enablechan:%d,req:%d",chan,m_VideoInfo.chParam2[chan]);
	LeaveCriticalSection(&m_csGram);
	
	SetEvent(m_hSendGram);
}

void CDvrTnel::StopVideoTran(int chan)
{
	if(Link_Working != m_LinkStatus) return ;
	if(chan >= m_FillVideoArray.GetSize()) 
		return;
	m_FillVideoArray[chan]->FillStop();
	EnterCriticalSection(&m_csGram);
	EncodeCopyName(m_CmdGram.chDestName,m_strDvrName);
	EncodeCopyName(m_CmdGram.chLocalName,m_chName);
	strcpy(m_VideoInfo.chCmd,CMD_VIDEO);
	m_VideoInfo.chParam2[chan] &= ~ENABLE_VIDEO_BIT;
	TRACE("CLT VIDEO:%d,Req:%d",chan,m_VideoInfo.chParam2[chan]);
	LeaveCriticalSection(&m_csGram);

	SetEvent(m_hSendGram);
}

void CDvrTnel::StartAllVideoTran()
{
	if(Link_Working != m_LinkStatus) return ;
	TRACE("Clt Start All video");
	EnterCriticalSection(&m_csGram);
	for(int i = 0; i < m_FillVideoArray.GetSize(); i++) {
		m_VideoInfo.chParam2[i] |= ENABLE_VIDEO_BIT;
		m_FillVideoArray[i]->SetRelayInterval(m_dwRelayInterval);
		m_FillVideoArray[i]->FillStart();
	}
	EncodeCopyName(m_CmdGram.chDestName,m_strDvrName);
	EncodeCopyName(m_CmdGram.chLocalName,m_chName);
	strcpy(m_VideoInfo.chCmd,CMD_VIDEO);
	LeaveCriticalSection(&m_csGram);

	SetEvent(m_hSendGram);
}

void CDvrTnel::StopAllVideoTran()
{
	if(Link_Working != m_LinkStatus) return ;
	EnterCriticalSection(&m_csGram);
	TRACE("Clt Stop All Video");
	for(int i = 0; i < m_FillVideoArray.GetSize(); i++) {
		m_VideoInfo.chParam2[i] &= ~ENABLE_VIDEO_BIT;
		m_FillVideoArray[i]->FillStop();
	}
	EncodeCopyName(m_CmdGram.chDestName,m_strDvrName);
	EncodeCopyName(m_CmdGram.chLocalName,m_chName);
	strcpy(m_VideoInfo.chCmd,CMD_VIDEO);
	LeaveCriticalSection(&m_csGram);

	SetEvent(m_hSendGram);
}

BOOL CDvrTnel::StartRecord(int chan,LPCTSTR FileName)
{
	if(Link_Working != m_LinkStatus) return FALSE;
	if(chan >= m_FillVideoArray.GetSize()) return FALSE;
	return m_FillVideoArray[chan]->StartRecord(FileName);
}

BOOL CDvrTnel::StopRecord(int chan)
{
	if(Link_Working != m_LinkStatus) return FALSE;
	if(chan >= m_FillVideoArray.GetSize()) return FALSE;
	m_FillVideoArray[chan]->StopRecord();

	return TRUE;
}

DWORD CDvrTnel::GetRecFilePos(int chan)
{
	if(Link_Working != m_LinkStatus) return 0;
	if(chan >= m_FillVideoArray.GetSize()) return 0;

	return m_FillVideoArray[chan]->GetRecFilePos();
}

BOOL CDvrTnel::Snap(int chan,LPCTSTR FileName)
{
	if(Link_Working != m_LinkStatus) return FALSE;
	if(chan >= m_FillVideoArray.GetSize()) return FALSE;
	return m_FillVideoArray[chan]->Snap(FileName);
}

BOOL CDvrTnel::SetVideoPos(int chan,int left,int top,int width,int height)
{
	if(chan >= m_FillVideoArray.GetSize()) 
		return FALSE;
	m_FillVideoArray[chan]->SetVideoRect(left,top,width,height);

	return TRUE;
}

BOOL CDvrTnel::ShowVideo(int chan,BOOL bShow)
{
	if(chan >= m_FillVideoArray.GetSize()) 
		return FALSE;
	m_FillVideoArray[chan]->ShowVideo(bShow);

	return TRUE;
}

BOOL CDvrTnel::RefreshVideo(int chan)
{
	if(chan >= m_FillVideoArray.GetSize()) 
		return FALSE;
	m_FillVideoArray[chan]->RefreshVideo();

	return TRUE;
}

UINT CDvrTnel::SetCmdGram(PCMDINFO pCmd)
{
	if(Link_Working != m_LinkStatus) return CMD_LINKING;
	EnterCriticalSection(&m_csGram);
	EncodeCopyName(m_CmdGram.chDestName,m_strDvrName);
	EncodeCopyName(m_CmdGram.chLocalName,m_chName);
	memcpy(&m_CmdGram.Info,pCmd,sizeof(CMDINFO));
	memcpy(&m_LiveInfo,pCmd,sizeof(CMDINFO));
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
		if(TUNNEL_ACK_OFFLINE == pTnel->byAct) {
			m_uExitReason = EXIT_DVR_OFFLINE;
		}
		else if(TUNNEL_ACK_NODEVICE == pTnel->byAct) {
			m_uExitReason = EXIT_DVR_NONAME;
		}
		else if(TUNNEL_ACK_DVRBUSY == pTnel->byAct) {
			m_uExitReason = EXIT_DVR_BUSY;
		}
		else {
			m_uExitReason = EXIT_DVR_LOST;
		}
		m_bHopeless = TRUE;
		return FALSE;
	}
	//// 名称要一致,否则就是一个名称连接了两个DVR
	CString strDestName = DecodeName(pTnel->chDstName);
	if(!UncaseCompare(m_strDvrName,strDestName)) {
		m_uExitReason = EXIT_UNKNOW;
		return FALSE;
	}
	if(m_bOnlyNeedAddr) {
		if(g_StationLinked != NULL) {
			g_StationLinked(m_strShowName,pTnel->dwDstIPaddr,FALSE);
		}
		m_uExitReason = EXIT_FORCE_EXIT;
		m_bHopeless = TRUE;
		return FALSE;
	}
	EnterCriticalSection(&m_csGram);
	m_DvrAddr.sin_family = AF_INET;
	m_DvrAddr.sin_addr.s_addr = pTnel->dwDstIPaddr;
	m_DvrAddr.sin_port = pTnel->wDstPort;  //// 直接发来的，就不用htons来转换了
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

BYTE CDvrTnel::DealwithCnctGram(POPENTNEL pOpen,SOCKADDR_IN IPAddr)
{
	if(Link_Working == m_LinkStatus || Link_HandShaking == m_LinkStatus) return TUNNEL_ACK_SUCCESS;
	//// 有可能还是Link_Applying的时候，对方就发来这个数据包。
	//// 这个是可以确认.byAct是TUNNEL_REQUIRE。如果是 TUNNEL_ACK_DUPLINK，则在管理层进行处理了，不用进入这里
	//// 就没有必要再发确认了，直接发送命令就可以
	if(TUNNEL_REQUIRE!=pOpen->byAct && TUNNEL_ACK_SUCCESS!=pOpen->byAct) {
		//// 比如DVR连接数已经满了，理论上还有可能同一个客户名字的第二个连接
		m_bHopeless = TRUE;
		m_uExitReason = EXIT_DVR_BUSY;
		return pOpen->byAct;
	}
	EnterCriticalSection(&m_csGram);
	m_DvrAddr = IPAddr;
	m_bOnline = TRUE;
	m_LinkStatus = Link_HandShaking;
	//// 这里发送握手请求
	/*
	EncodeCopyName(m_CmdGram.chDestName,m_strDvrName);
	EncodeCopyName(m_CmdGram.chLocalName,m_chName);
	//// 虽然这里已经连接上，但是由于有些直接连接的，所以在MSG里面确认是否连接的回调函数
	*/
	m_HshkGram.byAct = HSHK_STARTING;
	EncodeCopyName(m_HshkGram.chDestName,m_strDvrName);
	EncodeCopyName(m_HshkGram.chLocalName,m_chName);
	LeaveCriticalSection(&m_csGram);
	SetEvent(m_hSendGram);

	return TUNNEL_ACK_SUCCESS;
}

BOOL CDvrTnel::DealwithHshkACK(BYTE byAct,LPCTSTR pCltName)
{
	if(Link_HandShaking != m_LinkStatus) {
		//// 可能已经转到CMD状态了
		return TRUE;
	}
	if(0 != strcmp(m_chName,pCltName)) {
		//// 可能是恶意攻击
		return FALSE;
	}
	if(!m_bFound) {
		//// 第一次连接要通知回调函数
		if(g_StationLinked != NULL) {
			g_StationLinked(m_strShowName,m_DvrAddr.sin_addr.s_addr,TRUE);
		}
		m_bFound = TRUE;		
	}
	if(HSHK_ACK_STARTING == byAct) {
		//// 开始测试网络
		m_dwNaviPackCount = 0;
		EnterCriticalSection(&m_csGram);
		m_HshkGram.byAct = HSHK_NAVIGATING;
		EncodeCopyName(m_HshkGram.chDestName,m_strDvrName);
		EncodeCopyName(m_HshkGram.chLocalName,m_chName);
		g_pManager->PushSData(sizeof(HSHK_GRAM),m_DvrAddr,&m_HshkGram);
		TRACE("Client send first netvigate");
		m_dwRelayStart = GetTickCount();
		LeaveCriticalSection(&m_csGram);
	}
	else if(HSHK_ACK_NETSTATUS == byAct) {
		//// 开始命令传输过程
		EnterCriticalSection(&m_csGram);
		m_LinkStatus = Link_Working;
		EncodeCopyName(m_CmdGram.chDestName,m_strDvrName);
		EncodeCopyName(m_CmdGram.chLocalName,m_chName);
		LeaveCriticalSection(&m_csGram);
		SetEvent(m_hSendGram);
		TRACE("Client Handshaking finish");
	}
	return TRUE;
}

BOOL CDvrTnel::DealwithMsgGram(PCMDINFO pWorkInfo,LPCTSTR pCltName)
{
	if(Link_Working != m_LinkStatus) {
		//// 可能是恶意攻击
		return FALSE;
	}
	if(0 != strcmp(m_chName,pCltName)) {
		//// 可能是恶意攻击
		return FALSE;
	}
	/*  肯定在握手状态已经找到了
	if(!m_bFound) {
		if(g_StationLinked != NULL) {
			g_StationLinked(m_strShowName,m_DvrAddr.sin_addr.s_addr,TRUE);
		}
		m_bFound = TRUE;		
	}
	*/
	if(0 == strcmp(pWorkInfo->chCmd,MSG_VIDEO)) {
		EnterCriticalSection(&m_csGram);
		memcpy(&m_VideoMsg,pWorkInfo,sizeof(CMDINFO));
		LeaveCriticalSection(&m_csGram);
	}
	else if(0 == strcmp(pWorkInfo->chCmd,MSG_AUDIO)) {
		EnterCriticalSection(&m_csGram);
		memcpy(&m_AudioMsg,pWorkInfo,sizeof(CMDINFO));
		LeaveCriticalSection(&m_csGram);
	}
	else if(0 == strcmp(pWorkInfo->chCmd,MSG_EXIT)) {
		m_uExitReason = EXIT_DVR_LOST;
		m_bHopeless = TRUE;
	}
	else {
		m_nLastRcvTick = 0;
		if(g_RecvFromStation != NULL) {
			g_RecvFromStation(m_strShowName,(PNETPACKET)pWorkInfo);
		}
	}
	//// MakeLiveGram();//// 这个时候可以发送心跳命令了，否则一直发送最后一个命令
	return TRUE;
}

BOOL CDvrTnel::DealwithVideoGram(PVPACKHEADER pVHeader,PBYTE pVData,LPCTSTR pCltName)
{
	if(Link_Working != m_LinkStatus) {
		//// 可能是恶意攻击
		return FALSE;
	}
	if(0 != strcmp(m_chName,pCltName)) {
		//// 可能是恶意攻击
		return FALSE;
	}
	if(pVHeader->byChanNum >= (BYTE)m_FillVideoArray.GetSize()) {
		//// 可能数据传输错误
		return FALSE;
	}
	
	return m_FillVideoArray[pVHeader->byChanNum]->FillPacket(pVHeader,pVData);
}

BOOL CDvrTnel::DealwithNaviData(LPCTSTR pCltName)
{
//// 	TRACE("Get Navi Data,PackCount:%d",m_dwNaviPackCount);
	if(Link_HandShaking != m_LinkStatus) {
		return FALSE;
	}
	if(0 != strcmp(m_chName,pCltName)) {
		return FALSE;
	}
	if(!m_bNaviDataArrived) {
		m_dwRelayInterval = (GetTickCount()-m_dwRelayStart+50);    ///// 增加50ms,确认时间长一点，等待时间
		m_bNaviDataArrived = TRUE;
		SetEvent(m_hNaviData);
	}
	m_dwNaviPackCount ++;
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

/* obsolete
void CDvrTnel::MakeLiveGram()
{
	EnterCriticalSection(&m_csGram);
	m_CmdGram.Info = m_LiveInfo;
	LeaveCriticalSection(&m_csGram);
	m_nKeepliveTick = 0;
}
*/

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
			if(Link_Applying == m_LinkStatus) {
				g_pManager->PushSData(sizeof(CLTREQ_GRAM),m_NetSAddr,&m_ReqGram);
			}
			else if(Link_Creating == m_LinkStatus) {
				g_pManager->PushSData(sizeof(OPENTNEL),m_DvrAddr,&m_OpenGram);
			}
			else if(Link_HandShaking == m_LinkStatus) {
				g_pManager->PushSData(sizeof(HSHK_GRAM),m_DvrAddr,&m_HshkGram);
			}
			else {
				g_pManager->PushSData(sizeof(WORK_GRAM),m_DvrAddr,&m_CmdGram);
			}
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

BOOL CDvrTnel::CalcBandWidth()
{
	DWORD dwWait;
	HANDLE hEv[2];
	hEv[0] = m_hNaviData;
	hEv[1] = m_hStopWorking;
	DWORD dwTimeout = INFINITE;
	while (TRUE) {
		dwWait = WaitForMultipleObjects(2,hEv,FALSE,dwTimeout);
		if(WAIT_OBJECT_0 == dwWait) {
			dwTimeout = 1000;
			continue;
		}
		else if(WAIT_TIMEOUT == dwWait) {
			m_dwBandWidth = m_dwNaviPackCount;  //// 每秒钟收到的数据量
			/// 发送握手确认
			EnterCriticalSection(&m_csGram);
			m_HshkGram.byAct = HSHK_NETSTATUS;
			EncodeCopyName(m_HshkGram.chDestName,m_strDvrName);
			EncodeCopyName(m_HshkGram.chLocalName,m_chName);
			TRACE("Client send handshaking Status,Nwid:%d,Interval:%d",m_dwBandWidth,m_dwRelayInterval);
			m_HshkGram.dwParam1 = m_dwBandWidth; 
			m_HshkGram.dwParam2 = m_dwRelayInterval;
			LeaveCriticalSection(&m_csGram);
			SetEvent(m_hSendGram);
			/*
			if(m_dwBandWidth < 200) {
				m_dwRelayInterval = (m_dwRelayInterval < 1000) ? 1000 : m_dwRelayInterval;
			}
			*/
			break;
		}
		else {
			break;
		}
	}

	return TRUE;
}

UINT CDvrTnel::CalcBandWidthProc(LPVOID p)
{
	CDvrTnel* pDvr = (CDvrTnel*)p;
	pDvr->CalcBandWidth();
	pDvr->DecreaseThreadCount();

	return 1;
}