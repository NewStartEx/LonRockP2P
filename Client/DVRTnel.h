#ifndef _DVRTNEL_H
#define _DVRTNEL_H

//// DVR �����õĲ���������ͬ�����Ƶ�Client����һ����
//// ������ÿ������Ψһ�����ƣ���˿���������ͬ��DVR
enum LinkStatus { Link_Applying, Link_Creating, Link_Found};

/// ԭ�����õ�ʱ���е�̫���ˡ�
#define MAX_REQ_COUNT			30  //// ��෢����ô�������ֱ���յ�NetS�Ļظ���ӦΪ1��һ��
#define MAX_TNEL_COUNT			30  //// ������1��һ��������Ϊ
#define MAX_REPLY_INTERVAL		30   //// ��Ű����û���յ���Ϣ����Ϊ�Է�����
#define KEEPLIVE_INTERVAL		5   //// �����ô���������һ��������

//// ��ɾ����ԭ�򡣡���
#define EXIT_UNKNOW		 0
#define EXIT_SOCKET_ERR	 1
#define EXIT_DVR_OFFLINE 2
#define EXIT_USER_DO	 3
#define EXIT_DVR_BUSY	 4
#define EXIT_LINK_DUP	 5
#define EXIT_NETS_LOST   6
#define EXIT_DVR_NONAME  7

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

	BOOL InitResource(SOCKET sDVR,PCMDINFO pFirstCmd);
	BOOL SelfChecking(); //// �Ժ�������
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
	CString m_strDvrName,m_strShowName; //// ʵ�����ƺ���ʾ���ơ���ֱ����ʱ��ʵ�����ƺ���ʾ���Ʋ�һ��
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
	int m_nSendTnelCount; //// ��ϵͳ�����ļ��� ����ʱ �Զ�����������Manager������
	int m_nLastRcvTick;
	int m_nKeepliveTick;
	UINT m_uExitReason;
	BOOL m_bReplied; //// ����յ�DVR������ķ�Ӧ������ͨ�������������򣬷������һ���������������֤ͨѶ����
	void MakeName();   //// �Ժ�Ҫ�޸�
	void MakeLiveGram();   //// ���յ���Ϣ�ظ���������ͨ�������Ժ�Ҫ�޸�
	BOOL SendGram();
	static UINT SendGramProc(LPVOID p);
private:
};

typedef CArray<CDvrTnel*,CDvrTnel*> CDvrTnelArray;
#endif // _DVRTNEL_H