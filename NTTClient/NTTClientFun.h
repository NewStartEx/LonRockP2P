
// The following ifdef block is the standard way of creating macros which make exporting 
// from a DLL simpler. All files within this DLL are compiled with the NTTSTATION_EXPORTS
// symbol defined on the command line. this symbol should not be defined on any project
// that uses this DLL. This way any other project whose source files include this file see 
// NTTSTATION_API functions as being imported from a DLL, wheras this DLL sees symbols
// defined with this macro as being exported.
#ifndef _NTTCLIENTFUN_H
#define _NTTCLIENTFUN_H

#ifdef __cplusplus
extern "C" {
#endif

#ifndef NTTCLIENT_LIB
#ifdef NTTCLIENT_EXPORTS
#define NTTCLIENT_API __declspec(dllexport)
#else
#define NTTCLIENT_API __declspec(dllimport)
#endif
#else 
#define NTTCLIENT_API
#endif // NTTCLIENT_LIB

typedef enum  _NTTCLT_ERROR_CODE {
	NTTCLT_OK = 0,
	NTTCLT_RES_ERR,				//// 显示系统资源错误            //// The window's handle which renders video is error
	NTTCLT_NETINIT_ERR,			//// 网络初始化错误                    //// The network doesn't initialized
	NTTCLT_CONNECT_ERR,			//// 指定的网络句柄未连接              //// The Server doesn't connect
	NTTCLT_FORMAT_ERR,			//// 字串格式错误。NTT不可以使用空格,".","*"
	NTTCLT_SERVER_ERR,          //// 网络服务器错误
	NTTCLT_COMPRESSOR_ERR,		//// 视频压缩引擎发生错误              //// The video compressor error
	NTTCLT_CHANNEL_ERR,			//// 选择的通道错误                    //// The channel selected not exist
	NTTCLT_FILENAME_ERR,		//// 录相或拍照的文件名错误            //// File name error
	NTTCLT_RECORD_ERR,			//// 启动录像出错                      //// Start Record error
	NTTCLT_SNAP_ERR				//// 抓拍显示图像时未打开显示功能      //// Snap picture error
} NTTCLT_ERROR_CODE;

#define NTT_CMD_VIDEO	"VIDEO"
#define NTT_MSG_VIDEO	"VIDEO"
#define NTT_CMD_AUDIO	"AUDIO"
#define NTT_MSG_AUDIO	"AUDIO"

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

typedef int (*STATIONLINKED)(LPCTSTR strStation, ULONG addr, BOOL bWork);
typedef int (*RECVFROMSTATION)(LPCTSTR strStation, PVOID pInBuffer);
typedef int (*STATIONLEFT)(LPCTSTR strStation,UINT uReason);

//// 连接DVR的方式
#define LINK_TUNNEL	0
#define LINK_CMD	1

//// Station 被删除的原因
#define EXIT_UNKNOW		 0
#define EXIT_SOCKET_ERR	 1
#define EXIT_DVR_LOST	 2
#define EXIT_FORCE_EXIT	 3
#define EXIT_DVR_BUSY	 4
#define EXIT_LINK_DUP	 5
#define EXIT_NETS_LOST   6
#define EXIT_VER_OLD	 7
#define EXIT_DVR_OFFLINE 8
#define EXIT_DVR_NONAME	 9

NTTCLIENT_API void NcltSetNServerName(LPCTSTR NetServer);
NTTCLIENT_API NTTCLT_ERROR_CODE NcltStartUp(HWND hVidWnd);
NTTCLIENT_API void				NcltCleanUp();
NTTCLIENT_API void				NcltSetCallback(STATIONLINKED, RECVFROMSTATION, STATIONLEFT);
NTTCLIENT_API NTTCLT_ERROR_CODE NcltConnectStation(LPCTSTR StationName,UINT uLinkMode,PNETPACKET pCmd,WORD wDataPort);
NTTCLIENT_API void				NcltAbandonStation(LPCTSTR StationName,BOOL bInCallback);
NTTCLIENT_API NTTCLT_ERROR_CODE NcltCreateVideoResource(LPCTSTR StationName,int nChanCount,BOOL bInCallback);
NTTCLIENT_API NTTCLT_ERROR_CODE NcltCreateAudioResource(LPCTSTR StationName,int nChanIndex,BOOL bInCallback);
NTTCLIENT_API int				NcltGetChannelCount(LPCTSTR StationName,BOOL bInCallback);
NTTCLIENT_API NTTCLT_ERROR_CODE NcltSendCmd(LPCTSTR StationName,PNETPACKET pCmd,BOOL bInCallback);
NTTCLIENT_API NTTCLT_ERROR_CODE NcltStartVideoTran(LPCTSTR StationName,int chan,BOOL bInCallback);
NTTCLIENT_API NTTCLT_ERROR_CODE NcltStartAllVideoTran(LPCTSTR StationName,BOOL bInCallback);
NTTCLIENT_API NTTCLT_ERROR_CODE NcltStartAudioTran(LPCTSTR StationName,int chan,BOOL bInCallback);
NTTCLIENT_API NTTCLT_ERROR_CODE NcltStopVideoTran(LPCTSTR StationName,int chan,BOOL bInCallback);
NTTCLIENT_API NTTCLT_ERROR_CODE NcltStopAllVideoTran(LPCTSTR StationName,BOOL bInCallback);
NTTCLIENT_API NTTCLT_ERROR_CODE NcltStopAudioTran(LPCTSTR StationName,int chan,BOOL bInCallback);
NTTCLIENT_API NTTCLT_ERROR_CODE NcltChangeVideoWnd(HWND hVidWnd);
NTTCLIENT_API NTTCLT_ERROR_CODE NcltSetVideoPos(LPCTSTR StationName,int chan,int left,int top,int width,int height,BOOL bInCallback);
NTTCLIENT_API NTTCLT_ERROR_CODE NcltShowVideo(LPCTSTR StationName,int chan,BOOL bShow,BOOL bInCallback);
NTTCLIENT_API NTTCLT_ERROR_CODE NcltRefreshVideo(LPCTSTR StationName,int chan,BOOL bInCallback);
NTTCLIENT_API NTTCLT_ERROR_CODE NcltStartRecord(LPCTSTR StationName,int chan,LPCTSTR FileName,BOOL bInCallback);
NTTCLIENT_API NTTCLT_ERROR_CODE NcltStopRecord(LPCTSTR StationName,int chan,BOOL bInCallback);
NTTCLIENT_API DWORD				NcltGetRecFilePos(LPCTSTR StationName,int chan,BOOL bInCallback);
NTTCLIENT_API NTTCLT_ERROR_CODE NcltSnap(LPCTSTR StationName,int chan,LPCTSTR FileName,BOOL bInCallback);
NTTCLIENT_API void NcltSetNServerName(LPCTSTR NetServer);

#ifdef __cplusplus
}
#endif

#endif // _NTTSTATIONFUN_H