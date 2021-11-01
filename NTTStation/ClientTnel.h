//// 2010-5-2: ����������ȷ��״̬���Ȳ������粢�ҷ���ACK���������Ͳ������ݵ��߳�
#ifndef _CLIENTTNEL_H
#define _CLIENTTNEL_H

#define MAX_TNEL_COUNT	10

#define GRAM_TYPE_TNEL	0
#define GRAM_TYPE_HSHK	1
#define GRAM_TYPE_MSG	2

//// ������ط���Ҫ����һ���¼��ˣ���Ϊ������ǱȽ϶������

#define CREATE_TNEL_INTERVAL	1000 //// ÿ��ô��ʱ�䷢��һ���������
#define MAX_TNEL_SEND_COUNT		30   //// ��Ű����
#define MAX_SILENCE_INTERVAL	30  //// ��Ű����û�з�����Ϣ����Ϊ�Է�����
#include "SendVideo.h"
// �����󴴽������̡߳�
// �߳����ж���Է��Ƿ����ӡ���û�����ӣ�����������ٱ���,ÿ��һ�Σ�
// ���ӳɹ����߷��ʹ�����������ʱʱ����Ϊ����
class CClientTnel
{
public:
	//// ��Act������Ҫ������������Ļ���ֱ�����ӵ�
	CClientTnel(char *Name,DWORD dwIPAddr,WORD wPort,BYTE byAct);  
	~CClientTnel();
	static int m_TnelCount;  //// ��������Գ���һ������
	BOOL InitResource(LPCTSTR DVRName);   //// ���￪ʼ���߳�
	BOOL IsMyself(LPCTSTR CltName); //// ���������ʶͬһ�����Ե����ݣ�������һ�������������Σ�����ʹ��IP�����ƣ�
	BYTE DealwithCnctGram(PCNCTDESC pTnelInfo); 
	BOOL DealwithHshkGram(PHSHKDESC pHshkInfo);
	BOOL CheckCmdGram(PCMDDESC pWorkInfo);

	BOOL SetMsgGram(PCMDINFO pWorkInfo);   //// �����������
	BOOL CheckVideoMsg(PCMDINFO pVideoInfo);
	BOOL CheckAudioMsg(PCMDINFO pAudioInfo);
	void SetExit();
	//// �ҵ��Է����򶴳ɹ�������ϵ����
	BOOL Found() {
		return m_bTnelOpened;
	}
	void SetExitMark() {
		m_bForceExit = TRUE;
	}
	void EnableVideo(BOOL bEnable);
	void EnableAudio(BOOL bEnable);
	BOOL CheckVResdGram(PVRESDPACKET pVResdPackt,DWORD dwIPAddr,LPCTSTR DvrName);
	//// �������NetS�������Ĵ����������´�
	BOOL RetryTnel(LPCTSTR DVRName); /// ������Ҫ�Ժ��������ڴ˿ͻ�������û�п�ͨ�����ʱ�򣬶Է��ַ��������ʹ��
	//// ������Ҫ��һ���Լ����������ִ�ʧ�ܲ��ҳ�ʱ����ɾ�������
	BOOL SelfChecking(); ///// ������Ҫ�Ժ���,�����ж� ����Ƿ�ͨ���Է��೤ʱ��û�з�����������
protected:
	CString m_strName;  
	CString m_strDvrName; //// �����û������Ʊ�־��������DVR��Ψһ��ʶ��Ҳ������Client����������
	SOCKADDR_IN m_ClientAddr;
	BYTE  m_byMsgAct;  
	BOOL  m_bTnelOpened;
	BOOL  m_bForceExit,m_bCanVideo,m_bCanAudio;
	int  m_nSendTnelCount; //// ��ϵͳ�����ļ��� ����ʱ �Զ�����������Manager������
	int	 m_nSendGramInterval;  //// ����ϵ������ļ��
	int  m_nLastCmdJump;  //// �ж�û���յ�������ʱ��
	int	  m_nGramType;
	OPENTNEL m_OpenTnel;
	HSHK_GRAM m_HshkACKGram;
	WORK_GRAM m_CmdGram;

	CSendVideoArray m_SendVideoArray;

	CRITICAL_SECTION m_csGram,m_csCount;
	HANDLE	m_hNormalGram,m_hNaviData,m_hStopWorking;  //// m_hNaviData �Ƿ��ʹ���������ݵ��¼�
	DWORD m_dwNwidPackets,m_dwNetRelayInterval; //// �������ʹ�������ʱ��
	HshkStatus m_HshkStatus;  //// ���ֵĵ�ǰ״̬
	BOOL SetHshkACK(PHSHK_GRAM pHshkGram);  //// ����������Ϣ

	void MakeTnelGram(LPCTSTR FirstName,LPCTSTR SecondName,BYTE byAct);
	LONG m_nThreadCount;
	void IncreaseThreadCount() 
	{	InterlockedIncrement(&m_nThreadCount);
	}
	void DecreaseThreadCount() 
	{	InterlockedDecrement(&m_nThreadCount);
	}
	BOOL SendGram();
	static UINT SendGramProc(LPVOID p);
private:
};

typedef CArray<CClientTnel*,CClientTnel*> CCltTnelArray;
#endif // _CLIENTTNEL_H