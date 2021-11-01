// DVRDlg.h : header file
//

#if !defined(AFX_DVRDLG_H__FBBC3097_E737_4E04_9CF1_26A4541190B7__INCLUDED_)
#define AFX_DVRDLG_H__FBBC3097_E737_4E04_9CF1_26A4541190B7__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "DvrManager.h"
/////////////////////////////////////////////////////////////////////////////
// CDVRDlg dialog

class CDVRDlg : public CDialog
{
// Construction
public:
	CDVRDlg(CWnd* pParent = NULL);	// standard constructor

// Dialog Data
	//{{AFX_DATA(CDVRDlg)
	enum { IDD = IDD_DVR_DIALOG };
	CString	m_strKeyName;
	CString	m_strContent;
	CString	m_strInfo;
	CString	m_strPassword;
	//}}AFX_DATA

	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CDVRDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	HICON m_hIcon;
	CDVRManager *m_pManager;

	// Generated message map functions
	//{{AFX_MSG(CDVRDlg)
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	afx_msg void OnBtnApply();
	afx_msg void OnBtnTest();
	afx_msg void OnBtnDelete();
	afx_msg void OnDestroy();
	virtual void OnOK();
	afx_msg void OnTimer(UINT nIDEvent);
	//}}AFX_MSG
	virtual void OnCancel();
	afx_msg void OnNetSFeedback(WPARAM wParam,LPARAM lParam);
	afx_msg void OnNetSTunnel(WPARAM wParam,LPARAM lParam);
	afx_msg void OnCltTunnel(WPARAM wParam,LPARAM lParam);
	afx_msg void OnNetSNoresp(WPARAM,LPARAM lParam);
	DECLARE_MESSAGE_MAP()
};

int LinkClient(HANDLE hClient, ULONG addr);
int RcvClientCmd(HANDLE hClient, PVOID pInBuffer,PVOID pOutBuffer);
int ClientLeft(HANDLE hClient);

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_DVRDLG_H__FBBC3097_E737_4E04_9CF1_26A4541190B7__INCLUDED_)
