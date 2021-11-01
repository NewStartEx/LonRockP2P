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
	m_ClientAddr.sin_port = wPort;  //// �����ǶԷ�ֱ�ӷ�����Port,���Բ�����htons ת��
	if(CONNECT_CMD == byAct) {
		m_bTnelOpened = TRUE;
		m_nGramType = GRAM_TYPE_MSG;
		m_nSendGramInterval = INFINITE;
		m_byMsgAct = CONNECT_MSG;  //// �����ʾ˫���Ѿ���ϵ�ϣ������������á��������������б�������˲�����
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

// �����DVRName��Cmd������ӶԷ���������ȡ��Tnel��������DVRֱ�Ӹ�
BOOL CClientTnel::InitResource(LPCTSTR DVRName)
{
	m_strDvrName = DVRName;
	ASSERT(m_strDvrName.GetLength() < NAME_BUF_LENGTH);

	if(GRAM_TYPE_TNEL == m_nGramType) {
		//// �������������
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

//// ���������ʶͬһ�����Ե����ݣ�������һ�������������Σ�����ʹ��IP�����ƣ�
BOOL CClientTnel::IsMyself(LPCTSTR CltName)
{
	//// CltName ��ʵ��һЩ����
	CString str = CltName;
	return (m_strName==str);
}

//// ʹ�ô˺�����ԭ���Ǽ���Զ��ֹͣ���Ӻ��ֿ�ʼ������������������û�б�ɾ��
//// ���������DVR �����ṩ��
BOOL CClientTnel::RetryTnel(LPCTSTR DVRName)
{
	//// �˴�������裺�������������������Ѿ�ʹ�õ�Client ���Ʋ�ͬ�������Ѿ�ʹ�õ���ֱ�����ӵ�����
	//// ���������Ҫ�����Ѿ�ʹ���������ӵı���

	CString strNewDvrName = DVRName;
	//// ���Ʋ�ͬ����ʱȷ��ԭ��ʹ�õ���ֱ������
	if(!UncaseCompare(strNewDvrName,m_strDvrName)) {
		MakeTnelGram(strNewDvrName,m_strDvrName,TUNNEL_ACK_DUPLINK);
		return FALSE;
	}
	EnterCriticalSection(&m_csCount);
	if(m_bTnelOpened) {
		//// ���ʱ��Ҫ���ָ���ʱ�����������
		m_nLastCmdJump = 0;
	}
	else {
		//// ���ʱ��Ҫ���ָ���ʱ��Ŀ�ͨ��
		m_nSendTnelCount = 0;
		if(INFINITE == m_nSendGramInterval) {
			//// ���Ƿǳ�С�����¼���ֻ���û��Լ��ȴ�һ��ʱ������ֶ���ʼ���Ӳ����������
			m_OpenTnel.dwType = FOURCC_OPEN;
			m_OpenTnel.byAct = TUNNEL_REQUIRE;
			char chTmp[NAME_BUF_LENGTH];
			int nLength = m_strDvrName.GetLength();
			memset(chTmp,0,NAME_BUF_LENGTH);
			strcpy(chTmp,m_strDvrName);
			PushinRandom(m_OpenTnel.chFirstName,chTmp,NAME_BUF_LENGTH,nLength,RANDOM_TRANS_NAMEKEY);
			PushinRandom(m_OpenTnel.chSecondName,chTmp,NAME_BUF_LENGTH,nLength,RANDOM_TRANS_NAMEKEY);
			m_nSendGramInterval = CREATE_TNEL_INTERVAL;
			SetEvent(m_hNormalGram);  //// �����߳�
		}
	}
	LeaveCriticalSection(&m_csCount);

	return TRUE;
}

//// �Լ졣�������FALSE,˵��û�д������壬��Ҫɾ��
BOOL CClientTnel::SelfChecking()
{
	if(m_bForceExit) return FALSE;  //// ǿ��ɾ����־�����û�����
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

//// �Է��������������Ϣ�������ַ��һ�£������Ѿ����ӣ�����Ѿ����ӣ���ֱ��������
//// �������ȷ�ϵ��ǣ������������ʱ�����ͻ�������һ�µ�ʱ�������ַ�϶���һ�µ�
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
	//// ��������£���ַ��ͬ�����ӵ����Ʋ�ͬ��ͬʱ���ֵ�
	//// �����жϵ�����˳��Ҫע��
	memset(chTmp,0,NAME_BUF_LENGTH);
	if(!ThrowRandom(chTmp,pTnelInfo->OpenGram.chSecondName,&nLength,NAME_BUF_LENGTH,NAME_BUF_LENGTH,RANDOM_TRANS_NAMEKEY)) {
		return NTT_UNKNOW_ERROR;
	}
	if(nLength > MAX_NAME_LENGTH || nLength < MIN_NAME_LENGTH) {
		return NTT_UNKNOW_ERROR;
	}
	//// strTmpName = chTmp;
	if(!UncaseCompare(chTmp,m_strDvrName))  {
		//// ��������������Ʋ�ͬ��������Ϊͬһ���ͻ��˵ĵڶ�������
		MakeTnelGram(chTmp,m_strDvrName,TUNNEL_ACK_DUPLINK);
		return TUNNEL_ACK_DUPLINK;
	}
	/*
	if((pTnelInfo->addr.sin_addr.s_addr!=m_ClientAddr.sin_addr.s_addr) ||
		pTnelInfo->addr.sin_port!=m_ClientAddr.sin_port) {
		//// ����������ӵ�ַ��ͬ��������Ϊͬһ���ͻ��˵ĵڶ�������
		MakeTnelGram(strTmpName,m_strDvrName,TUNNEL_ACK_DUPLINK);
		return TUNNEL_ACK_DUPLINK;
	}
	*/
	if(TUNNEL_REQUIRE==pTnelInfo->OpenGram.byAct) {
		if(!m_bTnelOpened) {
			//// Ϊ��ȷ���Է��ĵ�ַ�����յ������������ַΪ�Է������ĵ�ַ
			EnterCriticalSection(&m_csGram);
			memcpy(&m_ClientAddr,&pTnelInfo->addr,sizeof(SOCKADDR_IN));
			LeaveCriticalSection(&m_csGram);
			m_bTnelOpened = TRUE;
			m_byMsgAct = TUNNEL_MSG;
		}
		MakeTnelGram(m_strDvrName,m_strDvrName,TUNNEL_ACK_SUCCESS);
		return TUNNEL_ACK_SUCCESS;
	}
	//// ����˵���Է��������ǻ�Ӧ��Ϣ
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
	ASSERT(m_strName == chTmp);  //// ��Ϊ����һ���Ѿ��жϹ���
	if(!ThrowRandom(chTmp,pHshkInfo->HshkGram.chDestName,&nLength,NAME_BUF_LENGTH,NAME_BUF_LENGTH,RANDOM_TRANS_NAMEKEY)) {
		return FALSE;
	}
	if(nLength > MAX_NAME_LENGTH || nLength < MIN_NAME_LENGTH) {
		return FALSE;
	}
	
	//// �����ʽ��ȷ,������
	if(m_bTnelOpened && (!UncaseCompare(m_strDvrName,chTmp))) return FALSE;  //// һ���ͻ�ʹ���������ϵķ�ʽ����DVR
	
	//// �����ʽ��ȷ
	EnterCriticalSection(&m_csCount);
	m_nLastCmdJump = 0;
	LeaveCriticalSection(&m_csCount);
	if(!m_bTnelOpened) {
		//// �ж��Ƿ���ͨ��ֱ�����ӵ�;�����ġ����������ֿ����ԣ�
		//// 1���Է��Ѿ��յ���ߵĴ򶴱��ģ�ȷ�������ͨ��Ȼ���ͱ��ģ�����ʱ���ػ�û���յ��Է��Ļظ����ģ�
		//// 2���Է�ͨ��ֱ�����ӵķ�ʽ����
		EnterCriticalSection(&m_csGram);
		if((pHshkInfo->addr.sin_addr.s_addr!=m_ClientAddr.sin_addr.s_addr) ||
			(pHshkInfo->addr.sin_port!=m_ClientAddr.sin_port)) {
			//// ˵�����������ֱ�����ӹ�����
			m_ClientAddr = pHshkInfo->addr;
			m_byMsgAct = CONNECT_MSG;
			m_strDvrName = chTmp; 
		}
		else {
			//// �Է��Ѿ��յ��򶴱��ģ����ǻظ�������ĳ��ԭ��û���յ������ҷ������������
			m_byMsgAct = TUNNEL_MSG;
			//// Ϊ���������������һ��DVRName��ֵ��������һ�������Ե���ֱ�����ӵĵ�ַ�������ַ��ͬ��
			//// ��Client��ֱ������Internet�ϡ���DVR��ͨ���˿�ӳ�����ֱ������������ʽ���ӣ�
			//// ��ʱClient������DVR���ƿ��ܺ�������Ʋ�ͬ��
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

//// �п���ֱ�����ӵġ����ʱ�������ͨ�������棬���ÿ�������ˣ����غ��ɹ����̴߳�����������
//// �����ʱ�������û�п�ͨ�����ж��Ƿ��������һ�����ӡ�ͬʱ��������ظ�������
BOOL CClientTnel::CheckCmdGram(PCMDDESC pWorkInfo)
{
	char chTmp[NAME_BUF_LENGTH];
	int nLength = 0;
	//// TRACE("Get Client Cmd");
	if(pWorkInfo->WorkGram.dwType != FOURCC_PUSH) return FALSE;
	if(!ThrowRandom(chTmp,pWorkInfo->WorkGram.chLocalName,&nLength,NAME_BUF_LENGTH,NAME_BUF_LENGTH,RANDOM_TRANS_NAMEKEY)) {
		return FALSE;
	}
	ASSERT(m_strName == chTmp);  //// ��Ϊ����һ���Ѿ��жϹ���
	if(!ThrowRandom(chTmp,pWorkInfo->WorkGram.chDestName,&nLength,NAME_BUF_LENGTH,NAME_BUF_LENGTH,RANDOM_TRANS_NAMEKEY)) {
		return FALSE;
	}
	if(nLength > MAX_NAME_LENGTH || nLength < MIN_NAME_LENGTH) {
		return FALSE;
	}
	if(!m_bTnelOpened) return FALSE; //// ������ǰ�İ汾���ݣ���������
	if(m_bTnelOpened && (!UncaseCompare(m_strDvrName,chTmp))) return FALSE;  //// һ���ͻ�ʹ���������ϵķ�ʽ����DVR
	
	//// �����ʽ��ȷ
	EnterCriticalSection(&m_csCount);
	m_nLastCmdJump = 0;
	LeaveCriticalSection(&m_csCount);

	return TRUE;
}

BOOL CClientTnel::CheckVResdGram(PVRESDPACKET pVResdPackt,DWORD dwIPAddr,LPCTSTR DvrName)
{
	if(m_ClientAddr.sin_addr.s_addr != dwIPAddr) {
		//// ���ﲻ�ж϶˿ںţ���Ϊ�Ժ�Ҳ���ͨ����ͬ�˿ڷ��ͻش���Ϣ
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
	m_nLastCmdJump = 0; //// ˵���Է����ڻ����ʱ�����Ϊ��������̫����������ʧ��
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
	//// ....������С��һ����ֵ��ʱ�򣬾Ͳ�����Ƶ��
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
			m_SendVideoArray[i]->SetNetStatus(dwNWidPerVChannel,m_dwNetRelayInterval);  //// ���ô����������ʱ
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
				m_nSendGramInterval = INFINITE; //// �Է���һ
				LeaveCriticalSection(&m_csGram);
			}
			else if(GRAM_TYPE_HSHK == m_nGramType) {
				EnterCriticalSection(&m_csGram);
				//// ����HandShake ACK
				g_pManager->PushSData(sizeof(HSHK_GRAM),m_ClientAddr,&m_HshkACKGram);
				m_nSendGramInterval = INFINITE;  //// �Է���һ
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
			pDataHeader->byAct = 0x45; //// ��ʱû���ã��Ժ�۲�
			memset(chTmp,0,NAME_BUF_LENGTH);
			nLength = m_strDvrName.GetLength();
			strcpy(chTmp,m_strDvrName);
			PushinRandom(pDataHeader->chLocalName,chTmp,NAME_BUF_LENGTH,nLength,RANDOM_TRANS_NAMEKEY);
			memset(chTmp,0,NAME_BUF_LENGTH);
			nLength = m_strName.GetLength();
			strcpy(chTmp,m_strName);
			PushinRandom(pDataHeader->chDestName,chTmp,NAME_BUF_LENGTH,nLength,RANDOM_TRANS_NAMEKEY);
			DWORD dwCur = GetTickCount();
			//// ֻ����1�������
			int count = 0;
			do {
				g_pManager->PushSData(INTERNET_MUP_LENGTH,m_ClientAddr,chNaviData);
				if(1000 == count) {  //// ��������Ŀ���ǽ���������������ΪĿǰ���縺�ز����ܴﵽ��ȫ���Ͷ��ܹ�ͬʱ���յ�
					count = 0;
					Sleep(1);
				}
			} while((GetTickCount()-dwCur) < 1000);
			TRACE("Send NetWid Navigating Count");
		}
		else if(WAIT_TIMEOUT == wait) { //// ��
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
