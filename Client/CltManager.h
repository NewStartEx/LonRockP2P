#ifndef _CLTMANAGER_H
#define _CLTMANAGER_H

#include "NTTProtocol.h"
#include "CltGlobal.h"
#include "DvrTnel.h"

//// ����DVR�ķ�ʽ
#define LINK_TUNNEL	0
#define LINK_CMD	1

//// ����DVR����ʧ�ܵ�ԭ��
#define OBJECT_SUCCESS	0
#define OBJECT_NETS_ERR	1
#define OBJECT_DVR_ERR	2

#define CHECK_OVER_INTERVAL	1000 //// ��˼��ʱ����һ�Σ��������������ݵȡ��������ʱ�䵽������һ��
#define DVR_RCV_BUF_LENGTH	1024 //// ����BUF����

class CCltManager
{
public:
	CCltManager();
	~CCltManager();
	BOOL StartWorking(HWND hWnd,LPCTSTR NetSName);
	void StopWorking();

	HANDLE StartLink(LPCTSTR DvrName,int nLinkType,PCMDINFO pFirstCmd,BYTE &Error);
	void StopLink(LPCTSTR DvrName);
	void StopLink(HANDLE hDvr);
	BOOL SendCmd(LPCTSTR DvrName,PCMDINFO pCmd);
	BOOL SendCmd(HANDLE hDvr,PCMDINFO pCmd);
	LPCTSTR GetDvrName(HANDLE hDvr);

protected:
	HWND m_hMsgWnd;
	BOOL m_bWorking;
	CString m_strNetSName;
	SOCKADDR_IN m_LocalAddr;
	ULONG m_uNetSAddr;
	WORD  m_wNetSPort,m_wDvrPort,m_wLocalPort;
	SOCKET m_sRcvGram;
	CDvrTnelArray m_DvrTnelArray;
	LONG m_nThreadCount;
	CTnelMsgArray m_TnelMsgArray;
	CCnctDescArray m_CnctDescArray;
	CMsgGramArray m_MsgGramArray;
	CRITICAL_SECTION m_csGram,m_csDvrArray,m_csRes;
	HANDLE m_hRcvGram,m_hTnelGram,m_hCnctGram,m_hMsgGram,m_hStopWorking;

	void IncreaseThreadCount() 
	{	InterlockedIncrement(&m_nThreadCount);
	}
	void DecreaseThreadCount() 
	{	InterlockedDecrement(&m_nThreadCount);
	}

	UINT KeepRcving();
	UINT DistribGram();
	UINT CheckOver();
	UINT DealwithTnelGram();
	UINT DealwithCnctGram();
	UINT DealwithMsgGram();
	BOOL LegalTnel(PTNELMSG_GRAM pTnel,char *pDvrName); //// ����Ϣ�Ƿ�Ϸ�
	BOOL LegalCnct(POPENTNEL pOpen,char *pDvrName); //// �������Ϣ�Ƿ�Ϸ�
	BOOL LegalMsg(PWORK_GRAM pMsg,char *pDvrName,char *pCltName);
	CDvrTnel *GetDvr(char* Name);
	
	static UINT RcvGramProc(LPVOID p);
	static UINT DistribGramProc(LPVOID p);
	static UINT CheckOverProc(LPVOID p);
private:
};
#endif // _CLTMANAGER_H