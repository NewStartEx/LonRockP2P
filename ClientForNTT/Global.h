#ifndef _GLOBAL
#define _GLOBAL
#define LENCH 19
#define ONLINE 0x32
#define NOTEXIST 0x33
#define NOTONLINE 0x34
#define BUSY 0x35
#define REPEATLINE 0x36
#define NETERROR -1
#define RECVMSGTYPE 0x5354454E
#define RECVVATITLE 0x53565848
#define SENDMSGTYPE 0x454C4F48
#define VICO_BUFFER_LENGTH 0x00200000

//内存以1字节对齐
#pragma pack(1)
//客户端向地址中转服务器发送的消息格式[UDP协议]
typedef struct UDPSendFormat{
	DWORD type;
	char content;
	char SerUser[LENCH];
	char CliID[LENCH];
	char NONO[9];
}UDPSendFormat;
//地址中间服务器回复给客户端的消息格式[UDP协议]
typedef struct UDPRecvFormat{
	DWORD type;
	char content;
	char SerUser[LENCH];
	DWORD SevIP;
	char NONO[2];
	char NONOPORT[2];
	char NONOKey[4];
}UDPRecvFormat;
//客户端向工作组发送请求的消息格式[TCP协议]
typedef struct TCPComFormat{
	char mainMsg[16];
	char subMsg[16];
	char shparameter[32];
	char longparameter[64];
}TCPComFormat;
//客户端向工作站发送视频相关的请求的消息格式[TCP协议]
typedef struct VioFSend{
	char content[16];
	char parameter[16];
}VioFSend;
//客户端接收工作站回复的视频格式的消息格式[TCP协议]
typedef struct VioFRecv{
	char title[16];
	char parameter[48];
}VioFRecv;
//VioFRecv的parameter格式
typedef struct VAFormat{
	DWORD dwTitle;
	DWORD dwWidth;
	DWORD dwHight;
	DWORD dwAudio;
}VAFormat;
//内存对齐恢复默认
#pragma pack()

typedef struct
{
	HI_U32 u32Width;	/* 视频宽 */
	HI_U32 u32Height;	/* 视频高 */
} HI_S_VideoHeader;

typedef struct
{
    HI_U32 u32Format;	/*音频格式*/
} HI_S_AudioHeader;

typedef struct 
{
	HI_U32 u32SysFlag;
	HI_S_VideoHeader struVHeader;
	HI_S_AudioHeader struAHeader;
} HI_S_SysHeader;

extern CString g_szLanguagePath;

void g_SetDialogStrings(CDialog *pDlg,UINT uDlgID); //对话框集体对换
CString g_LoadString(CString szID); //单字符串对换
//设置 协议 端口 IP地址
void SetSocketInfo(SOCKADDR_IN&, short, u_short, void*);
//客户端向地址中转服务器发送数组中第3和第4个字段的数据加密
BOOL Encrypt(char* buff, int len);
//初始化网络
void InitNet();

#endif