// NetServerDlg.h : header file
// 界面上面需要显示的消息：
//     1、服务器启动后活动的DVR
//     2、客户端连接的行为

#if !defined(AFX_NETSERVERDLG_H__4B0795F1_4993_44B7_9757_2D5C3A87232F__INCLUDED_)
#define AFX_NETSERVERDLG_H__4B0795F1_4993_44B7_9757_2D5C3A87232F__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "NetSManager.h"
/////////////////////////////////////////////////////////////////////////////
// CNetServerDlg dialog
#define ClEAR_DVRLIST_TIMER 1
class CNetServerDlg : public CDialog
{
// Construction
public:
	CNetServerDlg(CWnd* pParent = NULL);	// standard constructor

// Dialog Data
	//{{AFX_DATA(CNetServerDlg)
	enum { IDD = IDD_NETSERVER_DIALOG };
	CListBox	m_LbxWorkRecord;
	CListCtrl	m_lstActiveDVR;
	//}}AFX_DATA

	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CNetServerDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	HICON m_hIcon;
	BOOL m_bSysClosed;
	RECT m_rcAccountInfo;
	
	int m_nOnlineNum;
	CNetSManager *m_pManager;

	int ArrangDvrPos(LPCTSTR DvrName);
	int OrderMakeDvrPos(LPCTSTR DvrName,int startIndex, int stopIndex);
	int  SearchDvrList(LPCTSTR DvrName);
	int  OrderFindDvrList(LPCTSTR DvrName,int startIndex,int stopIndex);
	// Generated message map functions
	//{{AFX_MSG(CNetServerDlg)
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	afx_msg void OnDestroy();
	virtual void OnOK();
	afx_msg void OnBtnClear();
	afx_msg void OnClose();
	afx_msg void OnTimer(UINT nIDEvent);
	afx_msg UINT OnNcHitTest(CPoint point);
	//}}AFX_MSG
	virtual void OnCancel();
	afx_msg void OnDvrChanged(WPARAM wParam, LPARAM lParam);
	afx_msg void OnClientRequest(WPARAM wParam, LPARAM lParam);
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_NETSERVERDLG_H__4B0795F1_4993_44B7_9757_2D5C3A87232F__INCLUDED_)
