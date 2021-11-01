
// The following ifdef block is the standard way of creating macros which make exporting 
// from a DLL simpler. All files within this DLL are compiled with the NTTSTATION_EXPORTS
// symbol defined on the command line. this symbol should not be defined on any project
// that uses this DLL. This way any other project whose source files include this file see 
// NTTSTATION_API functions as being imported from a DLL, wheras this DLL sees symbols
// defined with this macro as being exported.
#ifndef _NTTSTATIONFUN_H
#define _NTTSTATIONFUN_H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef NTTSTATION_EXPORTS
#define NTTSTATION_API __declspec(dllexport)
#else
#define NTTSTATION_API __declspec(dllimport)
#endif

#define DEV_TYPE_CARD	0
#define DEV_TYPE_NDVR	1

typedef struct _CMPR_FRAME_INFO
{
	ULONG  nFrmLength;
	WORD   wCmprIndex;  //// frame's sequence
	BYTE   byFlags;
	BYTE   byReserved;
} CMPR_FRAME_INFO;

typedef struct _CMPR_SAMPLE
{
	CMPR_FRAME_INFO LastIFrmInfo;
	CMPR_FRAME_INFO CurFrmInfo;
} CMPR_SAMPLE,*PCMPR_SAMPLE;

#define NTT_CMD_VIDEO	"VIDEO"
#define NTT_MSG_VIDEO	"VIDEO"
#define NTT_CMD_AUDIO	"AUDIO"
#define NTT_MSG_AUDIO	"AUDIO"

#define NTT_SUBMSG_ENABLE	"enable"
#define NTT_SUBMSG_DISABLE	"disable"

#define NTT_ENABLE_VIDEO_BIT	(1<<0)  //// 这一位置1说明要传输视频了
#define NTT_ENABLE_AUDIO_BIT	(1<<0)  //// 这一位置1说明要传输音频了

#define INFO_MAIN_LENGTH	0x10
#define INFO_SUB_LENGTH		0x10
#define INFO_PARAM1_LENGTH	0x20
#define INFO_PARAM2_LENGTH	0x40

typedef struct _NETPACKET {
	char  chMsg[INFO_MAIN_LENGTH];
	char  chSubMsg[INFO_SUB_LENGTH];
	char  chParam1[INFO_PARAM1_LENGTH];
	char  chParam2[INFO_PARAM2_LENGTH];
} NETPACKET, *PNETPACKET;

typedef enum _Apply_Type {
	Apply_Register,
	Apply_Update,
	Apply_Wakeup,
	Apply_Delete
} Apply_type;

#define NTT_REGISTERED		0
#define NTT_NOTREGISTER		1
#define NTT_SERVERBUSY		2
#define NTT_SLEEPING		3
#define NTT_NETWORKERR		4   
#define NTT_ACCOUNTALIVE	5

//// 通过消息让使用者知道内部行为
//// NetS的回应
#define WM_NETS_FEEDBACK	(WM_USER+1)
//// WPARAM 是NetS返回值
//// 发出打洞的消息
#define WM_NETS_TUNNEL		(WM_USER+2)
//// WPARAM 是对方地址
#define WM_CLT_TUNNEL		(WM_USER+3)
//// WPARAM 是对方地址
//// LPARAM 是最终打洞结果，成功0还是超时1
#define WM_NETS_NORESP		(WM_USER+4)

//// 向NetS发送命令的返回状态
#define SENDLINK_FAILED		0
#define SENDLINK_SUCCESS	1
#define SENDLINK_EXIST		2
#define SENDLINK_NONAME		3
#define SENDLINK_NONETS		4
#define SENDLINK_ILLEGAL	5

#define NTT_STRFORMAT_ERROR 98    //// 字符串格式错误
#define NTT_RES_UNINIT		99    //// 没有初始化资源

#define MAX_NSTNSTRING_LENGTH	16
#define MIN_NSTNSTRING_LENGTH	4

typedef int (*CONNECTEDFROMCLIENT)(HANDLE hClient, ULONG addr);
typedef int (*RECVFROMCLIENT)(HANDLE hClient, PNETPACKET pInPacket,PNETPACKET pOutPacket);
typedef int (*DISCONNECTEDFROMCLIENT)(HANDLE hClient);

typedef void (*PRESTARTSTREAMPROC)(int,LPVOID );
typedef void (*POSTSTOPSTREAMPROC)(int);
typedef BOOL (*ASKFORAUDIOSAMPLE)(int,BOOL,LPVOID );

//// Station Initialization
NTTSTATION_API BOOL NstnStartWorking(DWORD* dwDevID,WORD wWebPort,DWORD dwType);
//// Station Uninitialization
NTTSTATION_API void NstnStopWorking();
NTTSTATION_API void NstnChangeWebPort(WORD wWebPort);
NTTSTATION_API BOOL NstnGetLocalInfo(ULONG *IPAddr,WORD *wNTTPort,WORD *wWebPort,UINT *NstnStatus,char *chNstnName);
NTTSTATION_API UINT NstnApplyNetServer(LPCTSTR NstnName,LPCTSTR NstnPassword,Apply_type type);
NTTSTATION_API BOOL NstnResetLastNstnName(char *chNstnName);
NTTSTATION_API void NstnEnableMsgNotify(HWND hMsgWnd,BOOL enable);
NTTSTATION_API BOOL NstnInitDataChannel(int nChanCount);
NTTSTATION_API BOOL NstnEnableVideoTransfer(HANDLE hClient,BOOL bEnable,BOOL bInCallback);
NTTSTATION_API BOOL NstnEnableAudioTransfer(HANDLE hClient,BOOL bEnable,BOOL bInCallback);
NTTSTATION_API BOOL NstnPushVideoSample( int channel,    
            /* [in,out] */ CMPR_SAMPLE *psample,
            /* [in] */ PVOID pLastIFrmBuffer,
            /* [in] */ PVOID pCurFrmBuffer);
NTTSTATION_API BOOL NstnPushAudioSample(int channel, 
					 /* [in] */ ULONG nLength,
					 /* [in] */ PVOID pSampleBuffer);

NTTSTATION_API void NstnSetStatusCallback(CONNECTEDFROMCLIENT,RECVFROMCLIENT,DISCONNECTEDFROMCLIENT);
NTTSTATION_API void NstnSetStreamCallback(PRESTARTSTREAMPROC,POSTSTOPSTREAMPROC,ASKFORAUDIOSAMPLE);
NTTSTATION_API BOOL NstnDismissClient(HANDLE hClient,BOOL bInCallback);
NTTSTATION_API void NstnSetNServerName(LPCTSTR NetServer);

#ifdef __cplusplus
}
#endif

#endif // _NTTSTATIONFUN_H