#ifndef _DVRManager_H
#define _DVRManager_H

#include "NTTProtocol.h"
#include "DVRGlobal.h"
#include "LinkNetS.h"
#include "ClientTnel.h"

//// 以下更改在2009-11-21:
//// 测试后发现名称没有，m_bGotGlobalName 应该为FALSE,ResetLastGlobalName 可以恢复
#define CHECK_OVER_INTERVAL	1000 //// 如此间隔时间检查一次
#define DVR_RCV_BUF_LENGTH	1024 //// 接收BUF长度
class CDVRManager
{
public:
	CDVRManager(DWORD dwType, DWORD dwID[4]);// 这里的参数可以设置设备识别号等
	~CDVRManager();
	BOOL StartWorking(LPCTSTR Path,LPCTSTR NetSName);
	void StopWorking();

	void EnableMsgWnd(HWND hWnd,BOOL bEnable) {
		if(bEnable)
			m_hMsgWnd = hWnd;
		else
			m_hMsgWnd = NULL;
	}
	BOOL SetGlobalName(LPCTSTR Name,LPCTSTR Password); 
	BOOL ResetLastGlobalName(CString &strGlobalName);
	//// 用户输入方法
	BYTE RegisterUnit();  //// 
	BYTE UnregisterUnit();
	BYTE ManuUpdate();
	BYTE WakeupUnit();
	void EnableVideoTransfer(HANDLE hClt, BOOL bEnable,BOOL bInCallback);
	void EnableAudioTransfer(HANDLE hClt, BOOL bEnable,BOOL bInCallback);
	void SetRemoveMark(HANDLE hClt,BOOL bInCallback);

	CString GetGlobalName() {
		return m_strGlobalName;
	}
	WORD GetDvrPort() {
		return m_wDVRPort;
	}
	UINT GetRegisterStatus() {
		return m_uRegisterStatus;
	}

	BOOL PushSData(int nLength, SOCKADDR_IN sAddr, PVOID pData);
	//// 获得工作报文反馈以及发送命令信息
protected:
	BOOL m_bWorking;
	BOOL m_bSleeping;
	UINT m_uRegisterStatus;
	BOOL m_bGotGlobalName,m_bNameSet,m_bApplying;
	CString m_strGlobalName,m_strCheckingName;
	char m_chPassword[NAME_BUF_LENGTH],m_chCheckingPassword[NAME_BUF_LENGTH];
	CNetSFBArray m_NetSFBArray;
	CTnelMsgArray m_TnelMsgArray;
	CCnctDescArray m_CnctDescArray;
	CHshkDescArray m_HshkDescArray;
	CCmdDescArray  m_CmdDescArray;
	CVResdDescArray m_VResdDescArray;
	CRITICAL_SECTION m_csGram,m_csCltArray,m_csRes;
	HANDLE m_hRcvGram,m_hNetSFBGram,m_hTnelGram,m_hCnctGram,m_hHshkGram,m_hCmdGram,m_hVResdGram,m_hStopWorking;
	HWND m_hMsgWnd;

	CString m_strInfoFileName;
	CString m_strNetSName; //// NetS' host name
	WORD  m_wNetSPort;
	CLinkNetS *m_pLinkNetS;
	CCltTnelArray m_CltTnelArray;
	
	SOCKET m_sGram; 
	WORD m_wDVRPort;
	LONG m_nThreadCount;
	SOCKADDR_IN m_LocalAddr;

	void IncreaseThreadCount() 
	{	InterlockedIncrement(&m_nThreadCount);
	}
	void DecreaseThreadCount() 
	{	InterlockedDecrement(&m_nThreadCount);
	}
	BOOL ReadDVRInfo();
	BOOL WriteDVRInfo();
	BYTE UnitExit();

	APPLY_GRAM m_DVRInfo; //// DVR信息

	//// 加解密处理
	BOOL SecretEncode2File(/*out*/char *pName,/*out*/char *pPassword);
	BOOL SecretEncode2Apply(/*out*/char *pName,/*out*/char *pPassword);
	BOOL SecretDecode3File(/*in*/char *pName,/*in*/char *pPassword);

	UINT KeepRcving();
	UINT DistribGram();
	UINT CheckOver();
	UINT DealWithNetSFBGram();
	UINT DealWithTnelGram();
	UINT DealWithCnctGram();
	UINT DealWithHshkGram();
	UINT DealWithCmdGram();
	UINT DealWithVResdGram();
	UINT NormalNetSChecking();
	UINT NormalCltChecking();
	void DvrExitNotify();
	BOOL LegalTnel(PTNELMSG_GRAM pTnel,char *pCltName); //// 打洞信息是否合法
	BOOL LegalCnct(POPENTNEL pOpen,char *pCltName); //// 开隧道信息是否合法
	BOOL LegalCmd(PWORK_GRAM pCmd,char *pCltName,char *pDvrName);
	BOOL LegalHshk(PHSHK_GRAM pHshk,char *pCltName,char *pDvrName);
	BOOL LegalVResd(PVRESD_GRAM pVResd,char *pCltName,char *pDvrName);
	CClientTnel* GetClt(char *Name);

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
extern CDVRManager *g_pManager;

#endif // _DVRManager_H