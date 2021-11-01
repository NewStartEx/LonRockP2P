#ifndef _CLTMANAGER_H
#define _CLTMANAGER_H

#include "NTTProtocol.h"
#include "CltGlobal.h"
#include "DvrTnel.h"

//// 创建DVR对象失败的原因
#define OBJECT_SUCCESS	0
#define OBJECT_NETS_ERR	1
#define OBJECT_DVR_ERR	2

#define CHECK_OVER_INTERVAL	1000 //// 如此间隔时间检查一次，并发送请求数据等。如果心跳时间到，心跳一次
#define RCV_BUF_LENGTH		1024 //// 接收BUF长度 必须大于最大数据报长度
#define GRAM_BUFFER_COUNT	1000  //// 接收数据报的缓冲个数

typedef struct _GRAMDESC {
	int nLength;
	SOCKADDR_IN IPAddr;
	char* pBuf;
} GRAMDESC,*PGRAMDESC;

class CCltManager
{
public:
	CCltManager();
	~CCltManager();
	BOOL StartWorking(LPCTSTR NetSName);
	void StopWorking();

	HANDLE StartLink(LPCTSTR ShowName,int nLinkType,PCMDINFO pFirstCmd,WORD wDvrPort,BYTE &Error);
	void StopLink(LPCTSTR ShowName,BOOL bInCallback);
	void StopLink(HANDLE hDvr,BOOL bInCallback);
	BOOL SendCmd(LPCTSTR ShowName,PCMDINFO pCmd,BOOL bInCallback);
	BOOL SendCmd(HANDLE hDvr,PCMDINFO pCmd,BOOL bInCallback);
	LPCTSTR GetDvrName(HANDLE hDvr);

	BOOL CreateVideoResource(LPCTSTR ShowName,int nChanCount,BOOL bInCallback);
	int  GetVideoChannelCount(LPCTSTR ShowName,BOOL bInCallback);
	BOOL StartVideoTran(LPCTSTR ShowName,int chan,BOOL bInCallback);
	BOOL StartAllVideoTran(LPCTSTR ShowName,BOOL bInCallback);
	BOOL StopVideoTran(LPCTSTR ShowName,int chan,BOOL bInCallback);
	BOOL StopAllVideoTran(LPCTSTR ShowName,BOOL bInCallback);
	BOOL StartRecord(LPCTSTR ShowName,int chan,LPCTSTR FileName,BOOL bInCallback);
	BOOL StopRecord(LPCTSTR ShowName,int chan,BOOL bInCallback);
	BOOL Snap(LPCTSTR ShowName,int chan,LPCTSTR FileName,BOOL bInCallback);
	DWORD GetRecFilePos(LPCTSTR ShowName,int chan,BOOL bInCallback);

	BOOL SetVideoPos(LPCTSTR ShowName,int chan,int left,int top,int width,int height,BOOL bInCallback);
	BOOL ShowVideo(LPCTSTR ShowName,int chan,BOOL bShow,BOOL bInCallback);
	BOOL RefreshVideo(LPCTSTR ShowName,int chan,BOOL bInCallback);
	BOOL PushSData(int nLength, SOCKADDR_IN sAddr, PVOID pData);
protected:
	BOOL m_bWorking;
	CString m_strNetSName;
	SOCKADDR_IN m_LocalAddr;
	ULONG m_uNetSAddr;
	WORD  m_wNetSPort,m_wLocalPort;
	SOCKET m_sGram;
	CDvrTnelArray m_DvrTnelArray;
	LONG m_nThreadCount;
	/*
	CTnelMsgArray m_TnelMsgArray;
	CCnctDescArray m_CnctDescArray;
	CMsgGramArray m_MsgGramArray;
	*/
	//// PBYTE m_pDataBuffer[DATA_BUFFER_COUNT];
	GRAMDESC m_GramDesc[GRAM_BUFFER_COUNT];
	int m_nReadGramIndex,m_nWriteGramIndex;
	CRITICAL_SECTION /*m_csGram,*/m_csDvrArray,m_csRes;
	HANDLE m_hRcvGram,m_hDealwithGram,/*m_hTnelGram,m_hCnctGram,m_hMsgGram,m_hDataGram,*/m_hStopWorking;

	void IncreaseThreadCount() 
	{	InterlockedIncrement(&m_nThreadCount);
	}
	void DecreaseThreadCount() 
	{	InterlockedDecrement(&m_nThreadCount);
	}

	UINT KeepRcving();
	UINT DistribGram();
	UINT CheckOver();
	UINT DealwithTnelGram(PTNELMSG_GRAM pTnel);
	UINT DealwithCnctGram(POPENTNEL pOpen,SOCKADDR_IN IPAddr);
	UINT DealwithHshkACK(PHSHK_GRAM pHshk);
	UINT DealwithMsgGram(PWORK_GRAM pMsg);
	UINT DealwithDataGram(PVOID pDataBuf); 
	BOOL LegalTnel(PTNELMSG_GRAM pTnel,char *pDvrName); //// 打洞信息是否合法
	BOOL LegalCnct(POPENTNEL pOpen,char *pDvrName); //// 开隧道信息是否合法
	BOOL LegalHshkACK(PHSHK_GRAM pHshk,char *pDvrName,char *pCltName);
	BOOL LegalMsg(PWORK_GRAM pMsg,char *pDvrName,char *pCltName);
	BOOL LegalData(PDATA_GRAM_HEADER pDataHeader,char *pDvrName,char *pCltName);
	CDvrTnel *GetDvr(char* Name);
	
	static UINT RcvGramProc(LPVOID p);
	static UINT DistribGramProc(LPVOID p);
	static UINT CheckOverProc(LPVOID p);

	//// 发送数据使用的资源：
	HANDLE m_hSendData;
	SENDDATADESC m_SendDataDesc[SENDDATA_BUFFER_COUNT];
	int m_nReadSDataIndex,m_nWriteSDataIndex;
	BOOL m_bSendBufFull;
	CRITICAL_SECTION m_csSData;
	UINT SendGram();
	static UINT SendGramProc(LPVOID p);
private:
};
extern CCltManager* g_pManager;

#endif // _CLTMANAGER_H