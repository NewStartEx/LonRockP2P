////  2009.12.14 : 增加了DVR排序算法，搜索速度更快
#include "stdafx.h"
#include "NetSManager.h"
#include "NetSDVR.h"
#include "Global.h"
#include "NetSGlobal.h"
#include "NTTProtocol.h"

//// 就目前的认识来说，整个系统只能有一个相同地址的UDP Socket.
int g_nAccountNum = 0;
CRITICAL_SECTION g_csSocket;  
CNetSManager::CNetSManager()
{
	InitializeCriticalSection(&g_csSocket);
	m_nThreadCount = 0;
	m_hMsgWnd = NULL;
	m_bWorking = FALSE;
	m_wServerPort = NETSERVER_PORT;
	memset(&m_DVRMsg,0,sizeof(DVRMSG));
	memset(&m_TunnelMsg,0,sizeof(CLTMSG));
	m_sRcvGram = INVALID_SOCKET;
	m_sSendGram = INVALID_SOCKET;
	m_LastCltGetStatus = 0;  //// 不是状态里的任何一个，这样第一个状态请求才不会重叠
	InitializeCriticalSection(&m_csGram);
	InitializeCriticalSection(&m_csDvr);
	m_hCltGram = CreateEvent(NULL,FALSE,FALSE,NULL);
	m_hDVRGram = CreateEvent(NULL,FALSE,FALSE,NULL);
	m_hRcvGram = WSACreateEvent();
	m_hStopWorking = CreateEvent(NULL,TRUE,FALSE,NULL);
}

CNetSManager::~CNetSManager()
{
	if(m_bWorking) {
		SetEvent(m_hStopWorking);
	}
	while (m_nThreadCount > 0) {
		Sleep(50);
	}
////	Sleep(100); //// 以防万一
	for(int i = 0; i < m_NetSDVRArray.GetSize(); i++) {
		delete m_NetSDVRArray[i];
	}
	m_NetSDVRArray.RemoveAll();
	CloseHandle(m_hCltGram);
	CloseHandle(m_hDVRGram);
	WSACloseEvent(m_hRcvGram);
	CloseHandle(m_hStopWorking);
	closesocket(m_sRcvGram);
	DeleteCriticalSection(&m_csGram);
	DeleteCriticalSection(&m_csDvr);
	DeleteCriticalSection(&g_csSocket);
}

BOOL CNetSManager::StartWorking(HWND hWnd,LPCTSTR Path)
{
	if(m_bWorking) return FALSE;
	m_hMsgWnd = hWnd;
	SOCKADDR_IN LocalAddr;
	memset(&LocalAddr,0,sizeof(SOCKADDR_IN));

	LocalAddr.sin_family = AF_INET;
	LocalAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	LocalAddr.sin_port = htons(m_wServerPort);

	int on = 1;
	m_sRcvGram = socket(AF_INET,SOCK_DGRAM,0);
	if(SOCKET_ERROR ==
		bind(m_sRcvGram, (SOCKADDR *FAR)&LocalAddr,sizeof(SOCKADDR_IN))) {
		closesocket(m_sRcvGram);
		m_sRcvGram = INVALID_SOCKET;
		return FALSE;
	}
	if(SOCKET_ERROR == (WSAEventSelect(m_sRcvGram,m_hRcvGram,FD_READ))) {
		closesocket(m_sRcvGram);
		m_sRcvGram = INVALID_SOCKET;
		return FALSE;
	}

	m_sSendGram = m_sRcvGram;

	TRACE("NetServer start working");
	m_bWorking = TRUE;
	m_strDVRFileName = Path;
	m_strBackupFileName = Path;
	m_strDVRFileName += DVR_LIST_FILENAME;
	m_strBackupFileName += BACKUP_FILENAME;
	ReadDVRListFile();

	DWORD dwID;
	IncreaseThreadCount();
	CreateThread(NULL,0,(LPTHREAD_START_ROUTINE)RcvGramProc,this,0,&dwID);
	IncreaseThreadCount();
	CreateThread(NULL,0,(LPTHREAD_START_ROUTINE)HandleGramProc,this,0,&dwID);
	IncreaseThreadCount();
	CreateThread(NULL,0,(LPTHREAD_START_ROUTINE)CheckDVRProc,this,0,&dwID);
	return TRUE;
}

void CNetSManager::StopWorking()
{
	//// WriteDVRList在处理线程里面使用
#ifdef _DEBUG
	WriteDVRListFile();
#endif
	SetEvent(m_hStopWorking);
	m_bWorking = FALSE;
}

BOOL CNetSManager::CheckBackupFile()
{
	FILE *hf = fopen(m_strBackupFileName,"rb");
	if(NULL == hf) return FALSE;
	DWORD dwHead=0,dwEnd=0;
	fread(&dwHead,1,sizeof(DWORD),hf);
	fseek(hf,-4,SEEK_END);
	fread(&dwEnd,1,sizeof(UINT),hf);
	fclose(hf);

	if(dwHead != FOURCC_NETS || dwEnd != FOURCC_FEND) {
		return FALSE;
	}

	return TRUE;
}

BOOL CNetSManager::ReadDVRListFile()
{
	FILE *hf = fopen(m_strDVRFileName,"rb");
	if(NULL == hf) return FALSE;
	DWORD dwHead,dwEnd=0;
	UINT uSize;
	DVR_ITEM dvrItem;
	int nRead;
	BOOL bGetListFile = TRUE;
	BOOL bGetBackupFile = CheckBackupFile();
	nRead = fread(&dwHead,1,sizeof(DWORD),hf);
	fseek(hf,-4,SEEK_END);
	nRead = fread(&dwEnd,1,sizeof(UINT),hf);
	fclose(hf);
	if(dwHead != FOURCC_NETS) {
		bGetListFile = FALSE;
	}
	//// 再看末尾是不是FOURCC_FEND,是就继续，
	//// 否则找备份文件。如果没有备份文件，就直接使用这个，下次写的时候写入备份文件
	if(dwEnd != FOURCC_FEND) {
		if(bGetBackupFile)  {
			bGetListFile = FALSE;
		}
	}
	if(!bGetListFile) {
		if(bGetBackupFile) {
			CopyFile(m_strBackupFileName,m_strDVRFileName,FALSE);
		}
		else {
			return FALSE;
		}
	}
	hf = fopen(m_strDVRFileName,"rb");
	ASSERT(NULL != hf);
	fseek(hf,sizeof(DWORD),SEEK_SET);
	nRead = fread(&uSize,1,sizeof(UINT),hf);
	
	DVR_INFO DvrInfo;
	DWORD dwIPAddr;
	WORD  wPort;
	SYSTEMTIME stLastLogin;
	for(UINT i = 0; i < uSize; i++) {
		nRead = fread(&dvrItem,1,sizeof(DVR_ITEM),hf);
		if(nRead != sizeof(DVR_ITEM)) {
			break;
		}
		memset(&DvrInfo,0,sizeof(DVR_INFO));
		if(CheckDVRListItem(&dvrItem,&DvrInfo,&dwIPAddr,&wPort,&stLastLogin)) {
#ifdef _DEBUG
			CString strtmp = DvrInfo.chName;
			strtmp.MakeUpper();
			if(strtmp.Left(7) == _T("MAODONG")) {
				int k = 1;
			}
#endif
			CNetSDVR *pDVR = new CNetSDVR(m_sSendGram,DvrInfo,dwIPAddr,wPort,stLastLogin);
			ArrangDVR(pDVR);
		}
	}
	fclose(hf);
	
	if(!bGetBackupFile) {
		WriteBackupFile();
	}
	g_nAccountNum = m_NetSDVRArray.GetSize();
	return TRUE;
}

BOOL CNetSManager::WriteBackupFile()
{
	FILE *hf = fopen(m_strBackupFileName,"wb");
	if(NULL == hf) return FALSE;
	DWORD dwHead = FOURCC_NETS,dwEnd = FOURCC_FEND;
	int	  nSize = 0;
	DVR_ITEM dvrItem;
	EnterCriticalSection(&m_csDvr);
	fwrite(&dwHead,sizeof(DWORD),1,hf);
	nSize = m_NetSDVRArray.GetSize();
	fwrite(&nSize,sizeof(int),1,hf);
	for(int i = 0; i < nSize; i++) {
		if(m_NetSDVRArray[i]->FetchSavingInfo(&dvrItem))
			fwrite(&dvrItem,sizeof(DVR_ITEM),1,hf);
	}
	LeaveCriticalSection(&m_csDvr);
	fwrite(&dwEnd,sizeof(DWORD),1,hf);
	fclose(hf);

	return TRUE;
}

BOOL CNetSManager::WriteDVRListFile()
{
	if(!WriteBackupFile()) return FALSE;

	if(!CopyFile(m_strBackupFileName,m_strDVRFileName,FALSE)) {
		return FALSE;
	}
	//// 备份到以日期命名的文件夹里面
	//// DVR_LIST_FILENAME
	CString strBackFile = GetCurrentPath();
	CTime ctToday = CTime::GetCurrentTime();
	CString strSubDir;
	strSubDir.Format("\\%04d_%02d_%02d",ctToday.GetYear(),ctToday.GetMonth(),ctToday.GetDay());
	strBackFile += strSubDir;
	CreateDirectory(strBackFile,NULL);
	strBackFile += DVR_LIST_FILENAME;
	CopyFile(m_strBackupFileName,strBackFile,TRUE); //// 一天只覆盖一次
	g_nAccountNum = m_NetSDVRArray.GetSize();
	return TRUE;
}

BOOL CNetSManager::CheckDVRListItem(/*IN*/PDVR_ITEM pItem, /*OUT*/PDVR_INFO pDvrInfo, /*OUT*/DWORD *pIPAddr, /*OUT*/WORD *pPort, /*OUT*/LPSYSTEMTIME pstLastLink)
{
	char chTmp[NAME_BUF_LENGTH];
	int nLength = 0;
	if(!ThrowRandom(chTmp,pItem->chName,&nLength,NAME_BUF_LENGTH,NAME_BUF_LENGTH,RANDOM_TRANS_NAMEKEY)) 
		return FALSE;
	if(nLength < MIN_NAME_LENGTH || nLength > MAX_NAME_LENGTH) 
		return FALSE;
	strcpy(pDvrInfo->chName,chTmp);
	if(!ThrowRandom(chTmp,pItem->chPassword,&nLength,NAME_BUF_LENGTH,NAME_BUF_LENGTH,RANDOM_TRANS_NAMEKEY)) 
		return FALSE;
	if(nLength != MD5_CODE_LENGTH) 
		return FALSE;
	memcpy(pDvrInfo->chPassword,chTmp,NAME_BUF_LENGTH);

	//// if(pItem->chName[NAME_BUF_LENGTH-1] != 0) return FALSE;
	//// if(pItem->chPassword[NAME_BUF_LENGTH-1] != 0) return FALSE;
	pDvrInfo->dwType = pItem->dwType;
	pDvrInfo->byAct = pItem->byAct1;
	
	
	pDvrInfo->dwDevID[0] = pItem->dwDevID[0];
	pDvrInfo->dwDevID[1] = pItem->dwDevID[1];
	pDvrInfo->byStatus = pItem->byAct2;
	pDvrInfo->dwReserved = pItem->dwReserved;

	*pIPAddr = pItem->dwIPaddr;
	*pPort = pItem->wPort;

	pstLastLink->wYear = pItem->wLastLinkYear;
	pstLastLink->wMonth = pItem->wLastLinkMonth;
	pstLastLink->wDay = pItem->wLastLinkDay;
	pstLastLink->wHour = pItem->wLastLinkHour;
	pstLastLink->wMinute = pItem->wLastLinkMinute;
	pstLastLink->wSecond = pItem->wLastLinkSecond;

	return TRUE;
}

BOOL CNetSManager::LegalDvrApply(PAPPLY_GRAM pGram,char *pDvrName,char *pPassword)
{
	char chTmp[NAME_BUF_LENGTH];
	int nLength = 0;
	if(!ThrowRandom(chTmp,pGram->chName,&nLength,NAME_BUF_LENGTH,NAME_BUF_LENGTH,RANDOM_APPLY_NAMEKEY)) {
		return FALSE;
	}
	if(nLength > MAX_NAME_LENGTH || nLength < MIN_NAME_LENGTH) {
		return FALSE;
	}
	strcpy(pDvrName,chTmp);
	if(!ThrowRandom(chTmp,pGram->chPassword,&nLength,NAME_BUF_LENGTH,NAME_BUF_LENGTH,RANDOM_APPLY_PASSKEY)) {
		return FALSE;
	}
	if(MD5_CODE_LENGTH != nLength) {
		return FALSE;
	}
	memcpy(pPassword,chTmp,MD5_CODE_LENGTH);

	return TRUE;
}

BOOL CNetSManager::LegalCltReq(PCLTREQ_GRAM pGram,char *pDvrName,char *pCltName)
{
	char chTmp[NAME_BUF_LENGTH];
	int nLength = 0;
	if(TUNNEL_REQUIRE != pGram->byAct && TUNNEL_ASKIPADDR != pGram->byAct) {
		return FALSE;
	}
	if(!ThrowRandom(chTmp,pGram->chDstName,&nLength,NAME_BUF_LENGTH,NAME_BUF_LENGTH,RANDOM_TRANS_NAMEKEY)) {
		return FALSE;
	}
	if(nLength > MAX_NAME_LENGTH || nLength < MIN_NAME_LENGTH) {
		return FALSE;
	}
	strcpy(pDvrName,chTmp);
	if(!ThrowRandom(chTmp,pGram->chName,&nLength,NAME_BUF_LENGTH,NAME_BUF_LENGTH,RANDOM_TRANS_NAMEKEY)) {
		return FALSE;
	}
	if(nLength > MAX_NAME_LENGTH || nLength < MIN_NAME_LENGTH) {
		return FALSE;
	}
	strcpy(pCltName,chTmp);

	return TRUE;
}

//// 小的在前面，大的在后面
void CNetSManager::ArrangDVR(CNetSDVR* pDVR)
{
	CString strName1,strName2;
	DVRMSG DvrInfo;
	int count = m_NetSDVRArray.GetSize();
	if(0 == count) {
		m_NetSDVRArray.Add(pDVR);
		return ;
	}
	memset(&DvrInfo,0,sizeof(DVRMSG));
	pDVR->FetchDVRInfo(&DvrInfo);
	strName1 = DvrInfo.chName;
	strName1.MakeUpper();
#ifdef _DEBUG
	/*
	CString strtmp = strName1.Left(7);
	CString strtmp1 = strtmp.Right(2);
	if(strtmp1 == _T("27")) {
		int k = 1;
	}
	*/
	
	if(strName1.Left(3) == _T("888")) {
		int k = 1;
	}
#endif
	if(1 == count) {
		m_NetSDVRArray[0]->FetchDVRInfo(&DvrInfo);
		strName2 = DvrInfo.chName;
		strName2.MakeUpper();
		ASSERT(strName1 != strName2);
		if(strName1 > strName2) {
			m_NetSDVRArray.Add(pDVR);
		}
		else {
			m_NetSDVRArray.InsertAt(0,pDVR);
		}
	}
	else {
		OrderDVRArray(pDVR,0,count);
	}

}

void CNetSManager::OrderDVRArray(CNetSDVR *pDVR,int startIndex,int stopIndex)
{
	CString strName1,strName2;
	DVRMSG DvrInfo;
	memset(&DvrInfo,0,sizeof(DVRMSG));
	pDVR->FetchDVRInfo(&DvrInfo);
	strName1 = DvrInfo.chName;
	strName1.MakeUpper();
	if((stopIndex - startIndex) < 10) {
		for(int i = startIndex; i < stopIndex; i++) {
			m_NetSDVRArray[i]->FetchDVRInfo(&DvrInfo);
			strName2 = DvrInfo.chName;
			strName2.MakeUpper();
			ASSERT(strName1 != strName2);
			if(strName1 < strName2) {
				break;
			}
		}
		m_NetSDVRArray.InsertAt(i,pDVR);
		return ;
	}
	int newPos = (startIndex+stopIndex)  >> 1; // 二分
	m_NetSDVRArray[newPos]->FetchDVRInfo(&DvrInfo);
	strName2 = DvrInfo.chName;
	strName2.MakeUpper();
	ASSERT(strName1 != strName2);
	if(strName1 < strName2) {
		OrderDVRArray(pDVR,startIndex,newPos);
	}
	else {
		OrderDVRArray(pDVR,newPos,stopIndex);
	}
}

int CNetSManager::SearchDVR(LPCTSTR DVRName)
{
	CString strName1,strName2;
	DVRMSG DvrInfo;
	int count;
	count = m_NetSDVRArray.GetSize();
	if(0 == count) {
		return -1;
	}
	strName1 = DVRName;
	strName1.MakeUpper();
	memset(&DvrInfo,0,sizeof(DVRMSG));
	if(1 == count) {
		m_NetSDVRArray[0]->FetchDVRInfo(&DvrInfo);
		strName2 = DvrInfo.chName;
		strName2.MakeUpper();
		if(strName1 == strName2) {
			return 0;
		}
		else {
			return -1;
		}
	}
	else {
		return OrderFindDVR(DVRName,0,count);
	}
}

int CNetSManager::OrderFindDVR(LPCTSTR DvrName,int startIndex,int stopIndex)
{
	CString strName1,strName2;
	DVRMSG DvrInfo;
	memset(&DvrInfo,0,sizeof(DVRMSG));
	strName1 = DvrName;
	strName1.MakeUpper();
	if((stopIndex-startIndex) < 10) {
		int Index = -1;
		for(int i = startIndex; i < stopIndex; i++) {
			m_NetSDVRArray[i]->FetchDVRInfo(&DvrInfo);
			strName2 = DvrInfo.chName;
			strName2.MakeUpper();
			if(strName1 == strName2) {
				Index = i;
				break;
			}
		}
		return Index;
	}
	int newPos = (startIndex+stopIndex) >> 1; 
	m_NetSDVRArray[newPos]->FetchDVRInfo(&DvrInfo);
	strName2 = DvrInfo.chName;
	strName2.MakeUpper();
	if(strName1 < strName2) {
		return OrderFindDVR(strName1,startIndex,newPos);
	}
	else {
		return OrderFindDVR(strName1,newPos,stopIndex);
	}
}

UINT CNetSManager::KeepRcving()
{
	ASSERT(m_sRcvGram != INVALID_SOCKET);
	SOCKADDR_IN DestAddr;
	DWORD dwWait;
	DVRDESC dvrDesc;
	CLTDESC cltDesc;
	char rcvBuf[NETS_RCV_BUF_LENGTH];
	int nRcvLength;
	HANDLE hEv[2];
	memset(rcvBuf,0,NETS_RCV_BUF_LENGTH);
	hEv[0] = m_hRcvGram;
	hEv[1] = m_hStopWorking;
	while (m_bWorking) {
		dwWait = WSAWaitForMultipleEvents(2,hEv,FALSE,INFINITE,FALSE);
		if(WAIT_OBJECT_0 == dwWait) {
			WSAResetEvent(hEv[0]);
			memset(&DestAddr,0,sizeof(SOCKADDR_IN));
			int len = sizeof(SOCKADDR_IN);
			//// EnterCriticalSection(&g_csSocket); 接收和发送没有冲突
			nRcvLength = recvfrom(m_sRcvGram,rcvBuf,NETS_RCV_BUF_LENGTH,0,(SOCKADDR*)&DestAddr,&len);
			//// LeaveCriticalSection(&g_csSocket);
			TRACE("Rcv Gram length:%d",nRcvLength);
			if(sizeof(APPLY_GRAM) == nRcvLength) {
				//// DVR's Gram
				memcpy(&dvrDesc.dvrGram,rcvBuf,sizeof(APPLY_GRAM));
				dvrDesc.addr = DestAddr;
				EnterCriticalSection(&m_csGram);
				m_DVRDescArray.Add(dvrDesc);
				LeaveCriticalSection(&m_csGram);
				SetEvent(m_hDVRGram);
			}
			else if(sizeof(CLTREQ_GRAM) == nRcvLength) {
				//// Client's Gram
				memcpy(&cltDesc.cltGram,rcvBuf,sizeof(CLTREQ_GRAM));
				cltDesc.addr = DestAddr;
				EnterCriticalSection(&m_csGram);
				m_CltDescArray.Add(cltDesc);
				LeaveCriticalSection(&m_csGram);
				SetEvent(m_hCltGram);
			}
			else {
				//// 莫名数据
				;
			}
		}
		else if((WAIT_OBJECT_0+1) == dwWait) {
			break; //// 退出循环
		}
		else {
			//// 这个是不可能的，这里做一个保护
			TRACE("Recv Thread Impossible Happen");
		}
	}
	return 1;
}

UINT CNetSManager::CheckGram()
{
	ASSERT(m_sSendGram != INVALID_SOCKET);
	DWORD dwWait;
	HANDLE hEv[3];
	
	hEv[0] = m_hDVRGram;
	hEv[1] = m_hCltGram;
	hEv[2] = m_hStopWorking;
	while (m_bWorking) {
		dwWait = WaitForMultipleObjects(3,hEv,FALSE,INFINITE); //// 10 秒超时一次
		if(WAIT_OBJECT_0 == dwWait) {
			DealWithDvrGram();
		}
		else if((WAIT_OBJECT_0+1) == dwWait) {
			DealWithCltGram();
		}
		else if((WAIT_OBJECT_0+2) == dwWait) {
			break;
		}
		else {
			TRACE("Handle Gram Thread Impossible Happen");
		}
	}
	return 1;
}

UINT CNetSManager::CheckDVR()
{
	DWORD dwWait;
	while (m_bWorking) {
		dwWait = WaitForSingleObject(m_hStopWorking,10000); 
		if(WAIT_TIMEOUT == dwWait) {
			NormalDVRChecking();
		}
		else {
			break;
		}
	}

	return 1;
}

UINT CNetSManager::DealWithDvrGram()
{
	int nNetSDVRcount,nLength;
	DVRDESC dvrDesc;
	APPLY_GRAM fdGram;
	BOOL bFound = FALSE,bNeedSaveFile = FALSE;
	CString strName;
	DWORD dwIPAddr;
	WORD  wPort;
	SYSTEMTIME stCur;
	UINT  uRe;
	WPARAM wpara = WPARAM_DVR_UNKNOW;
	char chDvrName[NAME_BUF_LENGTH],chPassword[NAME_BUF_LENGTH];

	int count = m_DVRDescArray.GetSize();
	DWORD dwStartWorking,dwWorkPoint;
	dwStartWorking = GetTickCount();
	while (count > 0) {
		dwWorkPoint = GetTickCount();
		if((dwWorkPoint-dwStartWorking) > MAX_WORK_TIME) {
			//// 连续工作时间太长要存盘
			WriteDVRListFile();
			bNeedSaveFile = FALSE;
			dwStartWorking = dwWorkPoint;
			TRACE("Dealwith DVR Gram too long, Save File");
		}
		EnterCriticalSection(&m_csGram);
		dvrDesc = m_DVRDescArray[0];
		m_DVRDescArray.RemoveAt(0);
		LeaveCriticalSection(&m_csGram);
		count = m_DVRDescArray.GetSize();
		if(!LegalDvrApply(&dvrDesc.dvrGram,chDvrName,chPassword)) {
			continue; //// 格式错误 
		}
		fdGram = dvrDesc.dvrGram;
		EnterCriticalSection(&m_csDvr);
		nNetSDVRcount = m_NetSDVRArray.GetSize();
		bFound = FALSE;
		strName = chDvrName;
		int nDvrIndex = SearchDVR(strName);
		/*
		for(int i = 0; i < nNetSDVRcount; i++) {
			if(m_NetSDVRArray[i]->IsMyself(strName)) {
				bFound = TRUE;
				break;
			}
		}
		*/
		if(-1 != nDvrIndex) {
			//// 叫NetSDVR 处理
			dwIPAddr = (DWORD)dvrDesc.addr.sin_addr.s_addr;
			wPort = (WORD)dvrDesc.addr.sin_port;
			//// 加密处理 
			uRe = m_NetSDVRArray[nDvrIndex]->CheckDVRGram(&fdGram,chPassword,dwIPAddr,wPort);
			TRACE("NETS: Feedback:%x",uRe);
			if(ACK_DELETE_SUCCESS == uRe) {
				delete m_NetSDVRArray[nDvrIndex];
				m_NetSDVRArray.RemoveAt(nDvrIndex);
				EnterCriticalSection(&g_csSocket);
				sendto(m_sSendGram,(char*)&fdGram,sizeof(APPLY_GRAM),0,(SOCKADDR*)&dvrDesc.addr,sizeof(SOCKADDR_IN));
				WaitForSockWroten(m_sSendGram,3);
				LeaveCriticalSection(&g_csSocket);
				bNeedSaveFile = TRUE;
				wpara = WPARAM_DVR_DELETED;
			}
			else if(ACK_SLEEP_SUCCESS == uRe) {
				EnterCriticalSection(&g_csSocket);
				sendto(m_sSendGram,(char*)&fdGram,sizeof(APPLY_GRAM),0,(SOCKADDR*)&dvrDesc.addr,sizeof(SOCKADDR_IN));
				WaitForSockWroten(m_sSendGram,3);
				LeaveCriticalSection(&g_csSocket);
				bNeedSaveFile = TRUE;
				wpara = WPARAM_DVR_OFFLINE;
			}
			else if((ACK_REGISTER_SUCCESS==uRe) || (ACK_UPDATE_OK==uRe) || (ACK_WAKEUP_SUCCESS==uRe)) {
				//// 有几种情况：1、已经注册了，现在又发送请求；2、手动更新；3、自动更新；4、激活
				//// 这几种情况都要让界面进行状态更新
				wpara = WPARAM_DVR_ONLINE;
			}
			else if(DVR_ACT_EXIT == uRe || ACK_SLEEP_SUCCESS == uRe) {
				wpara = WPARAM_DVR_OFFLINE;
			}
		}
		else {
			if((DVR_ACT_REGISTER == dvrDesc.dvrGram.byAct) /*|| (DVR_ACT_MANUALUPDATE==dvrDesc.dvrGram.byAct) ||
				(DVR_ACT_AUTOUPDATE==dvrDesc.dvrGram.byAct) 如果存储文件发生错误，这样来恢复以前的资料.应急*/) {
				dwIPAddr = (DWORD)dvrDesc.addr.sin_addr.s_addr;
				wPort = (WORD)dvrDesc.addr.sin_port;
				GetSystemTime(&stCur);  //// 第一次登录成功算作第一次连接时间
				//// 加密处理
				DVR_INFO dvrInfo = (DVR_INFO)fdGram;
				memset(dvrInfo.chName,0,NAME_BUF_LENGTH);
				strcpy(dvrInfo.chName,chDvrName);
				memcpy(dvrInfo.chPassword,chPassword,MD5_CODE_LENGTH);
				CNetSDVR *pDVR = new CNetSDVR(m_sSendGram,dvrInfo,dwIPAddr,wPort,stCur);
				ArrangDVR(pDVR);
				fdGram.dwType = FOURCC_NETS;
				fdGram.byAct = ACK_REGISTER_SUCCESS;
				nLength = strlen(chDvrName);
				PushinRandom(fdGram.chName,chDvrName,NAME_BUF_LENGTH,nLength,RANDOM_FEEDBACK_NAMEKEY);
				PushinRandom(fdGram.chPassword,chPassword,NAME_BUF_LENGTH,MD5_CODE_LENGTH,RANDOM_FEEDBACK_PASSKEY);
				
				EnterCriticalSection(&g_csSocket);
				sendto(m_sSendGram,(char*)&fdGram,sizeof(APPLY_GRAM),0,(SOCKADDR*)&dvrDesc.addr,sizeof(SOCKADDR_IN));
				WaitForSockWroten(m_sSendGram,3);
				LeaveCriticalSection(&g_csSocket);
				bNeedSaveFile = TRUE;
				wpara = WPARAM_DVR_ADDED;
			}
			else if((DVR_ACT_DELETE==dvrDesc.dvrGram.byAct)||(DVR_ACT_MANUALUPDATE==dvrDesc.dvrGram.byAct)|| 
				(DVR_ACT_WAKEUP==dvrDesc.dvrGram.byAct)) {
				fdGram.dwType = FOURCC_NETS;
				fdGram.byAct = ACK_DEVICE_NOTEXIST;
				nLength = strlen(chDvrName);
				PushinRandom(fdGram.chName,chDvrName,NAME_BUF_LENGTH,nLength,RANDOM_FEEDBACK_NAMEKEY);
				PushinRandom(fdGram.chPassword,chPassword,NAME_BUF_LENGTH,MD5_CODE_LENGTH,RANDOM_FEEDBACK_PASSKEY);

				EnterCriticalSection(&g_csSocket);
				sendto(m_sSendGram,(char*)&fdGram,sizeof(APPLY_GRAM),0,(SOCKADDR*)&dvrDesc.addr,sizeof(SOCKADDR_IN));
				WaitForSockWroten(m_sSendGram,3);
				LeaveCriticalSection(&g_csSocket);
			}
		}
		LeaveCriticalSection(&m_csDvr);
		if(wpara != WPARAM_DVR_UNKNOW) {
			m_DVRMsg.dwType = dvrDesc.dvrGram.dwType;
			m_DVRMsg.dwDevID[0] = dvrDesc.dvrGram.dwDevID[0];
			m_DVRMsg.dwDevID[1] = dvrDesc.dvrGram.dwDevID[1];
			strcpy(m_DVRMsg.chName,chDvrName);
			m_DVRMsg.dwIPaddr = dvrDesc.addr.sin_addr.s_addr;
			::SendMessage(m_hMsgWnd,WM_DVR_CHANGED,wpara,(LPARAM)&m_DVRMsg);
		}
	}
	if(bNeedSaveFile) {
		WriteDVRListFile();
	}

	return 1;
}

UINT CNetSManager::DealWithCltGram()
{
	int nNetSDVRcount,nLength;
	CLTDESC cltDesc;
	SOCKADDR_IN dvrAddr;
	BOOL bFound = FALSE;
	CString strDvrName;
	TNELMSG_GRAM TnelGram;
	DWORD dwDvrIPAddr;
	WORD  wDvrPort;
	WPARAM wpara;
	char chDvrName[NAME_BUF_LENGTH],chCltName[NAME_BUF_LENGTH];

	int count = m_CltDescArray.GetSize();
	DWORD dwStartWorking = GetTickCount();
	while (count > 0) {
		if((GetTickCount()-dwStartWorking) > MAX_WORK_TIME) {
			//// 工作时间太长，要处理别的事务
			TRACE("Dealwith Client Gram too long, maybe attack happening");
			break;
		}
		EnterCriticalSection(&m_csGram);
		cltDesc = m_CltDescArray[0];
		m_CltDescArray.RemoveAt(0);
		LeaveCriticalSection(&m_csGram);
		count = m_CltDescArray.GetSize();
		if(!LegalCltReq(&cltDesc.cltGram,chDvrName,chCltName)) {
			continue; //// 格式错误	
		}
		/*
		if(cltDesc.cltGram.chDstName[NAME_BUF_LENGTH-1] != 0 ||
			cltDesc.cltGram.chName[NAME_BUF_LENGTH-1] != 0 || 
			TUNNEL_REQUIRE != cltDesc.cltGram.byAct) {
			continue; //// 格式错误	
		}
		*/
		EnterCriticalSection(&m_csDvr);
		nNetSDVRcount = m_NetSDVRArray.GetSize();
		bFound = FALSE;
		strDvrName = chDvrName;
		int nDvrIndex = SearchDVR(strDvrName);
		/*
		for(int i = 0; i < nNetSDVRcount; i++) {
			if(m_NetSDVRArray[i]->IsMyself(strDvrName)) {
				bFound = TRUE;
				break;
			}
		}
		*/
		if(-1 != nDvrIndex) {
			m_NetSDVRArray[nDvrIndex]->FetchCltTunnelInfo(&TnelGram);
			//// 向客户端发送打洞报文
			EnterCriticalSection(&g_csSocket);
			/*
			//// 对IE里不能做NVR连接的暂时性屏蔽/////
			if(0x5A != cltDesc.cltGram.chReserved[0] && FOURCC_NDVR == TnelGram.dwReserved) {
				TnelGram.byAct = TUNNEL_ACK_NODEVICE;
			}
			*/
			sendto(m_sSendGram,(char*)&TnelGram,sizeof(TNELMSG_GRAM),0,(SOCKADDR*)&cltDesc.addr,sizeof(SOCKADDR_IN));
			WaitForSockWroten(m_sSendGram,3);
			LeaveCriticalSection(&g_csSocket);
			if(TUNNEL_ACK_SUCCESS == TnelGram.byAct && TUNNEL_REQUIRE == cltDesc.cltGram.byAct) {
				m_NetSDVRArray[nDvrIndex]->FetchPosition(&dwDvrIPAddr,&wDvrPort);
				memset(&dvrAddr,0,sizeof(SOCKADDR_IN));
				dvrAddr.sin_family = AF_INET;
				dvrAddr.sin_addr.s_addr = dwDvrIPAddr;
				dvrAddr.sin_port = wDvrPort; //// htons(wDvrPort); nvcd
				//// 创建报文
				memset(&TnelGram,0,sizeof(TNELMSG_GRAM));
				TnelGram.dwType = FOURCC_HOLE;
				TnelGram.byAct = TUNNEL_REQUIRE;
				nLength = strlen(chCltName);
				PushinRandom(TnelGram.chDstName,chCltName,NAME_BUF_LENGTH,nLength,RANDOM_TRANS_NAMEKEY);
				//// memcpy(TnelGram.chDstName,cltDesc.cltGram.chName,NAME_BUF_LENGTH);
				TnelGram.dwDstIPaddr = (DWORD)cltDesc.addr.sin_addr.s_addr;
				TnelGram.wDstPort = cltDesc.addr.sin_port;
				TRACE("NetS - CltReq:%s,Addr%x",strDvrName,TnelGram.dwDstIPaddr);
				EnterCriticalSection(&g_csSocket);
				sendto(m_sSendGram,(char*)&TnelGram,sizeof(TNELMSG_GRAM),0,(SOCKADDR*)&dvrAddr,sizeof(SOCKADDR_IN));
				WaitForSockWroten(m_sSendGram,3);
				LeaveCriticalSection(&g_csSocket);
				wpara = WPARAM_CLT_WORKING;
			}
			else if(TUNNEL_ASKIPADDR == cltDesc.cltGram.byAct) {
				wpara = WPARAM_CLT_WORKING;
			}
			else {
				wpara = WPARAM_CLT_OFFDEV;
			}
		}
		else {
			memset(&TnelGram,0,sizeof(TNELMSG_GRAM));
			TnelGram.dwType = FOURCC_NETS;
			TnelGram.byAct = TUNNEL_ACK_NODEVICE;
			nLength = strlen(chDvrName);
			PushinRandom(TnelGram.chDstName,chDvrName,NAME_BUF_LENGTH,nLength,RANDOM_TRANS_NAMEKEY);
			//// memcpy(TnelGram.chDstName,cltDesc.cltGram.chDstName,NAME_BUF_LENGTH);
			EnterCriticalSection(&g_csSocket);
			sendto(m_sSendGram,(char*)&TnelGram,sizeof(TNELMSG_GRAM),0,(SOCKADDR*)&cltDesc.addr,sizeof(SOCKADDR_IN));
			WaitForSockWroten(m_sSendGram,3);
			LeaveCriticalSection(&g_csSocket);
			wpara = WPARAM_CLT_NODEVICE;
		}
		LeaveCriticalSection(&m_csDvr);
		/// memcpy(&m_TunnelMsg,&cltDesc.cltGram,sizeof(TNELMSG_GRAM));
		CLTMSG LastMsg;
		memcpy(&LastMsg,&m_TunnelMsg,sizeof(CLTMSG));
		memset(&m_TunnelMsg,0,sizeof(CLTMSG));
		strcpy(m_TunnelMsg.chCltName,chCltName);
		strcpy(m_TunnelMsg.chDvrName,chDvrName);
		m_TunnelMsg.dwCltAddr = cltDesc.addr.sin_addr.s_addr;

		//////// 检查是否是相同的请求 ////////////
#if 1
		BOOL bSameReq = FALSE;
		if(0 == strcmp(m_TunnelMsg.chCltName,LastMsg.chCltName) && 
			0 == strcmp(m_TunnelMsg.chDvrName,LastMsg.chDvrName) &&
			m_TunnelMsg.dwCltAddr == LastMsg.dwCltAddr) {
			bSameReq = TRUE;
		}
#else
		BOOL bSameReq = TRUE;
		int i;
		PBYTE pLast,pCur;
		pLast = (PBYTE)&LastMsg;
		pCur = (PBYTE)&m_TunnelMsg;

		for(i = 0; i < sizeof(CLTMSG); i++) {
			if(*pLast != *pCur) {
				bSameReq = FALSE;
				break;
			}
			pLast++;
			pCur++;
		}
#endif
		if(bSameReq) {
			if(wpara != m_LastCltGetStatus) {
				bSameReq = FALSE;
			}
		}
		///////// 检查完毕 /////////////////////
		if(!bSameReq) {
			::SendMessage(m_hMsgWnd,WM_CLIENT_REQUEST,wpara,(LPARAM)&m_TunnelMsg);
			m_LastCltGetStatus = wpara;
		}
	}
	return 1;
}

UINT CNetSManager::NormalDVRChecking()
{
	BOOL bDvrStatusChanged = FALSE;
	BOOL bDvrPositionChanged = FALSE;
	WPARAM wparm;
	BYTE byDvrStatus;
	EnterCriticalSection(&m_csDvr);
	int count = m_NetSDVRArray.GetSize();
	for(int i = 0; i < count; i++) {
		if(m_NetSDVRArray[i]->SelfChanged(&bDvrStatusChanged)) {
			bDvrPositionChanged = TRUE;
		}
		if(bDvrStatusChanged) {
			m_NetSDVRArray[i]->FetchDVRInfo(&m_DVRMsg);
			byDvrStatus = m_NetSDVRArray[i]->GetStatus();
			wparm = DVR_Online == GETACTIVE(byDvrStatus) ? WPARAM_DVR_ONLINE : WPARAM_DVR_OFFLINE;
			//// TRACE("NormalDVRChecking: ");
			::SendMessage(m_hMsgWnd,WM_DVR_CHANGED,wparm,(LPARAM)&m_DVRMsg);
		}
	}
	LeaveCriticalSection(&m_csDvr);
	if(bDvrPositionChanged) {
		//// WriteDVRListFile(); //// 这样存储没必要
	}

	return 1;
}

UINT CNetSManager::RcvGramProc(LPVOID p)
{
	UINT uRe;
	CNetSManager *pManager = (CNetSManager*)p;
	uRe = pManager->KeepRcving();
	pManager->DecreaseThreadCount();

	return uRe;
}

UINT CNetSManager::HandleGramProc(LPVOID p)
{
	UINT uRe;
	CNetSManager *pManager = (CNetSManager*)p;
	uRe = pManager->CheckGram();
	pManager->DecreaseThreadCount();

	return uRe;
}

UINT CNetSManager::CheckDVRProc(LPVOID p)
{
	UINT uRe;
	CNetSManager *pManager = (CNetSManager*)p;
	uRe = pManager->CheckDVR();
	pManager->DecreaseThreadCount();

	return uRe;
}
