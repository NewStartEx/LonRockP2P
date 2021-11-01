#ifndef _CHANNELINFO_H
#define _CHANNELINFO_H

#include "NTTStationFun.h"

typedef struct _VIDEO_FRAME_HEADER {
	BYTE byChanNum;       //// Channel Index
	BYTE byFrmInfo;       //// Frame Info: Key Frame, .... 
	WORD wFrmIndex;
	ULONG uFrmLength;
} VIDEO_FRAME_HEADER,*PVIDEO_FRAME_HEADER;

typedef struct _VIDEO_FRAME_PACKET
{
	VIDEO_FRAME_HEADER frmHeader;
	BYTE VideoBuffer[1];
} VIDEO_FRAME_PACKET ,*PVIDEO_FRAME_PACKET;

typedef struct _SAMPLE_HISINFO {
	PBYTE pAddress;
	ULONG uLength;
	WORD  wSampleIndex;
	BYTE  bySampleInfo;
	BYTE  byReserved;
} SAMPLE_HISINFO;
typedef CArray <SAMPLE_HISINFO,SAMPLE_HISINFO> CHisSampleArray;

#define HIS_BUFFER_LENGTH		0x200000  //// 2M 
#define NORMAL_FRM_LENGTH		0x20000
#define AUDIO_NET_SAMPLES_SIZE	0x1000  /// !!!!

#define KEYFRAME	(1<<1)   //// XVID Keyframe 

//// #define TEST_TRAN	
class CChannelInfo
{
public:
	CChannelInfo(ULONG ChanNum);
	~CChannelInfo();

	ULONG GetChannelNum()
	{
		return m_nNum;
	}
	
	void SetChannelNum(ULONG num)
	{
		m_nNum = num;
	}

	BOOL IsStart()
	{
		return (m_uVSendingThread > 0);
	}

	BOOL HasAudio() {
		return m_bHasAudio;
	}
	BOOL SendHeader(PVOID dstBuffer);
	void CopyHeader(ULONG FormatSize, PVOID srcBuffer);

	BYTE m_byTest; /// nvcd
	void Start();
	void Stop();
	BOOL SendSample(ULONG &bufferLength, PVIDEO_FRAME_PACKET &FrmPacket, BOOL bOnlyKeyFrm);
	BOOL FetchSample(PVIDEO_FRAME_HEADER pFrmHeader, ULONG &bufferLength, PBYTE &pDataBuffer);  //// 如果没有找到，返回FALSE.这个肯定是在历史数据里面找
	void CopySampleEx(CMPR_SAMPLE* psample,PVOID pLastIFrmBuffer,PVOID pCurFrmBuffer);
	void CopyAudioSample(ULONG length,PVOID pSampleBuffer);
	BOOL SendAudioSample(ULONG * pSampleLength, PBYTE pAudioBuffer,ULONG &Index);
	void ResetAudiostatus() {
		m_AudioIndex = 0;
	}
	void ResetState()
	{
		m_bFirstFrm = TRUE;
		m_wCurSampleIndex = 0;
		m_LatestIFrmInfo.wCmprIndex = (WORD)-1;
		m_LatestIFrmInfo.nFrmLength = 0;
		ResetHistoryFrm();
	}
protected:
	ULONG m_nNum;
	DWORD m_dwFrmTime;
	BITMAPINFOHEADER * m_pHeader;

	/*
	CMPR_FRAME_INFO m_SampleInfo;
	PBYTE m_SampleBuffer;
	DWORD m_SampleLength;
	ULONG m_SampleBufLength;
	BYTE  m_byFrameType;
	*/
	
	BOOL m_bExit;
	BOOL m_bFirstFrm;
	CMPR_FRAME_INFO m_LatestIFrmInfo;
	PBYTE m_LatestIFrmBuffer;

	PBYTE m_pHisBuffer[2];
	PBYTE m_pHisNextFrmAddr[2];
	CHisSampleArray m_HisSampleArray[2];
	int	  m_nHisBufIndex;
	void PushFrm2History(CMPR_FRAME_INFO* pFInfo,PVOID pFrmBuffer);
	int GetNextInHistory(WORD wFrmIndex,int *pnHisBufIndex);
	void ResetHistoryFrm();
	
	CRITICAL_SECTION m_cSection;
	HANDLE	m_hSampleReady;      // for new sample 
	WORD	m_wCurSampleIndex;
	LONG m_uVSendingThread;

	BOOL   m_bHasAudio;
	CRITICAL_SECTION m_cAudioSection;
	HANDLE m_hAudioReady;  //// for new audio sample
	PBYTE  m_pAudioBuffer;    //// to get audio source.it maybe larger than 1024 bytes, so it's difficult to send by UDP
	ULONG  m_AudioSampleLength; 
	ULONG  m_AudioIndex;

};

typedef CArray<CChannelInfo*, CChannelInfo*> CChanInfoArray;
extern CChanInfoArray g_ChanInfoArray;

#endif // _CHANNELINFO_H