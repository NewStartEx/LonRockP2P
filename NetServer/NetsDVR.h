#ifndef _NETSDVR_H
#define _NETSDVR_H

#include "NTTProtocol.h"
#include "NetSGlobal.h"

#define DVR_ONLINE_TIMEOUT	60 //// 初始为0，管理每次扫描加1，每次更新恢复。如果为该值，表示离线。现在是60x10秒

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
	//// 下面函数返回就是ACK的值。
	//// 如果是返回删除成功，则由调用线程发送数据报，同时删除该DVR元素
	//// 如果此时正在发送数据，返回Busy,什么都不处理
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
	DWORD m_dwIPAddr,m_dwErrIPAddr; /// Err 是对方发来是信息错误，有可能不是原来的DVR发送的
	WORD  m_wPort,m_wErrPort;
	BOOL  m_bErrFeedback;

	SOCKET m_sFeedback;
	APPLY_GRAM m_curGram;
	SYSTEMTIME m_stLastLink; 

	BYTE m_byStatus;
	int  m_nOnlineTimeout;
	BOOL m_bStopWorking;
	BOOL m_bPositionChanged,m_bStatusChanged; //// 这两个参数在管理自查的时候复位
	BOOL CheckSleepDown();

	LONG m_nThreadCount;
	CRITICAL_SECTION m_csSending;
	BOOL SendFeedBack();
	static UINT SendFeedBackProc(LPVOID p);
private:
};

typedef CArray<CNetSDVR*, CNetSDVR*> CNetSDVRArray;
#endif // _NETSDVR_H