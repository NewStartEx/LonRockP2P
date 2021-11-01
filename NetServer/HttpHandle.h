#ifndef _HANDLEHTTP_H
#define _HANDLEHTTP_H

#define MAINPAGE "Main.html"
#define MAX_DYNAMIC_CONTENT 20
#define MAX_WEB_FILE 20

#define HFILE int
typedef enum _FilePos { FBEGIN,FCUR,FEND } FilePos;
typedef enum _Language { En,SChn,TChn,Unkown } Language;

#define HTTP_PORT	88
typedef struct _DYNCONTENT
{
	char variableName[80];
	char* (*pfunc)();
} DYNCONTENT;

class CHandleHttp 
{
public:
	CHandleHttp();
	~CHandleHttp();

	static SOCKET m_sConnection;
	static WORD	  m_wWebPort;	
	static HANDLE m_hFinish;
	static HANDLE m_hChgPort;
	static BOOL IsWorking() {
		return (m_lHttpThread != 0); //// (INVALID_SOCKET != m_sConnection);
	}
	static LONG m_lHttpThread;
	static void IncreaseHttpThreadCount();
	static void DecreaseHttpThreadCount();
	void InitContent();
	int addDynamicContent(char *name, char*(*function)());
	void getDynamicContent(char* name, char *content);
	UINT HandleReq();

	SOCKET GetSocket()
	{
		return m_sClient;
	}

	CHandleHttp& operator=(SOCKET sAccept)
	{
		m_sClient = sAccept;
		return *this;
	}

	static UINT HttpServerProc(LPVOID p);
	static UINT HandleReqProc(LPVOID p);
protected:

	SOCKET	m_sClient;
	HANDLE  m_hNetEvent[2];

	int SendGramHead(DWORD FileSize);
//	FileContent WebFileList[MAX_WEB_FILE];
	DYNCONTENT dynamicContent[MAX_DYNAMIC_CONTENT];

	Language ParseLanguage(char *inbuf,int size); 
	char* GetURL(char *inbuf,int size);
	void getFilename(char *inbuf,char *filename, int start);
	BOOL IsWebPage(char *filename);

	// Simulate file operation
	FILE* LookforFile(char* filename,DWORD* fileSize);
	// 
	void SendMainPage(FILE* hf);
	void SendNormalPage(FILE* hf);

};

char* addressFunc();
char* ipFunc();

#endif  // _HANDLEHTTP_H