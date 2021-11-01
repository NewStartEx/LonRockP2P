#ifndef _NETSDVR_H
#define _NETSDVR_H

#include "NTTProtocol.h"
#include "NetSGlobal.h"

#define DVR_ONLINE_TIMEOUT	60 //// ��ʼΪ0������ÿ��ɨ���1��ÿ�θ��»ָ������Ϊ��ֵ����ʾ���ߡ�������60x10��

class CNetSDVR
{
public:
	CNetSDVR(SOCKET sFeedback,APPLY_GRAM DvrInfo,DWORD dwIPAddr,WORD wPort,SYSTEMTIME stLastLogin);
	~CNetSDVR();
	static UINT m_CertainKindCount;

	BOOL FetchSavingInfo(PDVR_ITEM pItem);
	BOOL FetchCltTunnelInfo(PTNELMSG_GRAM pGram);
	void FetchPosition(DWORD* pdwIPAddr,WORD* pwPort) {
		*pdwIPAddr = m_dwIPAddr;
		*pwPort = m_wPort;
	}
	BYTE GetStatus() {
		return m_byStatus;
	}
	BOOL IsMyself(LPCTSTR Name);
	BOOL FetchDVRInfo(PDVRMSG pDvrMsg);
	//// ���溯�����ؾ���ACK��ֵ��
	//// ����Ƿ���ɾ���ɹ������ɵ����̷߳������ݱ���ͬʱɾ����DVRԪ��
	//// �����ʱ���ڷ������ݣ�����Busy,ʲô��������
	UINT CheckDVRGram(PAPPLY_GRAM pGram,char *pPassword,DWORD dwNewIPAddr,WORD wNewPort); 
	BOOL SelfChanged(BOOL *pStatusChanged);
	void IncreaseThreadCount() 
	{	InterlockedIncrement(&m_nThreadCount);
	}
	void DecreaseThreadCount() 
	{	InterlockedDecrement(&m_nThreadCount);
	}
protected:
	DVR_INFO m_DvrInfo;
	DWORD m_dwIPAddr,m_dwErrIPAddr; /// Err �ǶԷ���������Ϣ�����п��ܲ���ԭ����DVR���͵�
	WORD  m_wPort,m_wErrPort;
	BOOL  m_bErrFeedback;

	SOCKET m_sFeedback;
	APPLY_GRAM m_curGram;
	SYSTEMTIME m_stLastLink; 

	BYTE m_byStatus;
	int  m_nOnlineTimeout;
	BOOL m_bStopWorking;
	BOOL m_bPositionChanged,m_bStatusChanged; //// �����������ڹ����Բ��ʱ��λ
	BOOL CheckSleepDown();

	LONG m_nThreadCount;
	CRITICAL_SECTION m_csSending;
	BOOL SendFeedBack();
	static UINT SendFeedBackProc(LPVOID p);
private:
};

typedef CArray<CNetSDVR*, CNetSDVR*> CNetSDVRArray;
#endif // _NETSDVR_H