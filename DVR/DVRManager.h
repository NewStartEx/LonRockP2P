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
	CDVRManager(DWORD dwType, DWORD dwID0, DWORD dwID1);// ����Ĳ������������豸ʶ��ŵ�
	~CDVRManager();
	BOOL StartWorking(HWND hWnd,LPCTSTR Path,LPCTSTR NetSName);
	void StopWorking();

	BOOL SetGlobalName(LPCTSTR Name,LPCTSTR Password); 
	BOOL ResetLastGlobalName(CString &strGlobalName);
	//// �û����뷽��
	BYTE RegisterUnit();  //// 
	BYTE UnregisterUnit();
	BYTE ManuUpdate();

	CString GetGlobalName() {
		return m_strGlobalName;
	}

	//// ��ù������ķ����Լ�����������Ϣ
protected:
	BOOL m_bWorking;
	BOOL m_bGotGlobalName,m_bNameSet;
	CString m_strGlobalName,m_strCheckingName;
	char m_chPassword[NAME_BUF_LENGTH],m_chCheckingPassword[NAME_BUF_LENGTH];
	CNetSFBArray m_NetSFBArray;
	CTnelMsgArray m_TnelMsgArray;
	CCnctDescArray m_CnctDescArray;
	CCmdDescArray  m_CmdDescArray;
	CRITICAL_SECTION m_csGram,m_csCltArray,m_csRes;
	HANDLE m_hRcvGram,m_hNetSFBGram,m_hTnelGram,m_hCnctGram,m_hCmdGram,m_hStopWorking;
	HWND m_hMsgWnd;

	CString m_strInfoFileName;
	CString m_strNetSName; //// NetS' host name
	WORD  m_wNetSPort;
	CLinkNetS *m_pLinkNetS;
	CCltTnelArray m_CltTnelArray;
	
	SOCKET m_sRcvGram; 
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
	UINT DealWithCmdGram();
	UINT NormalNetSChecking();
	UINT NormalCltChecking();
	BOOL LegalTnel(PTNELMSG_GRAM pTnel,char *pCltName); //// ����Ϣ�Ƿ�Ϸ�
	BOOL LegalCnct(POPENTNEL pOpen,char *pCltName); //// �������Ϣ�Ƿ�Ϸ�
	BOOL LegalCmd(PWORK_GRAM pCmd,char *pCltName,char *pDvrName);
	CClientTnel* GetClt(char *Name);

	static UINT RcvGramProc(LPVOID p);
	static UINT DistribGramProc(LPVOID p);
	static UINT CheckOverProc(LPVOID p);

private:
};
#endif // _DVRManager_H