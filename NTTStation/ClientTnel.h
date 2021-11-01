//// 2010-5-2: 增加了握手确认状态。先测试网络并且发送ACK。创建发送测试数据的线程
#ifndef _CLIENTTNEL_H
#define _CLIENTTNEL_H

#define MAX_TNEL_COUNT	10

#define GRAM_TYPE_TNEL	0
#define GRAM_TYPE_HSHK	1
#define GRAM_TYPE_MSG	2

//// 如果是重发就要用另一个事件了，因为这个会是比较多的数量

#define CREATE_TNEL_INTERVAL	1000 //// 每这么多时间发送一次隧道请求
#define MAX_TNEL_SEND_COUNT		30   //// 大概半分钟
#define MAX_SILENCE_INTERVAL	30  //// 大概半分钟没有发送信息，则为对方断线
#include "SendVideo.h"
// 创建后创建发送线程。
// 线程里判断与对方是否连接。如没有连接，则发送隧道开辟报文,每秒一次，
// 连接成功或者发送次数超过，超时时间设为无限
class CClientTnel
{
public:
	//// 有Act参数，要考虑是隧道来的还是直接连接的
	CClientTnel(char *Name,DWORD dwIPAddr,WORD wPort,BYTE byAct);  
	~CClientTnel();
	static int m_TnelCount;  //// 这个不可以超过一定次数
	BOOL InitResource(LPCTSTR DVRName);   //// 这里开始打洞线程
	BOOL IsMyself(LPCTSTR CltName); //// 用这个来标识同一个电脑的数据，不允许一个电脑连接两次（比如使用IP和名称）
	BYTE DealwithCnctGram(PCNCTDESC pTnelInfo); 
	BOOL DealwithHshkGram(PHSHKDESC pHshkInfo);
	BOOL CheckCmdGram(PCMDDESC pWorkInfo);

	BOOL SetMsgGram(PCMDINFO pWorkInfo);   //// 发送命令返回字
	BOOL CheckVideoMsg(PCMDINFO pVideoInfo);
	BOOL CheckAudioMsg(PCMDINFO pAudioInfo);
	void SetExit();
	//// 找到对方，打洞成功或者联系正常
	BOOL Found() {
		return m_bTnelOpened;
	}
	void SetExitMark() {
		m_bForceExit = TRUE;
	}
	void EnableVideo(BOOL bEnable);
	void EnableAudio(BOOL bEnable);
	BOOL CheckVResdGram(PVRESDPACKET pVResdPackt,DWORD dwIPAddr,LPCTSTR DvrName);
	//// 如果又是NetS发送来的打洞请求，则重新打洞
	BOOL RetryTnel(LPCTSTR DVRName); /// 参数需要以后考量。是在此客户存在又没有开通隧道的时候，对方又发命令过来使用
	//// 这里需要有一个自检程序，如果发现打洞失败并且超时，则删除这个类
	BOOL SelfChecking(); ///// 参数需要以后考量,这里判断 隧道是否开通，对方多长时间没有发送心跳命令
protected:
	CString m_strName;  
	CString m_strDvrName; //// 发给用户的名称标志，可能是DVR的唯一标识，也可能是Client发来的名称
	SOCKADDR_IN m_ClientAddr;
	BYTE  m_byMsgAct;  
	BOOL  m_bTnelOpened;
	BOOL  m_bForceExit,m_bCanVideo,m_bCanAudio;
	int  m_nSendTnelCount; //// 对系统操作的计数 看超时 自动更新命令由Manager来发送
	int	 m_nSendGramInterval;  //// 发送系列命令的间隔
	int  m_nLastCmdJump;  //// 判断没有收到心跳的时间
	int	  m_nGramType;
	OPENTNEL m_OpenTnel;
	HSHK_GRAM m_HshkACKGram;
	WORK_GRAM m_CmdGram;

	CSendVideoArray m_SendVideoArray;

	CRITICAL_SECTION m_csGram,m_csCount;
	HANDLE	m_hNormalGram,m_hNaviData,m_hStopWorking;  //// m_hNaviData 是发送带宽测试数据的事件
	DWORD m_dwNwidPackets,m_dwNetRelayInterval; //// 网络带宽和传输来回时延
	HshkStatus m_HshkStatus;  //// 握手的当前状态
	BOOL SetHshkACK(PHSHK_GRAM pHshkGram);  //// 处理握手信息

	void MakeTnelGram(LPCTSTR FirstName,LPCTSTR SecondName,BYTE byAct);
	LONG m_nThreadCount;
	void IncreaseThreadCount() 
	{	InterlockedIncrement(&m_nThreadCount);
	}
	void DecreaseThreadCount() 
	{	InterlockedDecrement(&m_nThreadCount);
	}
	BOOL SendGram();
	static UINT SendGramProc(LPVOID p);
private:
};

typedef CArray<CClientTnel*,CClientTnel*> CCltTnelArray;
#endif // _CLIENTTNEL_H