// DVR.h : main header file for the DVR application
//

#if !defined(AFX_DVR_H__E471069E_0DE3_41F6_8479_4C4F4F608762__INCLUDED_)
#define AFX_DVR_H__E471069E_0DE3_41F6_8479_4C4F4F608762__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#ifndef __AFXWIN_H__
	#error include 'stdafx.h' before including this file for PCH
#endif

#include "resource.h"		// main symbols

/////////////////////////////////////////////////////////////////////////////
// CDVRApp:
// See DVR.cpp for the implementation of this class
//

class CDVRApp : public CWinApp
{
public:
	CDVRApp();

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CDVRApp)
	public:
	virtual BOOL InitInstance();
	//}}AFX_VIRTUAL

// Implementation

	//{{AFX_MSG(CDVRApp)
		// NOTE - the ClassWizard will add and remove member functions here.
		//    DO NOT EDIT what you see in these blocks of generated code !
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};


/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_DVR_H__E471069E_0DE3_41F6_8479_4C4F4F608762__INCLUDED_)
