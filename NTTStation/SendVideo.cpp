#include "stdafx.h"
#include "SendVideo.h"
#include "ChannelInfo.h"
#include "Global.h"
#include "DVRManager.h"

CSendVideo::CSendVideo(int ChanIndex) :
m_bSending(FALSE),
m_bSendFinish(TRUE)
{
	memset(&m_sockCltAddr,0,sizeof(SOCKADDR_IN));
	m_nIndex = ChanIndex;
	memset(&m_vResdHeader,0,sizeof(VIDEO_FRAME_HEADER));
	m_uFrmBufLength = NORMAL_FRM_LENGTH + sizeof(VIDEO_FRAME_HEADER);
	PVOID pBuf = new BYTE[m_uFrmBufLength];
	m_pFrmPacket = (PVIDEO_FRAME_PACKET)pBuf;
	memset(m_pFrmPacket,0,m_uFrmBufLength);
	
	m_dwSendPackCount = 0;
	m_bWaitIFrmACK = FALSE;
	m_dwNwidPackets = DEFAULT_NWID_PACKETS/8; //// Ĭ��8ͨ������
	m_dwRelayInterval = DEFAULT_NETRELAY_INTERVAL;
	
	m_uResdFrmLength = NORMAL_FRM_LENGTH;
	m_pResdFrmBuffer = new BYTE[m_uResdFrmLength];
	memset(m_pResdFrmBuffer,0,m_uResdFrmLength);
	m_pSendFrmGram = new BYTE[INTERNET_MUP_LENGTH];
	memset(m_pSendFrmGram,0,INTERNET_MUP_LENGTH);
	m_pResdGram = new BYTE[INTERNET_MUP_LENGTH];
	memset(m_pResdGram,0,INTERNET_MUP_LENGTH);
	for(int i = 0; i < 2; i++) {
		for(int j = 0; j < PACK_BUF_COUNT; j++) {
			m_pSentPack[i][j] = new BYTE[INTERNET_MUP_LENGTH];
			memset(m_pSentPack[i][j],0,INTERNET_MUP_LENGTH);
		}
	}
	m_nCurPackBufArrayIndex = 0;
	m_nCurPackBufIndex = 0;
	m_nCurPackFrmTableIndex = 0;
	m_wPackIndex = 0; //// ͨ������ͷVIDE���ж��Ƿ��ǿհ�
	m_wACKFrmIndex = 0;
	m_nNWidReduceTick = NWID_REDUCE_COUNT;
	m_bResdHappened = FALSE;
	m_bResetPackIndex = FALSE;
	m_bIFrmACKMaking = FALSE;
	m_nThreadCount = 0;
	m_hIFrmACK = CreateEvent(NULL,FALSE,FALSE,NULL);
	m_hResdFrm = CreateEvent(NULL,FALSE,FALSE,NULL);
	m_hRestartSend = CreateEvent(NULL,TRUE,FALSE,NULL);
	m_hStopWorking = CreateEvent(NULL,TRUE,FALSE,NULL);
	InitializeCriticalSection(&m_csRes);
	InitializeCriticalSection(&m_csPackBuf);
	InitializeCriticalSection(&m_csPackCounting);
}

CSendVideo::~CSendVideo()
{
	StopSending();
	delete m_pFrmPacket;
	delete m_pResdFrmBuffer;
	delete m_pSendFrmGram;
	delete m_pResdGram;
	for(int i = 0; i < 2; i++) {
		for(int j = 0; j < PACK_BUF_COUNT; j++) {
			delete m_pSentPack[i][j];
		}
	}
	CloseHandle(m_hResdFrm);
	CloseHandle(m_hIFrmACK);
	CloseHandle(m_hRestartSend);
	CloseHandle(m_hStopWorking);
	DeleteCriticalSection(&m_csRes);
	DeleteCriticalSection(&m_csPackBuf);
	DeleteCriticalSection(&m_csPackCounting);
}

BOOL CSendVideo::InitResource(LPCTSTR DvrName,LPCTSTR CltName,SOCKADDR_IN sockAddr)
{
	m_strDvrName = DvrName;
	int nLength = m_strDvrName.GetLength();
	if(nLength > MAX_NAME_LENGTH || nLength < MIN_NAME_LENGTH) {
		m_strDvrName.Empty();
		return FALSE;
	}
	m_strCltName = CltName;
	nLength = m_strCltName.GetLength();
	if(nLength > MAX_NAME_LENGTH || nLength < MIN_NAME_LENGTH) {
		m_strDvrName.Empty();
		m_strCltName.Empty();
		return FALSE;
	}
	m_sockCltAddr = sockAddr;

	return TRUE;
}

BOOL CSendVideo::StartSending()
{
	if(m_bSending) return TRUE;
	if(m_nIndex >= g_ChanInfoArray.GetSize()) return FALSE;
	m_bSending = TRUE;
	g_ChanInfoArray[m_nIndex]->Start();
	m_bSendFinish = FALSE;
	m_dwSendPackCount = 0;
	m_bWaitIFrmACK = FALSE;
	m_nCurPackBufArrayIndex = 0;
	m_nCurPackBufIndex = 0;
	m_nCurPackFrmTableIndex = 0;
	m_wPackIndex = 0; 
	m_wACKFrmIndex = 0;
	m_nNWidReduceTick = NWID_REDUCE_COUNT;
	m_bResdHappened = FALSE;
	m_bResetPackIndex = FALSE;
	m_bIFrmACKMaking = FALSE;
	for(int i = 0; i < 2; i++) {
		for(int j = 0; j < PACK_BUF_COUNT; j++) {
			memset(m_pSentPack[i][i],0,INTERNET_MUP_LENGTH);
		}
		m_PackFrmTableArray[i].RemoveAll();
	}
	memset(&m_vResdHeader,0,sizeof(VIDEO_FRAME_HEADER));
	memset(m_pFrmPacket,0,m_uFrmBufLength);
	ResetEvent(m_hStopWorking);
	m_PackResdArray.RemoveAll();

	DWORD dwID;
	IncreaseThreadCount();
	CreateThread(NULL,0,(LPTHREAD_START_ROUTINE)SendFrmProc,this,0,&dwID);
	IncreaseThreadCount();
	CreateThread(NULL,0,(LPTHREAD_START_ROUTINE)SentStatisticProc,this,0,&dwID);
	IncreaseThreadCount();
	CreateThread(NULL,0,(LPTHREAD_START_ROUTINE)ResendProc,this,0,&dwID);

	return TRUE;
}

BOOL CSendVideo::StopSending()
{
	if(!m_bSending) return TRUE;
	m_bSending = FALSE;
	SetEvent(m_hStopWorking);
	while (m_nThreadCount > 0) {
		Sleep(50);
	}
	g_ChanInfoArray[m_nIndex]->Stop();

	return TRUE;
}

BOOL CSendVideo::AskForResend(PVRESDPACKET pVResdPackt)
{
	ASSERT((BYTE)m_nIndex == pVResdPackt->byChanNum);
	int i,j;
	if(RESET_DECODE == pVResdPackt->byResdNum) {
		EnterCriticalSection(&m_csRes);
		if(m_wPackIndex > 100) {
			//// �����෢�ķ�����100˵�����������ͻ��˵Ľ��ջ�����
			m_bResetPackIndex = TRUE;
		}
		LeaveCriticalSection(&m_csRes);
		return TRUE; 
	}
	if(GETFRM_ACK == pVResdPackt->byResdNum) {
		//// I֡�ش�ȷ��
		if(m_wACKFrmIndex == pVResdPackt->wFrmIndex) {
			EnterCriticalSection(&m_csRes);
			m_bWaitIFrmACK = FALSE;
			m_nNWidReduceTick = NWID_REDUCE_COUNT;
			LeaveCriticalSection(&m_csRes);
			SetEvent(m_hIFrmACK);
			TRACE("Ch%d,Confirm IFrm:%d,CurFrm:%d",m_nIndex,pVResdPackt->wFrmIndex,m_pFrmPacket->frmHeader.wFrmIndex);
			return TRUE;
		}
		else {
			TRACE("CH%d,Station Get I Frm confirm:%d",m_nIndex,pVResdPackt->wFrmIndex);
			return FALSE; //// �������ڵȴ�����һ֡
		}
	}
	//// ӵ������
	if(TRAFFIC_CONTROL == pVResdPackt->byResdNum) {
		if(!m_bWaitIFrmACK) {
			EnterCriticalSection(&m_csRes);
			//// ���m_bWaitIFrmACK�Ѿ�ΪTRUE,����ܶԷ�������ĳ��ԭ�����������ǰ����Ϣ
			m_bWaitIFrmACK = TRUE;
			LeaveCriticalSection(&m_csRes);
			EnterCriticalSection(&m_csPackBuf);
			m_PackResdArray.RemoveAll();
			LeaveCriticalSection(&m_csPackBuf);
			TRACE("chan:%d Find Trafic control:%d",m_nIndex,m_dwNwidPackets);
		}
		if(m_bWaitIFrmACK && m_bIFrmACKMaking && (m_nNWidReduceTick>0)) {
			//// ���ڵȴ�һ��I֡��ȷ�ϣ����յ�ӵ����˵����Ҫ���ͷ���������
			EnterCriticalSection(&m_csRes);
			m_dwNwidPackets = (m_dwNwidPackets*4/5)+1; //// ������һ��
			m_nNWidReduceTick--;
			TRACE("CH%d Get Traffic Control to %d when Making I Frm",m_nIndex,m_dwNwidPackets);
			LeaveCriticalSection(&m_csRes);
		}
		char chTmp[NAME_BUF_LENGTH];
		int nNameLength;
		char chBuf[INTERNET_MUP_LENGTH];
		memset(chBuf,0,INTERNET_MUP_LENGTH);
		PVIDEO_GRAM pVGram = (PVIDEO_GRAM)chBuf;
		pVGram->gramHeader.dwType = FOURCC_VIDE;
		pVGram->gramHeader.byAct = VIDEO_ACTVERSION;  //// ͨ���汾�����жϴ������
		memset(chTmp,0,NAME_BUF_LENGTH);
		nNameLength = m_strDvrName.GetLength();
		strcpy(chTmp,m_strDvrName);
		PushinRandom(pVGram->gramHeader.chLocalName,chTmp,NAME_BUF_LENGTH,nNameLength,RANDOM_TRANS_NAMEKEY);
		memset(chTmp,0,NAME_BUF_LENGTH);
		nNameLength = m_strCltName.GetLength();
		strcpy(chTmp,m_strCltName);
		PushinRandom(pVGram->gramHeader.chDestName,chTmp,NAME_BUF_LENGTH,nNameLength,RANDOM_TRANS_NAMEKEY);
		pVGram->vHeader.byChanNum = pVResdPackt->byChanNum;
		pVGram->vHeader.byFrmInfo = TRAFFIC_ACK;
		g_pManager->PushSData(INTERNET_MUP_LENGTH,m_sockCltAddr,pVGram);
		return TRUE;
	}
	//// ASSERT(pVResdPackt->byResdNum <= TRAFFIC_THRESHOLD);
	if(pVResdPackt->byResdNum >= TRAFFIC_THRESHOLD) {
		return FALSE; //// ��ֹ�ͻ��˵��Գ���
	}
	EnterCriticalSection(&m_csPackBuf);
	int nPackCount = m_PackResdArray.GetSize();
	int nAskForCount = (int)pVResdPackt->byResdNum;
	//// ���Լٶ�һ�����ﲻ��������ͬ��Ҫ���ش�����ţ���˱Ƚ��Ƿ����ظ��ĵ�ʱ��ֻҪ�Ƚ�ԭ������ľͿ�����
	BOOL bExist = FALSE;
	for(i = 0; i < nAskForCount; i++) {
		bExist = FALSE;
		for(j = 0; j < nPackCount; j++) {
			if(pVResdPackt->wResdIndex[i] == m_PackResdArray[j]) {
				bExist = TRUE;
				break;
			}
		}
		if(!bExist) {
			m_PackResdArray.Add(pVResdPackt->wResdIndex[i]);
		}
	}
	LeaveCriticalSection(&m_csPackBuf);
	SetEvent(m_hResdFrm);
	return TRUE;
}

void CSendVideo::Resend()
{
	BOOL bFetchVideo;
	int nLastPackLength,nNameLength,nMaxNum,i,j;
	char chTmp[NAME_BUF_LENGTH];
	PBYTE pCur = NULL;
	PVIDEO_GRAM pVGram = (PVIDEO_GRAM)m_pResdGram;
	HANDLE hEv[2];
	WORD wResdPackIndex;
	DWORD dwWaitSendRestart;
	hEv[0] = m_hRestartSend;
	hEv[1] = m_hStopWorking;

	BOOL bNeedResd = TRUE;
	do {
		///// ��֤�̹߳���������
		EnterCriticalSection(&m_csPackBuf);
		if(0 == m_PackResdArray.GetSize()) {
			bNeedResd = FALSE;
		}
		if(bNeedResd) {
			wResdPackIndex = m_PackResdArray[0];
			m_PackResdArray.RemoveAt(0);
			m_bResdHappened = TRUE;
		}
		LeaveCriticalSection(&m_csPackBuf);
		if(bNeedResd) {
			if(m_dwSendPackCount >= m_dwNwidPackets) {
				dwWaitSendRestart = WaitForMultipleObjects(2,hEv,FALSE,3000);
				if((WAIT_OBJECT_0+1) == dwWaitSendRestart) {
					//// ֹͣ��Ƶ����
					return;
				}
				else if(WAIT_TIMEOUT == dwWaitSendRestart) {
					EnterCriticalSection(&m_csRes);
					m_bWaitIFrmACK = TRUE;  //// ��������������������
					TRACE("CH%d Resend wait Check Timeout ",m_nIndex);
					LeaveCriticalSection(&m_csRes);
				}
			}
			//// ���ڻ�����������
			PVIDEO_GRAM pPack;
			BOOL bFind = FALSE;
			EnterCriticalSection(&m_csPackBuf);
			for(i = 0; i < 2; i++) {
				for(j = 0; j < PACK_BUF_COUNT; j++) {
					pPack = (PVIDEO_GRAM)m_pSentPack[i][j];
					if(FOURCC_VIDE==pPack->gramHeader.dwType && pPack->vHeader.wPackIndex == wResdPackIndex) {
						bFind = TRUE;
						memcpy(pVGram,pPack,INTERNET_MUP_LENGTH);
						break;
					}
				}
				if(bFind) {
					break;
				}
			}
			LeaveCriticalSection(&m_csPackBuf);
			if(bFind) {
				g_pManager->PushSData(INTERNET_MUP_LENGTH,m_sockCltAddr,pVGram);
				//// TRACE("CH:%d Resd Pack %d at Packet buffer %d_%d",m_nIndex,wResdPackIndex,i,j);
				EnterCriticalSection(&m_csPackCounting);
				m_dwSendPackCount++;
				LeaveCriticalSection(&m_csPackCounting);
			}
			else {
				//// ���б������Ҷ�Ӧ��֡
				int nTableSize = 0;
				PACKFRMTABLE PackFrmTable;
				EnterCriticalSection(&m_csPackBuf);
				for(i = 0; i < 2; i++) {
					nTableSize = m_PackFrmTableArray[i].GetSize();
					for(j = 0; j < nTableSize; j++) {
						if(m_PackFrmTableArray[i][j].wPackIndex == wResdPackIndex) {
							PackFrmTable = m_PackFrmTableArray[i][j];
							bFind = TRUE;
							break;
						}
					}
					if(bFind) {
						break;
					}
				}
				LeaveCriticalSection(&m_csPackBuf);
				if(!bFind) {
					EnterCriticalSection(&m_csPackBuf);
					m_PackResdArray.RemoveAll();
					LeaveCriticalSection(&m_csPackBuf);
					EnterCriticalSection(&m_csRes);
					m_bWaitIFrmACK = TRUE;
					TRACE("CH%d Resend didn't Find PackIndex:%d",m_nIndex,wResdPackIndex);
					LeaveCriticalSection(&m_csRes);
					bNeedResd = FALSE;
					break; //// ����Ҫ���ش���
				}
				m_vResdHeader.wFrmIndex = PackFrmTable.wFrmIndex;
				bFetchVideo = g_ChanInfoArray[m_nIndex]->FetchSample(&m_vResdHeader,m_uResdFrmLength,m_pResdFrmBuffer);
				if(!bFetchVideo) {
					//// ������ǰ��֡���������,���ʱ���ٷ��µ��Ѿ�û�����壬Ҫ��I֡��ʼ��
					EnterCriticalSection(&m_csPackBuf);
					m_PackResdArray.RemoveAll();
					LeaveCriticalSection(&m_csPackBuf);
					EnterCriticalSection(&m_csRes);
					m_bWaitIFrmACK = TRUE;
					TRACE("CH%d Resend didn't Fetch out FrmIndex:%d,PackIndex:%d",m_nIndex,PackFrmTable.wFrmIndex,wResdPackIndex);
					LeaveCriticalSection(&m_csRes);
					bNeedResd = FALSE;
				}
				else {
					pVGram->gramHeader.dwType = FOURCC_VIDE;
					pVGram->gramHeader.byAct = VIDEO_ACTVERSION;  //// ͨ���汾�����жϴ������
					pVGram->vHeader.byChanNum = m_vResdHeader.byChanNum;
					pVGram->vHeader.byFrmInfo = (BYTE)PackFrmTable.wPackInfo;
					pVGram->vHeader.wFrmIndex = PackFrmTable.wFrmIndex;
					pVGram->vHeader.wPackIndex = PackFrmTable.wPackIndex;
					pVGram->vHeader.byPackInFrm = (BYTE)PackFrmTable.wPackInFrm;
					if(0 == m_vResdHeader.uFrmLength) { 
						nMaxNum = 0;
						nLastPackLength = 0;
					}
					else {
						nLastPackLength = m_vResdHeader.uFrmLength % MAX_VPDATA_LENGTH;
						nMaxNum = m_vResdHeader.uFrmLength / MAX_VPDATA_LENGTH;
						nMaxNum = nLastPackLength > 0 ? nMaxNum : nMaxNum-1; //// ����
						nLastPackLength = nLastPackLength > 0 ? nLastPackLength : MAX_VPDATA_LENGTH; //// ���һ�������ݳ���
					}
					pVGram->vHeader.byMaxPackNum = (BYTE)nMaxNum;
					pVGram->vHeader.dwFrmLength = m_vResdHeader.uFrmLength;
					memset(chTmp,0,NAME_BUF_LENGTH);
					nNameLength = m_strDvrName.GetLength();
					strcpy(chTmp,m_strDvrName);
					PushinRandom(pVGram->gramHeader.chLocalName,chTmp,NAME_BUF_LENGTH,nNameLength,RANDOM_TRANS_NAMEKEY);
					memset(chTmp,0,NAME_BUF_LENGTH);
					nNameLength = m_strCltName.GetLength();
					strcpy(chTmp,m_strCltName);
					PushinRandom(pVGram->gramHeader.chDestName,chTmp,NAME_BUF_LENGTH,nNameLength,RANDOM_TRANS_NAMEKEY);
					pCur = m_pResdFrmBuffer + (MAX_VPDATA_LENGTH*PackFrmTable.wPackInFrm);
					if((WORD)nMaxNum == PackFrmTable.wPackInFrm) {
						memcpy(pVGram->vData,pCur,nLastPackLength);
					}
					else {
						memcpy(pVGram->vData,pCur,MAX_VPDATA_LENGTH);
					}
					g_pManager->PushSData(INTERNET_MUP_LENGTH,m_sockCltAddr,pVGram);
					TRACE("CH%d Resd at Frm buffer",m_nIndex);
					EnterCriticalSection(&m_csPackCounting);
					m_dwSendPackCount++;
					LeaveCriticalSection(&m_csPackCounting);				
				}
			}
		}
	} while(bNeedResd&&m_bSending);
	///////////////////////////////////////////////

	return;
}

void CSendVideo::SendFrm()
{
	ASSERT(m_nIndex < g_ChanInfoArray.GetSize());
	BOOL bFetchVideo,bOnlyKeyFrm=FALSE,bConfirmFrm=FALSE;
	int nLastPackLength,nNameLength,nMaxNum,i;
	PBYTE pCur = NULL;
	HANDLE hEvRestart[2],hEvConfirm[2];
	DWORD dwWaitSendRestart,dwWaitConfirm;
	char chTmp[NAME_BUF_LENGTH];
	PVIDEO_GRAM pVGram = (PVIDEO_GRAM)m_pSendFrmGram;
	hEvRestart[0] = m_hRestartSend;
	hEvRestart[1] = m_hStopWorking;
	hEvConfirm[0] = m_hIFrmACK;
	hEvConfirm[1] = m_hStopWorking;
	DWORD dwWaitAckTimeout = m_dwRelayInterval<<2;
////	dwWaitAckTimeout = dwWaitAckTimeout < 6000 ? 6000 : dwWaitAckTimeout;
	do {
		if(m_bResetPackIndex) {
			bOnlyKeyFrm = TRUE;
			m_wPackIndex = 0;
			EnterCriticalSection(&m_csRes);
			m_bResetPackIndex = FALSE;
			LeaveCriticalSection(&m_csRes);
		}
		if(m_bWaitIFrmACK) {
			bConfirmFrm = TRUE;
			bOnlyKeyFrm = TRUE;
			TRACE("CH%d,Only Need I Frm,now Frm:%d",m_nIndex,m_pFrmPacket->frmHeader.wFrmIndex);
		}
		bFetchVideo = g_ChanInfoArray[m_nIndex]->SendSample(m_uFrmBufLength,m_pFrmPacket,bOnlyKeyFrm); 		
		if(!bFetchVideo) {
			bOnlyKeyFrm = TRUE;
		////	MessageLogEx("C:\\NttStation.log",TRUE,"Doesn't find Frm");
			continue;
		}
		else {
			if(bOnlyKeyFrm) {
				TRACE("CH%d get I Frm:%d",m_nIndex,m_pFrmPacket->frmHeader.wFrmIndex);
				bOnlyKeyFrm = FALSE;
			}
		}
		pVGram->gramHeader.dwType = FOURCC_VIDE;
		pVGram->gramHeader.byAct = VIDEO_ACTVERSION;  //// ͨ���汾�����жϴ������
		pVGram->vHeader.byChanNum = m_pFrmPacket->frmHeader.byChanNum;
		if(bConfirmFrm) {
			TRACE("CH%d Frm%d need confirm",m_nIndex,m_pFrmPacket->frmHeader.wFrmIndex);
			pVGram->vHeader.byFrmInfo = BYTE(m_pFrmPacket->frmHeader.byFrmInfo | GETFRM_CONFIRM);
			m_wACKFrmIndex = m_pFrmPacket->frmHeader.wFrmIndex;
			m_bIFrmACKMaking = TRUE;
		}
		else {
			pVGram->vHeader.byFrmInfo = m_pFrmPacket->frmHeader.byFrmInfo;
		}
		pVGram->vHeader.wFrmIndex = m_pFrmPacket->frmHeader.wFrmIndex;
		if(0 == m_pFrmPacket->frmHeader.uFrmLength) {
			nMaxNum = 0;
			nLastPackLength = 0;
		}
		else {
			nLastPackLength = m_pFrmPacket->frmHeader.uFrmLength % MAX_VPDATA_LENGTH;
			nMaxNum = m_pFrmPacket->frmHeader.uFrmLength / MAX_VPDATA_LENGTH;
			nMaxNum = nLastPackLength > 0 ? nMaxNum : nMaxNum-1; //// ����
			nLastPackLength = nLastPackLength > 0 ? nLastPackLength : MAX_VPDATA_LENGTH; //// ���һ�������ݳ���
		}
		pVGram->vHeader.byMaxPackNum = (BYTE)nMaxNum;
		pVGram->vHeader.dwFrmLength = m_pFrmPacket->frmHeader.uFrmLength;
		pCur = m_pFrmPacket->VideoBuffer;

		/*
		FILE *hf = fopen("e:\\nttSend.dat","ab");
		DWORD dwKK = 0x34363248;
		fwrite(&dwKK,sizeof(DWORD),1,hf);
		fwrite(m_pFrmPacket->VideoBuffer,m_pFrmPacket->frmHeader.uFrmLength,1,hf);
		fclose(hf);
		*/

		/*
		MessageLogEx("E:\\SendVideo.log",FALSE,"FrmIndex:%d,FrmLen:%d",
			m_pFrmPacket->frmHeader.wFrmIndex,m_pFrmPacket->frmHeader.uFrmLength);
		*/
		//// TRACE("Fetch Frm length:%d,MaxPack:%d",m_pFrmPacket->frmHeader.uFrmLength,pVGram->vHeader.byMaxPackNum);
		for(i = 0; i <= nMaxNum; i++) {
			memset(chTmp,0,NAME_BUF_LENGTH);
			nNameLength = m_strDvrName.GetLength();
			strcpy(chTmp,m_strDvrName);
			PushinRandom(pVGram->gramHeader.chLocalName,chTmp,NAME_BUF_LENGTH,nNameLength,RANDOM_TRANS_NAMEKEY);
			memset(chTmp,0,NAME_BUF_LENGTH);
			nNameLength = m_strCltName.GetLength();
			strcpy(chTmp,m_strCltName);
			PushinRandom(pVGram->gramHeader.chDestName,chTmp,NAME_BUF_LENGTH,nNameLength,RANDOM_TRANS_NAMEKEY);
			pVGram->vHeader.byPackInFrm = (BYTE)i;
			if(i == nMaxNum) {
				memcpy(pVGram->vData,pCur,nLastPackLength);
			}
			else {
				memcpy(pVGram->vData,pCur,MAX_VPDATA_LENGTH);
				pCur += MAX_VPDATA_LENGTH;
			}
			pVGram->vHeader.wPackIndex = m_wPackIndex++;
			
			/*
			MessageLogEx("E:\\SendVideo.log",FALSE,"FrmIndex:%d,MaxNum:%d,CurNum:%d,PackLen:%d",
				pVGram->vHeader.wFrmIndex,pVGram->vHeader.byMaxPackNum,pVGram->vHeader.byPackIndex,pVGram->vHeader.wVideoLength);
			*/
			/*
			TRACE("CH %d Send:FrmIndex:%d,MaxNum:%d,CurNum:%d,PackLen:%d", m_nIndex,
				pVGram->vHeader.wFrmIndex,pVGram->vHeader.byMaxPackNum,pVGram->vHeader.byPackIndex,pVGram->vHeader.wVideoLength);
			*/
			
			/*
			hf = fopen("e:\\Sendvideo.dat","ab");
			fwrite(&dwKK,sizeof(DWORD),1,hf);
			fwrite(&pVGram->vHeader,1,sizeof(VPACKHEADER),hf);
			fwrite(pVGram->vData,1,pVGram->vHeader.wVideoLength,hf);
			fclose(hf);
			*/
			g_pManager->PushSData(INTERNET_MUP_LENGTH,m_sockCltAddr,pVGram);
			EnterCriticalSection(&m_csPackBuf);
			if(PACK_BUF_COUNT == m_nCurPackBufIndex) {
				//// ������
				m_nCurPackBufArrayIndex = (1 == m_nCurPackBufArrayIndex) ? 0 : 1;
				for(int nPackIndex = 0; nPackIndex < PACK_BUF_COUNT; nPackIndex++) {
					memset(m_pSentPack[m_nCurPackBufArrayIndex][nPackIndex],0,INTERNET_MUP_LENGTH);
				}
				m_nCurPackBufArrayIndex = 0;
				m_nCurPackBufIndex = 0;
			}
			memcpy(m_pSentPack[m_nCurPackBufArrayIndex][m_nCurPackBufIndex],pVGram,INTERNET_MUP_LENGTH);
			m_nCurPackBufIndex++;
			int nTableLen = m_PackFrmTableArray[m_nCurPackFrmTableIndex].GetSize();
			if(nTableLen >= PACKFRMTABLE_LENGTH) {
				m_nCurPackFrmTableIndex = (1==m_nCurPackFrmTableIndex) ? 0 : 1;
				m_PackFrmTableArray[m_nCurPackFrmTableIndex].RemoveAll();
			}
			PACKFRMTABLE PackFrmTable;
			PackFrmTable.wFrmIndex = pVGram->vHeader.wFrmIndex;
			PackFrmTable.wPackIndex = pVGram->vHeader.wPackIndex;
			PackFrmTable.wPackInfo = pVGram->vHeader.byFrmInfo;
			PackFrmTable.wPackInFrm = pVGram->vHeader.byPackInFrm;
			m_PackFrmTableArray[m_nCurPackFrmTableIndex].Add(PackFrmTable);
			LeaveCriticalSection(&m_csPackBuf);
			EnterCriticalSection(&m_csPackCounting);
			m_dwSendPackCount ++;
			//// TRACE("SendFrm Packcount:%d,Max pack:%d",m_dwSendPackCount,m_dwNwidPackets);
			LeaveCriticalSection(&m_csPackCounting);
			if(m_dwSendPackCount >= m_dwNwidPackets) {
				dwWaitSendRestart = WaitForMultipleObjects(2,hEvRestart,FALSE,2000);
				if((WAIT_OBJECT_0+1) == dwWaitSendRestart) {
					//// ֹͣ��Ƶ����
					m_bSendFinish = TRUE;
					return;
				}
				else if(WAIT_TIMEOUT == dwWaitSendRestart) {
					bOnlyKeyFrm = TRUE;  //// ��������������ͺ������
				}
			}
		} //// for(i = 0; i <= nMaxNum; i++)
		//// m_bWaitIFrmACK �ڻش����������޸�
		TRACE("Ch%d Send Video Frm Index:%d",m_nIndex,m_pFrmPacket->frmHeader.wFrmIndex);
		if(bConfirmFrm) {
			g_pManager->PushSData(INTERNET_MUP_LENGTH,m_sockCltAddr,pVGram);
			//// һ֡�����һ�������´���һ�Σ�ȷ���Է��յ�
			TRACE("CH%d Wait Frm:%d",m_nIndex,m_pFrmPacket->frmHeader.wFrmIndex);
			DWORD dwMinPackets = TRAFFIC_THRESHOLD > m_dwNwidPackets ? TRAFFIC_THRESHOLD : m_dwNwidPackets;
			DWORD dwTmpTime =  (pVGram->vHeader.byMaxPackNum / dwMinPackets)*2;
			if(dwTmpTime > 6) { //// �����ܱ�֤ʱ��
				dwWaitAckTimeout = (dwTmpTime * 1000);
			}
			else {
				dwWaitAckTimeout = 6000;
			}
			dwWaitAckTimeout = 15000;
			dwWaitConfirm = WaitForMultipleObjects(2,hEvConfirm,FALSE,dwWaitAckTimeout);  
			if(WAIT_OBJECT_0 == dwWaitConfirm) {
				bConfirmFrm = FALSE;
				bOnlyKeyFrm = FALSE;
				m_bIFrmACKMaking = FALSE;
				TRACE("CH%d Wait Confirm I Frm OK",m_nIndex);
			}
			else if((WAIT_OBJECT_0+1) == dwWaitConfirm) {
				m_bSendFinish = TRUE;
				TRACE("CH%d Exit",m_nIndex);
				return;
			}
			else { 
				bOnlyKeyFrm = TRUE;//// ��ʱ��˵����һ֡û����ȫ��������I֡ȷ��
				TRACE("CH%d Wait Confirm I Frm Timeout",m_nIndex);
			}
		}
	} while(m_bSending);

	m_bSendFinish = TRUE;
}

UINT CSendVideo::SendFrmProc(LPVOID p)
{
	CSendVideo *pSend = (CSendVideo*)p;
	pSend->SendFrm();
	pSend->DecreaseThreadCount();
	return 1;
}

BOOL CSendVideo::WaitResdEvent()
{
	DWORD dwWait;
	HANDLE hEv[2];
	hEv[0] = m_hResdFrm;
	hEv[1] = m_hStopWorking;
	dwWait = WaitForMultipleObjects(2,hEv,FALSE,INFINITE);
	if(WAIT_OBJECT_0 == dwWait) 
		return TRUE;
	else
		return FALSE;
}

UINT CSendVideo::ResendProc(LPVOID p)
{
	CSendVideo *pSend = (CSendVideo*)p;
	while (pSend->WaitResdEvent()) {
		pSend->Resend();
	}
	pSend->DecreaseThreadCount();

	return 1;
}

void CSendVideo::SentStatistic()
{
	DWORD dwWait;
	while (TRUE) {
		dwWait = WaitForSingleObject(m_hStopWorking,1000);
		if(WAIT_TIMEOUT == dwWait) {
			EnterCriticalSection(&m_csPackCounting);
			if(!m_bResdHappened && !m_bWaitIFrmACK) { //// û�з��ֶ�������
				m_dwNwidPackets = m_dwNwidPackets < 0x7FFFFFFF ? ++m_dwNwidPackets : m_dwNwidPackets;
				TRACE("CH%d Increase Pack Threthold,%d",m_nIndex,m_dwNwidPackets);
			}
			TRACE("CH%d Check NWid:%d,ResdHappen:%d,bWaitIFrmACK:%d",m_nIndex,m_dwNwidPackets,m_bResdHappened,m_bWaitIFrmACK);
			m_bResdHappened = FALSE;
			m_dwSendPackCount = 0;
			LeaveCriticalSection(&m_csPackCounting);
			PulseEvent(m_hRestartSend);
		}
		else {
			break;  //// �˳�ѭ��
			TRACE("Statistic quit");
		}
	}
}

UINT CSendVideo::SentStatisticProc(LPVOID p)
{
	CSendVideo *pSend = (CSendVideo*)p;
	pSend->SentStatistic();
	pSend->DecreaseThreadCount();
	return 1;
}