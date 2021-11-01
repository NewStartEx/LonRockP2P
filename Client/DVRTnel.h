#ifndef _DVRTNEL_H
#define _DVRTNEL_H

//// DVR 端设置的不可以两个同样名称的Client连接一个。
//// 而这里每个类有唯一的名称，因此可以连接相同的DVR
enum LinkStatus { Link_Applying, Link_Creating, Link_Found};

/// 原来设置的时间有点太长了。
#define MAX_REQ_COUNT			30  //// 最多发送这么多次请求，直到收到NetS的回复，应为1秒一次
#define MAX_TNEL_COUNT			30  //// 这里是1秒一个发送行为
#define MAX_REPLY_INTERVAL		30   //// 大概半分钟没有收到信息，则为对方断线
#define KEEPLIVE_INTERVAL		5   //// 间隔这么多次数发送一个心跳包

//// 被删除的原因。。。
#define EXIT_UNKNOW		 0
#define EXIT_SOCKET_ERR	 1
#define EXIT_DVR_OFFLINE 2
#define EXIT_USER_DO	 3
#define EXIT_DVR_BUSY	 4
#define EXIT_LINK_DUP	 5
#define EXIT_NETS_LOST   6
#define EXIT_DVR_NONAME  7

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

	BOOL InitResource(SOCKET sDVR,PCMDINFO pFirstCmd);
	BOOL SelfChecking(); //// 以后考量内容
	LPCTSTR GetDvrName() {
		return m_strShowName;
	}

	void SetLiveCmd(PCMDINFO pLiveCmd) {
		EnterCriticalSection(&m_csGram);
		memcpy(&m_LiveInfo,pLiveCmd,sizeof(CMDINFO));
		LeaveCriticalSection(&m_csGram);
	}
	BOOL IsMyself(LPCTSTR ShowName);
	BOOL FindDvr(LPCTSTR DvrName);
	UINT SetCmdGram(PCMDINFO pCmd);
	BOOL DealwithTnelGram(PTNELMSG_GRAM pTnel); 
	BYTE DealwithCnctGram(PCNCTDESC pTnelInfo); 
	BOOL DealwithMsgGram(PCMDINFO pWorkInfo,LPCTSTR pCltName);

	static int m_TotalCount;

protected:
	int  m_nIndex;
	char m_chName[NAME_BUF_LENGTH];
	CString m_strDvrName,m_strShowName; //// 实际名称和显示名称。在直连的时候，实际名称和显示名称不一致
	SOCKET m_sDVR;
	SOCKADDR_IN m_NetSAddr,m_DvrAddr;
	BOOL m_bFound,m_bHopeless;
	LinkStatus m_LinkStatus;
	HANDLE m_hSendGram,m_hStopWorking;
	CLTREQ_GRAM m_ReqGram;
	TNELMSG_GRAM m_TnelGram;
	OPENTNEL m_OpenGram;
	WORK_GRAM m_CmdGram;
	CMDINFO m_LiveInfo;

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
	void MakeName();   //// 以后要修改
	void MakeLiveGram();   //// 接收到消息回复后，设置普通心跳。以后要修改
	BOOL SendGram();
	static UINT SendGramProc(LPVOID p);
private:
};

typedef CArray<CDvrTnel*,CDvrTnel*> CDvrTnelArray;
#endif // _DVRTNEL_H