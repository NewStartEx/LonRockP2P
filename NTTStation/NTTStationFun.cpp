#include "stdafx.h"
#include "NTTStationFun.h"
#include "Global.h"
#include "DVRGlobal.h"
#include "DVRManager.h"
#include "HttpHandle.h"
#include "ChannelInfo.h"

CDVRManager *g_pManager = NULL; 

CONNECTEDFROMCLIENT g_ConnectedFromClient = NULL;
RECVFROMCLIENT g_RecvFromClient = NULL;
DISCONNECTEDFROMCLIENT g_DisconnectedFromClient = NULL;

PRESTARTSTREAMPROC g_PreStartStreamProc = NULL;
POSTSTOPSTREAMPROC g_PostStopStreamProc = NULL;
ASKFORAUDIOSAMPLE  g_AskForAudioSample = NULL;

CString g_strNetS;
NTTSTATION_API void NstnSetNServerName(LPCTSTR NetServer)
{
	g_strNetS = NetServer;
}

NTTSTATION_API BOOL NstnStartWorking(DWORD* dwDevID,WORD wWebPort,DWORD dwType)
{
	WSADATA wsaData;
	
	WORD wVersionRequested = MAKEWORD(2, 2);
	int nResult = WSAStartup(wVersionRequested, &wsaData);
	if (nResult != 0) {
	////	AfxMessageBox("不能启动网络版本低");
		return FALSE;
	}
	
	if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2)
	{
	////	AfxMessageBox("网络版本低");
		WSACleanup();
		return FALSE;
	}
	if(DEV_TYPE_NDVR == dwType)
		g_pManager = new CDVRManager(FOURCC_NDVR,dwDevID);
	else
		g_pManager = new CDVRManager(FOURCC_CARD,dwDevID); 
	char chPath[MAX_PATH];
////	GetSystemDirectory(chPath,MAX_PATH);
	strcpy(chPath,"C:");
#if FIXNETS
	if(!g_pManager->StartWorking(chPath,NETS_NAME)) {
#else
	/************************************************************************/
	if(g_strNetS.IsEmpty()) {
		g_strNetS = NETS_NAME;
	}
	/************************************************************************/
	if(!g_pManager->StartWorking(chPath,g_strNetS)) {
#endif
		delete g_pManager;
		g_pManager = NULL;
		return FALSE;
	}
	//// 这里创建发送数据的资源

	CHandleHttp::m_hChgPort = CreateEvent(NULL,TRUE,FALSE,NULL);
	CHandleHttp::m_hFinish = CreateEvent(NULL,TRUE,FALSE,NULL);
	CHandleHttp::m_wWebPort = 0==wWebPort ? HTTP_PORT : wWebPort;
	DWORD dwID;
	CreateThread(NULL,0,(LPTHREAD_START_ROUTINE)CHandleHttp::HttpServerProc,NULL,0,&dwID);

	InitLogFile();
	return TRUE;
}

NTTSTATION_API void NstnStopWorking()
{
////	FILE * hf = fopen("d:\\testntt.txt","w");
	SetEvent(CHandleHttp::m_hFinish);
////	fprintf(hf,"Before stop ntt ");
	while (CHandleHttp::IsWorking()) {
		Sleep(50);
	}
////	fprintf(hf,"Http Web quit");
	CloseHandle(CHandleHttp::m_hChgPort);
	CloseHandle(CHandleHttp::m_hFinish);
	if(g_pManager != NULL) {
////		fprintf(hf,"stop manager");
		g_pManager->StopWorking();
////		fprintf(hf,"delete manager");
		delete g_pManager;
		g_pManager = NULL;
	}
////	fprintf(hf,"delete channel");
	for(int i = 0; i < g_ChanInfoArray.GetSize(); i++) {
		delete g_ChanInfoArray[i];
	}
	g_ChanInfoArray.RemoveAll();
	//// 这里清除发送数据的资源
////	fprintf(hf,"start to cleanup");
	WSACleanup();
////	fprintf(hf,"WSA cleanup");
////	fclose(hf);
}

NTTSTATION_API void NstnChangeWebPort(WORD wWebPort)
{
	CHandleHttp::m_wWebPort = wWebPort;
	SetEvent(CHandleHttp::m_hChgPort);
}

NTTSTATION_API BOOL NstnGetLocalInfo(ULONG *IPAddr,WORD *wNTTPort,WORD *wWebPort,UINT *NstnStatus,char *chNstnName)
{
	char name[256];
	HOSTENT  *phostent;
	gethostname(name,256);
	phostent = gethostbyname(name);
	if(NULL == phostent) {
		*IPAddr = 0;
		return FALSE;
	}
	memcpy(IPAddr,phostent->h_addr_list[0],phostent->h_length);
	*wWebPort = CHandleHttp::m_wWebPort;
	if(NULL == g_pManager) {
		*wNTTPort = 0;
		return FALSE;
	}
	*wNTTPort = g_pManager->GetDvrPort();
	*NstnStatus = g_pManager->GetRegisterStatus();
	CString strGlobalName = g_pManager->GetGlobalName();
	strcpy(chNstnName,strGlobalName);
	return TRUE;
}

NTTSTATION_API UINT NstnApplyNetServer(LPCTSTR NstnName,LPCTSTR NstnPassword,Apply_type type)
{
	if(NULL == g_pManager) return NTT_RES_UNINIT;
	if(Apply_Update != type) {
		if(!g_pManager->SetGlobalName(NstnName,NstnPassword)) return NTT_STRFORMAT_ERROR;
	}
	UINT uRe;
	switch(type) {
	case Apply_Register:
		uRe = g_pManager->RegisterUnit();
		break;
	case Apply_Update:
		uRe = g_pManager->ManuUpdate();
		break;
	case Apply_Wakeup:
		uRe = g_pManager->WakeupUnit();
		break;
	case Apply_Delete:
		uRe = g_pManager->UnregisterUnit();
		break;
	default:
		uRe = SENDLINK_ILLEGAL;
	}

	return uRe;
}

NTTSTATION_API BOOL NstnResetLastNstnName(char *chNstnName)
{
	if(NULL == g_pManager) return FALSE;
	CString strLastlName;
	if(!g_pManager->ResetLastGlobalName(strLastlName)) 
		return FALSE;
	strcpy(chNstnName,strLastlName);
	
	return TRUE;
}

NTTSTATION_API void NstnEnableMsgNotify(HWND hMsgWnd,BOOL enable)
{
	if(NULL == g_pManager) return;
	g_pManager->EnableMsgWnd(hMsgWnd,enable);
}

NTTSTATION_API BOOL NstnInitDataChannel(int nChanCount)
{
	if(g_ChanInfoArray.GetSize() != 0) return FALSE;
	ASSERT(nChanCount > 0);

	CChannelInfo *pChan;
	for(int i = 0; i < nChanCount; i++) {
		pChan = new CChannelInfo(i);
		g_ChanInfoArray.Add(pChan);
	}
	return TRUE;
}

NTTSTATION_API BOOL NstnEnableVideoTransfer(HANDLE hClient,BOOL bEnable,BOOL bInCallback)
{
	if(NULL == g_pManager) return FALSE;
	g_pManager->EnableVideoTransfer(hClient,bEnable,bInCallback);
	return TRUE;
}

NTTSTATION_API BOOL NstnEnableAudioTransfer(HANDLE hClient,BOOL bEnable,BOOL bInCallback)
{
	if(NULL == g_pManager) return FALSE;
	g_pManager->EnableAudioTransfer(hClient,bEnable,bInCallback);
	return TRUE;
}

NTTSTATION_API BOOL NstnPushVideoSample( int channel,    
            /* [in,out] */ CMPR_SAMPLE *psample,
            /* [in] */ PVOID pLastIFrmBuffer,
            /* [in] */ PVOID pCurFrmBuffer)
{
	if(channel >= g_ChanInfoArray.GetSize())
		return FALSE;

	//// TRACE("Push Video Sample");
	g_ChanInfoArray[channel]->CopySampleEx(psample,pLastIFrmBuffer,pCurFrmBuffer);
	return TRUE;
}

NTTSTATION_API BOOL NstnPushAudioSample(int channel, 
					 /* [in] */ ULONG nLength,
					 /* [in] */ PVOID pSampleBuffer)
{
	return TRUE;
}

NTTSTATION_API void NstnSetStatusCallback(CONNECTEDFROMCLIENT ConnectedFromClient,RECVFROMCLIENT RecvFromClient,DISCONNECTEDFROMCLIENT DisconnectedFromClient)
{
	g_ConnectedFromClient = ConnectedFromClient;
	g_RecvFromClient = RecvFromClient;
	g_DisconnectedFromClient = DisconnectedFromClient;
}

NTTSTATION_API void NstnSetStreamCallback(PRESTARTSTREAMPROC PreStartStreamProc,POSTSTOPSTREAMPROC PostStopStreamProc,ASKFORAUDIOSAMPLE AskForAudioSample)
{
	g_PreStartStreamProc = PreStartStreamProc;
	g_PostStopStreamProc = PostStopStreamProc;
	g_AskForAudioSample = AskForAudioSample;
}

NTTSTATION_API BOOL NstnDismissClient(HANDLE hClient,BOOL bInCallback)
{
	if(NULL == g_pManager) return FALSE;
	g_pManager->SetRemoveMark(hClient,bInCallback);
	return TRUE;
}
