#include "stdafx.h"
#include "Global.h"
#include "ChannelInfo.h"

CChanInfoArray g_ChanInfoArray;

extern PRESTARTSTREAMPROC g_PreStartStreamProc;   //// 开始启动视频传输
extern POSTSTOPSTREAMPROC g_PostStopStreamProc;   //// 停止音视频传输
extern ASKFORAUDIOSAMPLE  g_AskForAudioSample;    //// 要求音频传输

CChannelInfo::CChannelInfo(ULONG ChanNum):
m_bFirstFrm(TRUE),
m_wCurSampleIndex(0),
m_nHisBufIndex(0),
m_bHasAudio(FALSE),
m_uVSendingThread(0),
m_AudioIndex(0)
{
	m_nNum = ChanNum;
	m_pHeader = new BITMAPINFOHEADER;

	m_LatestIFrmBuffer = new BYTE[NORMAL_FRM_LENGTH];
	for(int i = 0; i < 2; i++) {
		m_pHisBuffer[i] = new BYTE[HIS_BUFFER_LENGTH];
		m_pHisNextFrmAddr[i] = m_pHisBuffer[i];
	}

	m_dwFrmTime = GetTickCount();
	m_LatestIFrmInfo.wCmprIndex = (WORD)-1;
	m_LatestIFrmInfo.nFrmLength = 0;
	
	m_hSampleReady = CreateEvent(NULL,TRUE,FALSE,NULL);  // notify 
	InitializeCriticalSection(&m_cSection);

	m_hAudioReady = CreateEvent(NULL,TRUE,FALSE,NULL);
	InitializeCriticalSection(&m_cAudioSection);
	m_AudioSampleLength = AUDIO_NET_SAMPLES_SIZE;  //// 这里以后要修改，和
	m_pAudioBuffer = new BYTE[m_AudioSampleLength];
	m_bExit = FALSE;
}

CChannelInfo::~CChannelInfo()
{
	m_bExit = TRUE;
	SetEvent(m_hSampleReady); //// 要回调函数退出
	SetEvent(m_hAudioReady);
	delete m_pHeader;
	delete m_LatestIFrmBuffer;
	delete m_pHisBuffer[0];
	delete m_pHisBuffer[1];
	
	delete m_pAudioBuffer;
	DeleteCriticalSection(&m_cSection);
	CloseHandle(m_hSampleReady);  
	DeleteCriticalSection(&m_cAudioSection);
	CloseHandle(m_hAudioReady);
}

//// 暂时不用
BOOL CChannelInfo::SendHeader(PVOID dstBuffer)
{
	memcpy(dstBuffer,m_pHeader,sizeof(BITMAPINFOHEADER));
	return TRUE;
}

void CChannelInfo::CopyHeader(ULONG FormatSize, PVOID srcBuffer)
{
	ASSERT(FormatSize <= sizeof(BITMAPINFOHEADER));
	memcpy(m_pHeader, srcBuffer, FormatSize); 
}

void CChannelInfo::Start()
{
	//// TRACE("Channel %d Start Thread:%d",m_nNum,m_uVSendingThread);
	if(0 == m_uVSendingThread){
		ResetState();
		g_PreStartStreamProc(m_nNum,&m_bHasAudio);
#ifdef TEST_TRAN
		m_byTest = 1; //// nvcd
#endif
	}
	InterlockedIncrement(&m_uVSendingThread);
}

void CChannelInfo::Stop()
{
	TRACE("Channel %d Stop thread:%d",m_nNum,m_uVSendingThread);
	if(0 == m_uVSendingThread) return;
	InterlockedDecrement(&m_uVSendingThread);
	if(0 == m_uVSendingThread) {
		TRACE("Channel %d Really Stop Sending",m_nNum);
		g_PostStopStreamProc(m_nNum);
	}
}

////BOOL CChannelInfo::SendSample(DWORD &PacketLength, ULONG &bufferLength, PBYTE &PacketBuffer, BOOL &bPause, ULONG &Index)
BOOL CChannelInfo::SendSample(ULONG &bufferLength, PVIDEO_FRAME_PACKET &FrmPacket, BOOL bOnlyKeyFrm)
{
	ULONG wait;
	ULONG bufSize;
	int nFrmIndex,nHisIndex;
	PVOID pNewBuffer;
	BOOL bGetFrm = FALSE;
	if(0 == m_uVSendingThread)  //// doesn't start transfering engin
		return FALSE;
////	TRACE("The sent index %d,now %d",Index,m_SampleIndex);
	WORD Index = FrmPacket->frmHeader.wFrmIndex;
	if(Index == m_wCurSampleIndex)
	{
		wait = WaitForSingleObject(m_hSampleReady,5000);
		if(wait != WAIT_OBJECT_0) {
			TRACE("Fetch %d Sample Time Out!",m_nNum);
			return FALSE;   //// Too long time not fetch data,
		}
		else if(m_bExit) {
			return FALSE;
		}
		else
			; //// TRACE("Wait Sample OK");
	}

	CSelfLock csMem(&m_cSection);   //// 这里使用互斥量做保护
	BOOL bNeedLastIFrm = FALSE;
	if(bOnlyKeyFrm) {
		if(Index >= m_LatestIFrmInfo.wCmprIndex && (Index-m_LatestIFrmInfo.wCmprIndex < 0x8000)) {
			FrmPacket->frmHeader.wFrmIndex = m_wCurSampleIndex;  //// 如果到了一个WORD满的时候，不这样做会一直得不到下一个I帧的数据。
			return FALSE;
		}
		else {
			bNeedLastIFrm = TRUE;
		}
	}
	if(bNeedLastIFrm) {
		nFrmIndex = -1;
	}
	else {
		nFrmIndex = GetNextInHistory(Index,&nHisIndex);
	}
	if(-1 != nFrmIndex) {
		bufSize = sizeof(VIDEO_FRAME_PACKET) + m_HisSampleArray[nHisIndex][nFrmIndex].uLength;
		if(bufferLength < bufSize)
		{
			delete FrmPacket;
			bufferLength = bufSize;
			pNewBuffer = new BYTE[bufSize];
			FrmPacket = (PVIDEO_FRAME_PACKET)pNewBuffer;
		}
		FrmPacket->frmHeader.byChanNum = (BYTE)m_nNum;
		FrmPacket->frmHeader.wFrmIndex = m_HisSampleArray[nHisIndex][nFrmIndex].wSampleIndex;
		FrmPacket->frmHeader.uFrmLength = m_HisSampleArray[nHisIndex][nFrmIndex].uLength;
		FrmPacket->frmHeader.byFrmInfo = m_HisSampleArray[nHisIndex][nFrmIndex].bySampleInfo;
		memcpy(FrmPacket->VideoBuffer,m_HisSampleArray[nHisIndex][nFrmIndex].pAddress,
			m_HisSampleArray[nHisIndex][nFrmIndex].uLength);
		if( 3 == m_nNum ) {
			TRACE("CH[%d] Send Current Frame %d ,length:%d,HisIndex:%d,FrmPos:%d",
				m_nNum,(Index+1),FrmPacket->frmHeader.uFrmLength,nHisIndex,nFrmIndex);
		}
		bGetFrm = TRUE;
	}
	else if(Index < m_LatestIFrmInfo.wCmprIndex || bNeedLastIFrm) {
		bufSize = sizeof(VIDEO_FRAME_PACKET) + m_LatestIFrmInfo.nFrmLength;
		if(bufferLength < bufSize)
		{
			delete FrmPacket;
			bufferLength = bufSize;
			pNewBuffer = new BYTE[bufSize];
			FrmPacket = (PVIDEO_FRAME_PACKET)pNewBuffer;
		}
		FrmPacket->frmHeader.byChanNum = (BYTE)m_nNum;
		FrmPacket->frmHeader.wFrmIndex = m_LatestIFrmInfo.wCmprIndex;
		FrmPacket->frmHeader.uFrmLength = m_LatestIFrmInfo.nFrmLength;
		FrmPacket->frmHeader.byFrmInfo = m_LatestIFrmInfo.byFlags;
		memcpy(FrmPacket->VideoBuffer,m_LatestIFrmBuffer,m_LatestIFrmInfo.nFrmLength);
		bGetFrm = TRUE;
	}
	else {
		FrmPacket->frmHeader.wFrmIndex = m_wCurSampleIndex;
		bGetFrm = FALSE;
	}
	return bGetFrm;
}

//// 如果没有找到，返回FALSE.这个肯定是在历史数据里面找
BOOL CChannelInfo::FetchSample(PVIDEO_FRAME_HEADER pFrmHeader, ULONG &bufferLength, PBYTE &pDataBuffer)
{
	CSelfLock csMem(&m_cSection);

	if(pFrmHeader->wFrmIndex == m_LatestIFrmInfo.wCmprIndex) {
		if(bufferLength < m_LatestIFrmInfo.nFrmLength) {
			delete pDataBuffer;
			bufferLength = m_LatestIFrmInfo.nFrmLength;
			pDataBuffer = new BYTE[bufferLength];
		}
		pFrmHeader->byChanNum = (BYTE)m_nNum;
		pFrmHeader->uFrmLength = m_LatestIFrmInfo.nFrmLength;
		pFrmHeader->byFrmInfo = m_LatestIFrmInfo.byFlags;
		memcpy(pDataBuffer,m_LatestIFrmBuffer,m_LatestIFrmInfo.nFrmLength);
		//// TRACE("Channel %d resd Latest I Frm:%d",m_nNum,m_LatestIFrmInfo.nFrmLength);
		return TRUE;
	}
	BOOL bGetFrm = FALSE;
	int nHisIndex,i;
	for( nHisIndex = 0; nHisIndex < 2; nHisIndex++) {
		for(i = 0; i < m_HisSampleArray[nHisIndex].GetSize(); i++) {
			if(pFrmHeader->wFrmIndex == m_HisSampleArray[nHisIndex][i].wSampleIndex) {
				bGetFrm = TRUE;
				break;
			}
		}
		if(bGetFrm) {
			break;
		}
	}
	//// TRACE("Channel %d resd Frm:%d,i:%d,HisIndex:%d,result:%d",m_nNum,pFrmHeader->wFrmIndex,i,nHisIndex,bGetFrm);
	if(!bGetFrm) return FALSE;
	if(bufferLength < m_HisSampleArray[nHisIndex][i].uLength) {
		delete pDataBuffer;
		bufferLength = m_HisSampleArray[nHisIndex][i].uLength;
		pDataBuffer = new BYTE[bufferLength];
	}
	pFrmHeader->byChanNum = (BYTE)m_nNum;
	pFrmHeader->byFrmInfo = m_HisSampleArray[nHisIndex][i].bySampleInfo;
	pFrmHeader->uFrmLength = m_HisSampleArray[nHisIndex][i].uLength;
	memcpy(pDataBuffer,m_HisSampleArray[nHisIndex][i].pAddress,m_HisSampleArray[nHisIndex][i].uLength);
	return TRUE;
}

BOOL CChannelInfo::SendAudioSample(ULONG * pSampleLength, PBYTE pAudioBuffer,ULONG &Index)
{
	DWORD wait;
	if(Index == m_AudioIndex) {
		wait = WaitForSingleObject(m_hAudioReady,5000);
		if(wait != WAIT_OBJECT_0) {
			TRACE("Fetch Audio Time out!");
			return FALSE;
		}
		else if(m_bExit) {
			return FALSE;
		}
	}
	EnterCriticalSection(&m_cAudioSection);
	memcpy(pAudioBuffer,m_pAudioBuffer,m_AudioSampleLength);
	*pSampleLength = m_AudioSampleLength;
	Index = m_AudioIndex;
	LeaveCriticalSection(&m_cAudioSection);
	return TRUE;
}

void CChannelInfo::CopyAudioSample(ULONG length,PVOID pSampleBuffer)
{
	EnterCriticalSection(&m_cAudioSection);
	ASSERT(length <= m_AudioSampleLength);
#ifdef _DEBUG
	if(length > m_AudioSampleLength) {
		AfxMessageBox("The sample length larger than Buffer length in ChannelInfo!");
	}
#endif
	memcpy(m_pAudioBuffer,pSampleBuffer,m_AudioSampleLength);
	m_AudioIndex++;
	PulseEvent(m_hAudioReady);
	LeaveCriticalSection(&m_cAudioSection);
	TRACE("Copy Audio length:%d",length);
}

void CChannelInfo::CopySampleEx(CMPR_SAMPLE* psample,PVOID pLastIFrmBuffer,PVOID pCurFrmBuffer)
{

	CSelfLock csMem(&m_cSection);
	if(m_bFirstFrm && 0 == (psample->CurFrmInfo.byFlags&KEYFRAME)) {
		return; //// 第一帧不是I帧则抛弃
	}
	else {
		m_bFirstFrm = FALSE;
	}
	if(psample->CurFrmInfo.byFlags & KEYFRAME) {
		if(3 == m_nNum) {
			//// TRACE("DVR Get I FrmIndex:%d,length:%d",psample->CurFrmInfo.wCmprIndex,psample->CurFrmInfo.nFrmLength);
		}
		//// 当前帧是I帧
		if(m_LatestIFrmInfo.nFrmLength < psample->CurFrmInfo.nFrmLength)
		{
			delete m_LatestIFrmBuffer;
			m_LatestIFrmBuffer = new BYTE[psample->CurFrmInfo.nFrmLength];
		}
		m_LatestIFrmInfo = psample->CurFrmInfo;
		m_wCurSampleIndex = psample->CurFrmInfo.wCmprIndex;
#ifndef TEST_TRAN
		memcpy(m_LatestIFrmBuffer,pLastIFrmBuffer,m_LatestIFrmInfo.nFrmLength);
#else
		memset(m_LatestIFrmBuffer,0xFF,m_LatestIFrmInfo.nFrmLength);
#endif
		
	}
	else {
		if(m_LatestIFrmInfo.wCmprIndex != psample->LastIFrmInfo.wCmprIndex)
		{
			if( 3== m_nNum) {
				TRACE("DVR CurFrm not I Frm,IFrm:%d,Length:%d",
					psample->LastIFrmInfo.wCmprIndex,psample->LastIFrmInfo.nFrmLength);
			}
			if(m_LatestIFrmInfo.nFrmLength < psample->LastIFrmInfo.nFrmLength)
			{
				delete m_LatestIFrmBuffer;
				m_LatestIFrmBuffer = new BYTE[psample->LastIFrmInfo.nFrmLength];
			}
			m_LatestIFrmInfo = psample->LastIFrmInfo;
#ifndef TEST_TRAN
			memcpy(m_LatestIFrmBuffer,pLastIFrmBuffer,m_LatestIFrmInfo.nFrmLength);
#else
			memset(m_LatestIFrmBuffer,0xFF,m_LatestIFrmInfo.nFrmLength);
#endif
		}
		/*
		if( 3== m_nNum) {
			TRACE("DVR Get CurFrm:%d,length:%d,Last Frm:%d",
				psample->CurFrmInfo.wCmprIndex,psample->CurFrmInfo.nFrmLength,m_wCurSampleIndex);
		}
		*/
		/*
		if(psample->CurFrmInfo.wCmprIndex == (m_wCurSampleIndex+1)) {
			//// 如果帧不连续，就不存入历史缓冲
			PushFrm2History(&psample->CurFrmInfo,pCurFrmBuffer);
			m_wCurSampleIndex = psample->CurFrmInfo.wCmprIndex;
		}
		*/
		//// 直接存入
		PushFrm2History(&psample->CurFrmInfo,pCurFrmBuffer);
		m_wCurSampleIndex = psample->CurFrmInfo.wCmprIndex;
	}
	//// TRACE("Cur Index:%d,LastI:%d",m_wSampleIndex,m_wLatestIFrmIndex);
	PulseEvent(m_hSampleReady); //// if someone waiting for new frame...
}

void CChannelInfo::PushFrm2History(CMPR_FRAME_INFO* pFInfo,PVOID pFrmBuffer)
{
	SAMPLE_HISINFO hisSampleInfo;
	ASSERT(pFInfo->nFrmLength < HIS_BUFFER_LENGTH);
	DWORD nRemainLength = HIS_BUFFER_LENGTH - (m_pHisNextFrmAddr[m_nHisBufIndex] - m_pHisBuffer[m_nHisBufIndex]);
	if(pFInfo->nFrmLength > nRemainLength) {
		m_nHisBufIndex = (0 == m_nHisBufIndex) ? 1 : 0;
		m_HisSampleArray[m_nHisBufIndex].RemoveAll();
		m_pHisNextFrmAddr[m_nHisBufIndex] = m_pHisBuffer[m_nHisBufIndex];
	}

	//// 即便该帧的数据长度是0也存入，客户端好判断
	hisSampleInfo.pAddress = m_pHisNextFrmAddr[m_nHisBufIndex];
	hisSampleInfo.wSampleIndex = pFInfo->wCmprIndex;
	hisSampleInfo.uLength = pFInfo->nFrmLength;
	hisSampleInfo.bySampleInfo = pFInfo->byFlags;
	m_HisSampleArray[m_nHisBufIndex].Add(hisSampleInfo);
	memcpy(m_pHisNextFrmAddr[m_nHisBufIndex],pFrmBuffer,pFInfo->nFrmLength);
	m_pHisNextFrmAddr[m_nHisBufIndex] += pFInfo->nFrmLength;
	//// TRACE("Add His Array FrmIndex:%d,HisIndex:%d",hisSampleInfo.wSampleIndex,m_nHisBufIndex);
	return;
}

int CChannelInfo::GetNextInHistory(WORD wFrmIndex,int *pnHisBufIndex)
{
	int listIndex = -1;
	WORD wFrmIndexWant = wFrmIndex + 1;
	for(int nHisBufIndex = 0; nHisBufIndex < 2; nHisBufIndex++) {
		for(int i = 0; i < m_HisSampleArray[nHisBufIndex].GetSize(); i++) {
			if(wFrmIndexWant == m_HisSampleArray[nHisBufIndex][i].wSampleIndex) {
				*pnHisBufIndex = nHisBufIndex;
				return i;
			}
		}
	}
	return listIndex;
}

void CChannelInfo::ResetHistoryFrm()
{
	for(int i = 0; i < 2; i++) {
		m_HisSampleArray[i].RemoveAll();
		m_pHisNextFrmAddr[i] = m_pHisBuffer[i];
	}
	m_nHisBufIndex = 0;
}