
// The following ifdef block is the standard way of creating macros which make exporting 
// from a DLL simpler. All files within this DLL are compiled with the MEMVICOCDEC_EXPORTS
// symbol defined on the command line. this symbol should not be defined on any project
// that uses this DLL. This way any other project whose source files include this file see 
// MEMVICOCDEC_API functions as being imported from a DLL, wheras this DLL sees symbols
// defined with this macro as being exported.
#ifndef _MEMVICOCDECFUN_H
#define _MEMVICOCDECFUN_H

#ifdef __cplusplus
extern "C" {
#endif

//// 错误代码
//// Error Code
typedef enum _MEMVICO_ERROR_CODE {
	MEMVICO_OK = 0,
	MEMVICO_DDRAW_ERR,
	MEMVICO_SHOWVIDEO_ERR,
	MEMVICO_NOTEXIST_ERR,
	MEMVICO_NOTPLAY_ERR,
	MEMVICO_MEM_ERR,
	MEMVICO_FILE_ERR,
	MEMVICO_EXIST_ERR
} MEMVICO_ERROR_CODE;

#ifndef MEMVICOC_LIB
#ifdef MEMVICOCDEC_EXPORTS
#define MEMVICOCDEC_API __declspec(dllexport)
#else
#define MEMVICOCDEC_API __declspec(dllimport)
#endif
#else
#define MEMVICOCDEC_API 
#endif

typedef enum _MemDataMode {
	realtime = 0,   ///// 实时数据传输
	remotefile     ///// 远程文件数据
} MemDataMode;

typedef enum _NotifyEvent {
	evFileEnd = 0,   ///// 到了文件末尾
	evSegmentEnd     ///// 一个录像片段结束
} NotifyEvent;

MEMVICOCDEC_API MEMVICO_ERROR_CODE InitMemVicoc(HWND hWnd);  //// 初始化并获得一个解码器句柄
MEMVICOCDEC_API void ReleaseMemVicoc();  //// 释放句柄
MEMVICOCDEC_API HANDLE OpenMemVicoc();    /// 获得一个句柄
MEMVICOCDEC_API void   CloseMemVicoc(HANDLE handle);   /// 关闭一个句柄
MEMVICOCDEC_API MEMVICO_ERROR_CODE ChangeVideoWnd(HWND hWnd);  //// 更换显示视频窗口句柄
MEMVICOCDEC_API MEMVICO_ERROR_CODE SetMemVideoScreenPos(HANDLE handle,RECT &rect); //// 使用屏幕位置
MEMVICOCDEC_API MEMVICO_ERROR_CODE GetMemVideoScreenPos(HANDLE handle,RECT *prect);
MEMVICOCDEC_API MEMVICO_ERROR_CODE GetActualMemVideoSize(HANDLE handle,int *pWidth,int *pHeight);
MEMVICOCDEC_API MEMVICO_ERROR_CODE RepaintMemVideo(HANDLE handle); //// WM_PAINT函数会冲掉以前的图像，尤其在PAUSE的时候
MEMVICOCDEC_API MEMVICO_ERROR_CODE SetDataMode(HANDLE handle,MemDataMode mode);
MEMVICOCDEC_API MEMVICO_ERROR_CODE PushData(HANDLE handle,LPBYTE lpData,int length);
MEMVICOCDEC_API MEMVICO_ERROR_CODE MemDecodingReset(HANDLE handle,BOOL bShow = TRUE);  //// 是否显示视频
MEMVICOCDEC_API MEMVICO_ERROR_CODE ShowMemVideo(HANDLE handle,BOOL bShow);  //// 显示视频与否
MEMVICOCDEC_API MEMVICO_ERROR_CODE GetCurrentDecodingTime(HANDLE handle,LPSYSTEMTIME pSystemTime);
MEMVICOCDEC_API MEMVICO_ERROR_CODE SetMute(HANDLE handle,BOOL enable); 
MEMVICOCDEC_API MEMVICO_ERROR_CODE SetMemPlayRate(HANDLE handle,double rate);  //// 只有在非实时模式下可以使用
MEMVICOCDEC_API MEMVICO_ERROR_CODE MemSnapFrame(HANDLE handle,LPCTSTR filename);  //// JPEG File Format
MEMVICOCDEC_API MEMVICO_ERROR_CODE SetNotifyMessage(HANDLE handle,HWND hMsgWnd,UINT msg,WPARAM wParam,LPARAM LParam,NotifyEvent nev,BOOL enable = TRUE);   //// 出现事情的时候发送消息

#ifdef __cplusplus
}
#endif

#endif // _MEMVICOCDECFUN_H