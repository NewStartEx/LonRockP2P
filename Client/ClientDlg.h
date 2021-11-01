// ClientDlg.h : header file
//

#if !defined(AFX_CLIENTDLG_H__0CA5525C_594E_4C76_9A0D_94004077A41C__INCLUDED_)
#define AFX_CLIENTDLG_H__0CA5525C_594E_4C76_9A0D_94004077A41C__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "CltManager.h"
/////////////////////////////////////////////////////////////////////////////
// CClientDlg dialog

class CClientDlg : public CDialog
{
// Construction
public:
	CClientDlg(CWnd* pParent = NULL);	// standard constructor

	void SetStatus(LPCTSTR status);
	void SetContent(LPCTSTR content);

// Dialog Data
	//{{AFX_DATA(CClientDlg)
	enum { IDD = IDD_CLIENT_DIALOG };
	CString	m_strKeyName;
	CString	m_strStatus;
	CString	m_strContent;
	//}}AFX_DATA

	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CClientDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	HICON m_hIcon;
	CCltManager *m_pManager;
	CString m_strOldName;

	// Generated message map functions
	//{{AFX_MSG(CClientDlg)
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	afx_msg void OnBtnSerch();
	afx_msg void OnDestroy();
	virtual void OnOK();
	//}}AFX_MSG
	virtual void OnCancel();
	afx_msg void OnDvrStatus(WPARAM wParam,LPARAM lParam);
	afx_msg void OnDvrContent(WPARAM wParam,LPARAM lParam);
	DECLARE_MESSAGE_MAP()
};

int LinkDvr(LPCTSTR strDVR, ULONG addr, BOOL bWork);
int RcvDvrMsg(LPCTSTR strDVR, PVOID pInBuffer);
int DvrLeft(LPCTSTR strDVR,UINT uReason);

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_CLIENTDLG_H__0CA5525C_594E_4C76_9A0D_94004077A41C__INCLUDED_)
