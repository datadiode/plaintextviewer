#include <windows.h>
#include <commctrl.h>
#include <richedit.h>

#define IDD_MAINWINDOW                          100
#define IDD_TEXTBOX                             101
#define IDM_WHOLE_WORD                          10000
#define IDM_NOTHING                             10001
#define IDM_REFRESH                             10002
#define IDC_LINE                                40000
#define IDM_BEGINS_WITH                         40000
#define IDC_TEXT                                40001
#define IDM_ENDS_WITH                           40001
#define IDM_INVERT                              40002
#define IDC_LIST                                40002
#define IDC_DROPTARGET                          40003
#define IDM_LITERAL                             40003
#define IDC_IDIOMS                              40004
#define IDM_IGNORE_CASE                         40004
#define IDC_SCROLL                              40005
#define IDM_CODEPAGE_UTF7                       40005
#define IDM_USE_AGREP                           40006
#define IDM_ABOUT                               40007
#define IDM_STOP                                40008
#define IDM_TABWIDTH                            40009
#define IDM_CODEPAGE_ANSI                       40010
#define IDM_CODEPAGE_OEM                        40011
#define IDM_CODEPAGE_UTF8                       40012
#define IDM_CODEPAGE_UCS2LE                     40013
#define IDM_CODEPAGE_UCS2BE                     40014
#define IDM_CODEPAGE_CUSTOM                     40015
#define IDM_OPEN                                40016
#define IDM_OPEN_UTF8                           40017
#define IDM_OPEN_UCS2LE                         40018
#define IDM_OPEN_UCS2BE                         40019
#define IDM_OPEN_XML                            40020
#define IDM_SELECT_FONT                         40021
#define IDM_USE_DEFAULT_FONT                    40022
