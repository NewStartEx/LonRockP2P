#include "StdAfx.h"
#include "HttpHandle.h"
#include "Global.h"

#define	DVRMAINCHN_NAME	"DVRMainChn.html"
#define DVRMAINCHT_NAME "DVRMainCht.html"
#define	DVRMAINENG_NAME	"DVRMainEng.html"

static	const char *normalHead = 
{
		"HTTP/1.1 200 OK\nContent-Length: %d\nConnection: close\n\n"
};

static	const char *errHead = 
{
		"HTTP/1.1 404 OK\n\n"
};

static const char *LanTitle = 
{
	"Accept-Language:"
};

static const char *URLName = 
{
	"Host: "
};

#define HTML_TYPE_COUNT	3
static const char *html[HTML_TYPE_COUNT] =
{
	{"htm"},
	{"asp"},
	{"xml"}
};

#define MAX_BUFFER 256
#define REQUEST_LENGTH 1024
#define MAX_HTTP_CONNECTION 5
#define URL_LENGTH 512

static char URL[URL_LENGTH];
static char dataline[80];

SOCKET CHandleHttp::m_sConnection = INVALID_SOCKET;
WORD   CHandleHttp::m_wWebPort = HTTP_PORT;	
HANDLE CHandleHttp::m_hFinish = NULL;
HANDLE CHandleHttp::m_hChgPort = NULL;

#define SAFE_DELETE(x) {if(x != NULL) {delete x; x = NULL;}}

UINT CHandleHttp::HttpServerProc(LPVOID p)
{
	int on = 1;
	SOCKADDR_IN servaddr; // WINDOWS' STYLE
	CHandleHttp * pHandle = NULL;

	m_sConnection = socket(AF_INET, SOCK_STREAM, 0);
	setsockopt(m_sConnection,SOL_SOCKET,SO_REUSEADDR,(CHAR*)&on,sizeof(on));
//	setsockopt(serverFd,SOL_SOCKET,SO_LINGER,(CHAR*)&on,sizeof(on));

	memset(&servaddr,0,sizeof(servaddr));

	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons(m_wWebPort);

	if(SOCKET_ERROR == bind(m_sConnection, (SOCKADDR *)&servaddr, sizeof(servaddr))) {
		closesocket(m_sConnection);
		m_sConnection = INVALID_SOCKET;
		return 1;
	}

	listen(m_sConnection,MAX_HTTP_CONNECTION);

	HANDLE hEvent[3];
	hEvent[0] = CreateEvent(NULL,FALSE,FALSE,NULL);
	hEvent[1] = m_hChgPort;
	hEvent[2] = m_hFinish;

	if(SOCKET_ERROR == (WSAEventSelect(m_sConnection,hEvent[0],FD_ACCEPT)))
	{
		MessageBox(NULL,"Network Error Happen!","Error",MB_OK);
		closesocket(m_sConnection);
	}
	
	while(1) 
	{
		DWORD wRe = WSAWaitForMultipleEvents(3,hEvent,FALSE,WSA_INFINITE,FALSE);
		if(WSA_WAIT_EVENT_0 == wRe)
		{
			if(NULL == (pHandle = new CHandleHttp))
			{
				MessageBox(NULL,"Memory resource is low!","danger!",MB_OK);
				closesocket(m_sConnection);
				return 0;
			}
			*pHandle = accept(m_sConnection,NULL,NULL);
			
			if(pHandle->GetSocket() <= 0)
			{
				SAFE_DELETE( pHandle );
				continue;
			}
			pHandle->HandleReq();
			SAFE_DELETE( pHandle );
		}
		else if(WSA_WAIT_EVENT_0 + 1 == wRe) {
			TRACE("Http Port Changed");
			ResetEvent(hEvent[1]);
			closesocket(m_sConnection);
			m_sConnection = INVALID_SOCKET;
			DWORD threadID;
			::CreateThread(NULL,0,(LPTHREAD_START_ROUTINE)HttpServerProc,NULL,0,&threadID);
			return 0;
		}
		else {
			TRACE("Http Thread End");
			closesocket(m_sConnection);
			m_sConnection = INVALID_SOCKET;
			return 0;
		}
		
	}
	CloseHandle(hEvent[0]);
	return 0;
}

CHandleHttp::CHandleHttp() 
{
	InitContent();
	m_hNetEvent[0] = CreateEvent(NULL,FALSE,FALSE,NULL);
	m_hNetEvent[1] = m_hFinish;
}

CHandleHttp::~CHandleHttp() 
{
	closesocket(m_sClient);
	//// m_nClients--;
	CloseHandle(m_hNetEvent[0]);
}

void CHandleHttp::InitContent()
{
	int i;
	for(i = 0; i < MAX_DYNAMIC_CONTENT; i++)
	{
		dynamicContent[i].variableName[0] = 0;
		dynamicContent[i].pfunc = NULL;
	}

	addDynamicContent("address",&addressFunc);
	addDynamicContent("ip",&ipFunc);
}

int CHandleHttp::addDynamicContent(char *name, char*(*function)())
{
	int i;
	/* first, ensure that the 'name' does not exist in the 
	* current list.
	*/
	for( i = 0; i < MAX_DYNAMIC_CONTENT; i++ )
	{
		if(dynamicContent[i].variableName[0] != 0)
		{
			if(!strcmp(name,dynamicContent[i].variableName))
			{
				return -1;
			}
		}
	}
	
	/* next ,look for an empty slot */
	for( i = 0; i < MAX_DYNAMIC_CONTENT; i++ )
	{
		if(dynamicContent[i].variableName[0] == 0)
		{
			strncpy(dynamicContent[i].variableName,name,strlen(name)); // 80 ???
			dynamicContent[i].pfunc = function;
			break;
		}
	}
	return 0;
}

void CHandleHttp::getDynamicContent(char* name, char *content)
{
	int j,i;
	
	for(j = 0; j < MAX_DYNAMIC_CONTENT; j++)
	{
	/* Search for the name in the list, avoid '.' trailing
	* name
		*/
		for ( i = 0; name[i] != '>'; i++)
		{
			if(dynamicContent[j].variableName[i] != name[i])  // dynamicContent[j] ???
				break;
		}
		if(name[i] == '>')
		{
			/* we've reached the end,so it was a good match */
			strcpy(content,dynamicContent[j].pfunc());  // dynamicContent[j].pfunc()???
			return;
		}
	}
	strcpy(content,"Not Found!");  
}

void CHandleHttp::getFilename(char *inbuf,char *filename, int start)
{
	int i=start,j=0;
	char out[256];
	BOOL bpost = FALSE;
	/*
	* Skip any initial spaces
	*/
	while(inbuf[i] == ' ') i++;
	i++;
	for(; i < strlen(inbuf); i++)
	{
		if(inbuf[i] == ' ')  // finish the first string
		{
			out[j] = 0;
			break;
		}
		out[j++] = inbuf[i];  // put the string into out buffer
		if(255 == j) {
			out[j] = 0;   //// 防止恶意程序从中捣鬼导致溢出
			break;
		}
	}
	if(0 == out[0])  // only root-searching
		strcpy(out,MAINPAGE);
	strcpy(filename,out);
}

BOOL CHandleHttp::IsWebPage(char *filename)
{
	unsigned int i=0;
	char *postfix = filename;
	int length = strlen(filename);
	postfix += (length-1);
	for(i = length-1; i > 0; i--)
	{
		if('.' == *postfix)
		{
			postfix ++;
			break;
		}
		else
		{
			postfix --;
		}
	}
	if(i <= 0)
		return FALSE;
	for(i = 0; i < HTML_TYPE_COUNT; i++) {
		if(0 == strnicmp(postfix,html[i],3))  {
			return TRUE;
		}
	}
	return FALSE;
}

Language CHandleHttp::ParseLanguage(char *inbuf,int size)
{
	char *pTmp = inbuf;
	int i;
	
	for( i = 0; i < size; i++ )
	{
		if(0 == strncmp(pTmp,LanTitle,strlen(LanTitle)))
		{
			pTmp += strlen(LanTitle);
			break;
		}
		pTmp ++;
	}
	
	if( i == size )
		return SChn;  // default is Simple Chinese.
	
	for( ; i< size , *pTmp != '\n'; i++ )
	{
		if( 0 == strncmp(pTmp,"cn",2) || 0 == strncmp(pTmp,"CN",2) )
			return SChn;
		if( 0 == strncmp(pTmp,"tw",2) || 0 == strncmp(pTmp,"TW",2) )
			return TChn;
		pTmp ++;
	}
	return En;
}

char* CHandleHttp::GetURL(char *inbuf,int size)
{
	char *pTmp = inbuf;
	int i,j;

	for( i = 0; i < size; i++ )
	{
		if(0 == strncmp(pTmp,URLName,strlen(URLName)))
		{
			pTmp += strlen(URLName);
			break;
		}
		pTmp ++;
	}

	if( i == size )
		return NULL;  // default is Simple Chinese.

	for( j=0; ( i < size && *pTmp != 0x0D ); i++,j++,pTmp++ )
	{
		URL[j] = *pTmp;
	}
	URL[j] = 0;

	return URL;
}


// Simulate file operation
FILE* CHandleHttp::LookforFile(char* filename,DWORD * fileSize)
{
	HANDLE hFile = CreateFile(filename,GENERIC_READ,FILE_SHARE_READ,
		NULL,OPEN_EXISTING,NULL,NULL);
	if(INVALID_HANDLE_VALUE == hFile)
		return NULL;
	*fileSize = GetFileSize(hFile,NULL);
	CloseHandle(hFile);
	return fopen(filename,"rb");
}
// 
void CHandleHttp::SendMainPage(FILE *fstrm)
{
	char databuffer[MAX_BUFFER],content[256];
	BOOL isdynamic=FALSE;
	int i=0,readsize=0,count=0;
	while(1)
	{
		Sleep(1); //// 使Web缓冲有处理时间
		readsize = fread(databuffer,1,MAX_BUFFER,fstrm);
		
		if(isdynamic)
		{
			getDynamicContent(databuffer,content);
			if(TryToSend(m_sClient,content,strlen(content),0) < 0)
				return;
			isdynamic = FALSE;
			for(i = 0; databuffer[i] != '>'; i++) ;
			count = ++i;
		}
		for(; i < readsize-6; i++)
		{
			if(databuffer[i] == '<')  // 
			{
				if(!strncmp(&databuffer[i+1],"DATA",4))
				{
					fseek(fstrm,i+6-readsize,SEEK_CUR);// the position of dynamic content.
					isdynamic = TRUE;
					break;
				}
			}
		}
		if(TryToSend(m_sClient,&databuffer[count],i-count,0) < 0)
			return;
		if(!isdynamic)
		{
			if(feof(fstrm))
			{
				TryToSend(m_sClient,&databuffer[i],6,0);
				fclose(fstrm);
				break;
			}
			else
				fseek(fstrm,-6,SEEK_CUR);  // backforward for next time
		}
		count = 0;
		i = 0;
	}
}

void CHandleHttp::SendNormalPage(FILE* hf)
{
	char databuffer[MAX_BUFFER];
	int readsize = 0;
	int sentcount = 0;
	DWORD dwSendBegin,dwSendInv=0;
	int nSent = 0;
	while(!feof(hf))
	{
		readsize = fread(databuffer,1,MAX_BUFFER,hf);
		dwSendBegin = GetTickCount();
		if((nSent=TryToSend(m_sClient,databuffer,readsize,0)) < 0)
			return;
		if(nSent < readsize) return;  //// time out
		sentcount += readsize;
		dwSendInv = GetTickCount() - dwSendBegin;
		if(dwSendInv > 100) {
			Sleep(100);
		}
		if(sentcount >= 16384) {
			Sleep(10);
			sentcount = 0;
		}
	}
}

int CHandleHttp::SendGramHead(DWORD FileSize)
{
	char GramHead[1024];
	sprintf(GramHead,normalHead,FileSize);
	return TryToSend(m_sClient,GramHead,strlen(GramHead),0);
}

UINT CHandleHttp::HandleReq()
{
	char databuffer[REQUEST_LENGTH+1],filename[256];
	FILE* fstrm;
	Language lan;
////	WSANETWORKEVENTS netEvents;
	int err;
	
	databuffer[REQUEST_LENGTH] = 0; //// 有时候恶意程序发来的数据超长
	if(SOCKET_ERROR == (WSAEventSelect(m_sClient,m_hNetEvent[0],FD_READ)))
	{
		TRACE("Select Error in Http");
		return 0;
	}

	DWORD wRe = WSAWaitForMultipleEvents(2,m_hNetEvent,FALSE,1000,FALSE);
	if(WSA_WAIT_EVENT_0 != wRe)
	{
		TRACE("Cannot recv!\n");
		return 0;
	}

	//////// set directory to module path //////////////
	char curPathFile[REQUEST_LENGTH]; ////[_MAX_PATH];  防止恶意程序发来超长数据
	GetModuleFileName(NULL,curPathFile,_MAX_PATH);
	for(int i = strlen(curPathFile); i > 2 ; i--)
	{
		if(curPathFile[i] == '\\')
		{
			curPathFile[i] = '\0';
			break;
		}
	}
	strcat(curPathFile,"\\");
	//// SetCurrentDirectory(curPathFile);

	memset(databuffer,0,REQUEST_LENGTH);
	TRACE("Go into Http HandleReq,socket :[%d]",m_sClient);
	err = recv(m_sClient,databuffer,REQUEST_LENGTH,0);
	if(err == -1)
	{
		err = WSAGetLastError();
		TRACE("Recv err [%d]\n",err);
		return 0;
	}
	lan = ParseLanguage(databuffer,REQUEST_LENGTH);  // to know if IE support Chinese
	GetURL(databuffer,REQUEST_LENGTH);

	if(!strncmp(databuffer,"GET",3))
		getFilename(databuffer,filename,4);
	else if(!strncmp(databuffer,"POST",4))
		getFilename(databuffer,filename,5);
	else 
	     {
	     closesocket(m_sClient);
	     return 0;
		}

	TRACE("Get filename: %s \n",filename);

	if(!strcmp(filename,MAINPAGE))
		{
		if(lan == SChn)
			strcpy(filename,DVRMAINCHN_NAME);
		else if(lan == TChn)
			strcpy(filename,DVRMAINCHT_NAME);
		else
			strcpy(filename,DVRMAINENG_NAME);
		}
	strcat(curPathFile,filename);
	DWORD FileSize = 0;
	if(NULL != (fstrm=LookforFile(curPathFile,&FileSize)))
	{
		SendGramHead(FileSize);
		if(IsWebPage(curPathFile)) {
			TRACE("Send MainPage");
			//// if(TryToSend(m_sClient,normalHead,strlen(normalHead),0)>0)
			SendMainPage(fstrm);
		}
		else  //// nvcd
			SendNormalPage(fstrm);

		fclose(fstrm);
	}
	else
	{
		TryToSend(m_sClient,errHead,strlen(errHead),0);
	}
	WaitForSockWroten(m_sClient,5);
	
	int kkk = 0;
	do {
		kkk ++;
		err = recv(m_sClient,databuffer,REQUEST_LENGTH,0);
	} while (err > 0 && kkk < 1000);
	TRACE("Http Graceful closed,recv %d times",kkk);
	return 1;
}


char* addressFunc()
{
	char name[256];
	HOSTENT  *phostent;
	SOCKADDR_IN NetAddr;
	
	gethostname(name,256);
	phostent = gethostbyname(name);
	if(phostent != NULL)
	{
		memcpy(&NetAddr.sin_addr,phostent->h_addr_list[0],phostent->h_length);
	}
	sprintf(dataline,"%d",NetAddr.sin_addr.S_un.S_addr);
	return dataline;
}

char* ipFunc()  // modifying is needed
{
	char name[256];
	char port[16];
	HOSTENT  *phostent;
	SOCKADDR_IN NetAddr;
	unsigned int i;
	
	if(URL[0] == 0)
	{
	gethostname(name,256);
	phostent = gethostbyname(name);
	if(phostent != NULL)
	{
		memcpy(&NetAddr.sin_addr,phostent->h_addr_list[0],phostent->h_length);
	}
	sprintf(dataline,"%d.%d.%d.%d",NetAddr.sin_addr.S_un.S_un_b.s_b1,NetAddr.sin_addr.S_un.S_un_b.s_b2,
		NetAddr.sin_addr.S_un.S_un_b.s_b3,NetAddr.sin_addr.S_un.S_un_b.s_b4);
	}
	else
	{
		//// if IE connect from other port URL will be followed with an port number by ':'
		for(i = 0 ; i < strlen(URL); i++) {
			if(':' == URL[i]) {
				URL[i] = 0;
				break;
			}
		}
		strcpy(dataline,URL);
	}
	if(80 != CHandleHttp::m_wWebPort) // 
	{
		sprintf(port,":%d",CHandleHttp::m_wWebPort);
		strcat(dataline,port);
	}
	return dataline;
}

