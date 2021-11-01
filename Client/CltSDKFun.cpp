#include "stdafx.h"
#include "CltSDKFun.h"

CONNECTEDDVR g_ConnectedDVR = NULL;
RECVFROMDVR g_RecvFromDVR = NULL;
DISCONNECTEDDVR g_DisconnectedDVR = NULL;

void SetCltCallback(CONNECTEDDVR ConnectedDVR,RECVFROMDVR RecvFromDVR,DISCONNECTEDDVR DisconnectedDVR)
{
	g_ConnectedDVR = ConnectedDVR;
	g_RecvFromDVR = RecvFromDVR;
	g_DisconnectedDVR = DisconnectedDVR;
}

