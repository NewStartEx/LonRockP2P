#ifndef _GLOBAL_H
#define _GLOBAL_H

#define FIXNETS		1

class CSelfLock
{
public:
	CSelfLock(LPCRITICAL_SECTION pcs);
	~CSelfLock();
protected:
	LPCRITICAL_SECTION m_pcs;
private:
};
/*
BOOL UncaseCompare(char* buf1,char* buf2);
BOOL UncaseCompare(LPCTSTR buf1,char* buf2);
BOOL UncaseCompare(char* buf1,LPCTSTR buf2);
*/
BOOL UncaseCompare(LPCTSTR buf1,LPCTSTR buf2);

void PushinRandom(char *pDstBuf,char *pSrcBuf,int nBufLength,int nSrcLength,int nRandIndex);
BOOL ThrowRandom(char *pDstBuf,char *pSrcBuf,int *pDstLength,int nBufLength,int nSrcLength,int nRandIndex);
ULONG Name2IP(LPCTSTR strName);
int TryToSend( SOCKET s, const char *buf, int len ,int flag);  // send for nonblock TCP socket
int WaitForSockWroten(SOCKET s, long nTo);
CString GetCurrentPath();
void InitLogFile();
void MessageLogEx(char *filename,BOOL showTime,char *str255,...);
extern BOOL g_bLogMessage;

#endif // _GLOBAL_H