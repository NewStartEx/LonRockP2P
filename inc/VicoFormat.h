/*
 *	文件格式:
 *  文件头: VICOHEADER + 数据流HEADER + 数据。结尾要增加标志
 *	头标： "VICO". 视频流标"H264",音频流标"VGSM". 尾标: "VEND"
 */
#ifndef _VICOFORMAT_H
#define _VICOFORMAT_H

#define VICO_ID	0x4F434956
#define H264_ID	0x34363248
#define VGSM_ID	0x4D534756
#define VEOS_ID 0x534F4556  //// 流结束
#define VEND_ID 0x444E4556  //// 文件结束
typedef struct _VICOHEADER {
	DWORD ID;
	BYTE  reserved[32];
} VICOHEADER, *PVICOHEADER;

#define VICOC_PAL	0
#define VICOC_NTSC	1
#define GetVideoMode(x) (x&0xF)
#define GetLostVFrame(x) (x >> 4)
#define SetInsertFrame(x) (x << 4)

//// DWORD ID
typedef struct _H264HEADER {
	DWORD		dwIndex;     // 录像开始为0.这样就算在一个文件里也知道哪个是刚开始的录像
	SYSTEMTIME  stFrmTime;
	DWORD		dwInterval;  // 和上一帧的时间差  //// 要如实反映当时压缩的效果。如果时间长，就等些
	DWORD		dwLength;
	DWORD		dwCRC;       // 所有字段的异或
	WORD		wWidth;      // 备份文件的时候根据这个，来进行分段处理
	WORD		wHeight;
	BYTE		byVideoMode; // PAL: 0, NTSC:1
	BYTE		byKeyFrame;
	BYTE		strTitle[32];				
	BYTE		reserved[32];
} H264HEADER, *PH264HEADER;

//// DWORD ID
typedef struct _VGSMHEADER {
	DWORD		dwIndex;
	SYSTEMTIME smpTime;
	WAVEFORMATEX	Format;
	DWORD	   dwSampleSize;
	DWORD	   dwInterval;
	BYTE	   reserved[28];
} VGSMHEADER, *PVGSMHEADER;

#define ERR_CRC		0x01    //// CRC error
#define ERR_BUFFER	0x02    //// dwLength not large enough
#define ERR_EOF		0x04    //// at the end of file

#endif  //// _VICOFORMAT_H