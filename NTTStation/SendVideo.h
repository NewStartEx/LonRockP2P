#ifndef _SENDVIDEO_H
#define _SENDVIDEO_H

#include "NTTProtocol.h"
#include "ChannelInfo.h"

#define PACK_BUF_COUNT		200  //// һ���������ӱ����ķ��ͻ����� �������������Ļ���
typedef CArray <WORD,WORD> CPackResdArray;
struct PACKFRMTABLE {
	WORD wPackIndex;
	WORD wFrmIndex;
	WORD wPackInfo; //// �Ƿ�Ҫȷ�ϴ�����ɣ��Ƿ�I֡
	WORD wPackInFrm; //// ��Frm��İ�����
};
typedef CArray<PACKFRMTABLE,PACKFRMTABLE> CPackFrmTableArray;
#define PACKFRMTABLE_LENGTH		3000
class CSendVideo
{
public:
	CSendVideo(int ChanIndex);
	~CSendVideo();

	BOOL IsSending() {
		return m_bSending;
	}
	int GetIndex() {
		return m_nIndex;
	}
	BOOL StartSending();
	BOOL StopSending();
	BOOL AskForResend(PVRESDPACKET pVResdPackt);
	BOOL InitResource(LPCTSTR DvrName,LPCTSTR CltName,SOCKADDR_IN sockAddr);
	void SetNetStatus(DWORD dwNwidPackets,DWORD dwRelayInterval) {
		m_dwNwidPackets = dwNwidPackets;
		m_dwRelayInterval = dwRelayInterval < DEFAULT_NETRELAY_INTERVAL ? DEFAULT_NETRELAY_INTERVAL : dwRelayInterval;
		TRACE("Set NWIdPacks:%d,RelayInterval:%d",m_dwNwidPackets,m_dwRelayInterval);
	}

protected:
	SOCKADDR_IN m_sockCltAddr;
	int m_nIndex;
	BOOL m_bSending,m_bSendFinish;
	CString m_strDvrName,m_strCltName;

	ULONG m_uFrmBufLength;
	PVIDEO_FRAME_PACKET m_pFrmPacket;
	CPackResdArray m_PackResdArray;
	int m_nCurPackBufArrayIndex,m_nCurPackBufIndex,m_nCurPackFrmTableIndex;   //// �������ָʾ
	DWORD m_dwSendPackCount; //// һ�뷢�Ͱ�������
	BOOL m_bWaitIFrmACK,m_bResdHappened,m_bResetPackIndex,m_bIFrmACKMaking;
#define NWID_REDUCE_COUNT	2
	int m_nNWidReduceTick;
	WORD m_wACKFrmIndex;  //// ��ǰҪ������ɻش�ȷ�ϵ�֡����
	CRITICAL_SECTION m_csRes,m_csPackCounting,m_csPackBuf;

	DWORD m_dwNwidPackets,m_dwRelayInterval;
	WORD m_wPackIndex;
	VIDEO_FRAME_HEADER m_vResdHeader;
	PBYTE m_pResdFrmBuffer;
	ULONG m_uResdFrmLength;
	PBYTE m_pSendFrmGram,m_pResdGram,m_pSentPack[2][PACK_BUF_COUNT];
	CPackFrmTableArray m_PackFrmTableArray[2];
	HANDLE m_hIFrmACK,m_hRestartSend,m_hResdFrm,m_hStopWorking;
	LONG m_nThreadCount;
	void IncreaseThreadCount() 
	{	InterlockedIncrement(&m_nThreadCount);
	}
	void DecreaseThreadCount() 
	{	InterlockedDecrement(&m_nThreadCount);
	}
	void SendFrm();
	static UINT SendFrmProc(LPVOID p);
	void Resend();
	BOOL WaitResdEvent();
	static UINT ResendProc(LPVOID p);
	void SentStatistic();
	static UINT SentStatisticProc(LPVOID p);
private:
};

typedef CArray<CSendVideo*,CSendVideo*> CSendVideoArray;
#endif // _SENDVIDEO_H 