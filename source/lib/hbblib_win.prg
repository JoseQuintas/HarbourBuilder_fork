// hbblib_win.prg - runtime stubs for HarbourBuilder as library (Windows)
// Extracted from hbbuilder_win.prg (user app build)

INIT PROCEDURE _InitDPI()
   SetDPIAware()
return

#pragma BEGINDUMP
#include <hbapi.h>
#include <hbapiitm.h>
#include <windows.h>
HB_FUNC( UI_MSGBOX )         { MessageBoxA( GetActiveWindow(), hb_parc(1), hb_parc(2) ? hb_parc(2) : "App", 0x40 ); }
HB_FUNC( UI_MSGYESNO )      { hb_retl( MessageBoxA( GetActiveWindow(), hb_parc(1), hb_parc(2) ? hb_parc(2) : "Confirm", 0x24 ) == 6 ); }
HB_FUNC( MAC_RUNTIMEERRORDIALOG ) { hb_retni( 0 ); }
HB_FUNC( MAC_APPTERMINATE )  { }
HB_FUNC( UI_SCENE3DNEW )    { hb_retnint( 0 ); }
HB_FUNC( UI_EARTHVIEWNEW )  { hb_retnint( 0 ); }
HB_FUNC( UI_MAPNEW )        { hb_retnint( 0 ); }
HB_FUNC( UI_MAPSETREGION )  { }
HB_FUNC( UI_MAPADDPIN )     { }
HB_FUNC( UI_MAPCLEARPINS )  { }
HB_FUNC( UI_MASKEDITNEW )   { hb_retnint( 0 ); }
HB_FUNC( UI_STRINGGRIDNEW ) { hb_retnint( 0 ); }
HB_FUNC( UI_GRIDSETCELL )   { }
HB_FUNC( UI_GRIDGETCELL )   { hb_retc( "" ); }
   // W32_Exec*Dialog stubs - real impls live in hbbuilder_win.obj which DebugApp does NOT link
HB_FUNC( W32_EXECOPENDIALOG )  {
  OPENFILENAMEA ofn; char szFile[260] = ""; char szFilter[1024];
  const char * src = hb_parc(2); int si = 0, di = 0;
  while( src && src[si] && di < (int)sizeof(szFilter)-2 ) {
    szFilter[di++] = ( src[si] == 124 ) ? 0 : src[si]; si++; }
  szFilter[di++] = 0; szFilter[di] = 0;
  memset(&ofn,0,sizeof(ofn)); ofn.lStructSize=sizeof(ofn);
  ofn.hwndOwner=GetActiveWindow(); ofn.lpstrFilter=szFilter;
  ofn.lpstrFile=szFile; ofn.nMaxFile=260;
  ofn.lpstrTitle=hb_parclen(1)?hb_parc(1):NULL;
  ofn.lpstrInitialDir=(hb_parc(3)&&hb_parc(3)[0])?hb_parc(3):NULL;
  ofn.lpstrDefExt=(hb_parc(4)&&hb_parc(4)[0])?hb_parc(4):NULL;
  ofn.Flags=0x00001000 | 0x00000800 | 0x00000004;
  if(GetOpenFileNameA(&ofn)) hb_retc(szFile); else hb_retc("");
}
HB_FUNC( W32_EXECSAVEDIALOG )  {
  OPENFILENAMEA ofn; char szFile[260] = ""; char szFilter[1024];
  const char * src = hb_parc(2); const char * nm = hb_parc(5);
  int si = 0, di = 0;
  while( src && src[si] && di < (int)sizeof(szFilter)-2 ) {
    szFilter[di++] = ( src[si] == 124 ) ? 0 : src[si]; si++; }
  szFilter[di++] = 0; szFilter[di] = 0;
  if(nm && nm[0]) lstrcpynA(szFile, nm, 260);
  memset(&ofn,0,sizeof(ofn)); ofn.lStructSize=sizeof(ofn);
  ofn.hwndOwner=GetActiveWindow(); ofn.lpstrFilter=szFilter;
  ofn.lpstrFile=szFile; ofn.nMaxFile=260;
  ofn.lpstrTitle=hb_parclen(1)?hb_parc(1):NULL;
  ofn.lpstrInitialDir=(hb_parc(3)&&hb_parc(3)[0])?hb_parc(3):NULL;
  ofn.lpstrDefExt=(hb_parc(4)&&hb_parc(4)[0])?hb_parc(4):NULL;
  ofn.Flags=0x00000002 | 0x00000004;
  if(GetSaveFileNameA(&ofn)) hb_retc(szFile); else hb_retc("");
}
HB_FUNC( W32_EXECFONTDIALOG )  {
  CHOOSEFONTA cf; LOGFONTA lf; HDC hdc;
  const char * nm = hb_parc(1);
  memset(&lf,0,sizeof(lf));
  if(nm && nm[0]) lstrcpynA(lf.lfFaceName, nm, 32);
  else lstrcpyA(lf.lfFaceName, "Segoe UI");
  hdc=GetDC(NULL); lf.lfHeight=-MulDiv(hb_parni(2)>0?hb_parni(2):10,GetDeviceCaps(hdc,90),72);
  ReleaseDC(NULL,hdc);
  lf.lfWeight=(hb_parni(4)&1)?700:400;
  lf.lfItalic=(hb_parni(4)&2)?1:0; lf.lfUnderline=(hb_parni(4)&4)?1:0;
  lf.lfCharSet=1;
  memset(&cf,0,sizeof(cf)); cf.lStructSize=sizeof(cf);
  cf.hwndOwner=GetActiveWindow(); cf.lpLogFont=&lf;
  cf.rgbColors=hb_parni(3);
  cf.Flags=0x00000001 | 0x00000040 | 0x00000100;
  if(ChooseFontA(&cf)) {
    PHB_ITEM a; int sty=0,pts; hdc=GetDC(NULL);
    pts=MulDiv(-lf.lfHeight,72,GetDeviceCaps(hdc,90)); ReleaseDC(NULL,hdc);
    if(lf.lfWeight>=700) sty|=1; if(lf.lfItalic) sty|=2; if(lf.lfUnderline) sty|=4;
    a=hb_itemArrayNew(4); hb_arraySetC(a,1,lf.lfFaceName);
    hb_arraySetNI(a,2,pts); hb_arraySetNI(a,3,(int)cf.rgbColors);
    hb_arraySetNI(a,4,sty); hb_itemReturnRelease(a);
  } else hb_ret();
}
HB_FUNC( W32_EXECCOLORDIALOG ) {
  CHOOSECOLORA cc; static COLORREF custColors[16] = {0};
  memset(&cc,0,sizeof(cc)); cc.lStructSize=sizeof(cc);
  cc.hwndOwner=GetActiveWindow(); cc.rgbResult=(COLORREF)hb_parni(1);
  cc.lpCustColors=custColors; cc.Flags=0x00000001 | 0x00000002;
  if(ChooseColorA(&cc)) hb_retni((int)cc.rgbResult); else hb_retni(-1);
}
HB_FUNC( W32_ERRORDIALOG ) {
  const char * msg = hb_parc(1);
  HWND hDlg, hEdit, hBtn;
  MSG m; HFONT hF;
  hDlg = CreateWindowExA(0,"STATIC","Runtime Error",
    WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_VISIBLE,
    100,100,600,400,NULL,NULL,GetModuleHandle(NULL),NULL);
  hF = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
  hEdit = CreateWindowExA(WS_EX_CLIENTEDGE,"EDIT",msg,
    WS_CHILD|WS_VISIBLE|WS_VSCROLL|ES_MULTILINE|ES_READONLY|ES_AUTOVSCROLL,
    8,8,576,320,hDlg,NULL,GetModuleHandle(NULL),NULL);
  SendMessage(hEdit,WM_SETFONT,(WPARAM)hF,TRUE);
  hBtn = CreateWindowExA(0,"BUTTON","OK",
    WS_CHILD|WS_VISIBLE|BS_DEFPUSHBUTTON,
    260,340,80,28,hDlg,NULL,GetModuleHandle(NULL),NULL);
  SendMessage(hBtn,WM_SETFONT,(WPARAM)hF,TRUE);
  while(GetMessage(&m,NULL,0,0)) {
    TranslateMessage(&m); DispatchMessage(&m);
    if(m.message==WM_LBUTTONUP && m.hwnd==hBtn) break;
    if(m.message==WM_CLOSE || m.message==WM_DESTROY) break;
  }
  DestroyWindow(hDlg);
}
#pragma ENDDUMP
