// NETCLIDlg.h : header file
//

#if !defined(AFX_NETCLIDLG_H__22932640_2156_4369_BA63_4F801CC3E673__INCLUDED_)
#define AFX_NETCLIDLG_H__22932640_2156_4369_BA63_4F801CC3E673__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "VideoDlg.h"
#include "Global.h"

/////////////////////////////////////////////////////////////////////////////
// CNETCLIDlg dialog

class CNETCLIDlg : public CDialog
{
// Construction
public:
	CNETCLIDlg(CWnd* pParent = NULL);	// standard constructor
	static DWORD UdpRecvThread(LPVOID);	// ���յ�ַ��ת�������ظ�����Ϣ[UDPЭ��]
	static DWORD TCPCOMThread(LPVOID);	// ����վ��������,���չ���վ�Ļظ�������[TCPЭ��]
	static DWORD RecvVADateTh(LPVOID);	// ��������Ƶ����[TCPЭ��]
	void SetChannelCombo(int nCount);	// ��ͨ�������������б�
	SOCKET m_sockSrc;	// ��ַ��ת������[UDPЭ��]
	int m_nComThrState;	// ��¼�ͻ������ӹ���վ��ʱ�� ����վ��״̬
	char m_chSerUser[LENCH];
	DWORD m_dwSerIP;
// Dialog Data
	//{{AFX_DATA(CNETCLIDlg)
	enum { IDD = IDD_NETCLI_DIALOG };
	CButton	m_btnShowVio;
	CButton	m_btnStopVio;
	CComboBox	m_comboChannel;
	CString	m_strCs2User;
	int		m_nPort;
	int		m_nVAPort;
	//}}AFX_DATA

	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CNETCLIDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	HICON m_hIcon;
	BOOL m_bInitOK;
	CVideoDlg m_cSubDlg;
	SOCKET m_soVAData;
	HANDLE m_hExitVAThr, m_hExitVAThrOK;
	HANDLE m_hExitTCPThr;
	PLAYHANDLE m_hViode;
	// Generated message map functions
	//{{AFX_MSG(CNETCLIDlg)
	virtual BOOL OnInitDialog();
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	virtual void OnOK();
	afx_msg void OnMove(int x, int y);
	afx_msg void OnBtncon();
	afx_msg void OnClose();
	afx_msg void OnBtnshow();
	afx_msg void OnBtncut();
	afx_msg void OnBtnstopshow();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_NETCLIDLG_H__22932640_2156_4369_BA63_4F801CC3E673__INCLUDED_)
