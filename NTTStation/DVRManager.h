#ifndef _DVRManager_H
#define _DVRManager_H

#include "NTTProtocol.h"
#include "DVRGlobal.h"
#include "LinkNetS.h"
#include "ClientTnel.h"

//// ���¸�����2009-11-21:
//// ���Ժ�������û�У�m_bGotGlobalName Ӧ��ΪFALSE,ResetLastGlobalName ���Իָ�
#define CHECK_OVER_INTERVAL	1000 //// ��˼��ʱ����һ��
#define DVR_RCV_BUF_LENGTH	1024 //// ����BUF����
class CDVRManager
{
public:
	CDVRManager(DWORD dwType, DWORD dwID[4]);// ����Ĳ������������豸ʶ��ŵ�
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
	//// �û����뷽��
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
	//// ��ù������ķ����Լ�����������Ϣ
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

	APPLY_GRAM m_DVRInfo; //// DVR��Ϣ

	//// �ӽ��ܴ���
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
	BOOL LegalTnel(PTNELMSG_GRAM pTnel,char *pCltName); //// ����Ϣ�Ƿ�Ϸ�
	BOOL LegalCnct(POPENTNEL pOpen,char *pCltName); //// �������Ϣ�Ƿ�Ϸ�
	BOOL LegalCmd(PWORK_GRAM pCmd,char *pCltName,char *pDvrName);
	BOOL LegalHshk(PHSHK_GRAM pHshk,char *pCltName,char *pDvrName);
	BOOL LegalVResd(PVRESD_GRAM pVResd,char *pCltName,char *pDvrName);
	CClientTnel* GetClt(char *Name);

	static UINT RcvGramProc(LPVOID p);
	static UINT DistribGramProc(LPVOID p);
	static UINT CheckOverProc(LPVOID p);

	//// ��������ʹ�õ���Դ��
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