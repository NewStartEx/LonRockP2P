#ifndef _LINKNETS_H
#define _LINKNETS_H

#include "DvrGlobal.h"

#define MAX_REQSEND_COUNT	3 //// 10
#define REQ_INTERVAL	    5000    //// �ڵȴ���Ӧ��ʱ��ÿ5�뷢һ�Σ�ֱ���������
#define AUTOUPDATE_INTERVAL 60     //// ��ô���뷢��һ���Զ�����,���̫���˿���NAT�ر����ͨ����

//// ���¸�����2009-11-21:
//// Ϊ����NetS IP ���ĺ��Ҳ���������Ϊ���캯��ʹ��NetS���ƣ����Ҹ������ʹ��Name2IP

//// SelfChecking return value: 
class CLinkNetS
{
public:
	CLinkNetS(LPCTSTR NetSName,WORD Port);
	~CLinkNetS();
	
	BOOL InitSending(); // �������ʼ��
	BOOL FillReqInfo(PAPPLY_GRAM pInfo);
	BYTE CheckFeedback(PAPPLY_GRAM pFeedback,LPCTSTR ChkName,char *pChkPass);  //// �ظ���Ϣ
	void SetAutoUpdateTick(UINT uTick) {
		m_uSuspendTick = uTick;
	}
	ULONG GetNetSAddr(BOOL bRefresh = FALSE) ;
	BOOL SelfChecking(PAPPLY_GRAM pLinkInfo); //// ����NetS�Ƿ�����TRUE:������FALSE����������
protected:
	SOCKADDR_IN m_NetSAddr;
	APPLY_GRAM m_ReqInfo;
	CString m_strNetSName;
	int  m_nSendCount; //// ��ϵͳ�����ĵ���ʱ ����ʱ �Զ�����������Manager������
	int	 m_nSendInterval;  //// ����ϵ������ļ��
	UINT  m_uSuspendTick; //// �����Զ����µļ�����
	UINT  m_uCurSuspendTick; 

	BOOL m_bWaitForAck;
	BOOL m_bTimeout;
	BOOL m_bStopWorking;
	
	LONG m_nThreadCount;
	void IncreaseThreadCount() 
	{	InterlockedIncrement(&m_nThreadCount);
	}
	void DecreaseThreadCount() 
	{	InterlockedDecrement(&m_nThreadCount);
	}
	HANDLE m_hSendReq,m_hStopWorking;
	CRITICAL_SECTION m_csReqInfo; //// ��m_ReqInfo�ı���
	UINT SendReq();
	static UINT SendReqProc(LPVOID p);
private:
};
#endif // _LINKNETS_H