// NttClient.h : main header file for the NTTCLIENT DLL
//

#if !defined(AFX_NTTCLIENT_H__ECEDDB8E_71D7_423D_814D_D09B9941C4D3__INCLUDED_)
#define AFX_NTTCLIENT_H__ECEDDB8E_71D7_423D_814D_D09B9941C4D3__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#ifndef __AFXWIN_H__
	#error include 'stdafx.h' before including this file for PCH
#endif

#include "resource.h"		// main symbols

/////////////////////////////////////////////////////////////////////////////
// CNttClientApp
// See NttClient.cpp for the implementation of this class
//

class CNttClientApp : public CWinApp
{
public:
	CNttClientApp();

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CNttClientApp)
	//}}AFX_VIRTUAL

	//{{AFX_MSG(CNttClientApp)
		// NOTE - the ClassWizard will add and remove member functions here.
		//    DO NOT EDIT what you see in these blocks of generated code !
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};


/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_NTTCLIENT_H__ECEDDB8E_71D7_423D_814D_D09B9941C4D3__INCLUDED_)
