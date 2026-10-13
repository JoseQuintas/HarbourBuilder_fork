/*
 * hbblib_ext.cpp - runtime helpers for HarbourBuilder used as a library
 *
 * Win32 only. Fills gaps of the runtime when used from generated code
 * (dlgauto, hand written code):
 *   - live text/state of controls (TEdit:Value returns only the initial text)
 *   - focus
 *   - close a form from code (modal loop only ends on WM_CLOSE)
 *   - OnExit (WM_KILLFOCUS) and keyboard/dblclick events for any control,
 *     including TBrowse, whose events are not fired on Windows
 */

#include "hbide.h"
#include <commctrl.h>
#include <hbapiitm.h>
#include <hbvm.h>
#include <hbstack.h>

#define HBB_MAXEVENTS  1024

#define HBB_EV_EXIT     1   /* WM_KILLFOCUS              */
#define HBB_EV_KEYDOWN  2   /* WM_KEYDOWN, block( nKey ) */
#define HBB_EV_DBLCLICK 3   /* WM_LBUTTONDBLCLK          */

typedef struct
{
   TControl * pCtrl;
   int        nEvent;
   PHB_ITEM   pBlock;
} HBBEVENT;

/* per thread: a window belongs to the thread that created it, and hooks
   are installed by thread, so each GUI thread (JPA modules) has its own */
static thread_local HBBEVENT s_aEvents[ HBB_MAXEVENTS ];
static thread_local int      s_nEvents = 0;
static thread_local HHOOK    s_hHook   = NULL;
static thread_local HHOOK    s_hMsgHook = NULL;
static thread_local BOOL     s_bInEvent = FALSE;
static thread_local BOOL     s_lResult  = TRUE;   /* .F. returned by last event block */
static thread_local HWND     s_hSkipKill = NULL;  /* OnExit already done by Enter     */

static TControl * GetCtrlPar( int nParam )
{
   return ( TControl * ) ( LONG_PTR ) hb_parnint( nParam );
}

static HWND CtrlHwnd( int nParam )
{
   TControl * p = GetCtrlPar( nParam );
   return p ? p->FHandle : NULL;
}

static BOOL FireEvent( HWND hWnd, int nEvent, int nKey )
{
   int i;
   BOOL lFired = FALSE;

   s_lResult = TRUE;
   if( s_bInEvent || hWnd == NULL )
      return FALSE;
   for( i = 0; i < s_nEvents; i++ )
   {
      if( s_aEvents[ i ].pCtrl && s_aEvents[ i ].nEvent == nEvent &&
          s_aEvents[ i ].pCtrl->FHandle == hWnd )
      {
         s_bInEvent = TRUE;
         lFired = TRUE;
         if( hb_vmRequestReenter() )
         {
            hb_vmPushEvalSym();
            hb_vmPush( s_aEvents[ i ].pBlock );
            hb_vmPushInteger( nKey );
            hb_vmSend( 1 );
            if( HB_IS_LOGICAL( hb_stackReturnItem() ) && ! hb_itemGetL( hb_stackReturnItem() ) )
               s_lResult = FALSE;
            hb_vmRequestRestore();
         }
         s_bInEvent = FALSE;
      }
   }
   return lFired;
}

/* window is inside a HarbourBuilder form (class HbIdeFormN) */
static HWND HbbRootForm( HWND hWnd )
{
   HWND hRoot = GetAncestor( hWnd, GA_ROOT );
   char szClass[ 32 ];

   if( hRoot && GetClassNameA( hRoot, szClass, sizeof( szClass ) ) &&
       strncmp( szClass, "HbIdeForm", 9 ) == 0 )
      return hRoot;
   return NULL;
}

/* Enter/Escape on a form: IsDialogMessage() would send IDOK/IDCANCEL and
   TForm destroys the window without ending a modal loop (hangs).
   Enter = next control, Escape = WM_CLOSE */
static BOOL HbbDialogKey( MSG * msg )
{
   HWND hRoot;
   char szClass[ 32 ];

   if( msg->wParam != VK_RETURN && msg->wParam != VK_ESCAPE )
      return FALSE;
   if( ( hRoot = HbbRootForm( msg->hwnd ) ) == NULL )
      return FALSE;
   if( msg->wParam == VK_ESCAPE )
   {
      PostMessage( hRoot, WM_CLOSE, 0, 0 );
      return TRUE;
   }
   GetClassNameA( msg->hwnd, szClass, sizeof( szClass ) );
   if( lstrcmpiA( szClass, "Button" ) == 0 )
   {
      PostMessage( msg->hwnd, BM_CLICK, 0, 0 );
      return TRUE;
   }
   if( lstrcmpiA( szClass, "Edit" ) == 0 &&
       ( GetWindowLong( msg->hwnd, GWL_STYLE ) & ES_MULTILINE ) )
      return FALSE;
   /* OnExit (valid) runs with focus still on the control, as GET does */
   FireEvent( msg->hwnd, HBB_EV_EXIT, 0 );
   if( s_lResult && GetFocus() == msg->hwnd )
   {
      HWND hNext = GetNextDlgTabItem( hRoot, msg->hwnd, FALSE );
      if( hNext && hNext != msg->hwnd )
      {
         s_hSkipKill = msg->hwnd;
         SetFocus( hNext );
         s_hSkipKill = NULL;
      }
   }
   return TRUE;
}

static LRESULT CALLBACK HookProc( int nCode, WPARAM wParam, LPARAM lParam )
{
   if( nCode == HC_ACTION )
   {
      CWPSTRUCT * cwp = ( CWPSTRUCT * ) lParam;
      if( cwp->message == WM_KILLFOCUS && cwp->hwnd != s_hSkipKill )
         FireEvent( cwp->hwnd, HBB_EV_EXIT, 0 );
   }
   return CallNextHookEx( s_hHook, nCode, wParam, lParam );
}

/* keyboard and mouse come by the message queue (GetMessage/PeekMessage) */
static LRESULT CALLBACK MsgHookProc( int nCode, WPARAM wParam, LPARAM lParam )
{
   if( nCode == HC_ACTION && wParam == PM_REMOVE )
   {
      MSG * msg = ( MSG * ) lParam;
      /* without focus, keys go to active window as WM_SYSKEYDOWN */
      if( msg->message == WM_KEYDOWN ||
          ( msg->message == WM_SYSKEYDOWN && msg->wParam == VK_ESCAPE ) )
      {
         BOOL lFired = FireEvent( msg->hwnd, HBB_EV_KEYDOWN, ( int ) msg->wParam );
         if( msg->wParam == VK_RETURN || msg->wParam == VK_ESCAPE )
         {
            if( lFired || HbbDialogKey( msg ) )
               msg->message = WM_NULL;
         }
      }
      else if( msg->message == WM_LBUTTONDBLCLK )
         FireEvent( msg->hwnd, HBB_EV_DBLCLICK, 0 );
   }
   return CallNextHookEx( s_hMsgHook, nCode, wParam, lParam );
}

static void InstallHooks( void )
{
   if( s_hHook == NULL )
      s_hHook = SetWindowsHookEx( WH_CALLWNDPROC, HookProc, NULL, GetCurrentThreadId() );
   if( s_hMsgHook == NULL )
      s_hMsgHook = SetWindowsHookEx( WH_GETMESSAGE, MsgHookProc, NULL, GetCurrentThreadId() );
}

/* HBB_OnEvent( hCtrl, nEvent, bBlock ) */
HB_FUNC( HBB_ONEVENT )
{
   TControl * p = GetCtrlPar( 1 );
   PHB_ITEM pBlock = hb_param( 3, HB_IT_BLOCK );
   int nEvent = hb_parni( 2 );
   int i;

   if( p == NULL || pBlock == NULL )
      return;
   InstallHooks();
   for( i = 0; i < s_nEvents; i++ )
   {
      if( s_aEvents[ i ].pCtrl == p && s_aEvents[ i ].nEvent == nEvent )
      {
         hb_itemRelease( s_aEvents[ i ].pBlock );
         s_aEvents[ i ].pBlock = hb_itemNew( pBlock );
         return;
      }
   }
   for( i = 0; i < s_nEvents; i++ )   /* reuse free slot */
   {
      if( s_aEvents[ i ].pCtrl == NULL )
         break;
   }
   if( i == s_nEvents )
   {
      if( s_nEvents >= HBB_MAXEVENTS )
         return;
      s_nEvents++;
   }
   s_aEvents[ i ].pCtrl  = p;
   s_aEvents[ i ].nEvent = nEvent;
   s_aEvents[ i ].pBlock = hb_itemNew( pBlock );
}

/* HBB_ReleaseEvents( aCtrlHandles ) - when a dialog is closed */
HB_FUNC( HBB_RELEASEEVENTS )
{
   PHB_ITEM pArray = hb_param( 1, HB_IT_ARRAY );
   HB_SIZE n, nLen;
   int i;

   if( pArray == NULL )
      return;
   nLen = hb_arrayLen( pArray );
   for( n = 1; n <= nLen; n++ )
   {
      TControl * p = ( TControl * ) ( LONG_PTR ) hb_arrayGetNInt( pArray, n );
      for( i = 0; i < s_nEvents; i++ )
      {
         if( s_aEvents[ i ].pCtrl == p )
         {
            hb_itemRelease( s_aEvents[ i ].pBlock );
            s_aEvents[ i ].pBlock = NULL;
            s_aEvents[ i ].pCtrl  = NULL;
         }
      }
   }
}

/* HBB_CtrlHwnd( hCtrl ) --> nHwnd */
HB_FUNC( HBB_CTRLHWND )
{
   hb_retnint( ( HB_PTRUINT ) CtrlHwnd( 1 ) );
}

/* HBB_GetText( hCtrl ) --> cText (live text, synced to FText) */
HB_FUNC( HBB_GETTEXT )
{
   TControl * p = GetCtrlPar( 1 );

   if( p == NULL )
   {
      hb_retc_null();
      return;
   }
   if( p->FHandle )
   {
      int nLen = GetWindowTextLengthA( p->FHandle );
      char * buf = ( char * ) hb_xgrab( nLen + 1 );
      GetWindowTextA( p->FHandle, buf, nLen + 1 );
      lstrcpynA( p->FText, buf, sizeof( p->FText ) );
      hb_retclen_buffer( buf, nLen );
   }
   else
      hb_retc( p->FText );
}

/* HBB_SetFocus( hCtrl ) */
HB_FUNC( HBB_SETFOCUS )
{
   HWND hWnd = CtrlHwnd( 1 );
   if( hWnd )
      SetFocus( hWnd );
}

/* HBB_IsFocus( hCtrl ) --> lFocus */
HB_FUNC( HBB_ISFOCUS )
{
   HWND hWnd = CtrlHwnd( 1 );
   HWND hFocus = GetFocus();
   hb_retl( hWnd != NULL && ( hFocus == hWnd || IsChild( hWnd, hFocus ) ) );
}

/* HBB_FormPostClose( hForm ) - ends modal loop too */
HB_FUNC( HBB_FORMPOSTCLOSE )
{
   HWND hWnd = CtrlHwnd( 1 );
   if( hWnd )
      PostMessage( hWnd, WM_CLOSE, 0, 0 );
}

/* HBB_GetCheck( hCtrl ) --> lChecked */
HB_FUNC( HBB_GETCHECK )
{
   TControl * p = GetCtrlPar( 1 );
   if( p && p->FHandle )
      hb_retl( SendMessage( p->FHandle, BM_GETCHECK, 0, 0 ) == BST_CHECKED );
   else if( p && p->FControlType == CT_CHECKBOX )
      hb_retl( ( ( TCheckBox * ) p )->FChecked );
   else
      hb_retl( FALSE );
}

/* HBB_SetCheck( hCtrl, lChecked ) */
HB_FUNC( HBB_SETCHECK )
{
   TControl * p = GetCtrlPar( 1 );
   BOOL lCheck = hb_parl( 2 );
   if( p && p->FControlType == CT_CHECKBOX )
      ( ( TCheckBox * ) p )->FChecked = lCheck;
   if( p && p->FHandle )
      SendMessage( p->FHandle, BM_SETCHECK, lCheck ? BST_CHECKED : BST_UNCHECKED, 0 );
}

/* HBB_ComboGetSel( hCtrl ) --> nIndex 1 based, 0 = none */
HB_FUNC( HBB_COMBOGETSEL )
{
   HWND hWnd = CtrlHwnd( 1 );
   hb_retni( hWnd ? ( int ) SendMessage( hWnd, CB_GETCURSEL, 0, 0 ) + 1 : 0 );
}

/* HBB_ComboSetSel( hCtrl, nIndex ) 1 based */
HB_FUNC( HBB_COMBOSETSEL )
{
   HWND hWnd = CtrlHwnd( 1 );
   if( hWnd )
      SendMessage( hWnd, CB_SETCURSEL, ( WPARAM ) ( hb_parni( 2 ) - 1 ), 0 );
}

/* HBB_DtpGetDate( hCtrl ) --> cDate "YYYYMMDD" */
HB_FUNC( HBB_DTPGETDATE )
{
   HWND hWnd = CtrlHwnd( 1 );
   SYSTEMTIME st;
   char buf[ 9 ];

   if( hWnd && SendMessage( hWnd, DTM_GETSYSTEMTIME, 0, ( LPARAM ) &st ) == GDT_VALID )
   {
      hb_snprintf( buf, sizeof( buf ), "%04d%02d%02d", st.wYear, st.wMonth, st.wDay );
      hb_retc( buf );
   }
   else
      hb_retc( "" );
}

/* HBB_DtpSetDate( hCtrl, cDate "YYYYMMDD" ) */
HB_FUNC( HBB_DTPSETDATE )
{
   HWND hWnd = CtrlHwnd( 1 );
   const char * szDate = hb_parc( 2 );
   SYSTEMTIME st;

   if( hWnd == NULL || szDate == NULL )
      return;
   if( strlen( szDate ) != 8 || szDate[ 0 ] == ' ' )
   {
      SendMessage( hWnd, DTM_SETSYSTEMTIME, GDT_NONE, 0 );
      return;
   }
   memset( &st, 0, sizeof( st ) );
   st.wYear  = ( WORD ) ( ( szDate[0]-'0' ) * 1000 + ( szDate[1]-'0' ) * 100 + ( szDate[2]-'0' ) * 10 + ( szDate[3]-'0' ) );
   st.wMonth = ( WORD ) ( ( szDate[4]-'0' ) * 10 + ( szDate[5]-'0' ) );
   st.wDay   = ( WORD ) ( ( szDate[6]-'0' ) * 10 + ( szDate[7]-'0' ) );
   SendMessage( hWnd, DTM_SETSYSTEMTIME, GDT_VALID, ( LPARAM ) &st );
}

/* HBB_BrowseClear( hBrowse ) */
HB_FUNC( HBB_BROWSECLEAR )
{
   TControl * p = GetCtrlPar( 1 );
   if( p && p->FControlType == CT_BROWSE )
   {
      if( p->FHandle )
         SendMessage( p->FHandle, LVM_DELETEALLITEMS, 0, 0 );
      ( ( TBrowse * ) p )->FRowCount = 0;
   }
}

/* HBB_BrowseGetRow( hBrowse ) --> nRow 1 based, 0 = none */
HB_FUNC( HBB_BROWSEGETROW )
{
   HWND hWnd = CtrlHwnd( 1 );
   hb_retni( hWnd ? ( int ) SendMessage( hWnd, LVM_GETNEXTITEM, ( WPARAM ) -1, LVNI_SELECTED ) + 1 : 0 );
}

/* HBB_BrowseSetRow( hBrowse, nRow ) 1 based */
HB_FUNC( HBB_BROWSESETROW )
{
   HWND hWnd = CtrlHwnd( 1 );
   LVITEMA lvi;

   if( hWnd == NULL || hb_parni( 2 ) < 1 )
      return;
   memset( &lvi, 0, sizeof( lvi ) );
   lvi.stateMask = LVIS_SELECTED | LVIS_FOCUSED;
   lvi.state     = LVIS_SELECTED | LVIS_FOCUSED;
   SendMessage( hWnd, LVM_SETITEMSTATE, ( WPARAM ) ( hb_parni( 2 ) - 1 ), ( LPARAM ) &lvi );
   SendMessage( hWnd, LVM_ENSUREVISIBLE, ( WPARAM ) ( hb_parni( 2 ) - 1 ), FALSE );
   SendMessage( hWnd, LVM_SETEXTENDEDLISTVIEWSTYLE, LVS_EX_FULLROWSELECT, LVS_EX_FULLROWSELECT );
}

/* HBB_Init() - install hooks (Enter/Escape handling on forms) */
HB_FUNC( HBB_INIT )
{
   InstallHooks();
}

/* HBB_ScreenDpi() --> nDpi (LOGPIXELSY) */
HB_FUNC( HBB_SCREENDPI )
{
   HDC hDC = GetDC( NULL );
   hb_retni( GetDeviceCaps( hDC, LOGPIXELSY ) );
   ReleaseDC( NULL, hDC );
}

/* HBB_BrowseNoFooter( hBrowse ) - footer is a sibling window, not hidden with tab page */
HB_FUNC( HBB_BROWSENOFOOTER )
{
   TControl * p = GetCtrlPar( 1 );
   if( p && p->FControlType == CT_BROWSE )
   {
      TBrowse * br = ( TBrowse * ) p;
      if( br->FFooterWnd )
      {
         DestroyWindow( br->FFooterWnd );
         br->FFooterWnd = NULL;
      }
      if( br->FHandle )
         SetWindowPos( br->FHandle, NULL, br->FLeft, br->FTop, br->FWidth, br->FHeight, SWP_NOZORDER );
   }
}
