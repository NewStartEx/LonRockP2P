/*
 *	�ļ���ʽ:
 *  �ļ�ͷ: VICOHEADER + ������HEADER + ���ݡ���βҪ���ӱ�־
 *	ͷ�꣺ "VICO". ��Ƶ����"H264",��Ƶ����"VGSM". β��: "VEND"
 */
#ifndef _VICOFORMAT_H
#define _VICOFORMAT_H

#define VICO_ID	0x4F434956
#define H264_ID	0x34363248
#define VGSM_ID	0x4D534756
#define VEOS_ID 0x534F4556  //// ������
#define VEND_ID 0x444E4556  //// �ļ�����
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
	DWORD		dwIndex;     // ¼��ʼΪ0.����������һ���ļ���Ҳ֪���ĸ��Ǹտ�ʼ��¼��
	SYSTEMTIME  stFrmTime;
	DWORD		dwInterval;  // ����һ֡��ʱ���  //// Ҫ��ʵ��ӳ��ʱѹ����Ч�������ʱ�䳤���͵�Щ
	DWORD		dwLength;
	DWORD		dwCRC;       // �����ֶε����
	WORD		wWidth;      // �����ļ���ʱ���������������зֶδ���
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