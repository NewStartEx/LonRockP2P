//// 2009-12-22: 操作函数增加了 bInCallback的变量，意为在已经找到的DVR回调函数里操作，不使用互斥量

#include "stdafx.h"
#include "NTTClientFun.h"
#include "NTTProtocol.h"
#include "Global.h"
#include "CltGlobal.h"
#include "CltManager.h"
#include "MemVicocDecFun.h"

CCltManager *g_pManager = NULL;

STATIONLINKED g_StationLinked = NULL;
RECVFROMSTATION g_RecvFromStation = NULL;
STATIONLEFT g_StationLeft = NULL;

CString g_strNetS;
NTTCLIENT_API void NcltSetNServerName(LPCTSTR NetServer)
{
	g_strNetS = NetServer;
}

NTTCLIENT_API NTTCLT_ERROR_CODE NcltStartUp(HWND hVidWnd)
{
	WSADATA wsaData;
	
	WORD wVersionRequested = MAKEWORD(2, 2);
	int nResult = WSAStartup(wVersionRequested, &wsaData);
	if (nResult != 0)
		return NTTCLT_NETINIT_ERR;
	
	if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2)
	{
		WSACleanup();
		return NTTCLT_NETINIT_ERR;
	}
	g_pManager = new CCltManager;
#if FIXNETS
	if(!g_pManager->StartWorking(NETS_NAME)) {
#else
	/************************************************************************/
	if(g_strNetS.IsEmpty()) {
		g_strNetS = NETS_NAME;
	}
	/************************************************************************/
	if(!g_pManager->StartWorking(g_strNetS)) {
#endif
		delete g_pManager;
		g_pManager = NULL;
		return NTTCLT_RES_ERR;
	}
	if(MEMVICO_OK != InitMemVicoc(hVidWnd)) {
		return NTTCLT_RES_ERR;
	}

	return NTTCLT_OK;
}

NTTCLIENT_API void NcltCleanUp()
{
	if(NULL != g_pManager) {
		g_pManager->StopWorking();
		delete g_pManager;
		g_pManager = NULL;
	}
	ReleaseMemVicoc();
	WSACleanup();
}

NTTCLIENT_API void NcltSetCallback(STATIONLINKED StationLinked, RECVFROMSTATION RecvFromStation, STATIONLEFT StationLeft)
{
	g_StationLinked = StationLinked;
	g_RecvFromStation = RecvFromStation;
	g_StationLeft = StationLeft;
}

NTTCLIENT_API NTTCLT_ERROR_CODE NcltConnectStation(LPCTSTR StationName,UINT uLinkMode,PNETPACKET pCmd,WORD wDataPort)
{
	if(NULL == g_pManager) 
		return NTTCLT_RES_ERR;
	if(LINK_TUNNEL == uLinkMode) {
		CString strName = StationName;
		if(strName.IsEmpty()) return NTTCLT_FORMAT_ERR;
		for(int i = 0; i < strName.GetLength(); i++) {
			if(' '==strName[i] || '.'==strName[i] || '*'==strName) {
				return NTTCLT_FORMAT_ERR;
			}
		}
	}
	BYTE Err;
	ASSERT(sizeof(NETPACKET) == sizeof(CMDINFO));
	if(NULL == g_pManager->StartLink(StationName,uLinkMode,(PCMDINFO)pCmd,wDataPort,Err)) {
		if(OBJECT_NETS_ERR == Err) return NTTCLT_SERVER_ERR;
		else return NTTCLT_FORMAT_ERR;  //// 这时候肯定是名称错误造成的
	}

	return NTTCLT_OK;
}

NTTCLIENT_API void NcltAbandonStation(LPCTSTR StationName,BOOL bInCallback)
{
	if(NULL == g_pManager) return;
	g_pManager->StopLink(StationName,bInCallback);

}

NTTCLIENT_API NTTCLT_ERROR_CODE NcltCreateVideoResource(LPCTSTR StationName,int nChanCount,BOOL bInCallback)
{
	if(NULL == g_pManager) 
		return NTTCLT_RES_ERR;
	if(!g_pManager->CreateVideoResource(StationName,nChanCount,bInCallback)) {
		return NTTCLT_CONNECT_ERR;
	}
	else {
		return NTTCLT_OK;
	}
}

//// 返回-1说明还没有连接到Station
NTTCLIENT_API int NcltGetChannelCount(LPCTSTR StationName,BOOL bInCallback)
{
	int nCount;
	if(NULL == g_pManager)
		return -1;
	nCount = g_pManager->GetVideoChannelCount(StationName,bInCallback);
	return nCount;
}

NTTCLIENT_API NTTCLT_ERROR_CODE NcltSendCmd(LPCTSTR StationName,PNETPACKET pCmd,BOOL bInCallback)
{
	if(NULL == g_pManager) return NTTCLT_RES_ERR;
	if(!g_pManager->SendCmd(StationName,(PCMDINFO)pCmd,bInCallback)) {
		return NTTCLT_CONNECT_ERR;
	}
	
	return NTTCLT_OK;
}

NTTCLIENT_API NTTCLT_ERROR_CODE NcltStartVideoTran(LPCTSTR StationName,int chan,BOOL bInCallback)
{
	if(NULL == g_pManager) return NTTCLT_RES_ERR;
	if(!g_pManager->StartVideoTran(StationName,chan,bInCallback)) {
		return NTTCLT_CONNECT_ERR;
	}
	return NTTCLT_OK;
}

NTTCLIENT_API NTTCLT_ERROR_CODE NcltStartAllVideoTran(LPCTSTR StationName,BOOL bInCallback)
{
	if(NULL == g_pManager) return NTTCLT_RES_ERR;
	if(!g_pManager->StartAllVideoTran(StationName,bInCallback)) {
		return NTTCLT_CONNECT_ERR;
	}
	return NTTCLT_OK;
}

NTTCLIENT_API NTTCLT_ERROR_CODE NcltStartAudioTran(LPCTSTR StationName,int chan,BOOL bInCallback)
{
	return NTTCLT_OK;
}

NTTCLIENT_API NTTCLT_ERROR_CODE NcltStopVideoTran(LPCTSTR StationName,int chan,BOOL bInCallback)
{
	if(NULL == g_pManager) return NTTCLT_RES_ERR;
	if(!g_pManager->StopVideoTran(StationName,chan,bInCallback)) {
		return NTTCLT_CONNECT_ERR;
	}
	return NTTCLT_OK;
}

NTTCLIENT_API NTTCLT_ERROR_CODE NcltStopAudioTran(LPCTSTR StationName,int chan,BOOL bInCallback)
{
	return NTTCLT_OK;
}

NTTCLIENT_API NTTCLT_ERROR_CODE NcltStopAllVideoTran(LPCTSTR StationName,BOOL bInCallback)
{
	if(NULL == g_pManager) return NTTCLT_RES_ERR;
	if(!g_pManager->StopAllVideoTran(StationName,bInCallback)) {
		return NTTCLT_CONNECT_ERR;
	}
	return NTTCLT_OK;
}

NTTCLIENT_API NTTCLT_ERROR_CODE NcltChangeVideoWnd(HWND hVidWnd)
{
	if(MEMVICO_OK != ChangeVideoWnd(hVidWnd)) {
		return NTTCLT_RES_ERR;
	}
	return NTTCLT_OK;
}

NTTCLIENT_API NTTCLT_ERROR_CODE NcltSetVideoPos(LPCTSTR StationName,int chan,int left,int top,int width,int height,BOOL bInCallback)
{
	if(NULL == g_pManager) return NTTCLT_RES_ERR;
	if(!g_pManager->SetVideoPos(StationName,chan,left,top,width,height,bInCallback)) {
		return NTTCLT_CONNECT_ERR;
	}
	return NTTCLT_OK;
}

NTTCLIENT_API NTTCLT_ERROR_CODE NcltShowVideo(LPCTSTR StationName,int chan,BOOL bShow,BOOL bInCallback)
{
	if(NULL == g_pManager) return NTTCLT_RES_ERR;
	if(!g_pManager->ShowVideo(StationName,chan,bShow,bInCallback)) {
		return NTTCLT_CONNECT_ERR;
	}
	return NTTCLT_OK;
}

NTTCLIENT_API NTTCLT_ERROR_CODE NcltRefreshVideo(LPCTSTR StationName,int chan,BOOL bInCallback)
{
	if(NULL == g_pManager) return NTTCLT_RES_ERR;
	if(!g_pManager->RefreshVideo(StationName,chan,bInCallback)) {
		return NTTCLT_CONNECT_ERR;
	}
	return NTTCLT_OK;
}

NTTCLIENT_API NTTCLT_ERROR_CODE NcltStartRecord(LPCTSTR StationName,int chan,LPCTSTR FileName,BOOL bInCallback)
{
	if(NULL == g_pManager) return NTTCLT_RES_ERR;
	if(!g_pManager->StartRecord(StationName,chan,FileName,bInCallback)) {
		return NTTCLT_RECORD_ERR;
	}
	return NTTCLT_OK;
}

NTTCLIENT_API NTTCLT_ERROR_CODE NcltStopRecord(LPCTSTR StationName,int chan,BOOL bInCallback)
{
	if(NULL == g_pManager) return NTTCLT_RES_ERR;
	if(!g_pManager->StopRecord(StationName,chan,bInCallback)) {
		return NTTCLT_RECORD_ERR;
	}
	return NTTCLT_OK;
}

NTTCLIENT_API NTTCLT_ERROR_CODE NcltSnap(LPCTSTR StationName,int chan,LPCTSTR FileName,BOOL bInCallback)
{
	if(NULL == g_pManager) return NTTCLT_RES_ERR;
	if(!g_pManager->Snap(StationName,chan,FileName,bInCallback)) {
		return NTTCLT_SNAP_ERR;
	}
	return NTTCLT_OK;
}

NTTCLIENT_API DWORD	NcltGetRecFilePos(LPCTSTR StationName,int chan,BOOL bInCallback)
{
	if(NULL == g_pManager) return 0;
	return g_pManager->GetRecFilePos(StationName,chan,bInCallback);
}