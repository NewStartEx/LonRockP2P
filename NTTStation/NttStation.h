// NttStation.h : main header file for the NTTSTATION DLL
//

#if !defined(AFX_NTTSTATION_H__00AB3AC0_FC8E_4BA7_828B_2FB6BA06E3B6__INCLUDED_)
#define AFX_NTTSTATION_H__00AB3AC0_FC8E_4BA7_828B_2FB6BA06E3B6__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#ifndef __AFXWIN_H__
	#error include 'stdafx.h' before including this file for PCH
#endif

#include "resource.h"		// main symbols

/////////////////////////////////////////////////////////////////////////////
// CNttStationApp
// See NttStation.cpp for the implementation of this class
//

class CNttStationApp : public CWinApp
{
public:
	CNttStationApp();

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CNttStationApp)
	//}}AFX_VIRTUAL

	//{{AFX_MSG(CNttStationApp)
		// NOTE - the ClassWizard will add and remove member functions here.
		//    DO NOT EDIT what you see in these blocks of generated code !
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};


/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_NTTSTATION_H__00AB3AC0_FC8E_4BA7_828B_2FB6BA06E3B6__INCLUDED_)
