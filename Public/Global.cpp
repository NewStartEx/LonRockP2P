#include "stdafx.h"
#include "Global.h"

BOOL g_bLogMessage = FALSE;

BOOL UncaseCompare(LPCTSTR buf1,LPCTSTR buf2)
{
	CString str1,str2;
	str1 = buf1;
	str2 = buf2;
	str1.MakeUpper();
	str2.MakeUpper();
	return (str1 == str2);
}

void PushinRandom(char *pDstBuf,char *pSrcBuf,int nBufLength,int nSrcLength,int nRandIndex)
{
	ASSERT(nSrcLength > nRandIndex);
	srand(GetTickCount());
	BYTE byRandom = BYTE(rand()&0xFF);
	for(int i = 0; i < nBufLength; i++) {
		if(i < nRandIndex) {
			pDstBuf[i] = ((pSrcBuf[i]+byRandom)&0xFF);
		}
		else if(i == nRandIndex) {
			pDstBuf[i] = byRandom;
		}
		else if((nBufLength-1) == i) {
			pDstBuf[i] = BYTE((nSrcLength+byRandom)&0xFF);
		}
		else if(i <= nSrcLength) {
			pDstBuf[i] = ((pSrcBuf[i-1]+byRandom)&0xFF);
		}
		else {
			pDstBuf[i] = BYTE(rand());
		}
	}
}

BOOL ThrowRandom(char *pDstBuf,char *pSrcBuf,int *pDstLength,int nBufLength,int nSrcLength,int nRandIndex)
{
	ASSERT(nBufLength>nRandIndex && nSrcLength>nRandIndex);
	//// 这里确认 nLen 一定小于127才可以这样使用
	int nLen = (((pSrcBuf[nSrcLength-1]|0x100)-pSrcBuf[nRandIndex])&0xFF);
	if(nLen > (nSrcLength-2)) return FALSE;  //// 至少还有一个随机数
	if(0==nLen) return FALSE;
	memset(pDstBuf,0,nBufLength);
	for(int i = 0; i <= nLen; i++) {
		if(i < nRandIndex) {
			pDstBuf[i] = (((pSrcBuf[i]|0x100)-pSrcBuf[nRandIndex])&0xFF);
		}
		else if(i > nRandIndex) {
			pDstBuf[i-1] = (((pSrcBuf[i]|0x100)-pSrcBuf[nRandIndex])&0xFF);
		}
	}
	*pDstLength = nLen;
	return TRUE;
}

int WaitForSockWroten(SOCKET s, long nTo)
{
	int nRe;
	fd_set  WriteSet;
	TIMEVAL timeout;
	timeout.tv_sec = nTo;
	timeout.tv_usec = 0;
	FD_ZERO(&WriteSet);
	FD_SET(s,&WriteSet);
	
	nRe = select(0,NULL,&WriteSet,NULL,&timeout);
	return nRe;
}

ULONG Name2IP(LPCTSTR strName)
{
	ULONG uIP = 0;
	HOSTENT *host;

	uIP = inet_addr(strName);
	if (uIP==INADDR_NONE) 
	{	host=gethostbyname(strName);
		if (host) 
		{	uIP=*((ULONG *)host->h_addr_list[0]);
		}
	}
	return uIP;
}


int TryToSend( SOCKET s, const char *buf, int len ,int flag)
{
	TIMEVAL timeout;
	fd_set  WriteSet;
	int RealSend;
	int	Re,err;

	const char *lp = buf;
	int  length = len;
	
	do{
		timeout.tv_sec = 15;
		timeout.tv_usec = 0;
		
		FD_ZERO(&WriteSet);
		FD_SET(s,&WriteSet);
		
		Re = select(0,NULL,&WriteSet,NULL,&timeout);
		if(Re < 0)
		{
			err = WSAGetLastError();
			TRACE("Write Select Error: [%d]",err);
			if(WSAEWOULDBLOCK == err)
			{
				Sleep(10);
				continue;
			}
			else
				return Re;
		}
		else if(0 == Re)
		{
			TRACE("send time out\n");
			//// return (lp-buf);
			return Re;
		}
#if 1
		RealSend = length > 1024 ? 1024 : length;
		Re = send(s,lp,RealSend,0);
#else
		Re = send(s,lp,length,0);
#endif
		if(Re < 0)
		{
			err = WSAGetLastError();
			TRACE("send error: [%d]",err);
			return Re;
		}
		lp += Re;
		length -= Re;
	} while(length >0);

	return len;  // all the data is sent
	
}

CString GetCurrentPath()
{
	char pathName[MAX_PATH]; 
	DWORD l = GetModuleFileName( (HMODULE)NULL, pathName,MAX_PATH );
	for(int i = strlen(pathName); i > 2 ; i--) {
		if(pathName[i] == '\\')	{
			pathName[i] = '\0';
			break;
		}
	}
	CString WholeName = pathName;
	return WholeName;
}

#ifndef NTTCLIENT_LIB
void InitLogFile()
{

	FILE		*MsgFlag = NULL;

	if(NULL == (MsgFlag=fopen("C:\\ShowNttMsg.dat","r")))
	{
		g_bLogMessage = FALSE;
		return;
	}
	else
		g_bLogMessage = TRUE;
	fclose(MsgFlag);
}

#define MAX_STRING_LENGTH	1024
void MessageLogEx(char *filename,BOOL showTime,char *str255,...)
{
	char pszLogString[MAX_STRING_LENGTH];
	FILE		*MsgLogFile = NULL;
	CTime curTime = CTime::GetCurrentTime();

	if(!g_bLogMessage)
		return;
	if(NULL == (MsgLogFile = fopen( filename, "a" )))
		return;
	
	va_list DP;
	va_start(DP, str255);
	_vsnprintf( pszLogString, MAX_STRING_LENGTH, str255, DP );

	if (showTime)
		fprintf(MsgLogFile, "%s, Time is %s\n", pszLogString, curTime.Format("%Y-%m-%d %H-%M-%S"));
	else
		fprintf(MsgLogFile, "%s\n", pszLogString);
////	fflush( MsgLogFile );
	fclose( MsgLogFile );

}
#endif

CSelfLock::CSelfLock(LPCRITICAL_SECTION pcs)
{
	m_pcs = pcs;
	EnterCriticalSection(m_pcs);
}

CSelfLock::~CSelfLock()
{
	LeaveCriticalSection(m_pcs);
}
