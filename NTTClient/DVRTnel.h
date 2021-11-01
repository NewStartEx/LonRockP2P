#ifndef _DVRTNEL_H
#define _DVRTNEL_H
#ifndef MEMVICOC_LIB
#pragma  comment(lib,"MemVicocDec.lib")
#endif

#include "FillVideo.h"
//// DVR 端设置的不可以两个同样名称的Client连接一个。
//// 而这里每个类有唯一的名称，因此可以连接相同的DVR
enum LinkStatus { Link_Applying, Link_Creating, Link_HandShaking, Link_Working};

/// 原来设置的时间有点太长了。
#define MAX_REQ_COUNT			30  //// 最多发送这么多次请求，直到收到NetS的回复，应为1秒一次
#define MAX_TNEL_COUNT			30  //// 这里是1秒一个发送行为
#define MAX_REPLY_INTERVAL		30   //// 大概半分钟没有收到信息，则为对方断线
#define KEEPLIVE_INTERVAL		5   //// 间隔这么多次数发送一个心跳包

//// 发送命令失败的原因
#define CMD_SUCCESS		0
#define CMD_SOCK_FAIL	1
#define CMD_LINKING		2

void EncodeCopyName(char *pDstBuf,LPCTSTR pSrcBuf);
CString DecodeName(char *pName);

class CDvrTnel
{
public:
	CDvrTnel(LPCTSTR DvrGlobalName, ULONG NetSAddr, WORD wNetSPort);
	CDvrTnel(LPCTSTR DvrIPName,WORD wDvrPort);
	~CDvrTnel();

	BOOL InitResource(PCMDINFO pFirstCmd);
	BOOL SelfChecking(); //// 以后考量内容
	LPCTSTR GetDvrName() {
		return m_strShowName;
	}

	BOOL CreateVideoChannel(int nChanCount);
	int  GetChannelCount(); 
	void StartVideoTran(int chan);
	void StopVideoTran(int chan);
	void StartAllVideoTran();
	void StopAllVideoTran();
	BOOL StartRecord(int chan,LPCTSTR FileName);
	BOOL StopRecord(int chan);
	BOOL Snap(int chan,LPCTSTR FileName);
	DWORD GetRecFilePos(int chan);
	
	void SetLiveCmd(PCMDINFO pLiveCmd) {
		EnterCriticalSection(&m_csGram);
		memcpy(&m_LiveInfo,pLiveCmd,sizeof(CMDINFO));
		LeaveCriticalSection(&m_csGram);
	}
	BOOL IsMyself(LPCTSTR ShowName);
	BOOL FindDvr(LPCTSTR DvrName);
	UINT SetCmdGram(PCMDINFO pCmd);
	BOOL DealwithTnelGram(PTNELMSG_GRAM pTnel); 
	BYTE DealwithCnctGram(POPENTNEL pOpen,SOCKADDR_IN IPAddr); 
	BOOL DealwithHshkACK(BYTE byAct,LPCTSTR pCltName);
	BOOL DealwithMsgGram(PCMDINFO pWorkInfo,LPCTSTR pCltName);
	BOOL DealwithVideoGram(PVPACKHEADER pVHeader,PBYTE pVData,LPCTSTR pCltName);
	BOOL DealwithNaviData(LPCTSTR pCltName);
	BOOL SetVideoPos(int chan,int left,int top,int width,int height);
	BOOL ShowVideo(int chan,BOOL bShow);
	BOOL RefreshVideo(int chan);
	void SetAlive();
	void SetExitMark(UINT uExitReason);
	static int m_TotalCount;

protected:
	int  m_nIndex;
	char m_chName[NAME_BUF_LENGTH];
	CString m_strDvrName,m_strShowName; //// 实际名称和显示名称。在直连的时候，实际名称和显示名称不一致
	SOCKADDR_IN m_NetSAddr,m_DvrAddr;
	BOOL m_bFound,m_bHopeless,m_bOnline;
	LinkStatus m_LinkStatus;
	HANDLE m_hSendGram,m_hNaviData,m_hStopWorking;  //// 创建线程来处理带宽问题
	CLTREQ_GRAM m_ReqGram;
	TNELMSG_GRAM m_TnelGram;
	OPENTNEL m_OpenGram;
	HSHK_GRAM m_HshkGram;
	WORK_GRAM m_CmdGram;
	CMDINFO m_LiveInfo;
	CMDINFO m_VideoInfo;
	CMDINFO m_VideoMsg;  //// 检查Video传输情况
	CMDINFO m_AudioInfo;
	CMDINFO m_AudioMsg;
	CFillVideoArray m_FillVideoArray;

	BOOL m_bRcvVideo,m_bRcvAudio;
	BOOL m_bOnlyNeedAddr;

	LONG m_nThreadCount;
	CRITICAL_SECTION m_csGram,m_csCount;
	void IncreaseThreadCount() 
	{	InterlockedIncrement(&m_nThreadCount);
	}
	void DecreaseThreadCount() 
	{	InterlockedDecrement(&m_nThreadCount);
	}
	int m_nSendReqCount; 
	int m_nSendTnelCount; //// 对系统操作的计数 看超时 自动更新命令由Manager来发送
	int m_nLastRcvTick;
	int m_nKeepliveTick;
	UINT m_uExitReason;
	BOOL m_bReplied; //// 如果收到DVR对命令的反应，则发送通常的心跳，否则，发送最后一个命令。这样尽量保证通讯完整
	DWORD m_dwBandWidth,m_dwNaviPackCount,m_dwRelayInterval,m_dwRelayStart;
	BOOL m_bNaviDataArrived;
	void MakeName();   //// 以后要修改
	//// void MakeLiveGram();   //// 接收到消息回复后，设置普通心跳。以后要修改
	BOOL AllVideoNotified();
	BOOL AllAudioNotified();
	BOOL SendGram();
	BOOL CalcBandWidth();
	static UINT SendGramProc(LPVOID p);
	static UINT CalcBandWidthProc(LPVOID p);
private:
};

typedef CArray<CDvrTnel*,CDvrTnel*> CDvrTnelArray;
#endif // _DVRTNEL_H