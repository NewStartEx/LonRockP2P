#include "stdafx.h"
#include "NTTProtocol.h"
#include "Global.h"
#include "DVRGlobal.h"
#include "ClientTnel.h"

extern CRITICAL_SECTION g_csSocket;

int CClientTnel::m_TnelCount = 0;
CClientTnel::CClientTnel(char *Name,DWORD dwIPAddr,WORD wPort,BYTE byAct) 
{
	m_strName = Name;
	ASSERT(m_strName.GetLength() < NAME_BUF_LENGTH);
	memset(&m_OpenTnel,0,sizeof(OPENTNEL));
	memset(&m_CmdGram,0,sizeof(WORK_GRAM));
	memset(&m_ClientAddr,0,sizeof(SOCKADDR_IN));
	m_ClientAddr.sin_family = AF_INET;
	m_ClientAddr.sin_addr.s_addr = dwIPAddr;
	m_ClientAddr.sin_port = wPort;  //// �����ǶԷ�ֱ�ӷ�����Port,���Բ�����htons ת��
	if(CONNECT_CMD == byAct) {
		m_bTnelOpened = TRUE;
		m_nGramType = GRAM_TYPE_MSG;
		m_nSendGramInterval = INFINITE;
		m_byMsgAct = CONNECT_MSG;  //// �����ʾ˫���Ѿ���ϵ�ϣ������������á��������������б�������˲�����
	}
	else {
		m_bTnelOpened = FALSE;
		m_nGramType = GRAM_TYPE_TNEL;
		m_nSendGramInterval = CREATE_TNEL_INTERVAL;
	}
	m_nSendTnelCount = 0;
	m_nLastCmdJump = 0;
	m_nThreadCount = 0;
	m_sClient = INVALID_SOCKET;
	m_hNormalGram = CreateEvent(NULL,FALSE,FALSE,NULL);
	m_hStopWorking = CreateEvent(NULL,FALSE,FALSE,NULL);
	InitializeCriticalSection(&m_csGram);
	InitializeCriticalSection(&m_csCount);
	m_TnelCount++;
}

CClientTnel::~CClientTnel()
{
	//// ????
	SetEvent(m_hStopWorking);
	while (m_nThreadCount > 0) {
		Sleep(50);
	}
	DeleteCriticalSection(&m_csGram);
	DeleteCriticalSection(&m_csCount);
	CloseHandle(m_hNormalGram);
	CloseHandle(m_hStopWorking);
	m_TnelCount--;
}

// �����DVRName��Cmd������ӶԷ���������ȡ��Tnel��������DVRֱ�Ӹ�
BOOL CClientTnel::InitResource(LPCTSTR DVRName,SOCKET sClient)
{
	ASSERT(INVALID_SOCKET == m_sClient);
	m_strDvrName = DVRName;
	ASSERT(m_strDvrName.GetLength() < NAME_BUF_LENGTH);
	m_sClient = sClient;

	if(GRAM_TYPE_TNEL == m_nGramType) {
		//// �������������
		m_OpenTnel.dwType = FOURCC_OPEN;
		m_OpenTnel.byAct = TUNNEL_REQUIRE;
		memcpy(m_OpenTnel.chFirstName,m_strDvrName,m_strDvrName.GetLength());
		memcpy(m_OpenTnel.chSecondName,m_strDvrName,m_strDvrName.GetLength());
	}
	DWORD dwID;
	IncreaseThreadCount();
	CreateThread(NULL,0,(LPTHREAD_START_ROUTINE)CClientTnel::SendGramProc,this,0,&dwID);
	return TRUE;
}

//// ���������ʶͬһ�����Ե����ݣ�������һ�������������Σ�����ʹ��IP�����ƣ�
BOOL CClientTnel::IsMyself(LPCTSTR CltName)
{
	//// CltName ��ʵ��һЩ����
	CString str = CltName;
	return (m_strName==str);
}

//// ʹ�ô˺�����ԭ���Ǽ���Զ��ֹͣ���Ӻ��ֿ�ʼ������������������û�б�ɾ��
//// ���������DVR �����ṩ��
BOOL CClientTnel::RetryTnel(LPCTSTR DVRName)
{
	//// �˴�������裺�������������������Ѿ�ʹ�õ�Client ���Ʋ�ͬ�������Ѿ�ʹ�õ���ֱ�����ӵ�����
	//// ���������Ҫ�����Ѿ�ʹ���������ӵı���

	CString strNewDvrName = DVRName;
	//// ���Ʋ�ͬ����ʱȷ��ԭ��ʹ�õ���ֱ������
	if(!UncaseCompare(strNewDvrName,m_strDvrName)) {
		MakeTnelGram(strNewDvrName,m_strDvrName,TUNNEL_ACK_DUPLINK);
		return FALSE;
	}
	EnterCriticalSection(&m_csCount);
	if(m_bTnelOpened) {
		//// ���ʱ��Ҫ���ָ���ʱ�����������
		m_nLastCmdJump = 0;
	}
	else {
		//// ���ʱ��Ҫ���ָ���ʱ��Ŀ�ͨ��
		m_nSendTnelCount = 0;
		if(INFINITE == m_nSendGramInterval) {
			//// ���Ƿǳ�С�����¼���ֻ���û��Լ��ȴ�һ��ʱ������ֶ���ʼ���Ӳ����������
			m_OpenTnel.dwType = FOURCC_OPEN;
			m_OpenTnel.byAct = TUNNEL_REQUIRE;
			char chTmp[NAME_BUF_LENGTH];
			int nLength = m_strDvrName.GetLength();
			memset(chTmp,0,NAME_BUF_LENGTH);
			strcpy(chTmp,m_strDvrName);
			PushinRandom(m_OpenTnel.chFirstName,chTmp,NAME_BUF_LENGTH,nLength,RANDOM_TRANS_NAMEKEY);
			PushinRandom(m_OpenTnel.chSecondName,chTmp,NAME_BUF_LENGTH,nLength,RANDOM_TRANS_NAMEKEY);
			m_nSendGramInterval = CREATE_TNEL_INTERVAL;
			SetEvent(m_hNormalGram);  //// �����߳�
		}
	}
	LeaveCriticalSection(&m_csCount);

	return TRUE;
}

//// �Լ졣�������FALSE,˵��û�д������壬��Ҫɾ��
BOOL CClientTnel::SelfChecking()
{
	BOOL bExist = TRUE;
	EnterCriticalSection(&m_csCount);
	if(m_bTnelOpened) {
		if(m_nLastCmdJump > MAX_SILENCE_INTERVAL) {
			bExist = FALSE;
		}
		else {
			m_nLastCmdJump++;
		}
	}
	else {
		if(m_nSendTnelCount > MAX_TNEL_SEND_COUNT) {
			bExist = FALSE;
		}
		else {
			m_nSendTnelCount++;
		}
	}
	LeaveCriticalSection(&m_csCount);
	TRACE("DVR: Clt exit:%d",bExist);
	return bExist;
}

void CClientTnel::MakeTnelGram(LPCTSTR FirstName,LPCTSTR SecondName,BYTE byAct)
{
	CString strFirstName = FirstName;
	CString strSecondName = SecondName;
	ASSERT(strFirstName.GetLength()<=MAX_NAME_LENGTH && strSecondName.GetLength()<=MAX_NAME_LENGTH &&
		strFirstName.GetLength()>=MIN_NAME_LENGTH && strSecondName.GetLength()>=MIN_NAME_LENGTH);
	char chTmp[NAME_BUF_LENGTH];
	int nLength;
	EnterCriticalSection(&m_csGram);
	m_OpenTnel.dwType = FOURCC_OPEN;
	m_OpenTnel.byAct = byAct;
	memset(chTmp,0,NAME_BUF_LENGTH);
	nLength = strFirstName.GetLength();
	strcpy(chTmp,strFirstName);
	PushinRandom(m_OpenTnel.chFirstName,chTmp,NAME_BUF_LENGTH,nLength,RANDOM_TRANS_NAMEKEY);
	memset(chTmp,0,NAME_BUF_LENGTH);
	nLength = strSecondName.GetLength();
	strcpy(chTmp,strSecondName);
	PushinRandom(m_OpenTnel.chSecondName,chTmp,NAME_BUF_LENGTH,nLength,RANDOM_TRANS_NAMEKEY);
	EnterCriticalSection(&g_csSocket);
	sendto(m_sClient,(char*)&m_OpenTnel,sizeof(OPENTNEL),0,(SOCKADDR*)&m_ClientAddr,sizeof(SOCKADDR_IN));
	LeaveCriticalSection(&g_csSocket);
	LeaveCriticalSection(&m_csGram);
	return ;
}

//// �Է��������������Ϣ�������ַ��һ�£������Ѿ����ӣ�����Ѿ����ӣ���ֱ��������
//// �������ȷ�ϵ��ǣ������������ʱ�����ͻ�������һ�µ�ʱ�������ַ�϶���һ�µ�
BYTE CClientTnel::DealwithCnctGram(PCNCTDESC pTnelInfo)
{
	CString strTmpName;
	char chTmp[NAME_BUF_LENGTH];
	int nLength = 0;
	memset(chTmp,0,NAME_BUF_LENGTH);
	if(!ThrowRandom(chTmp,pTnelInfo->OpenGram.chFirstName,&nLength,NAME_BUF_LENGTH,NAME_BUF_LENGTH,RANDOM_TRANS_NAMEKEY)) {
		return NTT_UNKNOW_ERROR;
	}
	if(nLength > MAX_NAME_LENGTH || nLength < MIN_NAME_LENGTH) {
		return NTT_UNKNOW_ERROR;
	}
	//// strTmpName = chTmp;
	if(m_strName != chTmp) return NTT_UNKNOW_ERROR;
	//// ��������£���ַ��ͬ�����ӵ����Ʋ�ͬ��ͬʱ���ֵ�
	//// �����жϵ�����˳��Ҫע��
	memset(chTmp,0,NAME_BUF_LENGTH);
	if(!ThrowRandom(chTmp,pTnelInfo->OpenGram.chSecondName,&nLength,NAME_BUF_LENGTH,NAME_BUF_LENGTH,RANDOM_TRANS_NAMEKEY)) {
		return NTT_UNKNOW_ERROR;
	}
	if(nLength > MAX_NAME_LENGTH || nLength < MIN_NAME_LENGTH) {
		return NTT_UNKNOW_ERROR;
	}
	//// strTmpName = chTmp;
	if(!UncaseCompare(chTmp,m_strDvrName))  {
		//// ��������������Ʋ�ͬ��������Ϊͬһ���ͻ��˵ĵڶ�������
		MakeTnelGram(chTmp,m_strDvrName,TUNNEL_ACK_DUPLINK);
		return TUNNEL_ACK_DUPLINK;
	}
	/*
	if((pTnelInfo->addr.sin_addr.s_addr!=m_ClientAddr.sin_addr.s_addr) ||
		pTnelInfo->addr.sin_port!=m_ClientAddr.sin_port) {
		//// ����������ӵ�ַ��ͬ��������Ϊͬһ���ͻ��˵ĵڶ�������
		MakeTnelGram(strTmpName,m_strDvrName,TUNNEL_ACK_DUPLINK);
		return TUNNEL_ACK_DUPLINK;
	}
	*/
	if(TUNNEL_REQUIRE==pTnelInfo->OpenGram.byAct) {
		if(!m_bTnelOpened) {
			//// Ϊ��ȷ���Է��ĵ�ַ�����յ������������ַΪ�Է������ĵ�ַ
			EnterCriticalSection(&m_csGram);
			memcpy(&m_ClientAddr,&pTnelInfo->addr,sizeof(SOCKADDR_IN));
			LeaveCriticalSection(&m_csGram);
			m_bTnelOpened = TRUE;
			m_byMsgAct = TUNNEL_MSG;
		}
		MakeTnelGram(m_strDvrName,m_strDvrName,TUNNEL_ACK_SUCCESS);
		return TUNNEL_ACK_SUCCESS;
	}
	//// ����˵���Է��������ǻ�Ӧ��Ϣ
	if(m_bTnelOpened) {
		return TUNNEL_ACK_SUCCESS;  
	}
	else {
		m_bTnelOpened = TRUE;
		m_byMsgAct = TUNNEL_MSG;
		return TUNNEL_ACK_SUCCESS;
	}

}

//// �п���ֱ�����ӵġ����ʱ�������ͨ�������棬���ÿ�������ˣ����غ��ɹ����̴߳�����������
//// �����ʱ�������û�п�ͨ�����ж��Ƿ��������һ�����ӡ�ͬʱ��������ظ�������
BOOL CClientTnel::CheckCmdGram(PCMDDESC pWorkInfo)
{
	char chTmp[NAME_BUF_LENGTH];
	int nLength = 0;
	TRACE("Get Client Cmd");
	if(pWorkInfo->WorkGram.dwType != FOURCC_PUSH) return FALSE;
	if(!ThrowRandom(chTmp,pWorkInfo->WorkGram.chLocalName,&nLength,NAME_BUF_LENGTH,NAME_BUF_LENGTH,RANDOM_TRANS_NAMEKEY)) {
		return FALSE;
	}
	ASSERT(m_strName == chTmp);  //// ��Ϊ����һ���Ѿ��жϹ���
	if(!ThrowRandom(chTmp,pWorkInfo->WorkGram.chDestName,&nLength,NAME_BUF_LENGTH,NAME_BUF_LENGTH,RANDOM_TRANS_NAMEKEY)) {
		return FALSE;
	}
	if(nLength > MAX_NAME_LENGTH || nLength < MIN_NAME_LENGTH) {
		return FALSE;
	}
	if(m_bTnelOpened && (!UncaseCompare(m_strDvrName,chTmp))) return FALSE;  //// һ���ͻ�ʹ���������ϵķ�ʽ����DVR
	
	//// �����ʽ��ȷ
	EnterCriticalSection(&m_csCount);
	m_nLastCmdJump = 0;
	LeaveCriticalSection(&m_csCount);
	if(!m_bTnelOpened) {
		//// �ж��Ƿ���ͨ��ֱ�����ӵ�;�����ġ����������ֿ����ԣ�
		//// 1���Է��Ѿ��յ���ߵĴ򶴱��ģ�ȷ�������ͨ��Ȼ���ͱ��ģ�����ʱ���ػ�û���յ��Է��Ļظ����ģ�
		//// 2���Է�ͨ��ֱ�����ӵķ�ʽ����
		EnterCriticalSection(&m_csGram);
		if((pWorkInfo->addr.sin_addr.s_addr!=m_ClientAddr.sin_addr.s_addr) ||
			(pWorkInfo->addr.sin_port!=m_ClientAddr.sin_port)) {
			//// ˵�����������ֱ�����ӹ�����
			m_ClientAddr = pWorkInfo->addr;
			m_byMsgAct = CONNECT_MSG;
			m_strDvrName = chTmp; 
		}
		else {
			//// �Է��Ѿ��յ��򶴱��ģ����ǻظ�������ĳ��ԭ��û���յ������ҷ������������
			m_byMsgAct = TUNNEL_MSG;
			//// Ϊ���������������һ��DVRName��ֵ��������һ�������Ե���ֱ�����ӵĵ�ַ�������ַ��ͬ��
			//// ��Client��ֱ������Internet�ϡ���DVR��ͨ���˿�ӳ�����ֱ������������ʽ���ӣ�
			//// ��ʱClient������DVR���ƿ��ܺ�������Ʋ�ͬ��
			m_strDvrName = chTmp;
			
		}
		m_nLastCmdJump = 0;
		m_nSendTnelCount = 0;
		m_bTnelOpened = TRUE;
		LeaveCriticalSection(&m_csGram);
	}
	return TRUE;
}

BOOL CClientTnel::SetMsgGram(PCMDINFO pWorkInfo)
{
	char chTmp[NAME_BUF_LENGTH];
	int nLength;
	EnterCriticalSection(&m_csGram);
	memcpy(&m_CmdGram.Info,pWorkInfo,sizeof(CMDINFO));
	m_CmdGram.dwType = FOURCC_PUSH;
	memset(chTmp,0,NAME_BUF_LENGTH);
	nLength = m_strDvrName.GetLength();
	strcpy(chTmp,m_strDvrName);
	PushinRandom(m_CmdGram.chLocalName,chTmp,NAME_BUF_LENGTH,nLength,RANDOM_TRANS_NAMEKEY);
	memset(chTmp,0,NAME_BUF_LENGTH);
	nLength = m_strName.GetLength();
	strcpy(chTmp,m_strName);
	PushinRandom(m_CmdGram.chDestName,chTmp,NAME_BUF_LENGTH,nLength,RANDOM_TRANS_NAMEKEY);
	m_CmdGram.byAct = m_byMsgAct;
	m_nGramType = GRAM_TYPE_MSG;
	LeaveCriticalSection(&m_csGram);
	SetEvent(m_hNormalGram);

	return TRUE;
}

BOOL CClientTnel::SendGram()
{
	DWORD wait;
	HANDLE hEv[2];
	hEv[0] = m_hNormalGram;
	hEv[1] = m_hStopWorking;
	while (TRUE) {
		wait = WaitForMultipleObjects(2,hEv,FALSE,m_nSendGramInterval);
		if(WAIT_OBJECT_0 == wait) {
			if(GRAM_TYPE_MSG == m_nGramType) {
				EnterCriticalSection(&m_csGram);
				EnterCriticalSection(&g_csSocket);
				sendto(m_sClient,(char*)&m_CmdGram,sizeof(WORK_GRAM),0,(SOCKADDR*)&m_ClientAddr,sizeof(SOCKADDR_IN));
				LeaveCriticalSection(&g_csSocket);
				m_nSendGramInterval = INFINITE; //// �Է���һ
				LeaveCriticalSection(&m_csGram);
				continue;
			}
			if(GRAM_TYPE_TNEL == m_nGramType) {  
				EnterCriticalSection(&m_csGram);
				EnterCriticalSection(&g_csSocket);
				sendto(m_sClient,(char*)&m_OpenTnel,sizeof(OPENTNEL),0,(SOCKADDR*) &m_ClientAddr,sizeof(SOCKADDR_IN));
				LeaveCriticalSection(&g_csSocket);
				LeaveCriticalSection(&m_csGram);
				EnterCriticalSection(&m_csCount);
				m_nSendTnelCount++;
				LeaveCriticalSection(&m_csCount);
			}
		}
		else if(WAIT_TIMEOUT == wait) { //// ��
			ASSERT(GRAM_TYPE_TNEL == m_nGramType);
			if(m_bTnelOpened || (m_nSendTnelCount>MAX_TNEL_SEND_COUNT)) {
				m_nSendGramInterval = INFINITE; 
				continue;
			}
			EnterCriticalSection(&m_csGram);
			EnterCriticalSection(&g_csSocket);
			sendto(m_sClient,(char*)&m_OpenTnel,sizeof(OPENTNEL),0,(SOCKADDR*) &m_ClientAddr,sizeof(SOCKADDR_IN));
			LeaveCriticalSection(&g_csSocket);
			LeaveCriticalSection(&m_csGram);
			EnterCriticalSection(&m_csCount);
			m_nSendTnelCount++;
			LeaveCriticalSection(&m_csCount);
		}
		else {
			break;
		}
	}
	return TRUE;
}

UINT CClientTnel::SendGramProc(LPVOID p)
{
	CClientTnel *pClt = (CClientTnel*)p;
	pClt->SendGram();
	pClt->DecreaseThreadCount();
	return 1;
}
