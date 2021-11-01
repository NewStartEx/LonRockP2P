#include "stdafx.h"
#include "Global.h"
#include "NTTProtocol.h"
#include "CltManager.h"
#include "FillVideo.h"
#include "MemVicocDecFun.h"

#define TEST_RECV	0

CFillVideo::CFillVideo(int ChanIndex)
{
	m_nChannel = ChanIndex;
	m_hDecode = OpenMemVicoc();
	m_hDecFrm = CreateEvent(NULL,FALSE,FALSE,NULL);
	m_hStopWorking = CreateEvent(NULL,TRUE,FALSE,NULL);
	m_bFirstPacket = TRUE; //// First必须是IFrame,并且应该是包索引为0
	m_nOldestPackBufIndex = m_nNextPackBufIndex = 0;
	m_nCurDecBufIndex = 0;
	m_nThreadCount = 0;
	m_dwRelayInterval = DEFAULT_NETRELAY_INTERVAL;
	m_bPackBufFull = FALSE;
	m_bIFrmReset = FALSE;
	m_bTrafficACK = FALSE;
	m_bACKFrmRcving = FALSE;
	m_wCurACKFrmIndex = 0;
	m_wLatestDecFrmIndex = 0;
	m_bSendAnotherIFrmACK = FALSE;
	//// 录像使用
	m_bVideoWorking = FALSE;
	m_bRecording = FALSE;
	m_bFirstRectFrm = FALSE;
	m_nRecRemainLength = 0;
	m_hRecFile = NULL;
	m_pRecBuffer = NULL;
	m_pRecCur = NULL;
	m_dwWrittenPos = 0;
	m_dwFrmTime = 0;
/********************************
	接收和解码重传算法解析：使用缓冲池
	当收到一个包，
	  
*********************************/
	m_uFrmBufLength = NORMAL_FRM_LENGTH;
	PBYTE pBuf = new BYTE[m_uFrmBufLength];
	m_pVideoFrm = (PVIDEO_FRAME)pBuf;
	m_pPackBuffer = new BYTE[PACK_BUFFER_COUNT*MAX_VPDATA_LENGTH];
	PBYTE pCur = m_pPackBuffer;
	for(int i = 0; i < PACK_BUFFER_COUNT; i++) {
		memset(&m_PackTable[i],0,sizeof(PACKTABLE));
		m_PackTable[i].pBuf = pCur;
		pCur += MAX_VPDATA_LENGTH;
	}
	InitializeCriticalSection(&m_csRes);
	InitializeCriticalSection(&m_csRecord);
}

CFillVideo::~CFillVideo()
{
	SetEvent(m_hStopWorking);
	//// 要先停止解码再删除其他内容。否则解码器里面还使用这里的缓冲会出错
	if(m_bRecording) {
		StopRecord();
	}
	while (m_nThreadCount > 0) {
		Sleep(50);
	}
	CloseMemVicoc(m_hDecode);
	DeleteCriticalSection(&m_csRes);
	DeleteCriticalSection(&m_csRecord);
	delete m_pPackBuffer;
	delete (PBYTE)m_pVideoFrm;
	CloseHandle(m_hDecFrm);
	CloseHandle(m_hStopWorking);
	if(NULL != m_pRecBuffer) {
		delete m_pRecBuffer;
		m_pRecBuffer = NULL;
	}
}

BOOL CFillVideo::InitResource(LPCTSTR DvrName,LPCTSTR LocalName,SOCKADDR_IN sockAddr)
{
	m_strDvrName = DvrName;
	int nLength = m_strDvrName.GetLength();
	if(nLength > MAX_NAME_LENGTH || nLength < MIN_NAME_LENGTH) {
		m_strDvrName.Empty();
		return FALSE;
	}
	m_strLocalName = LocalName;
	nLength = m_strLocalName.GetLength();
	if(nLength > MAX_NAME_LENGTH || nLength < MIN_NAME_LENGTH) {
		m_strDvrName.Empty();
		m_strLocalName.Empty();
		return FALSE;
	}
	m_sockDvrAddr = sockAddr;
	
	return TRUE;
}

BOOL CFillVideo::FindPackPose(WORD wPackIndex, int* pnPackPos) 
{
	ASSERT(!m_bPackBufFull); ///// 这个时候就不会随便换OldestPos
	int nBufIndex = m_nOldestPackBufIndex;
	WORD wChkPackIndex = m_PackTable[m_nOldestPackBufIndex].wPackIndex; //// 这个值肯定存在
	BOOL bFind = FALSE;
	do {
		if(wChkPackIndex == wPackIndex) {
			*pnPackPos = nBufIndex;
			bFind = TRUE;
			break;
		}
		nBufIndex = (++nBufIndex) % PACK_BUFFER_COUNT;
		wChkPackIndex++;
	} while(nBufIndex != m_nOldestPackBufIndex);
	
	return bFind;
}

BOOL CFillVideo::PackInRange(WORD wIndex)
{
	int nOldIndex = m_nOldestPackBufIndex;
	int nNextIndex = m_nNextPackBufIndex;
	int nIndex = nOldIndex;
	BOOL bInRange = FALSE;
	while (nIndex != nNextIndex) {
		if(wIndex == nIndex) {
			bInRange = TRUE;
			break;
		}
		else {
			nIndex = (++nIndex) % PACK_BUFFER_COUNT;
		}
	}

	return bInRange;
}

BOOL CFillVideo::FillPacket(PVPACKHEADER pHeader,PBYTE pVideoData)
{
	ASSERT(pHeader->byChanNum == (BYTE)m_nChannel);

	//// TRACE("Ch %d get pack %d",m_nChannel,pHeader->wPackIndex);
	VRESD_GRAM vResdGram;
	char chTmp[NAME_BUF_LENGTH];
	UINT nLength;
	/*
	//// 测试：
	//// 有意识抛弃一包，看是否能在缓冲里找到
	DWORD dwTest = (GetTickCount()&0xFF)+1;
	if(pHeader->wPackIndex%dwTest == 13) {
		TRACE("CH %d Throw one pack:%d",m_nChannel,pHeader->wPackIndex);
		return TRUE;
	}
	*/
	if(m_bFirstPacket) {
		BOOL bResetRemote = FALSE;
		if (pHeader->wPackIndex > PACK_BUFFER_COUNT) {
			TRACE("Ch %d First too large",m_nChannel);
			bResetRemote = TRUE;
		}
		if(0 == (pHeader->byFrmInfo&KEYFRAME)) {
			TRACE("Ch %d First not I Frame",m_nChannel);
			bResetRemote = TRUE;
		}
		if(bResetRemote) {
			memset(&vResdGram,0,sizeof(VRESD_GRAM));
			vResdGram.vResdPacket.byResdNum = RESET_DECODE;
			vResdGram.vResdPacket.byChanNum = (BYTE)m_nChannel;
			memset(chTmp,0,NAME_BUF_LENGTH);
			nLength = (UINT)m_strLocalName.GetLength();
			strcpy(chTmp,m_strLocalName);
			PushinRandom(vResdGram.gramHeader.chLocalName,chTmp,NAME_BUF_LENGTH,nLength,RANDOM_TRANS_NAMEKEY);
			memset(chTmp,0,NAME_BUF_LENGTH);
			nLength = (UINT)m_strDvrName.GetLength();
			strcpy(chTmp,m_strDvrName);
			PushinRandom(vResdGram.gramHeader.chDestName,chTmp,NAME_BUF_LENGTH,nLength,RANDOM_TRANS_NAMEKEY);
			vResdGram.gramHeader.dwType = FOURCC_RESD;
			vResdGram.gramHeader.byAct = VIDEO_RESDVERSION; //// 根据版本号判断传输机制
			g_pManager->PushSData(sizeof(VRESD_GRAM),m_sockDvrAddr,&vResdGram);
			//// TRACE("CH%d Reset I Frm PackIndex:%d byFrmInfo:%d",m_nChannel,pHeader->wPackIndex,pHeader->byFrmInfo);
			return FALSE;
		}
		m_PackTable[pHeader->wPackIndex].wPackIndex = pHeader->wPackIndex;
		m_PackTable[pHeader->wPackIndex].byFrmInfo = pHeader->byFrmInfo;
		m_PackTable[pHeader->wPackIndex].byMaxPackNum = pHeader->byMaxPackNum;
		m_PackTable[pHeader->wPackIndex].byPackInFrm = pHeader->byPackInFrm;
		m_PackTable[pHeader->wPackIndex].dwFrmLength = pHeader->dwFrmLength;
		m_PackTable[pHeader->wPackIndex].wFrmIndex = pHeader->wFrmIndex;
		memcpy(m_PackTable[pHeader->wPackIndex].pBuf,pVideoData,MAX_VPDATA_LENGTH);
		m_nNextPackBufIndex = pHeader->byPackInFrm+1; //// 这个时候缓冲索引和包索引应该是一样的,Newest
		m_PackTable[0].wPackIndex = pHeader->wPackIndex-pHeader->byPackInFrm; //// 第一帧肯定是当前帧的第一包
		m_PackTable[pHeader->wPackIndex].bFilled = TRUE;
		m_bFirstPacket = FALSE;
		return TRUE;
	}
	if(pHeader->byFrmInfo&TRAFFIC_ACK) {
		if(m_bIFrmReset) {
			m_bTrafficACK = TRUE;
			//// TRACE("CH%d Get Traffic ACK ",m_nChannel);
			return TRUE; //// 这一包不是数据包
		}
		else {
			//// TRACE("CH%d Get Traffic ACK when OK",m_nChannel);
			return TRUE;
		}
	}
	if(pHeader->wPackIndex < m_PackTable[m_nOldestPackBufIndex].wPackIndex && 
		(m_PackTable[m_nOldestPackBufIndex].wPackIndex-pHeader->wPackIndex) < 0x8000) {
		//// 小于最后解码包. 后面的小于判断是对应索引非常大的时候轮转的情况
		return FALSE;
	}
	if((pHeader->byFrmInfo&GETFRM_CONFIRM)&&(pHeader->wFrmIndex != m_wCurACKFrmIndex)) {
		/// m_bIFrmReset 目的是避免第二次收到这样的包出错
		EnterCriticalSection(&m_csRes);
		//// TRACE("CH%d Frm %d Need Confirm",m_nChannel,pHeader->wFrmIndex);
		m_bACKFrmRcving = TRUE;
		m_wCurACKFrmIndex = pHeader->wFrmIndex;
		m_bIFrmReset = FALSE;
		m_bPackBufFull = FALSE;
		m_nOldestPackBufIndex = 0;
		m_nCurDecBufIndex = 0;
		m_bTrafficACK = FALSE;
		for(int i = 0; i < PACK_BUFFER_COUNT; i++) {
			m_PackTable[i].bFilled = 0;
			m_PackTable[i].wPackIndex = 0;
		}
		m_nNextPackBufIndex = pHeader->byPackInFrm+1;
		//// TRACE("Get Next Index In First Confirm :%d,FrmIndex:%d,PackIndex:%d", m_nNextPackBufIndex,pHeader->wFrmIndex,pHeader->wPackIndex);
		m_PackTable[0].wPackIndex = pHeader->wPackIndex-pHeader->byPackInFrm; //// 每次都要确定这一帧的包序列
		LeaveCriticalSection(&m_csRes);
	}
	
	if(m_bPackBufFull) {
		//// 认为缓冲满，就直接抛弃
		return FALSE;
	}
	BOOL bResult = TRUE;
	int nPackPos = 0;
	if(!FindPackPose(pHeader->wPackIndex,&nPackPos)) {
		m_bPackBufFull = TRUE;
		bResult = FALSE;
		/*
		TRACE("Pack %d Doesn't find pos,old:%d,oldPackIndex:%d,next:%d",
			pHeader->wPackIndex,m_nOldestPackBufIndex,m_PackTable[m_nOldestPackBufIndex],
			m_nNextPackBufIndex);
		*/
	}
	else{
		if(m_PackTable[nPackPos].bFilled) {
			return TRUE;
		}
		m_PackTable[nPackPos].wPackIndex = pHeader->wPackIndex;
		m_PackTable[nPackPos].byFrmInfo = pHeader->byFrmInfo;
		m_PackTable[nPackPos].byMaxPackNum = pHeader->byMaxPackNum;
		m_PackTable[nPackPos].byPackInFrm = pHeader->byPackInFrm;
		m_PackTable[nPackPos].dwFrmLength = pHeader->dwFrmLength;
		m_PackTable[nPackPos].wFrmIndex = pHeader->wFrmIndex;
		memcpy(m_PackTable[nPackPos].pBuf,pVideoData,MAX_VPDATA_LENGTH);
		if(!PackInRange(nPackPos)) {
			int nTmpBufIndex = (nPackPos+1) % PACK_BUFFER_COUNT;
		////	TRACE("Get nPackPos:%d,nTmpBufIndex:%d",nPackPos,nTmpBufIndex);
			if(nTmpBufIndex == m_nOldestPackBufIndex) {
				m_bPackBufFull = TRUE;
				bResult = FALSE;
			}
			else {
				m_nNextPackBufIndex = nTmpBufIndex; //// 这个时候确定这个的值
		//// TRACE("Get Next Index In Normal :%d,FrmIndex:%d,PackIndex:%d",m_nNextPackBufIndex,pHeader->wFrmIndex,pHeader->wPackIndex);
			}
		}
		m_PackTable[nPackPos].bFilled = TRUE;
		//// TRACE("Set Pack %d in Buf %d,NextBufIndex:%d",pHeader->wPackIndex,nPackPos,m_nNextPackBufIndex);
		if(pHeader->byPackInFrm == pHeader->byMaxPackNum) {
			SetEvent(m_hDecFrm); //// 这个时候开始解码
		}
	}

	//// 清除已经解码的包的填充标识
	int nDecBufIndex = m_nCurDecBufIndex;

	//// TRACE("DecIndex:%d,OldestInde:%d",nDecBufIndex,m_nOldestPackBufIndex);
	while (m_nOldestPackBufIndex != nDecBufIndex) {
		m_PackTable[m_nOldestPackBufIndex].bFilled = FALSE;
		m_nOldestPackBufIndex = (++m_nOldestPackBufIndex)%PACK_BUFFER_COUNT;
	}
	return TRUE;
}

PVOID CFillVideo::TryFrmBuffer(UINT uFrmLength)
{
	//// 必须是MAX_VPDATA_LENGTH的整数倍，才好复制
	UINT uBufLength = ((uFrmLength/MAX_VPDATA_LENGTH)+1)*MAX_VPDATA_LENGTH+sizeof(VIDEO_FRAME);
	if(m_uFrmBufLength >= uBufLength) {
		return m_pVideoFrm;
	}
	delete m_pVideoFrm;
	PBYTE pBuf = new BYTE[uBufLength];
	m_pVideoFrm = (PVIDEO_FRAME)pBuf;
	m_uFrmBufLength = uBufLength;
	memset(m_pVideoFrm,0,m_uFrmBufLength);
	return m_pVideoFrm;
}

void CFillVideo::ResetState()
{
	m_bFirstPacket = TRUE; //// First必须是IFrame,并且应该是包索引为0
	m_nOldestPackBufIndex = m_nNextPackBufIndex = 0;
	m_nCurDecBufIndex = 0;
	m_nThreadCount = 0;
	m_bPackBufFull = FALSE;
	m_bIFrmReset = FALSE;
	m_bTrafficACK = FALSE;
	m_bACKFrmRcving = FALSE;
	m_bSendAnotherIFrmACK = FALSE;
	m_wCurACKFrmIndex = 0;
	m_wLatestDecFrmIndex = 0;
	ResetEvent(m_hStopWorking);
	for(int i = 0; i < PACK_BUFFER_COUNT; i++) {
		m_PackTable[i].bFilled = 0;
		m_PackTable[i].wPackIndex = 0;
	}
	MemDecodingReset(m_hDecode);
}

BOOL CFillVideo::FillStart()
{
	if(m_nThreadCount > 0) return TRUE;
	ShowMemVideo(m_hDecode,TRUE);
	ResetState();
	m_bVideoWorking = TRUE;
	DWORD dwID;
	IncreaseThreadCount();
	CreateThread(NULL,0,(LPTHREAD_START_ROUTINE)ResdPackProc,this,0,&dwID);
	IncreaseThreadCount();
	CreateThread(NULL,0,(LPTHREAD_START_ROUTINE)DecodeFrameProc,this,0,&dwID);

	return TRUE;
}

void CFillVideo::FillStop()
{
	m_bVideoWorking = FALSE;
	ShowMemVideo(m_hDecode,FALSE);
	SetEvent(m_hStopWorking);
}

void CFillVideo::SetVideoRect(int left,int top,int width,int height)
{
	m_drwRect.left = left;
	m_drwRect.top = top;
	m_drwRect.right = left+width;
	m_drwRect.bottom = top+height;
	SetMemVideoScreenPos(m_hDecode,m_drwRect);
}

void CFillVideo::RefreshVideo()
{
	if(!m_bShow) return;
	RepaintMemVideo(m_hDecode);
}

void CFillVideo::ShowVideo(BOOL show)
{
	m_bShow = show;
	ShowMemVideo(m_hDecode,show);
}

BOOL CFillVideo::Snap(LPCTSTR FileName)
{
	if(!m_bVideoWorking) return FALSE;
	if(MEMVICO_OK != MemSnapFrame(m_hDecode,FileName)) {
		return FALSE;
	}
	return TRUE;
}

BOOL CFillVideo::StartRecord(LPCTSTR FileName)
{
	if(!m_bVideoWorking) return FALSE;
	if(m_bRecording) return FALSE;
	m_hRecFile = fopen(FileName,"wb");
	if(NULL == m_hRecFile) return FALSE;
	VICOHEADER vHeader;
	memset(&vHeader,0,sizeof(VICOHEADER));
	vHeader.ID = VICO_ID;
	fwrite(&vHeader,1,sizeof(VICOHEADER),m_hRecFile);

	EnterCriticalSection(&m_csRecord);
	if(NULL == m_pRecBuffer) {
		m_pRecBuffer = new BYTE[REC_BUF_LENGTH];
		m_nRecRemainLength = REC_BUF_LENGTH;
		m_pRecCur = m_pRecBuffer;
	}
	m_bRecording = TRUE;
	m_bFirstRectFrm = TRUE;
	m_dwWrittenPos = 0;
	LeaveCriticalSection(&m_csRecord);

	return TRUE;
}

void CFillVideo::StopRecord()
{
	if(NULL == m_hRecFile || !m_bRecording) {
		return;
	}
	int nRecLen;
	EnterCriticalSection(&m_csRecord);
	nRecLen = REC_BUF_LENGTH-m_nRecRemainLength;
	fwrite(m_pRecBuffer,nRecLen,1,m_hRecFile);
	m_nRecRemainLength = 0;
	DWORD dwEnd = VEND_ID;    //// 这里录像停止就是一个文件停止，不需要像主控端那样做流停止的动作
	fwrite(&dwEnd,1,sizeof(DWORD),m_hRecFile);
	m_dwWrittenPos += (nRecLen + sizeof(DWORD));
	fclose(m_hRecFile);
	m_bRecording = FALSE;
	LeaveCriticalSection(&m_csRecord);
	return;
}

void CFillVideo::DecodeFrm()
{
	BOOL bDataPushed = FALSE;
	VRESD_GRAM vResdGram;
	char chTmp[NAME_BUF_LENGTH];
	UINT nLength,i;
	memset(&vResdGram,0,sizeof(VRESD_GRAM));
	BOOL bRcvAll,bFrmACK;
	int nNewestPackBufIndex,nIndex;
	WORD wACKFrmIndex,wPackIndex;
	DWORD dwFrmLength,dwFrmIndex;
	BYTE byKeyFrm;
	do {
		bRcvAll = FALSE;
		bFrmACK = FALSE;
		///// 这里的互斥量为了保证接收线程收到ACK Frm的时候做修改要等待这里的操作完成
		EnterCriticalSection(&m_csRes);
		nNewestPackBufIndex = m_nNextPackBufIndex;
		nIndex = m_nCurDecBufIndex;
		/*
		TRACE("Enter Decode frm,m_nCurDecBufIndex:%d,PackInfrm:%d",
			nIndex,m_PackTable[nIndex].byPackInFrm);
		*/
		bRcvAll = TRUE;
		if(m_PackTable[nIndex].bFilled && m_PackTable[nIndex].byPackInFrm!=0) {
			//// 出错了，开始应该都是从0包开始才行
			m_bIFrmReset = TRUE;
			bRcvAll = FALSE;
			TRACE("Get Packet Error");
		}
		else {
			UINT uMaxPackNum = m_PackTable[nIndex].byMaxPackNum;
			for(i = 0; i <= uMaxPackNum; i++) {
				if(!m_PackTable[nIndex].bFilled) {
					bRcvAll = FALSE;
					break;
				}
				if(m_PackTable[nIndex].byFrmInfo&GETFRM_CONFIRM) {
					bFrmACK = TRUE;
				}
				nIndex = (++nIndex)%PACK_BUFFER_COUNT;
				/* 这个不需要。有可能最后一包就是NewestPack.主要看bFilled
				if(nIndex == nNewestPackBufIndex) {
					bRcvAll = FALSE;
					break;
				}
				*/
			}
		////	TRACE("From DecIndex:%d, MaxPackNum:%d,Get All:%d",m_nCurDecBufIndex,m_PackTable[m_nCurDecBufIndex].byMaxPackNum,bRcvAll);
			if(bRcvAll) {
				UINT uCpyLength,uCpyCount,uMidAddr;
				dwFrmLength = m_PackTable[m_nCurDecBufIndex].dwFrmLength;
				dwFrmIndex = m_PackTable[m_nCurDecBufIndex].wFrmIndex;
				byKeyFrm = (BYTE)(m_PackTable[m_nCurDecBufIndex].byFrmInfo & KEYFRAME);
				TryFrmBuffer(dwFrmLength);
				//// 尽量减少复制这样的操作
				if(m_nCurDecBufIndex+m_PackTable[m_nCurDecBufIndex].byMaxPackNum < PACK_BUFFER_COUNT) {
					///// 在同一个缓冲里
					memcpy(m_pVideoFrm->VideoBuffer,m_PackTable[m_nCurDecBufIndex].pBuf,dwFrmLength);
				}
				else {
					//// 分两段来复制
					uCpyCount = PACK_BUFFER_COUNT-m_nCurDecBufIndex;
					uCpyLength = MAX_VPDATA_LENGTH*uCpyCount;
					uMidAddr = uCpyLength;
					memcpy(m_pVideoFrm->VideoBuffer,m_PackTable[m_nCurDecBufIndex].pBuf,uCpyLength);
					uCpyLength = dwFrmLength-uCpyLength;
					memcpy(&m_pVideoFrm->VideoBuffer[uMidAddr],m_PackTable[0].pBuf,uCpyLength);
				}
				if(bFrmACK) {
					wACKFrmIndex = m_PackTable[m_nCurDecBufIndex].wFrmIndex;
					m_bACKFrmRcving = FALSE;
				}
				m_wLatestDecFrmIndex = m_PackTable[m_nCurDecBufIndex].wFrmIndex;
				wPackIndex = (m_PackTable[m_nCurDecBufIndex].wPackIndex + m_PackTable[m_nCurDecBufIndex].byMaxPackNum) + 1;
				m_nCurDecBufIndex = (m_nCurDecBufIndex+m_PackTable[m_nCurDecBufIndex].byMaxPackNum+1) % PACK_BUFFER_COUNT;
				m_PackTable[m_nCurDecBufIndex].wPackIndex = wPackIndex;
			}
		}
		LeaveCriticalSection(&m_csRes);
		if(bRcvAll) {
			m_pVideoFrm->dwID = H264_ID;
			m_pVideoFrm->frmHeader.byKeyFrame = byKeyFrm;
			m_pVideoFrm->frmHeader.dwLength = dwFrmLength;
			m_pVideoFrm->frmHeader.dwIndex = dwFrmIndex;
			GetLocalTime(&m_pVideoFrm->frmHeader.stFrmTime);
			UINT uLen = dwFrmLength+sizeof(H264HEADER)+sizeof(DWORD);
			PushData(m_hDecode,(LPBYTE)m_pVideoFrm,uLen);
			/*
			if(3 == m_nChannel) {
				TRACE("Decode Frm ,now CurDecBufIndex:%d",m_nCurDecBufIndex);
			}
			*/
			EnterCriticalSection(&m_csRecord);
			if(m_bRecording) {
				BOOL bCanSave = FALSE;
				if(m_bFirstRectFrm) {
					if(byKeyFrm & KEYFRAME) {
						bCanSave = TRUE;
						m_bFirstRectFrm = FALSE;
						m_pVideoFrm->frmHeader.dwInterval = 0;
						m_dwFrmTime = GetTickCount();
					}
				}
				else {
					DWORD dwCurTime = GetTickCount();
					m_pVideoFrm->frmHeader.dwInterval = dwCurTime-m_dwFrmTime;
					m_dwFrmTime = dwCurTime;
					bCanSave = TRUE;
				}
				if(bCanSave) {
					if(m_nRecRemainLength <= (int)uLen) {
						fwrite(m_pRecBuffer,(REC_BUF_LENGTH-m_nRecRemainLength),1,m_hRecFile);
						m_pRecCur = m_pRecBuffer;
						m_nRecRemainLength = REC_BUF_LENGTH;
					}
					m_pVideoFrm->frmHeader.dwInterval = 50;
					memcpy(m_pRecCur,m_pVideoFrm,uLen);
					m_nRecRemainLength -= uLen;
					m_pRecCur += uLen;
					m_dwWrittenPos += uLen;
				}
			}
			LeaveCriticalSection(&m_csRecord);
			if(bFrmACK) {
				//// 这个时候肯定是要在RcvAll的时候才做的
				vResdGram.vResdPacket.byResdNum = GETFRM_ACK;
				vResdGram.vResdPacket.byChanNum = (BYTE)m_nChannel;
				vResdGram.vResdPacket.wFrmIndex = wACKFrmIndex;
				memset(chTmp,0,NAME_BUF_LENGTH);
				nLength = (UINT)m_strLocalName.GetLength();
				strcpy(chTmp,m_strLocalName);
				PushinRandom(vResdGram.gramHeader.chLocalName,chTmp,NAME_BUF_LENGTH,nLength,RANDOM_TRANS_NAMEKEY);
				memset(chTmp,0,NAME_BUF_LENGTH);
				nLength = (UINT)m_strDvrName.GetLength();
				strcpy(chTmp,m_strDvrName);
				PushinRandom(vResdGram.gramHeader.chDestName,chTmp,NAME_BUF_LENGTH,nLength,RANDOM_TRANS_NAMEKEY);
				vResdGram.gramHeader.dwType = FOURCC_RESD;
				vResdGram.gramHeader.byAct = VIDEO_RESDVERSION; //// 根据版本号判断传输机制
				g_pManager->PushSData(sizeof(VRESD_GRAM),m_sockDvrAddr,&vResdGram);
				TRACE("CH%d Send I Frm:%d ACK,CurDecBufIndex:%d,NextPackBufIndex:%d",m_nChannel,wACKFrmIndex,m_nCurDecBufIndex,m_nNextPackBufIndex);
			}
		}
	} while (bRcvAll);

	return ;
}

BOOL CFillVideo::WaitDecodeEvent()
{
	HANDLE hEv[2];
	hEv[0] = m_hDecFrm;
	hEv[1] = m_hStopWorking;
	DWORD dwWait = WaitForMultipleObjects(2,hEv,FALSE,1000);
	if((WAIT_OBJECT_0+1) == dwWait) return FALSE; 
	else return TRUE; //// 等到事件或者超时都可以扫描视频数据
}

BOOL CFillVideo::WaitResdTimeout()
{
	DWORD dwTimeout;
	if(m_bIFrmReset && (m_dwRelayInterval<500)) {
		dwTimeout = 500;
	}
	else if((m_bACKFrmRcving && (m_dwRelayInterval<200))) {
		dwTimeout = 200;
	}
	else {
		dwTimeout = m_dwRelayInterval;
	}
	dwTimeout = m_dwRelayInterval;
	//// TRACE("CH%d :Interval:%d,Timeout:%d",m_nChannel,m_dwRelayInterval,dwTimeout);
	DWORD dwWait = WaitForSingleObject(m_hStopWorking,dwTimeout);
	if(WAIT_TIMEOUT == dwWait) return TRUE;
	else return FALSE;
}

void CFillVideo::CheckPack()
{
	int nCurDecBufIndex,nNextBufIndex;
	BOOL bPackBufFull;
	VRESD_GRAM vResdGram;
	char chTmp[NAME_BUF_LENGTH];
	UINT nLength;
	memset(&vResdGram,0,sizeof(VRESD_GRAM));
	BOOL bNeedResd = FALSE;
	if(m_bFirstPacket) {
		//// 第一次还没有收到，不处理
		//// TRACE("Check Pack before first Pack");
		return;
	}
	EnterCriticalSection(&m_csRes);
	nCurDecBufIndex = m_nCurDecBufIndex;
	nNextBufIndex = m_nNextPackBufIndex;
	bPackBufFull = m_bPackBufFull;
	if(bPackBufFull) {
		if(!m_bTrafficACK) {
			vResdGram.vResdPacket.byResdNum = TRAFFIC_CONTROL;
			m_bIFrmReset = TRUE;
			bNeedResd = TRUE;
			TRACE("Pack Buffer is Full Set Traffic");
		}
		else {
			bNeedResd = FALSE;
			TRACE("Pack Buffer is Full When Traffic");
		}
	}
	else {
		UINT uPackLost = 0;
		int nIndex = nCurDecBufIndex;
		WORD wPackIndex = m_PackTable[nIndex].wPackIndex;   //// 不管是否Filled,最老一包的包序列一定是有的,上面都已经计算出来了
		while (nIndex != nNextBufIndex) {
			if(!m_PackTable[nIndex].bFilled) {
				vResdGram.vResdPacket.wResdIndex[uPackLost] = wPackIndex;
				/*
				TRACE("PackBufIndex %d, Pack Index %d lost,NextBufIndex:%d,CurDecBufIndex:%d",
					nIndex,wPackIndex,nNextBufIndex,nCurDecBufIndex);
				*/
				uPackLost ++;
			}
			wPackIndex ++;
			nIndex = (++nIndex)%PACK_BUFFER_COUNT;
			if(TRAFFIC_THRESHOLD == uPackLost) {
				break;
			}
		}
		
		////	uPackLost = TRAFFIC_THRESHOLD; ////  nvcd test
			
		//// TRACE("Lost Pack:%d,CurDecBufIndex:%d,OldestBufIndex:%d,NextBufIndex:%d",uPackLost,nCurDecBufIndex,m_nOldestPackBufIndex,m_nNextPackBufIndex);
		
		if(0 == uPackLost) {
			EnterCriticalSection(&m_csRes);
			if(m_bACKFrmRcving) {
				//// 这个时候肯定0是最老的一包
				if((nNextBufIndex-nCurDecBufIndex) < (m_PackTable[0].byMaxPackNum+1)) {
					BYTE byResdNum = m_PackTable[nNextBufIndex-1].byMaxPackNum-m_PackTable[nNextBufIndex-1].byPackInFrm;
					BYTE byIndex;
					wPackIndex = m_PackTable[nNextBufIndex-1].wPackIndex+1;
					byResdNum = byResdNum > (TRAFFIC_THRESHOLD-1) ? (TRAFFIC_THRESHOLD-1) : byResdNum;
					for(byIndex = 0; byIndex < (TRAFFIC_THRESHOLD-1); byIndex++) {
						vResdGram.vResdPacket.wResdIndex[byIndex] = wPackIndex;
						wPackIndex++;
					}
					vResdGram.vResdPacket.byResdNum = byResdNum;
					bNeedResd = TRUE;
					TRACE("ACK Frm:Lost:%d,0:byMaxPackNum:%d,FrmIndex:%d",
						byResdNum,m_PackTable[0].byMaxPackNum,m_PackTable[0].wFrmIndex);
				}
			}
			else {
				bNeedResd = FALSE;
			}
			LeaveCriticalSection(&m_csRes);
		}
		else if(uPackLost >= TRAFFIC_THRESHOLD) {
			if(!m_bTrafficACK) {
				if(!m_bACKFrmRcving) {
					//// 表示没有收到对方的回应
					m_bIFrmReset = TRUE;
					vResdGram.vResdPacket.byResdNum = TRAFFIC_CONTROL;
					bNeedResd = TRUE;
					TRACE("CH%d Send Traffic info",m_nChannel);
				}
				else {
					vResdGram.vResdPacket.byResdNum = (TRAFFIC_THRESHOLD-1);
					bNeedResd = TRUE;
				}
			}
			else {
				bNeedResd = FALSE;
				//// TRACE("CH%d No need Send info",m_nChannel);
			}
		}
		else {
			vResdGram.vResdPacket.byResdNum = (BYTE)uPackLost;
			bNeedResd = TRUE;
		}
	}	
	LeaveCriticalSection(&m_csRes);
//	TRACE("Check Pack Table,uPackLost %d",uPackLost);
	vResdGram.vResdPacket.byChanNum = (BYTE)m_nChannel;
	memset(chTmp,0,NAME_BUF_LENGTH);
	nLength = (UINT)m_strLocalName.GetLength();
	strcpy(chTmp,m_strLocalName);
	PushinRandom(vResdGram.gramHeader.chLocalName,chTmp,NAME_BUF_LENGTH,nLength,RANDOM_TRANS_NAMEKEY);
	memset(chTmp,0,NAME_BUF_LENGTH);
	nLength = (UINT)m_strDvrName.GetLength();
	strcpy(chTmp,m_strDvrName);
	PushinRandom(vResdGram.gramHeader.chDestName,chTmp,NAME_BUF_LENGTH,nLength,RANDOM_TRANS_NAMEKEY);
	if(!bNeedResd) {
		BOOL bSendIFrmACK = FALSE;
		EnterCriticalSection(&m_csRes);
		if((m_nCurDecBufIndex==m_nNextPackBufIndex) && (m_wLatestDecFrmIndex==m_wCurACKFrmIndex) && !m_bSendAnotherIFrmACK) {
			vResdGram.vResdPacket.byResdNum = GETFRM_ACK;
			vResdGram.vResdPacket.wFrmIndex = m_wLatestDecFrmIndex;
			m_bSendAnotherIFrmACK = TRUE;
			bSendIFrmACK = TRUE;
		}
		LeaveCriticalSection(&m_csRes);
		if(bSendIFrmACK) {
			TRACE("Send Another Frm ACK");
			g_pManager->PushSData(sizeof(VRESD_GRAM),m_sockDvrAddr,&vResdGram);
		}
		
		return;
	}
	vResdGram.gramHeader.dwType = FOURCC_RESD;
	vResdGram.gramHeader.byAct = VIDEO_RESDVERSION; //// 根据版本号判断传输机制
	g_pManager->PushSData(sizeof(VRESD_GRAM),m_sockDvrAddr,&vResdGram);
}

UINT CFillVideo::DecodeFrameProc(LPVOID p)
{
	CFillVideo* pFill = (CFillVideo*)p;
	while (pFill->WaitDecodeEvent()) {
		pFill->DecodeFrm();
	}
	TRACE("Decode Pack Finish");
	pFill->DecreaseThreadCount();
	return 1;
}

UINT CFillVideo::ResdPackProc(LPVOID p)
{
	CFillVideo* pFill = (CFillVideo*)p;
	while (pFill->WaitResdTimeout()) {
		pFill->CheckPack();
	}
	TRACE("Check Resd Pack Finish");
	pFill->DecreaseThreadCount();
	return 1;
}
