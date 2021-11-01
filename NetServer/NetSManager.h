// 事务处理类。使用两个线程，
// 一个负责接收数据并分类：DVR发送的，放到DVR报文缓冲，设置接收到DVR报文事件；
//                         Client发送的，放到Client报文缓冲，设置接收到Client报文事件
// 放置报文的时候进入互斥量，在列表里增加；接收报文的时候要判断长度和类型。
// 另一个线程负责处理报文：在循环里，每取一个报文，进出一次互斥量，这样好让接收线程有时间放置报文；
//                         DVR的报文，查找列表和内容，判断是否增加或者删除DVR列表，和回复报文
//                         没有找到时，如果是注册或者删除信息，要由本线程回复报文；找到了并且DVR类返回删除成功，也要本线程回复报文
//                         同时要存储文件和向消息窗口报告(添加DVR元素或者删除成功时，即DVR列表发生变化时)，处理时间超过5秒或者处理完成，进行存储
//                         Client的报文，如果查找DVR列表成功，配置信息，向两边发送；如果查找DVR列表不成功，向Client发送失败消息
//                         每超时一次，检查DVR列表，发现有更新IP，端口的，进行保存

#ifndef _NetSManager_H
#define _NetSManager_H

#include "NTTProtocol.h"
#include "NetSGlobal.h"
#include "NetSDVR.h"

#define MAX_WORK_TIME	10000 //// 连续处理DVR时间超过这么多秒，要存盘一次
#define NETS_RCV_BUF_LENGTH	1024
class CNetSManager
{
public:
	CNetSManager();
	~CNetSManager();
	BOOL StartWorking(HWND hWnd,LPCTSTR Path);  //// 要处理一些显示消息
	void StopWorking();  
protected:
	HWND m_hMsgWnd;
	BOOL m_bWorking;
	CString m_strDVRFileName,m_strBackupFileName;
	LONG m_nThreadCount;
	void IncreaseThreadCount() 
	{	InterlockedIncrement(&m_nThreadCount);
	}
	void DecreaseThreadCount() 
	{	InterlockedDecrement(&m_nThreadCount);
	}

	SOCKET m_sRcvGram,m_sSendGram;
	WORD m_wServerPort;  //// 接收端口只能有一个socket,否则会导致接收不到

	//// 给主界面显示用
	DVRMSG m_DVRMsg;
	CLTMSG m_TunnelMsg;
	WPARAM m_LastCltGetStatus;
	
	CRITICAL_SECTION m_csGram;
	CDVRDescArray m_DVRDescArray;
	CCltDescArray m_CltDescArray;
	CRITICAL_SECTION m_csDvr;
	CNetSDVRArray m_NetSDVRArray;

	HANDLE m_hCltGram,m_hDVRGram,m_hRcvGram,m_hStopWorking;

	void ArrangDVR(CNetSDVR* pDVR);
	void OrderDVRArray(CNetSDVR *pDVR,int startIndex,int stopIndex);
	int  SearchDVR(LPCTSTR DVRName);
	int  OrderFindDVR(LPCTSTR DvrName,int startIndex,int stopIndex);
	BOOL ReadDVRListFile();
	BOOL WriteDVRListFile();
	BOOL CheckBackupFile();
	BOOL WriteBackupFile();
	BOOL CheckDVRListItem(/*IN*/PDVR_ITEM pItem, /*OUT*/PAPPLY_GRAM pDvrInfo, 
		/*OUT*/DWORD *pIPAddr, /*OUT*/WORD *pPort, /*OUT*/LPSYSTEMTIME pstLastLogin);
	UINT KeepRcving();
	UINT CheckGram();
	UINT CheckDVR();
	UINT DealWithDvrGram();
	UINT DealWithCltGram();
	UINT NormalDVRChecking();
	BOOL LegalDvrApply(PAPPLY_GRAM pGram,char *pDvrName,char *pPassword);
	BOOL LegalCltReq(PCLTREQ_GRAM pGram,char *pDvrName,char *pCltName);

	static UINT RcvGramProc(LPVOID p);
	static UINT HandleGramProc(LPVOID p);
	static UINT CheckDVRProc(LPVOID p);
private:
};
#endif // _NetSManager_H