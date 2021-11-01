#ifndef _FILLVIDEO_H
#define _FILLVIDEO_H

#include "NTTProtocol.h"
#include "VicoFormat.h"
#include "MemVicocDecFun.h"

typedef struct _VIDEO_FRAME
{
	DWORD dwID;
	H264HEADER  frmHeader;
	BYTE  VideoBuffer[1];
} VIDEO_FRAME ,*PVIDEO_FRAME;

struct PACKTABLE {
	BOOL bFilled;
	WORD wPackIndex;
	WORD wFrmIndex;
	BYTE byFrmInfo;
	BYTE byPackInFrm;
	BYTE byMaxPackNum;
	BYTE byReserved;
	DWORD dwFrmLength;
	PBYTE pBuf; //// 缓冲区的首地址
};

#define KEYFRAME	(1<<1)   //// XVID Keyframe 
#define NORMAL_FRM_LENGTH		0x20000
#define PACK_BUFFER_COUNT		1000
#define REC_BUF_LENGTH			0x100000

class CFillVideo
{
public:
	CFillVideo(int ChanIndex);
	~CFillVideo();

	BOOL InitResource(LPCTSTR DvrName,LPCTSTR LocalName,SOCKADDR_IN sockAddr);
	BOOL FillPacket(PVPACKHEADER pHeader,PBYTE pVideoData);
	BOOL FillStart();
	void FillStop();
	void SetRelayInterval(DWORD dwInterval) {
		m_dwRelayInterval = dwInterval < DEFAULT_NETRELAY_INTERVAL ? DEFAULT_NETRELAY_INTERVAL : dwInterval;
	}
	void SetVideoRect(int left,int top,int width,int height);
	RECT GetVideoRect() {
		return m_drwRect;
	}
	BOOL IsInside(POINT p) {
		return p.x>=m_drwRect.left&&p.x<=m_drwRect.right&&p.y>=m_drwRect.top&&p.y<m_drwRect.bottom&&m_bShow; /// m_Show means only show picture's class is valid
	}
	BOOL IsInside(int x,int y) {
		return x>=m_drwRect.left&&x<=m_drwRect.right&&y>=m_drwRect.top&&y<m_drwRect.bottom&&m_bShow; /// m_Show means only show picture's class is valid
	}
	int GetChannel() {
		return m_nChannel;
	}
	void ShowVideo(BOOL show);
	void RefreshVideo();
	BOOL Snap(LPCTSTR FileName);
	BOOL StartRecord(LPCTSTR FileName);
	void StopRecord();
	DWORD GetRecFilePos() {
		return m_dwWrittenPos;
	}

protected:
	SOCKADDR_IN m_sockDvrAddr;
	int m_nChannel;
	CString m_strDvrName,m_strLocalName;

	LONG m_nThreadCount;
	void IncreaseThreadCount() 
	{	InterlockedIncrement(&m_nThreadCount);
	}
	void DecreaseThreadCount() 
	{	InterlockedDecrement(&m_nThreadCount);
	}
	HANDLE m_hDecode;
	HANDLE m_hDecFrm,m_hStopWorking;
	BOOL m_bShow,m_bVideoWorking;
	RECT m_drwRect;
	void ResetState();
	PVOID TryFrmBuffer(UINT uFrmLength);
	BOOL PackInRange(WORD wIndex);
	BOOL FindPackPose(WORD wPackIndex, int* pnPackPos);

	int m_nOldestPackBufIndex,m_nNextPackBufIndex; //// 缓冲池里最早和最晚的包的索引
	int m_nCurDecBufIndex; //// 当前解码开始的包索引
	BOOL m_bFirstPacket;
	PACKTABLE m_PackTable[PACK_BUFFER_COUNT];
	PBYTE m_pPackBuffer; //// 被切割成PACK_BUFFER_COUNT份，每份的大小刚好是视频数据的传输大小

	FILE *m_hRecFile;
	BOOL  m_bRecording,m_bFirstRectFrm;
	int   m_nRecRemainLength;
	PBYTE m_pRecBuffer,m_pRecCur;  //// 录像资料保存
	DWORD m_dwWrittenPos;
	DWORD m_dwFrmTime;

	ULONG m_uFrmBufLength;
	PVIDEO_FRAME m_pVideoFrm;
	CRITICAL_SECTION m_csRes,m_csRecord;
	BOOL m_bPackBufFull;
	BOOL m_bIFrmReset;
	BOOL m_bTrafficACK;
	BOOL m_bACKFrmRcving;
	BOOL m_bSendAnotherIFrmACK;
	WORD m_wCurACKFrmIndex,m_wLatestDecFrmIndex;
	DWORD m_dwRelayInterval;

	BOOL WaitDecodeEvent();
	BOOL WaitResdTimeout();
	void DecodeFrm();
	static UINT DecodeFrameProc(LPVOID p);
	void CheckPack();
	static UINT ResdPackProc(LPVOID);
private:
};

typedef CArray<CFillVideo*, CFillVideo*> CFillVideoArray;
#endif // _FILLVIDEO_h