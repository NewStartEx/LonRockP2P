// NETCLI.h : main header file for the NETCLI application
//

#if !defined(AFX_NETCLI_H__E092EBA5_6A53_4238_A508_150076C85FFB__INCLUDED_)
#define AFX_NETCLI_H__E092EBA5_6A53_4238_A508_150076C85FFB__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#ifndef __AFXWIN_H__
	#error include 'stdafx.h' before including this file for PCH
#endif

#include "resource.h"		// main symbols

/////////////////////////////////////////////////////////////////////////////
// CNETCLIApp:
// See NETCLI.cpp for the implementation of this class
//

class CNETCLIApp : public CWinApp
{
public:
	CNETCLIApp();

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CNETCLIApp)
	public:
	virtual BOOL InitInstance();
	//}}AFX_VIRTUAL

// Implementation

	//{{AFX_MSG(CNETCLIApp)
		// NOTE - the ClassWizard will add and remove member functions here.
		//    DO NOT EDIT what you see in these blocks of generated code !
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};


/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_NETCLI_H__E092EBA5_6A53_4238_A508_150076C85FFB__INCLUDED_)
