#ifndef _CLIENTTNEL_H
#define _CLIENTTNEL_H

#define MAX_TNEL_COUNT	10

#define GRAM_TYPE_TNEL	0
#define GRAM_TYPE_MSG	1
//// ������ط���Ҫ����һ���¼��ˣ���Ϊ������ǱȽ϶������

#define CREATE_TNEL_INTERVAL	1000 //// ÿ��ô��ʱ�䷢��һ���������
#define MAX_TNEL_SEND_COUNT		30   //// ��Ű����
#define MAX_SILENCE_INTERVAL	30  //// ��Ű����û�з�����Ϣ����Ϊ�Է�����

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
	BOOL InitResource(LPCTSTR DVRName,SOCKET sClient);   //// ���￪ʼ���߳�
	BOOL IsMyself(LPCTSTR CltName); //// ���������ʶͬһ�����Ե����ݣ�������һ�������������Σ�����ʹ��IP�����ƣ�
	BYTE DealwithCnctGram(PCNCTDESC pTnelInfo); 
	BOOL CheckCmdGram(PCMDDESC pWorkInfo);

	BOOL SetMsgGram(PCMDINFO pWorkInfo);   //// �����������
	//// �ҵ��Է����򶴳ɹ�������ϵ����
	BOOL Found() {
		return m_bTnelOpened;
	}
	//// �������NetS�������Ĵ����������´�
	BOOL RetryTnel(LPCTSTR DVRName); /// ������Ҫ�Ժ��������ڴ˿ͻ�������û�п�ͨ�����ʱ�򣬶Է��ַ��������ʹ��
	//// ������Ҫ��һ���Լ����������ִ�ʧ�ܲ��ҳ�ʱ����ɾ�������
	BOOL SelfChecking(); ///// ������Ҫ�Ժ���,�����ж� ����Ƿ�ͨ���Է��೤ʱ��û�з�����������
protected:
	CString m_strName;  
	CString m_strDvrName; //// �����û������Ʊ�־��������DVR��Ψһ��ʶ��Ҳ������Client����������
	SOCKET m_sClient;
	SOCKADDR_IN m_ClientAddr;
	BYTE  m_byMsgAct;  
	BOOL  m_bTnelOpened;
	int  m_nSendTnelCount; //// ��ϵͳ�����ļ��� ����ʱ �Զ�����������Manager������
	int	 m_nSendGramInterval;  //// ����ϵ������ļ��
	int  m_nLastCmdJump;  //// �ж�û���յ�������ʱ��
	int	  m_nGramType;
	OPENTNEL m_OpenTnel;
	WORK_GRAM m_CmdGram;

	CRITICAL_SECTION m_csGram,m_csCount;
	HANDLE	m_hNormalGram,m_hResdGram,m_hStopWorking;  //// ������ʱ����Resend Gram

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