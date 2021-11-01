#ifndef _DVRTNEL_H
#define _DVRTNEL_H
#ifndef MEMVICOC_LIB
#pragma  comment(lib,"MemVicocDec.lib")
#endif

#include "FillVideo.h"
//// DVR �����õĲ���������ͬ�����Ƶ�Client����һ����
//// ������ÿ������Ψһ�����ƣ���˿���������ͬ��DVR
enum LinkStatus { Link_Applying, Link_Creating, Link_HandShaking, Link_Working};

/// ԭ�����õ�ʱ���е�̫���ˡ�
#define MAX_REQ_COUNT			30  //// ��෢����ô�������ֱ���յ�NetS�Ļظ���ӦΪ1��һ��
#define MAX_TNEL_COUNT			30  //// ������1��һ��������Ϊ
#define MAX_REPLY_INTERVAL		30   //// ��Ű����û���յ���Ϣ����Ϊ�Է�����
#define KEEPLIVE_INTERVAL		5   //// �����ô���������һ��������

//// ��������ʧ�ܵ�ԭ��
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
	BOOL SelfChecking(); //// �Ժ�������
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
	CString m_strDvrName,m_strShowName; //// ʵ�����ƺ���ʾ���ơ���ֱ����ʱ��ʵ�����ƺ���ʾ���Ʋ�һ��
	SOCKADDR_IN m_NetSAddr,m_DvrAddr;
	BOOL m_bFound,m_bHopeless,m_bOnline;
	LinkStatus m_LinkStatus;
	HANDLE m_hSendGram,m_hNaviData,m_hStopWorking;  //// �����߳��������������
	CLTREQ_GRAM m_ReqGram;
	TNELMSG_GRAM m_TnelGram;
	OPENTNEL m_OpenGram;
	HSHK_GRAM m_HshkGram;
	WORK_GRAM m_CmdGram;
	CMDINFO m_LiveInfo;
	CMDINFO m_VideoInfo;
	CMDINFO m_VideoMsg;  //// ���Video�������
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
	int m_nSendTnelCount; //// ��ϵͳ�����ļ��� ����ʱ �Զ�����������Manager������
	int m_nLastRcvTick;
	int m_nKeepliveTick;
	UINT m_uExitReason;
	BOOL m_bReplied; //// ����յ�DVR������ķ�Ӧ������ͨ�������������򣬷������һ���������������֤ͨѶ����
	DWORD m_dwBandWidth,m_dwNaviPackCount,m_dwRelayInterval,m_dwRelayStart;
	BOOL m_bNaviDataArrived;
	void MakeName();   //// �Ժ�Ҫ�޸�
	//// void MakeLiveGram();   //// ���յ���Ϣ�ظ���������ͨ�������Ժ�Ҫ�޸�
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