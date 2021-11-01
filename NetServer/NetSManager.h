// �������ࡣʹ�������̣߳�
// һ������������ݲ����ࣺDVR���͵ģ��ŵ�DVR���Ļ��壬���ý��յ�DVR�����¼���
//                         Client���͵ģ��ŵ�Client���Ļ��壬���ý��յ�Client�����¼�
// ���ñ��ĵ�ʱ����뻥���������б������ӣ����ձ��ĵ�ʱ��Ҫ�жϳ��Ⱥ����͡�
// ��һ���̸߳������ģ���ѭ���ÿȡһ�����ģ�����һ�λ��������������ý����߳���ʱ����ñ��ģ�
//                         DVR�ı��ģ������б�����ݣ��ж��Ƿ����ӻ���ɾ��DVR�б��ͻظ�����
//                         û���ҵ�ʱ�������ע�����ɾ����Ϣ��Ҫ�ɱ��̻߳ظ����ģ��ҵ��˲���DVR�෵��ɾ���ɹ���ҲҪ���̻߳ظ�����
//                         ͬʱҪ�洢�ļ�������Ϣ���ڱ���(���DVRԪ�ػ���ɾ���ɹ�ʱ����DVR�б����仯ʱ)������ʱ�䳬��5����ߴ�����ɣ����д洢
//                         Client�ı��ģ��������DVR�б�ɹ���������Ϣ�������߷��ͣ��������DVR�б��ɹ�����Client����ʧ����Ϣ
//                         ÿ��ʱһ�Σ����DVR�б������и���IP���˿ڵģ����б���

#ifndef _NetSManager_H
#define _NetSManager_H

#include "NTTProtocol.h"
#include "NetSGlobal.h"
#include "NetSDVR.h"

#define MAX_WORK_TIME	10000 //// ��������DVRʱ�䳬����ô���룬Ҫ����һ��
#define NETS_RCV_BUF_LENGTH	1024
class CNetSManager
{
public:
	CNetSManager();
	~CNetSManager();
	BOOL StartWorking(HWND hWnd,LPCTSTR Path);  //// Ҫ����һЩ��ʾ��Ϣ
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
	WORD m_wServerPort;  //// ���ն˿�ֻ����һ��socket,����ᵼ�½��ղ���

	//// ����������ʾ��
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