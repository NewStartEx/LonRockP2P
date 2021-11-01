#ifndef _LINKNETS_H
#define _LINKNETS_H

#include "DvrGlobal.h"

#define MAX_REQSEND_COUNT	3 //// 10
#define REQ_INTERVAL	    5000    //// 在等待回应的时候每5秒发一次，直到请求进入
#define AUTOUPDATE_INTERVAL 60     //// 这么多秒发送一次自动更新,如果太长了可能NAT关闭这个通道口

//// 以下更改在2009-11-21:
//// 为避免NetS IP 更改后找不到，更改为构造函数使用NetS名称，并且根据情况使用Name2IP

//// SelfChecking return value: 
class CLinkNetS
{
public:
	CLinkNetS(LPCTSTR NetSName,WORD Port);
	~CLinkNetS();
	
	BOOL InitSending(); // 在这里初始化
	BOOL FillReqInfo(PAPPLY_GRAM pInfo);
	BYTE CheckFeedback(PAPPLY_GRAM pFeedback,LPCTSTR ChkName,char *pChkPass);  //// 回复信息
	void SetAutoUpdateTick(UINT uTick) {
		m_uSuspendTick = uTick;
	}
	ULONG GetNetSAddr(BOOL bRefresh = FALSE) ;
	BOOL SelfChecking(PAPPLY_GRAM pLinkInfo); //// 返回NetS是否正常TRUE:正常，FALSE，不正常。
protected:
	SOCKADDR_IN m_NetSAddr;
	APPLY_GRAM m_ReqInfo;
	CString m_strNetSName;
	int  m_nSendCount; //// 对系统操作的倒计时 看超时 自动更新命令由Manager来发送
	int	 m_nSendInterval;  //// 发送系列命令的间隔
	UINT  m_uSuspendTick; //// 发送自动更新的检测次数
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
	CRITICAL_SECTION m_csReqInfo; //// 对m_ReqInfo的保护
	UINT SendReq();
	static UINT SendReqProc(LPVOID p);
private:
};
#endif // _LINKNETS_H