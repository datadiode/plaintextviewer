/*
 * Copyright (c) 2015 Jochen Neubeck
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM DISCLAIMS
 * ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL INTERNET SOFTWARE
 * CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 */
#include "resource.h"
#include <vld.h>
#include <commctrl.h>

#include <string.h>
#include <stdio.h>

#include "util.h"
#include "subclass.h"
#include "LineReader.h"
#include "Transcoder.h"
#include "VersionData.h"
#include "EncodingInfo.h"

static HINSTANCE hInstance;
static HINSTANCE hCharsets;

static struct {
	VersionData const *const pVersionData;
	VersionData const *const pStringFileInfo;
	LPCWSTR const ProductName;
	LPCWSTR const ProductVersion;
	LPCWSTR const LegalCopyright;
	LPCWSTR const Comments;
} VersionInfo = {
	VersionData::Load(),
	VersionInfo.pVersionData->Find(L"StringFileInfo")->First(),
	VersionInfo.pStringFileInfo->Find(L"ProductName")->Data(),
	VersionInfo.pStringFileInfo->Find(L"ProductVersion")->Data(),
	VersionInfo.pStringFileInfo->Find(L"LegalCopyright")->Data(),
	VersionInfo.pStringFileInfo->Find(L"Comments")->Data(),
};

static TCHAR IniPath[MAX_PATH];

static void InitWindowPlacement(HWND hwnd, LPCTSTR name)
{
	WindowPlacement wp;
	TCHAR buf[128];
	if (GetPrivateProfileString(_T("WindowPlacement"), name, NULL, buf, _countof(buf), IniPath))
	{
		wp.Parse(buf);
		if (wp.showCmd != SW_HIDE)
			SetWindowPlacement(hwnd, &wp);
	}
}

static void SaveWindowPlacement(HWND hwnd, LPCTSTR name)
{
	WindowPlacement wp;
	if (GetCapture() == NULL && IsWindowVisible(hwnd) && GetWindowPlacement(hwnd, &wp))
	{
		TCHAR buf[128];
		wp.Print(buf);
		WritePrivateProfileString(_T("WindowPlacement"), name, buf, IniPath);
	}
}

static void InitIcon(HWND hwnd)
{
	if (HICON hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(1)))
	{
		SendMessage(hwnd, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(hIcon));
		SendMessage(hwnd, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(hIcon));
	}
}

static BOOL FilenameExtensionQualifiesAs(LPCTSTR ext, LPCTSTR tag)
{
	TCHAR buf[MAX_PATH];
	return GetPrivateProfileString(_T("FileTypes"), tag, NULL, buf, _countof(buf), IniPath)
		&& PathMatchSpec(ext[0] == _T('.') && ext[1] != _T('\0') ? ext + 1 : _T("."), buf);
}

struct LineData
{
	DWORD LowPart;
	UINT DECLARE_BIT_FIELD(HighPart, 6);
	UINT DECLARE_BIT_FIELD(flags, 2);
	UINT DECLARE_BIT_FIELD(len, 24);
};

// Do a few compile-time sanity checks
C_ASSERT(sizeof(LineData) == 8);
C_ASSERT(MIN_BIT_FIELD_SIGNED(LineData, len) == 0x800000);
C_ASSERT(MAX_BIT_FIELD_SIGNED(LineData, len) == 0x7FFFFF);
C_ASSERT(MAX_BIT_FIELD_UNSIGNED(LineData, len) == 0xFFFFFF);

class TextBoxDialog : public Subclass
{
	HWND m_hwndText;
public:
	BSTR m_text;
	HFONT m_font;
	TCHAR m_title[512];
	explicit TextBoxDialog(BSTR text = NULL, HFONT font = NULL)
		: m_text(text)
		, m_font(font)
	{
		m_title[0] = _T('\0');
	}
private:
	virtual LRESULT DoMsg(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
	{
		switch (message)
		{
		case WM_INITDIALOG:
			m_hwndText = GetDlgItem(hwnd, IDC_TEXT);
			if (m_title[0])
				SetWindowText(hwnd, m_title);
			InitIcon(hwnd);
			if (m_font)
				SendMessage(m_hwndText, WM_SETFONT, reinterpret_cast<WPARAM>(m_font), 0);
			SetWindowTextW(m_hwndText, m_text);
			InitWindowPlacement(hwnd, _T("TextBox"));
			return TRUE;
		case WM_CTLCOLORSTATIC:
			return reinterpret_cast<LRESULT>(GetSysColorBrush(COLOR_WINDOW));
		case WM_SIZE:
			if (wParam != SIZE_MAXHIDE && wParam != SIZE_MAXSHOW && wParam != SIZE_MINIMIZED)
				SetWindowPos(m_hwndText, NULL, 0, 0, LOWORD(lParam), HIWORD(lParam), SWP_NOZORDER);
			// fall through
		case WM_EXITSIZEMOVE:
			SaveWindowPlacement(hwnd, _T("TextBox"));
			break;
		case WM_COMMAND:
			switch (wParam)
			{
			case IDOK:
			case IDCANCEL:
				EndDialog(hwnd, wParam);
				break;
			}
			return TRUE;
		}
		return FALSE;
	}
};

class MainWindow
	: public Subclass
	, public IDropTarget
	, Subclass::DlgItem<IDC_LINE>
	, Subclass::DlgItem<IDC_TEXT>
	, Subclass::DlgItem<IDC_LIST>
{
public:
	MainWindow(LPTSTR);
	~MainWindow()
	{
		Close();
	}

	STDMETHOD(QueryInterface)(REFIID riid, void **ppvObject)
	{
		*ppvObject = NULL;
		if (InlineIsEqualGUID(riid, IID_IUnknown) || InlineIsEqualGUID(riid, IID_IDropTarget))
			*ppvObject = static_cast<IDropTarget *>(this);
		else return E_NOINTERFACE;
		return S_OK;
	}
	STDMETHOD_(ULONG, AddRef)()
	{
		return 1;
	}
	STDMETHOD_(ULONG, Release)()
	{
		return 1;
	}
	STDMETHOD(DragEnter)(IDataObject *pDataObj, DWORD grfKeyState, POINTL pt, DWORD *pdwEffect)
	{
		return S_OK;
	}
	STDMETHOD(DragOver)(DWORD grfKeyState, POINTL pt, DWORD *pdwEffect)
	{
		TCHITTESTINFO ht = { { pt.x, pt.y }, 0 };
		MapWindowPoints(NULL, m_hwndDropTarget, &ht.pt, 1);
		int i = TabCtrl_HitTest(m_hwndDropTarget, &ht);
		TabCtrl_SetCurSel(m_hwndDropTarget, i);
		*pdwEffect = i != -1 ? DROPEFFECT_COPY : DROPEFFECT_NONE;
		return S_OK;
	}
	STDMETHOD(DragLeave)()
	{
		TabCtrl_SetCurSel(m_hwndDropTarget, -1);
		return S_OK;
	}
	STDMETHOD(Drop)(IDataObject *pDataObj, DWORD grfKeyState, POINTL pt, DWORD *pdwEffect)
	{
		int i = TabCtrl_SetCurSel(m_hwndDropTarget, -1);
		TCITEM item;
		item.mask = TCIF_PARAM;
		if (TabCtrl_GetItem(m_hwndDropTarget, i, &item))
		{
			FORMATETC descriptor_format = { CF_HDROP, NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
			STGMEDIUM storage = { 0, 0, 0 };
			HRESULT hr = pDataObj->GetData(&descriptor_format, &storage);
			if (SUCCEEDED(hr))
			{
				HDROP hDrop = reinterpret_cast<HDROP>(storage.hGlobal);
				TCHAR path[MAX_PATH];
				if (DragQueryFile(hDrop, 0, path, _countof(path)))
					Open(path, HIWORD(item.lParam));
				GlobalFree(storage.hGlobal);
			}
		}
		SetForegroundWindow(m_hwnd);
		return S_OK;
	}

private:
	virtual LRESULT DoMsg(HWND, UINT, WPARAM, LPARAM);
	virtual LRESULT DoMsg(Subclass::DlgItem<IDC_LINE> &, HWND, UINT, WPARAM, LPARAM);
	virtual LRESULT DoMsg(Subclass::DlgItem<IDC_TEXT> &, HWND, UINT, WPARAM, LPARAM);
	virtual LRESULT DoMsg(Subclass::DlgItem<IDC_LIST> &, HWND, UINT, WPARAM, LPARAM);
	void Init(HWND);
	void AddDropTarget(LPARAM);
	void Refresh();
	void UpdateLayout();
	void UpdateUseAgrep();
	void SetFont(HFONT);
	void Open(WORD);
	void SelectFont();
	void UseDefaultFont();
	void AboutBox();
	void UpdateWindowTitle();
	void AdjustScrollRange();
	void DoHScroll(WORD);
	void IndicateProgress(DWORD lines, DWORD ticks);
	void DoDrawItem(DRAWITEMSTRUCT *);
	void DoMeasureItem(MEASUREITEMSTRUCT *);
	void DoActivate(WPARAM);
	void Open(LPCTSTR, WORD);
	void SelectLine(int);
	void DoSearch(int);
	void DoStep(int, int = 0);
	void SetTabWidth(UINT);
	void SetCodePage(UINT);
	int InitCodePageMenu(HMENU, int);
	void ChooseIdiom();
	BSTR Transcode(BSTR) const;
	BSTR ReadLine(DWORD) const;
	void CopySelectionToClipboard();
	void SetEncodingInfoFromName(char *);

	LRESULT DoCustomDraw(NMLVCUSTOMDRAW *);
	LRESULT DoItemActivate(NMITEMACTIVATE *);
	LRESULT DoKeydown(NMLVKEYDOWN *);
	LRESULT DoGetObject(NMOBJECTNOTIFY *);

	typedef void (CALLBACK *DrawMenuProc)(DRAWITEMSTRUCT *);
	static void CALLBACK DrawMenuDefault(DRAWITEMSTRUCT *);
	static void CALLBACK DrawMenuDropdown(DRAWITEMSTRUCT *);
	static void CALLBACK DrawMenuCheckbox(DRAWITEMSTRUCT *);

	DWORD ReadThread();
	static DWORD WINAPI StartReadThread(LPVOID);

	LineData *Reserve(WORD);
	LineData *GetAt(DWORD) const;
	HANDLE Open(LPCTSTR);
	void Close();

	static const UINT ReadThreadFinishedTimer = 1;

	HWND m_hwnd;
	LONG_PTR m_super;
	HFONT m_font;
	HFONT const m_uifont;
	HMENU m_menu;
	HWND m_hwndLine;
	HWND m_hwndText;
	HWND m_hwndList;
	HWND m_hwndStatus;
	HWND m_hwndIdioms;
	HWND m_hwndScroll;
	HWND m_hwndDropTarget;
	LONG m_refcount;
	HANDLE m_thread;
	HANDLE m_handle; // Handle to current file
	LineData **m_index;
	bool m_stop;
	DWORD m_then;
	UINT m_lines;
	UINT m_width;
	UINT m_offset;
	UINT m_tabwidth;
	UINT m_codepage;
	UINT m_tabwidth_backup;
	UINT m_codepage_backup;
	LineReader::Encoding m_encoding;
	EncodingInfo const *m_encodinginfo;
	EncodingInfo m_genericencodinginfo;
	char m_delimiter;
	LONG m_right;
	LONG m_left;
	POINT m_fixpoint;
	TCHAR m_path[MAX_PATH];
};

MainWindow::MainWindow(LPTSTR arg)
	: m_hwnd(NULL)
	, m_super(NULL)
	, m_font(reinterpret_cast<HFONT>(GetStockObject(ANSI_FIXED_FONT)))
	, m_uifont(reinterpret_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT)))
	, m_menu(NULL)
	, m_hwndLine(NULL)
	, m_hwndText(NULL)
	, m_hwndStatus(NULL)
	, m_hwndIdioms(NULL)
	, m_hwndScroll(NULL)
	, m_hwndDropTarget(NULL)
	, m_refcount(0)
	, m_thread(NULL)
	, m_handle(INVALID_HANDLE_VALUE)
	, m_index(NULL)
	, m_stop(false)
	, m_then(0)
	, m_lines(0)
	, m_width(0)
	, m_offset(0)
	, m_tabwidth(8)
	, m_tabwidth_backup(m_tabwidth)
	, m_codepage(CP_ACP)
	, m_codepage_backup(m_codepage)
	, m_encoding(LineReader::NONE)
	, m_encodinginfo(NULL)
	, m_delimiter('\n')
{
	m_path[0] = _T('\0');

	while (size_t len = PathGetArgs(arg) - arg)
	{
		PathRemoveArgs(arg);
		PathUnquoteSpaces(arg);
		if (LPCTSTR option = EatPrefix(arg, _T("/ENCODING=")))
		{
			WORD mode = MAKEWORD(LineReader::NONE, '\n');
			if (PathMatchSpec(option, _T("ANSI")))
				mode = MAKEWORD(LineReader::ANSI, '\n');
			else if(PathMatchSpec(option, _T("UTF8")))
				mode = MAKEWORD(LineReader::UTF8, '\n');
			else if(PathMatchSpec(option, _T("UCS2LE")))
				mode = MAKEWORD(LineReader::UCS2LE, '\n');
			else if(PathMatchSpec(option, _T("UCS2BE")))
				mode = MAKEWORD(LineReader::UCS2BE, '\n');
			else if(PathMatchSpec(option, _T("XML")))
				mode = MAKEWORD(LineReader::NONE, '>');
			m_encoding = static_cast<LineReader::Encoding>(LOBYTE(mode));
			m_delimiter = static_cast<char>(HIBYTE(mode));
		}
		else if (LPCTSTR option = EatPrefix(arg, _T("/TABWIDTH=")))
		{
			UINT tabwidth = static_cast<UINT>(_ttoi(option));
			if (tabwidth >= 1 && tabwidth <= 32 && (tabwidth & tabwidth - 1) == 0)
				m_tabwidth_backup = m_tabwidth = tabwidth;
		}
		else
		{
			PathCanonicalize(m_path, arg);
		}
		arg += len;
	}

	TCHAR buf[128];
	if (GetPrivateProfileString(_T("Settings"), _T("Font"), NULL, buf, _countof(buf), IniPath))
	{
		LogFont lf;
		lf.Parse(buf);
		m_font = CreateFontIndirect(&lf);
	}
}

void MainWindow::Init(HWND hwnd)
{
	m_hwnd = hwnd;
	m_menu = GetMenu(hwnd);
	m_hwndLine = DlgItem<IDC_LINE>::Init(hwnd);
	m_hwndText = DlgItem<IDC_TEXT>::Init(hwnd);
	m_hwndScroll = GetDlgItem(hwnd, IDC_SCROLL);
	m_hwndList = DlgItem<IDC_LIST>::Init(hwnd);
	m_hwndDropTarget = GetDlgItem(hwnd, IDC_DROPTARGET);
	m_hwndIdioms = GetDlgItem(hwnd, IDC_IDIOMS);

	InitIcon(m_hwnd);

	AdjustScrollRange();

	int n = GetMenuItemCount(m_menu);
	MENUITEMINFO mii;
	mii.cbSize = sizeof mii;
	mii.fMask = MIIM_BITMAP | MIIM_DATA;
	mii.hbmpItem = HBMMENU_CALLBACK;
	mii.dwItemData = reinterpret_cast<ULONG_PTR>(DrawMenuDropdown);
	while (n)
	{
		--n;
		UINT state = GetMenuState(m_menu, n, MF_BYPOSITION);
		if (state & MF_HELP)
			continue;
		if ((state & MF_POPUP) == 0 || mii.dwItemData == reinterpret_cast<ULONG_PTR>(DrawMenuDropdown))
		{
			SetMenuItemInfo(m_menu, n, TRUE, &mii);
		}
		if (state & MF_POPUP)
		{
			HMENU menu = GetSubMenu(m_menu, n);
			int n = GetMenuItemCount(menu);
			mii.dwItemData = reinterpret_cast<ULONG_PTR>(DrawMenuDefault);
			while (n)
			{
				--n;
				SetMenuItemInfo(menu, n, TRUE, &mii);
			}
		}
		mii.dwItemData = reinterpret_cast<ULONG_PTR>(DrawMenuCheckbox);
	}

	if (GetPrivateProfileInt(_T("Settings"), _T("UseAgrep"), 0, IniPath))
	{
		CheckMenuItem(m_menu, IDM_USE_AGREP, MF_CHECKED);
		UpdateUseAgrep();
	}
	EnableMenuItem(m_menu, IDM_USE_AGREP, MF_ENABLED);

	TabCtrl_SetExtendedStyle(m_hwndDropTarget, TCS_EX_REGISTERDROP);
	AddDropTarget(MAKELPARAM(IDM_OPEN, MAKEWORD(LineReader::NONE, '\n')));
	AddDropTarget(MAKELPARAM(IDM_OPEN_UTF8, MAKEWORD(LineReader::UTF8, '\n')));
	AddDropTarget(MAKELPARAM(IDM_OPEN_UCS2LE, MAKEWORD(LineReader::UCS2LE, '\n')));
	AddDropTarget(MAKELPARAM(IDM_OPEN_UCS2BE, MAKEWORD(LineReader::UCS2BE, '\n')));
	AddDropTarget(MAKELPARAM(IDM_OPEN_XML, MAKEWORD(LineReader::NONE, '>')));
	TabCtrl_SetCurSel(m_hwndDropTarget, -1);

	TCITEM item;
	item.mask = TCIF_TEXT;
	item.pszText = _T("Idioms");
	TabCtrl_InsertItem(m_hwndIdioms, 0, &item);
	TabCtrl_SetCurSel(m_hwndIdioms, -1);

	SendMessage(m_hwndLine, EM_LIMITTEXT, 10, 0);
	ListView_SetExtendedListViewStyle(m_hwndList, LVS_EX_FULLROWSELECT);

	RECT rc;
	GetClientRect(m_hwndText, &rc);
	m_hwndStatus = CreateWindowEx(0, WC_STATIC, _T("Drop a file onto this area"), SS_CENTERIMAGE | WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS, 0, 0, 10000, rc.bottom, m_hwndText, NULL, NULL, NULL);

	if (WPARAM font = SendMessage(m_hwnd, WM_GETFONT, 0, 0))
	{
		SendMessage(m_hwndStatus, WM_SETFONT, font, 1);
	}

	SetFont(m_font);

	LVCOLUMN col;
	GetWindowRect(m_hwndLine, &rc);
	col.mask = LVCF_WIDTH;
	col.cx = rc.right - rc.left;
	ListView_InsertColumn(m_hwndList, 0, &col);
	col.cx = LVSCW_AUTOSIZE_USEHEADER;
	ListView_InsertColumn(m_hwndList, 1, &col);

	SetActiveWindow(m_hwnd);
	InitWindowPlacement(m_hwnd, _T("MainWindow"));
	Refresh();
}

void MainWindow::AddDropTarget(LPARAM param)
{
	TCITEM item;
	item.mask = TCIF_TEXT | TCIF_PARAM;
	item.lParam = param;
	TCHAR text[80];
	GetMenuString(m_menu, LOWORD(param), text, _countof(text), MF_BYCOMMAND);
	item.pszText = TrimString(text, _T("."));
	SHStripMneumonic(item.pszText);
	TabCtrl_InsertItem(m_hwndDropTarget, TabCtrl_GetItemCount(m_hwndDropTarget), &item);
}

void MainWindow::Refresh()
{
	if (m_path[0] != _T('\0'))
	{
		WORD mode = MAKEWORD(m_encoding, m_delimiter);
		m_delimiter = '\0'; // triggers update of IDM_USE_AGREP state
		Open(m_path, mode);
	}
}

void MainWindow::AboutBox()
{
	MSGBOXPARAMS params;
	ZeroMemory(&params, sizeof params);
	params.cbSize = sizeof params;
	params.dwStyle = MB_USERICON;
	params.hInstance = hInstance;
	params.lpszCaption = _T("About Plain Text Viewer");
	TCHAR text[1024];
	wsprintf(text, _T("%ls v%ls\n%ls\ncharacter-sets.dll v%ls\n%ls"),
		VersionInfo.ProductName,
		VersionInfo.ProductVersion,
		VersionInfo.LegalCopyright,
		hCharsets ? VersionData::Load(hCharsets)
			->Find(L"StringFileInfo")->First()
			->Find(L"FileVersion")->Data() : L"<missing>",
		VersionInfo.Comments);
	params.lpszText = text;
	params.lpszIcon = MAKEINTRESOURCE(1);
	params.hwndOwner = m_hwnd;
	MessageBoxIndirect(&params);
}

BSTR MainWindow::Transcode(BSTR text) const
{
	UINT count = SysStringByteLen(text);
	switch (m_codepage)
	{
	case 1201: // UCS2BE
		_swab(reinterpret_cast<LPSTR>(text), reinterpret_cast<LPSTR>(text), count);
		// fall through
	case 1200: // UCS2LE
		break;
	default:
		if (BSTR wide = SysAllocStringLen(NULL, count))
		{
			count = MultiByteToWideChar(m_codepage, m_codepage != CP_UTF8 ? MB_USEGLYPHCHARS : 0, reinterpret_cast<LPSTR>(text), count, wide, count);
			SysReAllocStringLen(&wide, NULL, count);
			SysFreeString(text);
			text = wide;
		}
		else
		{
			SysFreeString(text);
			text = NULL;
		}
		break;
	}
	return text;
}

BSTR MainWindow::ReadLine(DWORD dw) const
{
	BSTR wide = NULL;
	if (LineData *linedata = GetAt(dw))
	{
		LARGE_INTEGER pos;
		pos.LowPart = linedata->LowPart;
		pos.HighPart = linedata->HighPart;
		DWORD count = linedata->len;
		switch (m_codepage)
		{
		case 1200:
			if (pos.LowPart & 1)
				++pos.QuadPart;
			if (count & 1)
				++count;
			break;
		}
		SetFilePointerEx(m_handle, pos, NULL, FILE_BEGIN);
		if (BSTR text = SysAllocStringByteLen(NULL,  count))
		{
			ReadFile(m_handle, text, count, &count, NULL);
			wide = Transcode(text);
		}
	}
	return wide;
}

void MainWindow::CopySelectionToClipboard()
{
	if (OpenClipboard(m_hwnd))
	{
		IStream *pstm = NULL;
		if (SUCCEEDED(CreateStreamOnHGlobal(NULL, FALSE, &pstm)))
		{
			DWORD total = 0;
			int i = -1;
			while ((i = ListView_GetNextItem(m_hwndList, i, LVNI_SELECTED)) != -1)
			{
				if (total >= 0x400000)
				{
					MessageBox(m_hwnd, _T("Data beyond 4MB has been truncated!"), _T("Clipboard"), MB_ICONWARNING);
					break;
				}
				if (BSTR text = ReadLine(i))
				{
					DWORD count = SysStringByteLen(text);
					pstm->Write(text, count, NULL);
					total += count;
					SysFreeString(text);
				}
			}
			pstm->Write(L"", sizeof(wchar_t), NULL);
			HGLOBAL hGlobal = NULL;
			if (EmptyClipboard() && SUCCEEDED(GetHGlobalFromStream(pstm, &hGlobal)) && !SetClipboardData(CF_UNICODETEXT, hGlobal))
				GlobalFree(hGlobal);
			pstm->Release();
		}
		CloseClipboard();
	}
}

void MainWindow::SetEncodingInfoFromName(char *name)
{
	if (name != NULL)
	{
		RemoveLeadingZeros(name);
		m_encodinginfo = EncodingInfo::From<IMAGE_NT_HEADERS32>(hCharsets, name);
		if (m_encodinginfo == NULL)
		{
			if (sscanf(name, "windows-%hu",	&m_genericencodinginfo.cp) == 1 ||
				sscanf(name, "windows%hu",	&m_genericencodinginfo.cp) == 1 ||
				sscanf(name, "cp-%hu",		&m_genericencodinginfo.cp) == 1 ||
				sscanf(name, "cp%hu",		&m_genericencodinginfo.cp) == 1 ||
				sscanf(name, "msdos-%hu",	&m_genericencodinginfo.cp) == 1 ||
				sscanf(name, "msdos%hu",	&m_genericencodinginfo.cp) == 1)
			{
				name = NULL;
			}
		}
	}
	if (name == NULL)
	{
		wsprintfA(m_genericencodinginfo.name, "windows-%u", m_genericencodinginfo.cp);
		m_encodinginfo = &m_genericencodinginfo;
	}
}

static void GetCellRect(HWND hwnd, int iItem, int iSubItem, RECT &rc)
{
	RECT rch;
	ListView_GetItemRect(hwnd, iItem, &rc, LVIR_BOUNDS);
	ListView_GetSubItemRect(hwnd, iItem, iSubItem, LVIR_LABEL, &rch);
	if (iSubItem != 0)
		rc.left = rch.left;
	rc.right = rch.right;
}

UINT UnwrapLine(LPWSTR text, UINT n)
{
	bool eat = true;
	UINT w = 0;
	for (UINT i = 0; i < n; ++i)
	{
		switch (OLECHAR c = text[i])
		{
		case '\n':
			eat = true;
			if (w != 0)
				text[w++] = ' ';
			// fall through
		case '\r':
			break;
		case ' ':
		case '\t':
			if (!eat)
				text[w++] = ' ';
			break;
		default:
			eat = false;
			text[w++] = c;
			break;
		}
	}
	return w;
}

LRESULT MainWindow::DoCustomDraw(NMLVCUSTOMDRAW *pnm)
{
	RECT rc;

	switch (pnm->nmcd.dwDrawStage)
	{
	case CDDS_PREPAINT:
		return CDRF_NOTIFYITEMDRAW | CDRF_NOTIFYPOSTPAINT;

	case CDDS_ITEM | CDDS_PREPAINT:
		{
			UINT state = ListView_GetItemState(m_hwndList, pnm->nmcd.dwItemSpec, LVIS_SELECTED);
			LineData const *const linedata = GetAt(static_cast<DWORD>(pnm->nmcd.dwItemSpec));
			COLORREF bkgnd = GetSysColor(COLOR_WINDOW);
			COLORREF color = linedata && (linedata->flags & 1) ? RGB(255, 0, 0) : GetSysColor(COLOR_WINDOWTEXT);
			if (m_hwndList != GetFocus())
			{
				if (state & LVIS_SELECTED)
					bkgnd = GetSysColor(COLOR_BTNFACE);
				state = 0;
			}
			(state & LVIS_SELECTED ? SetBkColor : SetTextColor)(pnm->nmcd.hdc, color);
			(state & LVIS_SELECTED ? SetTextColor : SetBkColor)(pnm->nmcd.hdc, bkgnd);
		}
		return CDRF_NOTIFYSUBITEMDRAW | CDRF_NOTIFYPOSTPAINT;

	case CDDS_ITEM | CDDS_SUBITEM | CDDS_PREPAINT:
		// Erase the cell's background
		GetCellRect(m_hwndList, static_cast<DWORD>(pnm->nmcd.dwItemSpec), pnm->iSubItem, rc);
		ExtTextOut(pnm->nmcd.hdc, rc.left, rc.top, ETO_OPAQUE, &rc, NULL, 0, NULL);
		// Draw the cell's content
		switch (pnm->iSubItem)
		{
		case 0:
			{
				DrawEdge(pnm->nmcd.hdc, &rc, BDR_INNER, BF_RIGHT | BF_FLAT);

				rc.right = m_right;

				WCHAR text[12];
				DWORD count = wsprintfW(text, L"%u", pnm->nmcd.dwItemSpec + 1);

				SIZE ext;
				GetTextExtentPoint32(pnm->nmcd.hdc, L"", 1, &ext);
				rc.top += (rc.bottom - rc.top - ext.cy) / 2;

				DrawTextW(pnm->nmcd.hdc, text, count, &rc, DT_NOPREFIX | DT_RIGHT | DT_SINGLELINE);
			}
			break;

		case 1:
			{
				BSTR text = ReadLine(static_cast<DWORD>(pnm->nmcd.dwItemSpec));

				UINT const width = m_delimiter != '\n' ?
					UnwrapLine(text, SysStringLen(text)) :
					SysStringLen(text = ExpandTabs(text, m_tabwidth));

				if (m_width < width)
					m_width = width;

				if (m_offset < width)
				{
					rc.left = m_left;

					SIZE ext;
					UINT count = width - m_offset;
					UINT visible = 0;
					do
					{
						visible += 256;
						if (visible > count)
							visible = count;
						GetTextExtentPoint32(pnm->nmcd.hdc, text + m_offset, visible, &ext);
					} while (ext.cx < rc.right - rc.left && visible < count);

					rc.top += (rc.bottom - rc.top - ext.cy) / 2;
					ExtTextOutW(pnm->nmcd.hdc, rc.left, rc.top, 0, &rc, text + m_offset, visible, NULL);
				}

				SysFreeString(text);
			}
			break;
		}
		return CDRF_SKIPDEFAULT;

	case CDDS_ITEM | CDDS_POSTPAINT:
		{
			UINT state = ListView_GetItemState(m_hwndList, pnm->nmcd.dwItemSpec, LVIS_SELECTED | LVIS_FOCUSED);
			if ((state & LVIS_FOCUSED) && (pnm->nmcd.uItemState & CDIS_FOCUS))
			{
				ListView_GetItemRect(m_hwndList, pnm->nmcd.dwItemSpec, &rc, LVIR_BOUNDS);
				DrawFocusRect(pnm->nmcd.hdc, &rc);
			}
		}
		break;

	case CDDS_POSTPAINT:
		AdjustScrollRange();
		break;
	}
	return CDRF_DODEFAULT;
}

LRESULT MainWindow::DoItemActivate(NMITEMACTIVATE *pnm)
{
	if (BSTR text = ReadLine(pnm->iItem))
	{
		TextBoxDialog dlg(text, m_font);
		GetWindowText(m_hwnd, dlg.m_title, _countof(dlg.m_title));
		dlg.Modal(hInstance, MAKEINTRESOURCEW(IDD_TEXTBOX), m_hwnd);
		SysFreeString(text);
	}
	return 0;
}

LRESULT MainWindow::DoKeydown(NMLVKEYDOWN *pnm)
{
	WORD wMod = 0;
	if (GetKeyState(VK_MENU) < 0)
		wMod |= MOD_ALT;
	if (GetKeyState(VK_CONTROL) < 0)
		wMod |= MOD_CONTROL;
	if (GetKeyState(VK_SHIFT) < 0)
		wMod |= MOD_SHIFT;
	switch (MAKELONG(wMod, pnm->wVKey))
	{
	case MAKELONG(MOD_CONTROL, 'C'):
	case MAKELONG(MOD_CONTROL, VK_INSERT):
		CopySelectionToClipboard();
		return 1;
	case MAKELONG(MOD_SHIFT, VK_LEFT):
		PostMessage(m_hwnd, WM_HSCROLL, SB_LINELEFT, 0);
		return 1;
	case MAKELONG(MOD_SHIFT, VK_RIGHT):
		PostMessage(m_hwnd, WM_HSCROLL, SB_LINERIGHT, 0);
		return 1;
	case MAKELONG(MOD_CONTROL, VK_LEFT):
		PostMessage(m_hwnd, WM_HSCROLL, SB_PAGELEFT, 0);
		return 1;
	case MAKELONG(MOD_CONTROL, VK_RIGHT):
		PostMessage(m_hwnd, WM_HSCROLL, SB_PAGERIGHT, 0);
		return 1;
	}
	return 0;
}

LRESULT MainWindow::DoGetObject(NMOBJECTNOTIFY *pnm)
{
	pnm->hResult = QueryInterface(*pnm->piid, (void **)&pnm->pObject);
	return 0;
}

void MainWindow::UpdateLayout()
{
	m_fixpoint.x = 8;
	m_fixpoint.y = 8;
	if (HDC hdc = GetDC(m_hwndLine))
	{
		HFONT font = reinterpret_cast<HFONT>(SendMessage(m_hwndLine, WM_GETFONT, 0, 0));
		SelectObject(hdc, font);
		SIZE ext;
		GetTextExtentPoint32(hdc, L"0", 1, &ext);
		m_fixpoint.x += ext.cx * 10;
		m_fixpoint.y += ext.cy;
		ReleaseDC(m_hwndLine, hdc);
	}

	TabCtrl_SetItemSize(m_hwndDropTarget, 0, m_fixpoint.y);
	TabCtrl_SetItemSize(m_hwndIdioms, 0, m_fixpoint.y);

	RECT rc;
	LONG const idioms = TabCtrl_GetItemRect(m_hwndIdioms, 0, &rc) ? rc.right : 0;
	LONG const scroll = 2 * GetSystemMetrics(SM_CXHSCROLL);

	GetClientRect(m_hwnd, &rc);
	SetWindowPos(m_hwndLine, NULL, rc.left, rc.top, m_fixpoint.x, m_fixpoint.y, SWP_NOZORDER);

	SetWindowPos(m_hwndScroll, NULL, rc.right - scroll, rc.top, scroll, m_fixpoint.y, SWP_NOZORDER);
	SetWindowPos(m_hwndIdioms, NULL, rc.right - scroll - idioms, rc.top, idioms, m_fixpoint.y, SWP_NOZORDER);

	SetWindowPos(m_hwndText, NULL, m_fixpoint.x, rc.top, rc.right - m_fixpoint.x - idioms - scroll, m_fixpoint.y, SWP_NOZORDER);
	SetWindowPos(m_hwndList, NULL, rc.left, m_fixpoint.y, rc.right, rc.bottom - m_fixpoint.y, SWP_NOZORDER);
	GetClientRect(m_hwndText, &rc);
	SetWindowPos(m_hwndStatus, NULL, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, SWP_NOZORDER);
	GetWindowRect(m_hwndText, &rc);
	MapWindowPoints(NULL, m_hwnd, reinterpret_cast<POINT *>(&rc), 2);
	SetWindowPos(m_hwndDropTarget, NULL, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, SWP_NOZORDER);

	SendMessage(m_hwndLine, EM_GETRECT, 0, reinterpret_cast<LPARAM>(&rc));
	MapWindowPoints(m_hwndLine, m_hwndList, reinterpret_cast<POINT *>(&rc), 2);
	m_right = rc.right;
	SendMessage(m_hwndText, EM_GETRECT, 0, reinterpret_cast<LPARAM>(&rc));
	MapWindowPoints(m_hwndText, m_hwndList, reinterpret_cast<POINT *>(&rc), 2);
	m_left = rc.left;
	GetWindowRect(m_hwndLine, &rc);
	ListView_SetColumnWidth(m_hwndList, 0, rc.right - rc.left);
	ListView_SetColumnWidth(m_hwndList, 1, LVSCW_AUTOSIZE_USEHEADER);

	AdjustScrollRange();
	DoHScroll(SB_ENDSCROLL);
}

void MainWindow::SetFont(HFONT font)
{
	SendMessage(m_hwndLine, WM_SETFONT, reinterpret_cast<WPARAM>(font), 1);
	SendMessage(m_hwndText, WM_SETFONT, reinterpret_cast<WPARAM>(font), 1);
	int i = ListView_GetTopIndex(m_hwndList);
	RECT rc;
	if (ListView_GetItemRect(m_hwndList, i, &rc, LVIR_BOUNDS))
		ListView_Scroll(m_hwndList, 0, (rc.top - rc.bottom) * i);
	SendMessage(m_hwndList, WM_SETFONT, reinterpret_cast<WPARAM>(font), 1);
	if (ListView_GetItemRect(m_hwndList, 0, &rc, LVIR_BOUNDS))
		ListView_Scroll(m_hwndList, 0, (rc.bottom - rc.top) * i);
	UpdateLayout();
}

void MainWindow::Open(WORD mode)
{
	OPENFILENAME ofn;
	ZeroMemory(&ofn, sizeof ofn);
	ofn.lStructSize = sizeof ofn;
	ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
	ofn.hwndOwner = m_hwnd;
	TCHAR path[MAX_PATH];
	path[0] = _T('\0');
	ofn.lpstrFile = path;
	ofn.nMaxFile = _countof(path);
	TCHAR buf[4096];
	if (GetPrivateProfileSection(_T("FileFilters"), buf, _countof(buf), IniPath))
	{
		LPTSTR p = buf;
		while (LPTSTR q = _tcschr(p, _T('=')))
		{
			p = q + _tcslen(q) + 1;
			*q = _T('\0');
		}
		*p = _T('\0');
		ofn.lpstrFilter = buf;
		ofn.nFilterIndex = 1;
	}
	if (GetOpenFileName(&ofn))
		Open(path, mode);
}

void MainWindow::SelectFont()
{
	CHOOSEFONT cf;
	ZeroMemory(&cf, sizeof cf);
	cf.lStructSize = sizeof cf;
	cf.Flags = CF_INITTOLOGFONTSTRUCT | CF_BOTH;
	cf.hwndOwner = m_hwnd;
	LogFont lf;
	cf.lpLogFont = &lf;
	GetObject(m_font, sizeof lf, &lf);
	if (ChooseFont(&cf))
	{
		TCHAR buf[128];
		lf.Print(buf);
		WritePrivateProfileString(_T("Settings"), _T("Font"), buf, IniPath);
		if (HFONT font = CreateFontIndirect(&lf))
		{
			SetFont(font);
			DeleteObject(m_font);
			m_font = font;
		}
	}
}

void MainWindow::UseDefaultFont()
{
	if (HFONT font = reinterpret_cast<HFONT>(GetStockObject(ANSI_FIXED_FONT)))
	{
		WritePrivateProfileString(_T("Settings"), _T("Font"), NULL, IniPath);
		SetFont(font);
		DeleteObject(m_font);
		m_font = font;
	}
}

void MainWindow::SetTabWidth(UINT tabwidth)
{
	if (m_tabwidth != tabwidth)
		InvalidateRect(m_hwndList, NULL, TRUE);
	m_tabwidth = tabwidth;
	if (GetCapture() == NULL)
		m_tabwidth_backup = m_tabwidth;
}

void MainWindow::SetCodePage(UINT codepage)
{
	if (m_codepage != codepage)
		InvalidateRect(m_hwndList, NULL, TRUE);
	m_codepage = codepage;
	if (GetCapture() == NULL)
		m_codepage_backup = m_codepage;
}

void MainWindow::DrawMenuDefault(DRAWITEMSTRUCT *)
{
}

void MainWindow::DrawMenuDropdown(DRAWITEMSTRUCT *pdis)
{
	UINT uState = DFCS_SCROLLCOMBOBOX;
	if (pdis->itemState & ODS_SELECTED)
		uState |= DFCS_PUSHED;
	DrawFrameControl(pdis->hDC, &pdis->rcItem, DFC_SCROLL, uState);
}

void MainWindow::DrawMenuCheckbox(DRAWITEMSTRUCT *pdis)
{
	RECT rc = pdis->rcItem;
	SetBkColor(pdis->hDC, GetSysColor(pdis->itemState & ODS_SELECTED ? COLOR_HIGHLIGHT : COLOR_BTNFACE));
	SetTextColor(pdis->hDC, GetSysColor(pdis->itemState & ODS_SELECTED ? COLOR_HIGHLIGHTTEXT : COLOR_MENUTEXT));
	ExtTextOut(pdis->hDC, rc.left, rc.top, ETO_OPAQUE, &rc, NULL, 0, NULL);
	rc.left += 3;
	rc.right = rc.left + GetSystemMetrics(SM_CXMENUCHECK);
	UINT uState = DFCS_BUTTONCHECK | DFCS_FLAT;
	if (pdis->itemState & ODS_CHECKED)
		uState |= DFCS_CHECKED;
	rc.top += 1;
	rc.bottom -= 1;
	DrawFrameControl(pdis->hDC, &rc, DFC_BUTTON, uState);
	rc.left = rc.right + 4;
	rc.right = pdis->rcItem.right;
	TCHAR text[80];
	GetMenuString(reinterpret_cast<HMENU>(pdis->hwndItem), pdis->itemID, text, _countof(text), MF_BYCOMMAND);
	DrawText(pdis->hDC, text, -1, &rc, DT_SINGLELINE);
}

void MainWindow::DoDrawItem(DRAWITEMSTRUCT *pdis)
{
	reinterpret_cast<DrawMenuProc>(pdis->itemData)(pdis);
}

void MainWindow::DoMeasureItem(MEASUREITEMSTRUCT *pmis)
{
	if (pmis->itemID == 0)
	{
		pmis->itemWidth = GetSystemMetrics(SM_CXVSCROLL);
	}
	else if (GetMenuState(m_menu, pmis->itemID, MF_BYCOMMAND) & MF_OWNERDRAW)
	{
		pmis->itemWidth = GetSystemMetrics(SM_CXMENUCHECK) + 3;
		if (HDC hdc = GetDC(m_hwnd))
		{
			SelectObject(hdc, m_uifont);
			TCHAR text[80];
			GetMenuString(m_menu, pmis->itemID, text, _countof(text), MF_BYCOMMAND);
			RECT rc;
			SetRectEmpty(&rc);
			DrawText(hdc, text, -1, &rc, DT_SINGLELINE | DT_CALCRECT);
			pmis->itemWidth += rc.right - rc.left;
			ReleaseDC(m_hwnd, NULL);
		}
	}
	else
	{
		pmis->itemWidth = 0;
	}
}

int MainWindow::InitCodePageMenu(HMENU menu, int n)
{
	int i;
	while ((i = n) > 0 && GetMenuItemID(menu, --n) == IDM_CODEPAGE_CUSTOM)
		DeleteMenu(menu, n, MF_BYPOSITION);
	TCHAR buf[4096];
	MENUITEMINFO mii;
	mii.cbSize = sizeof mii;
	mii.fMask = MIIM_FTYPE | MIIM_STATE | MIIM_ID | MIIM_STRING | MIIM_BITMAP | MIIM_DATA;
	mii.fType = 0;
	mii.dwTypeData = buf;
	mii.hbmpItem = HBMMENU_CALLBACK;
	mii.dwItemData = reinterpret_cast<ULONG_PTR>(DrawMenuDefault);
	mii.wID = IDM_CODEPAGE_CUSTOM;
	if (m_encodinginfo)
	{
		mii.fState = m_encodinginfo->cp == 0 ? MFS_GRAYED :
			m_encodinginfo->cp == m_codepage ? MFS_CHECKED : MFS_UNCHECKED;
		wsprintf(buf, _T("%u\t%hs"), m_encodinginfo->cp, m_encodinginfo->name);
		InsertMenuItem(menu, ++n, TRUE, &mii);
	}
	if (GetPrivateProfileSection(_T("CodePages"), buf, _countof(buf), IniPath))
	{
		while (size_t len = _tcslen(mii.dwTypeData))
		{
			if (*mii.dwTypeData == _T('/'))
			{
				mii.fType |= MFT_MENUBREAK;
			}
			else if (*mii.dwTypeData != _T('#'))
			{
				if (*mii.dwTypeData == _T('\\'))
					mii.fType |= MFT_SEPARATOR;
				else
					mii.fType |= MFT_RADIOCHECK;
				UINT codepage = _ttoi(mii.dwTypeData);
				mii.fState = 0;
				if (m_codepage == codepage)
					mii.fState |= MFS_CHECKED;
				if (!IsValidCodePage(codepage))
					mii.fState |= MFS_GRAYED;
				if (LPTSTR q = _tcschr(mii.dwTypeData, _T('=')))
					*q = _T('\t');
				InsertMenuItem(menu, ++n, TRUE, &mii);
				mii.fType = 0;
			}
			mii.dwTypeData += len + 1;
		}
	}
	return i;
}

void MainWindow::ChooseIdiom()
{
	UINT const use_agrep = GetMenuState(m_menu, IDM_USE_AGREP, MF_BYCOMMAND) & MF_CHECKED;
	TCHAR buf[4096];
	if (GetPrivateProfileSection(use_agrep ? _T("AgrepIdioms") : _T("Idioms"), buf, _countof(buf), IniPath))
	{
		if (HMENU menu = CreatePopupMenu())
		{
			int n = 0;
			MENUITEMINFO mii;
			mii.cbSize = sizeof mii;
			mii.fMask = MIIM_FTYPE | MIIM_ID | MIIM_STRING;
			mii.fType = 0;
			mii.dwTypeData = buf;
			mii.wID = 1;
			while (size_t len = _tcslen(mii.dwTypeData))
			{
				if (*mii.dwTypeData == _T('/'))
				{
					mii.fType |= MFT_MENUBREAK;
				}
				else if (*mii.dwTypeData != _T('#'))
				{
					if (*mii.dwTypeData == _T('\\'))
						mii.fType |= MFT_SEPARATOR;
					else
						mii.fType |= MFT_RADIOCHECK;
					if (LPTSTR q = _tcschr(mii.dwTypeData, _T('=')))
						*q = _T('\t');
					InsertMenuItem(menu, n++, TRUE, &mii);
					mii.fType = 0;
					++mii.wID;
				}
				mii.dwTypeData += len + 1;
			}
			RECT rc;
			TabCtrl_GetItemRect(m_hwndIdioms, 0, &rc);
			ClientToScreen(m_hwndIdioms, reinterpret_cast<POINT *>(&rc.right));
			TPMPARAMS params = { sizeof params };
			params.rcExclude.left = rc.right;
			params.rcExclude.top = GetSystemMetrics(SM_XVIRTUALSCREEN);
			params.rcExclude.right = GetSystemMetrics(SM_CXVIRTUALSCREEN);
			params.rcExclude.bottom = GetSystemMetrics(SM_CYVIRTUALSCREEN);
			if (int choice = TrackPopupMenuEx(menu, TPM_RETURNCMD, rc.right, rc.bottom, m_hwnd, &params))
			{
				GetMenuString(menu, choice, buf, _countof(buf), MF_BYCOMMAND);
				if (LPTSTR q = _tcschr(buf, _T('\t')))
				{
					*q++ = _T('\0');
					SetFocus(m_hwndText);
					SetWindowText(m_hwndText, q);
					SendMessage(m_hwndText, EM_SETSEL, 0, -1);
					SendMessage(m_hwndText, EM_SETMODIFY, 1, 0);
				}
				if (LPTSTR p = _tcschr(buf, _T('[')))
				{
					LPTSTR q = _tcschr(buf, _T(']'));
					UINT flags = MF_CHECKED;
					while (++p < q)
					{
						UINT id = 0;
						switch (*p)
						{
						case _T('B'):
							id = IDM_BEGINS_WITH;
							break;
						case _T('E'):
							id = IDM_ENDS_WITH;
							break;
						case _T('L'):
							id = IDM_LITERAL;
							break;
						case _T('I'):
							id = IDM_IGNORE_CASE;
							break;
						case _T('-'):
							flags = MF_UNCHECKED;
						default:
							continue;
						}
						CheckMenuItem(m_menu, id, flags);
					}
					DrawMenuBar(m_hwnd);
				}
			}
			DestroyMenu(menu);
		}
	}
}

static void CALLBACK HideWindowWhenKeyReleased(HWND hwnd, UINT, UINT_PTR id, DWORD)
{
	if (GetAsyncKeyState(static_cast<int>(id)) >= 0)
	{
		SetWindowPos(hwnd, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_HIDEWINDOW | SWP_NOACTIVATE);
		KillTimer(hwnd, id);
	}
}

void MainWindow::DoActivate(WPARAM wParam)
{
	// (Un)establish hotkeys
	if (wParam != WA_INACTIVE)
	{
		RegisterHotKey(m_hwnd, IDM_REFRESH, 0, VK_F5);
	}
	else
	{
		UnregisterHotKey(m_hwnd, IDM_REFRESH);
	}
	// (Un)establish drop targets
	if (IsWindowVisible(m_hwnd) && IsWindowEnabled(m_hwnd))
	{
		UINT flags = SWP_NOMOVE | SWP_NOSIZE | SWP_HIDEWINDOW | SWP_NOACTIVATE;
		switch (wParam)
		{
		case WA_ACTIVE:
			if (GetAsyncKeyState(VK_LBUTTON) >= 0)
				break;
			SetTimer(m_hwndDropTarget, VK_LBUTTON, 100, HideWindowWhenKeyReleased);
			// fall through
		case WA_INACTIVE:
			flags = SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW | SWP_NOACTIVATE;
			break;
		}
		SetWindowPos(m_hwndDropTarget, NULL, 0, 0, 0, 0, flags);
	}
}

void MainWindow::UpdateUseAgrep()
{
	MENUITEMINFO mii;
	mii.cbSize = sizeof mii;
	mii.fMask = MIIM_ID | MIIM_BITMAP | MIIM_STRING | MIIM_STATE;
	mii.fState = 0;
	mii.hbmpItem = HBMMENU_CALLBACK; // triggers a new WM_MEASUREITEM
	switch (GetMenuState(m_menu, IDM_USE_AGREP, MF_BYCOMMAND) & (MF_CHECKED | MF_DISABLED))
	{
	case MF_CHECKED:
		WritePrivateProfileString(_T("Settings"), _T("UseAgrep"), _T("1"), IniPath);
		// fall through
	case MF_CHECKED | MF_DISABLED:
		mii.wID = IDM_WHOLE_WORD;
		mii.dwTypeData = _T("&Whole word");
		SetMenuItemInfo(m_menu, IDM_BEGINS_WITH, FALSE, &mii);
		mii.wID = IDM_NOTHING;
		mii.dwTypeData = _T("&Nothing");
		SetMenuItemInfo(m_menu, IDM_ENDS_WITH, FALSE, &mii);
		break;
	case MF_UNCHECKED:
		WritePrivateProfileString(_T("Settings"), _T("UseAgrep"), _T("0"), IniPath);
		// fall through
	case MF_UNCHECKED | MF_DISABLED:
		mii.wID = IDM_BEGINS_WITH;
		mii.dwTypeData = _T("&Begins with");
		SetMenuItemInfo(m_menu, IDM_WHOLE_WORD, FALSE, &mii);
		mii.wID = IDM_ENDS_WITH;
		mii.dwTypeData = _T("&Ends with");
		SetMenuItemInfo(m_menu, IDM_NOTHING, FALSE, &mii);
		break;
	}
}

LRESULT MainWindow::DoMsg(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch(uMsg)
	{
	case WM_INITDIALOG:
		m_super = SubclassDlgProc(hwnd);
		Init(hwnd);
		return TRUE;

	case WM_DESTROY:
		if (m_thread != NULL)
		{
			m_stop = true;
			WaitForSingleObject(m_thread, INFINITE);
		}
		break;

	case WM_NOTIFY:
		switch (reinterpret_cast<NMHDR *>(lParam)->idFrom)
		{
		case IDC_LIST:
			switch (reinterpret_cast<NMHDR *>(lParam)->code)
			{
			case NM_CUSTOMDRAW:
				return DoCustomDraw(reinterpret_cast<NMLVCUSTOMDRAW *>(lParam));
			case LVN_ITEMACTIVATE:
				return DoItemActivate(reinterpret_cast<NMITEMACTIVATE *>(lParam));
			case LVN_KEYDOWN:
				return DoKeydown(reinterpret_cast<NMLVKEYDOWN *>(lParam));
			}
			break;
		case IDC_DROPTARGET:
			switch (reinterpret_cast<NMHDR *>(lParam)->code)
			{
			case TCN_GETOBJECT:
				return DoGetObject(reinterpret_cast<NMOBJECTNOTIFY *>(lParam));
			case TCN_SELCHANGE:
				TabCtrl_SetCurSel(reinterpret_cast<NMHDR *>(lParam)->hwndFrom, -1);
				break;
			}
			break;
		case IDC_IDIOMS:
			switch (reinterpret_cast<NMHDR *>(lParam)->code)
			{
			case TCN_SELCHANGE:
				TabCtrl_SetCurSel(reinterpret_cast<NMHDR *>(lParam)->hwndFrom, -1);
				ChooseIdiom();
				break;
			}
			break;
		}
		return 0;

	case WM_INITMENUPOPUP:
		if (HIWORD(lParam) == 0)
		{
			HMENU menu = reinterpret_cast<HMENU>(wParam);
			int n = GetMenuItemCount(menu);
			if (CheckMenuItem(menu, IDM_CODEPAGE_ANSI, m_codepage == CP_ACP ? MF_CHECKED : MF_UNCHECKED) != 0xFFFFFFFF)
			{
				CheckMenuItem(menu, IDM_CODEPAGE_OEM, m_codepage == CP_OEMCP ? MF_CHECKED : MF_UNCHECKED);
				CheckMenuItem(menu, IDM_CODEPAGE_UTF8, m_codepage == CP_UTF8 ? MF_CHECKED : MF_UNCHECKED);
				CheckMenuItem(menu, IDM_CODEPAGE_UCS2LE, m_codepage == 1200 ? MF_CHECKED : MF_UNCHECKED);
				CheckMenuItem(menu, IDM_CODEPAGE_UCS2BE, m_codepage == 1201 ? MF_CHECKED : MF_UNCHECKED);
				n = InitCodePageMenu(menu, n);
			}
			else
			{
				n = CheckMenuInt(menu, n, IDM_TABWIDTH, m_tabwidth);
			}
		}
		break;

	case WM_MENUSELECT:
		if (HIWORD(wParam) == 0xFFFF && lParam == 0)
			break;
		SetTabWidth(m_tabwidth_backup);
		SetCodePage(m_codepage_backup);
		if ((HIWORD(wParam) & MF_POPUP) == 0)
		{
			HMENU menu = reinterpret_cast<HMENU>(lParam);
			int n = GetMenuItemCount(menu);
			switch (UINT id = LOWORD(wParam))
			{
			case IDM_TABWIDTH:
				if (UINT tabwidth = ParseMenuInt(menu, n, id))
					SetTabWidth(tabwidth);
				break;
			case IDM_CODEPAGE_ANSI:
				SetCodePage(CP_ACP);
				break;
			case IDM_CODEPAGE_OEM:
				SetCodePage(CP_OEMCP);
				break;
			case IDM_CODEPAGE_UTF8:
				SetCodePage(CP_UTF8);
				break;
			case IDM_CODEPAGE_UCS2LE:
				SetCodePage(1200);
				break;
			case IDM_CODEPAGE_UCS2BE:
				SetCodePage(1201);
				break;
			case IDM_CODEPAGE_CUSTOM:
				if (UINT codepage = ParseMenuInt(menu, n, id))
					SetCodePage(codepage);
				break;
			}
		}
		break;

	case WM_TIMER:
		switch (wParam)
		{
		case ~ReadThreadFinishedTimer:
			CloseHandle(m_thread);
			m_thread = NULL;
			KillTimer(m_hwnd, ReadThreadFinishedTimer);
			EnableMenuItem(m_menu, IDM_REFRESH, MF_ENABLED);
			EnableMenuItem(m_menu, IDM_STOP, MF_DISABLED | MF_GRAYED);
			DrawMenuBar(m_hwnd);
			if (GetWindowTextLength(m_hwndText))
				SendMessage(m_hwndText, EM_SETMODIFY, 1, 0);
			// fall through
		case ReadThreadFinishedTimer:
			switch (m_encoding)
			{
			case LineReader::UTF8:
				SetCodePage(CP_UTF8);
				break;
			case LineReader::UCS2LE:
				SetCodePage(1200);
				break;
			case LineReader::UCS2BE:
				SetCodePage(1201);
				break;
			default:
				SetCodePage(m_encodinginfo ? m_encodinginfo->cp : CP_ACP);
				break;
			}
			IndicateProgress(m_lines, GetTickCount() - m_then);
			int i = ListView_GetTopIndex(m_hwndList);
			RECT rc;
			if (ListView_GetItemRect(m_hwndList, i, &rc, LVIR_BOUNDS))
				ListView_Scroll(m_hwndList, 0, (rc.top - rc.bottom) * i);
			ListView_SetItemCount(m_hwndList, m_lines);
			if (ListView_GetItemRect(m_hwndList, 0, &rc, LVIR_BOUNDS))
				ListView_Scroll(m_hwndList, 0, (rc.bottom - rc.top) * i);
			ListView_SetColumnWidth(m_hwndList, 1, LVSCW_AUTOSIZE_USEHEADER);
			break;
		}
		return 0;

	case WM_SIZE:
		if (wParam != SIZE_MAXHIDE && wParam != SIZE_MAXSHOW && wParam != SIZE_MINIMIZED)
			UpdateLayout();
		// fall through
	case WM_EXITSIZEMOVE:
		SaveWindowPlacement(m_hwnd, _T("MainWindow"));
		break;

	case WM_HSCROLL:
		if (lParam == 0)
		{
			DoHScroll(LOWORD(wParam));
		}
		else if (int id = GetDlgCtrlID(reinterpret_cast<HWND>(lParam)))
		{
			switch (MAKELONG(id, wParam))
			{
			case MAKELONG(IDC_SCROLL, SB_LINELEFT):
				DoSearch(-1);
				break;
			case MAKELONG(IDC_SCROLL, SB_LINERIGHT):
				DoSearch(+1);
				break;
			}
		}
		break;

	case WM_DRAWITEM:
		DoDrawItem(reinterpret_cast<DRAWITEMSTRUCT *>(lParam));
		break;

	case WM_MEASUREITEM:
		DoMeasureItem(reinterpret_cast<MEASUREITEMSTRUCT *>(lParam));
		break;

	case WM_ACTIVATE:
		DoActivate(wParam);
		break;

	case WM_HOTKEY:
		if (GetMenuState(m_menu, static_cast<UINT>(wParam), MF_BYCOMMAND) & (MF_DISABLED | MF_GRAYED))
			break;
		// fall through
	case WM_COMMAND:
		switch (wParam)
		{
		case IDOK:
		case IDCANCEL:
			EndDialog(hwnd, wParam);
			break;
		case IDM_OPEN:
			Open(MAKEWORD(LineReader::NONE, '\n'));
			break;
		case IDM_OPEN_UTF8:
			Open(MAKEWORD(LineReader::UTF8, '\n'));
			break;
		case IDM_OPEN_UCS2LE:
			Open(MAKEWORD(LineReader::UCS2LE, '\n'));
			break;
		case IDM_OPEN_UCS2BE:
			Open(MAKEWORD(LineReader::UCS2BE, '\n'));
			break;
		case IDM_OPEN_XML:
			Open(MAKEWORD(LineReader::NONE, '>'));
			break;
		case IDM_REFRESH:
			Refresh();
			break;
		case IDM_STOP:
			m_stop = true;
			break;
		case IDM_SELECT_FONT:
			SelectFont();
			break;
		case IDM_USE_DEFAULT_FONT:
			UseDefaultFont();
			break;
		case IDM_ABOUT:
			AboutBox();
			break;
		case IDM_USE_AGREP:
		case IDM_WHOLE_WORD:
		case IDM_NOTHING:
		case IDM_BEGINS_WITH:
		case IDM_ENDS_WITH:
		case IDM_LITERAL:
		case IDM_IGNORE_CASE:
		case IDM_INVERT:
			CheckMenuItem(m_menu, static_cast<UINT>(wParam),
				GetMenuState(m_menu, static_cast<UINT>(wParam), MF_BYCOMMAND) & MF_CHECKED ^ MF_CHECKED);
			if (wParam == IDM_USE_AGREP)
				UpdateUseAgrep();
			DrawMenuBar(m_hwnd);
			if (GetWindowTextLength(m_hwndText))
				SendMessage(m_hwndText, EM_SETMODIFY, 1, 0);
			break;

		case IDM_CODEPAGE_ANSI:
		case IDM_CODEPAGE_OEM:
		case IDM_CODEPAGE_UTF8:
		case IDM_CODEPAGE_UCS2LE:
		case IDM_CODEPAGE_UCS2BE:
		case IDM_CODEPAGE_CUSTOM:
			SetCodePage(m_codepage);
			break;

		case IDM_TABWIDTH:
			SetTabWidth(m_tabwidth);
			break;

		case MAKEWPARAM(IDC_LINE, EN_SETFOCUS):
			DoStep(0, GetKeyState(VK_CONTROL) < 0 ? 2 : 0);
			break;

		case MAKEWPARAM(IDC_LINE, EN_KILLFOCUS):
			DoStep(0);
			break;

		case MAKEWPARAM(IDC_TEXT, EN_SETFOCUS):
			ShowWindow(m_hwndStatus, SW_HIDE);
			break;

		case MAKEWPARAM(IDC_TEXT, EN_KILLFOCUS):
			ShowWindow(m_hwndStatus, SW_SHOW);
			break;

		case MAKEWPARAM(IDC_LINE, EN_CHANGE):
			if (int n = ListView_GetItemCount(m_hwndList))
			{
				if (int i = GetDlgItemInt(hwnd, IDC_LINE, NULL, FALSE))
				{
					if (i > n)
						i = n;
					else if (i < 1)
						i = 1;
					SelectLine(i - 1);
				}
			}
			break;
		}
		break;

	case WM_SETFOCUS:
		SetFocus(m_hwndText);
		break;
    }

	return Default(m_super, hwnd, uMsg, wParam, lParam);
}

LRESULT MainWindow::DoMsg(Subclass::DlgItem<IDC_LINE> &DlgItem, HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_KEYDOWN:
		if (GetKeyState(VK_CONTROL) < 0)
		{
			DWORD dwTextLength = GetWindowTextLength(hwnd);
			DWORD dwSelStart = 0;
			DWORD dwSelEnd = 0;
			SendMessage(hwnd, EM_GETSEL, reinterpret_cast<WPARAM>(&dwSelStart), reinterpret_cast<LPARAM>(&dwSelEnd));
			int delta = 1;
			int shift = 0;
			if (dwSelEnd == dwSelStart + 1)
			{
				for (DWORD dw = dwSelEnd; dw < dwTextLength; ++dw)
					delta *= 10;
				shift = dwTextLength - dwSelStart;
			}
			switch (wParam)
			{
			case VK_LEFT:
				if (dwSelEnd > 0)
				{
					if (dwSelStart == dwSelEnd || (dwSelStart = --dwSelEnd) > 0)
						--dwSelStart;
					if (dwSelStart < dwSelEnd)
						SendMessage(hwnd, EM_SETSEL, dwSelStart, dwSelEnd);
				}
				return 0;
			case VK_RIGHT:
				if (dwSelStart < dwTextLength)
				{
					if (dwSelStart == dwSelEnd || (dwSelEnd = ++dwSelStart) < dwTextLength)
						++dwSelEnd;
					if (dwSelStart < dwSelEnd)
						SendMessage(hwnd, EM_SETSEL, dwSelStart, dwSelEnd);
				}
				return 0;
			case VK_UP:
				DoStep(-delta, shift);
				return 0;
			case VK_DOWN:
				DoStep(+delta, shift);
				return 0;
			case VK_HOME:
				SendMessage(hwnd, EM_SETSEL, 0, 1);
				return 0;
			case VK_END:
				SendMessage(hwnd, EM_SETSEL, dwTextLength - 1, dwTextLength);
				return 0;
			case VK_CONTROL:
				DoStep(0, 2);
				return 0;
			}
		}
		else
		{
			switch (wParam)
			{
			case VK_PRIOR:
				DoStep(1 - ListView_GetCountPerPage(m_hwndList));
				return 0;
			case VK_NEXT:
				DoStep(ListView_GetCountPerPage(m_hwndList) - 1);
				return 0;
			case VK_UP:
				DoStep(-1);
				return 0;
			case VK_DOWN:
				DoStep(+1);
				return 0;
			}
		}
		break;
	case WM_KEYUP:
		switch (wParam)
		{
		case VK_CONTROL:
			DoStep(0);
			return 0;
		}
		break;
	}
	return Default(DlgItem, hwnd, uMsg, wParam, lParam);
}

LRESULT MainWindow::DoMsg(Subclass::DlgItem<IDC_TEXT> &DlgItem, HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_KEYDOWN:
		switch (wParam)
		{
		case VK_UP:
			DoSearch(-1);
			return 0;
		case VK_DOWN:
			DoSearch(+1);
			return 0;
		case VK_F4:
			ChooseIdiom();
			return 0;
		}
		break;
	case WM_SYSKEYDOWN:
		switch (wParam)
		{
		case VK_DOWN:
			ChooseIdiom();
			return 0;
		}
		break;
	}
	return Default(DlgItem, hwnd, uMsg, wParam, lParam);
}

LRESULT MainWindow::DoMsg(Subclass::DlgItem<IDC_LIST> &DlgItem, HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_ERASEBKGND:
		if (int n = ListView_GetItemCount(hwnd))
		{
			RECT rect;
			ListView_GetItemRect(hwnd, n - 1, &rect, LVIR_BOUNDS);
			int bottom = rect.bottom;
			GetClientRect(hwnd, &rect);
			if (bottom < rect.bottom)
			{
				rect.top = bottom;
				FillRect(reinterpret_cast<HDC>(wParam), &rect, GetSysColorBrush(COLOR_WINDOW));
			}
			return TRUE;
		}
		break;
	}
	return Default(DlgItem, hwnd, uMsg, wParam, lParam);
}

void MainWindow::UpdateWindowTitle()
{
	TCHAR text[512];
	wsprintf(text, _T("%s - %s"), m_path, VersionInfo.ProductName);
	SetWindowText(m_hwnd, text);
}

void MainWindow::IndicateProgress(DWORD lines, DWORD elapsed)
{
	TCHAR text[128];
	wsprintf(text, _T("%hs lines / elapsed time: %hs ms%s"), NumToStr(lines), NumToStr(elapsed), &_T("\0 - STOPPED!")[m_stop]);
	SetWindowText(m_hwndStatus, text);
}

void MainWindow::AdjustScrollRange()
{
	RECT rc;
	GetClientRect(m_hwndList, &rc);
	SCROLLINFO si;
	si.cbSize = sizeof si;
	si.fMask = SIF_RANGE | SIF_PAGE | SIF_DISABLENOSCROLL;
	si.nMin = 0;
	si.nMax = m_width;
	si.nPage = (rc.right - ListView_GetColumnWidth(m_hwndList, 0)) / ListView_GetStringWidth(m_hwndList, _T("0"));
	SetScrollInfo(m_hwnd, SB_HORZ, &si, TRUE);
}

void MainWindow::DoHScroll(WORD code)
{
	SCROLLINFO si;
	si.cbSize = sizeof si;
	si.fMask = SIF_ALL;
	GetScrollInfo(m_hwnd, SB_HORZ, &si);
	switch (code)
	{
	case SB_THUMBPOSITION:
		si.fMask = SIF_POS;
		// fall through
	case SB_THUMBTRACK:
		si.nPos = si.nTrackPos;
		break;
	case SB_PAGELEFT:
		si.nPos -= si.nPage;
		// fall through
	case SB_LINERIGHT:
		++si.nPos;
		si.fMask = SIF_POS;
		break;
	case SB_PAGERIGHT:
		si.nPos += si.nPage;
		// fall through
	case SB_LINELEFT:
		--si.nPos;
		si.fMask = SIF_POS;
		break;
	}
	if (si.fMask != SIF_ALL)
	{
		SetScrollInfo(m_hwnd, SB_HORZ, &si, TRUE);
		GetScrollInfo(m_hwnd, SB_HORZ, &si);
	}
	if (m_offset != si.nPos)
	{
		m_offset = si.nPos;
		InvalidateRect(m_hwndList, NULL, TRUE);
	}
}

LineData *MainWindow::Reserve(WORD w)
{
	return m_index[w] = static_cast<LineData *>(CoTaskMemAlloc(0x10000 * sizeof(LineData)));
}

LineData *MainWindow::GetAt(DWORD dw) const
{
	return m_index && m_index[HIWORD(dw)] ? &m_index[HIWORD(dw)][LOWORD(dw)] : NULL;
}

HANDLE MainWindow::Open(LPCTSTR path)
{
	HANDLE h1 = CreateFile(path, FILE_GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
	HANDLE h2 = CreateFile(path, FILE_GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
	if (h1 != h2)
	{
		if (h1 == INVALID_HANDLE_VALUE)
		{
			CloseHandle(h2);
			h2 = INVALID_HANDLE_VALUE;
		}
		else if (h2 == INVALID_HANDLE_VALUE)
		{
			CloseHandle(h1);
			h1 = INVALID_HANDLE_VALUE;
		}
		else
		{
			m_index = static_cast<LineData **>(CoTaskMemAlloc(0x10000 * sizeof(LineData *)));
		}
	}
	m_handle = h1;
	return h2;
}

void MainWindow::Close()
{
	if (m_handle != INVALID_HANDLE_VALUE)
	{
		CloseHandle(m_handle);
		m_handle = INVALID_HANDLE_VALUE;
	}
	if (m_index)
	{
		DWORD i = LOWORD(m_lines) ? HIWORD(m_lines) + 1 : HIWORD(m_lines);
		while (i != 0)
			CoTaskMemFree(m_index[--i]);
		CoTaskMemFree(m_index);
		m_index = NULL;
	}
	m_lines = 0;
}

DWORD MainWindow::ReadThread()
{
	HANDLE handle = Open(m_path);
	if (handle != INVALID_HANDLE_VALUE)
	{
		LineReader reader = handle;
		UINT const limit = MAX_BIT_FIELD_UNSIGNED(LineData, len);
		DWORD const then = GetTickCount();
		DWORD ticks = then;
		LineData *lineindex = NULL;
		ULARGE_INTEGER pos = { 0, 0 };
		if (m_encoding == LineReader::NONE)
		{
			pos.QuadPart = reader.readBom(m_encoding, LineReader::GUESS);
			if (m_encoding == LineReader::GUESS)
			{
				char buffer[1024];
				size_t offset = pos.LowPart;
				buffer[reader.peekLineAnsi(offset, buffer, 80, '>')] = '\0';
				int n = 0;
				sscanf(buffer, "<?xml %n", &n);
				LPCTSTR const ext = n > 5 ? _T(".XML") : PathFindExtension(m_path);
				// Try to guess encoding from content
				if (FilenameExtensionQualifiesAs(ext, _T("XML")))
				{
					if (size_t length = reader.peekLineAnsi(offset, buffer, sizeof buffer - 1, '>'))
					{
						buffer[length] = '\0';
						if (LPSTR u = strrchr(buffer, '<'))
						{
							char *encoding = NULL;
							int n = 0;
							sscanf(_strlwr(++u), "?xml %n", &n);
							while (n)
							{
								u += n; n = 0;
								if (sscanf(u, "encoding=%n", &n), n)
									encoding = u + n;
								else if (u[n = static_cast<int>(strcspn(u, "= \t\r\n"))] == '=')
									++n;
								u += n; n = 0;
								if (*u != '\'' && *u != '\"')
									n = static_cast<int>(strcspn(u, " \t\r\n"));
								else if (char *v = strchr(u + 1, *u))
									n = static_cast<int>(v + 1 - u);
								u += n; n = 0;
								sscanf(u, " %n", &n);
								*u = '\0';
							}
							if (encoding)
							{
								SetEncodingInfoFromName(TrimString(encoding, "'\""));
							}
						}
					}
				}
				else if (FilenameExtensionQualifiesAs(ext, _T("HTML")))
				{
					while (size_t length = reader.peekLineAnsi(offset, buffer, sizeof buffer - 1, '>'))
					{
						buffer[length] = '\0';
						if (LPSTR u = strrchr(buffer, '<'))
						{
							char *http_equiv = NULL;
							char *content = NULL;
							char *charset = NULL; // HTML5
							int n = 0;
							sscanf(_strlwr(++u), "meta %n", &n);
							while (n)
							{
								u += n; n = 0;
								if (sscanf(u, "http-equiv=%n", &n), n)
									http_equiv = u + n;
								else if (sscanf(u, "content=%n", &n), n)
									content = u + n;
								else if (sscanf(u, "charset=%n", &n), n)
									charset = u + n;
								else if (u[n = static_cast<int>(strcspn(u, "= \t\r\n"))] == '=')
									++n;
								u += n; n = 0;
								if (*u != '\'' && *u != '\"')
									n = static_cast<int>(strcspn(u, " \t\r\n"));
								else if (char *v = strchr(u + 1, *u))
									n = static_cast<int>(v + 1 - u);
								u += n; n = 0;
								sscanf(u, " %n", &n);
								*u = '\0';
							}
							if (http_equiv && content &&
								strcmp(TrimString(http_equiv, "'\""), "content-type") == 0)
							{
								content = TrimString(content, "'\"");
								while (size_t len = strcspn(content += strspn(content, ";"), ";"))
								{
									int n = 0;
									if (sscanf(content, " charset = %n", &n), n)
									{
										content[len] = '\0';
										content += n;
										SetEncodingInfoFromName(TrimString(content, " \t\r\n"));
										break;
									}
									content += len;
								}
							}
							else if (charset)
							{
								SetEncodingInfoFromName(TrimString(charset, "'\""));
							}
						}
						offset += length;
					}
				}
				else if (FilenameExtensionQualifiesAs(ext, _T("RC")))
				{
					while (size_t length = reader.peekLineAnsi(offset, buffer, sizeof buffer - 1))
					{
						buffer[length] = '\0';
						if (sscanf(buffer, "#pragma code_page(%hu)", &m_genericencodinginfo.cp) == 1)
						{
							SetEncodingInfoFromName(NULL);
							break;
						}
						offset += length;
					}
				}
				else if (FilenameExtensionQualifiesAs(ext, _T("PO")))
				{
					while (size_t length = reader.peekLineAnsi(offset, buffer, sizeof buffer - 1))
					{
						buffer[length] = '\0';
						int n = 0;
						sscanf(_strlwr(buffer), "\"content-type:%n", &n);
						if (n)
						{
							char *content = buffer + n;
							Unslash(content);
							while (size_t len = strcspn(content += strspn(content, "\";"), "\";"))
							{
								int n = 0;
								if (sscanf(content, " charset = %n", &n), n)
								{
									content[len] = '\0';
									content += n;
									SetEncodingInfoFromName(TrimString(content, " \t\r\n"));
									break;
								}
								content += len;
							}
							break;
						}
						offset += length;
					}
				}
			}
		}
		wchar_t eol = m_delimiter;
		switch (m_encoding)
		{
		case LineReader::UCS2BE:
			eol <<= 8;
			// fall through
		case LineReader::UCS2LE:
			while (size_t len = reader.readLineWide(limit, eol))
			{
				if (LOWORD(m_lines) == 0)
				{
					lineindex = Reserve(HIWORD(m_lines));
					if (lineindex == NULL)
						break;
				}
				LineData &linedata = lineindex[LOWORD(m_lines)];
				linedata.LowPart = pos.LowPart;
				linedata.HighPart = pos.HighPart;
				linedata.flags = 0;
				linedata.len = len;
				pos.QuadPart += len;
				++m_lines;
				if (m_stop)
					break;
			}
			break;
		default:
			// assume octet encoding
			while (size_t len = reader.readLineAnsi(limit, static_cast<char>(eol)))
			{
				if (LOWORD(m_lines) == 0)
				{
					lineindex = Reserve(HIWORD(m_lines));
					if (lineindex == NULL)
						break;
				}
				LineData &linedata = lineindex[LOWORD(m_lines)];
				linedata.LowPart = pos.LowPart;
				linedata.HighPart = pos.HighPart;
				linedata.flags = 0;
				linedata.len = len;
				pos.QuadPart += len;
				++m_lines;
				if (m_stop)
					break;
			}
			break;
		}
		CloseHandle(handle);
	}
	PostMessage(m_hwnd, WM_TIMER, ~ReadThreadFinishedTimer, 0);
	return 0;
}

DWORD MainWindow::StartReadThread(LPVOID pv)
{
	return static_cast<MainWindow *>(pv)->ReadThread();
}

void MainWindow::Open(LPCTSTR path, WORD mode)
{
	if (m_thread != NULL)
		return;
	ListView_SetItemCount(m_hwndList, 0);
	ListView_SetColumnWidth(m_hwndList, 1, LVSCW_AUTOSIZE_USEHEADER);
	Close();
	m_lines = 0;
	m_width = 0;
	m_offset = 0;
	if (m_encoding != static_cast<LineReader::Encoding>(LOBYTE(mode)))
	{
		m_encoding = static_cast<LineReader::Encoding>(LOBYTE(mode));
		m_encodinginfo = NULL;
	}
	if (m_delimiter != static_cast<char>(HIBYTE(mode)))
	{
		m_delimiter = static_cast<char>(HIBYTE(mode));
		if (m_delimiter != '\n')
		{
			EnableMenuItem(m_menu, IDM_USE_AGREP, MF_DISABLED | MF_GRAYED);
			CheckMenuItem(m_menu, IDM_USE_AGREP, MF_CHECKED);
			UpdateUseAgrep();
		}
		else
		{
			UINT use_agrep = GetPrivateProfileInt(_T("Settings"), _T("UseAgrep"), 0, IniPath);
			CheckMenuItem(m_menu, IDM_USE_AGREP, use_agrep ? MF_CHECKED : MF_UNCHECKED);
			UpdateUseAgrep();
			EnableMenuItem(m_menu, IDM_USE_AGREP, MF_ENABLED);
		}
	}
	AdjustScrollRange();
	SetFocus(m_hwndLine);
	if (m_path != path)
		PathCanonicalize(m_path, path);
	UpdateWindowTitle();
	m_stop = false;
	m_then = GetTickCount();
	m_thread = CreateThread(NULL, 0, StartReadThread, this, 0, NULL);
	if (m_thread != NULL)
	{
		SetTimer(m_hwnd, ReadThreadFinishedTimer, 500, NULL);
		EnableMenuItem(m_menu, IDM_REFRESH, MF_DISABLED | MF_GRAYED);
		EnableMenuItem(m_menu, IDM_STOP, MF_ENABLED);
		DrawMenuBar(m_hwnd);
	}
}

void MainWindow::SelectLine(int i)
{
	ListView_SetItemState(m_hwndList, -1, 0, LVIS_SELECTED);
	ListView_SetItemState(m_hwndList, i, LVIS_FOCUSED | LVIS_SELECTED, LVIS_FOCUSED | LVIS_SELECTED);
	ListView_EnsureVisible(m_hwndList, i, FALSE);
	ListView_SetSelectionMark(m_hwndList, i);
}

void MainWindow::DoSearch(int direction)
{
	if (int n = ListView_GetItemCount(m_hwndList))
	{
		if (SendMessage(m_hwndText, EM_GETMODIFY, 0, 0))
		{
			for (int i = 0; i < n; ++i)
			{
				m_index[HIWORD(i)][LOWORD(i)].flags &= ~1;
			}
			UINT const use_agrep = GetMenuState(m_menu, IDM_USE_AGREP, MF_BYCOMMAND) & MF_CHECKED;
			if (BSTR text = GetWindowText(m_hwndText))
			{
				// Apply convenience shortcuts for FINDSTR only when not using AGREP
				if (!use_agrep)
				{
					if (UINT count = ResolveRepetitionOperators(CountingPointer<OLECHAR>(), text))
					{
						if (BSTR resolved = SysAllocStringLen(NULL, count))
						{
							ResolveRepetitionOperators(resolved, text);
							SysFreeString(text);
							text = resolved;
						}
					}
				}
				//   261 chars quoted path to exe
				// + 261 chars quoted path to input file plus
				// +  58 chars noise and spaces
				// +  2n chars all-escaped text
				if (BSTR const cmd = SysAllocStringLen(NULL, 261 + 261 + 58 + SysStringByteLen(text)))
				{
					Transcoder transcoder(m_codepage);
					HANDLE hReadPipe = m_codepage == 1200 || m_codepage == 1201 ? transcoder.Open(m_path) : NULL;
					LPTSTR args = cmd;
					if (use_agrep)
					{
						static const TCHAR path[] = _T("BIN\\AGREP.EXE");
						if (SHRegGetPath(HKEY_LOCAL_MACHINE, _T("SOFTWARE\\GnuWin32\\Tre"), _T("InstallPath"), args, 0) == ERROR_SUCCESS)
							PathAppend(args, path);
						else
							GetFileTitle(path, args, MAX_PATH);
						PathQuoteSpaces(args);
						args = PathGetArgs(args);
						args += wsprintf(args, _T(" -n"));
						if (GetMenuState(m_menu, IDM_WHOLE_WORD, MF_BYCOMMAND) & MF_CHECKED)
							*args++ = 'w';
						if (GetMenuState(m_menu, IDM_NOTHING, MF_BYCOMMAND) & MF_CHECKED)
							*args++ = 'y';
						if (GetMenuState(m_menu, IDM_LITERAL, MF_BYCOMMAND) & MF_CHECKED)
							*args++ = 'k';
						if (GetMenuState(m_menu, IDM_IGNORE_CASE, MF_BYCOMMAND) & MF_CHECKED)
							*args++ = 'i';
						if (GetMenuState(m_menu, IDM_INVERT, MF_BYCOMMAND) & MF_CHECKED)
							*args++ = 'v';
						if (m_delimiter >= '!')
							args += wsprintf(args, _T("Md%c"), m_delimiter);
					}
					else
					{
						GetSystemDirectory(args, MAX_PATH);
						PathAppend(args, _T("FINDSTR.EXE"));
						PathQuoteSpaces(args);
						args = PathGetArgs(cmd);
						args += wsprintf(args, _T(" /N"));
						if (GetMenuState(m_menu, IDM_BEGINS_WITH, MF_BYCOMMAND) & MF_CHECKED)
							*args++ = 'B';
						if (GetMenuState(m_menu, IDM_ENDS_WITH, MF_BYCOMMAND) & MF_CHECKED)
							*args++ = 'E';
						if (GetMenuState(m_menu, IDM_LITERAL, MF_BYCOMMAND) & MF_CHECKED)
							*args++ = 'L';
						else
							*args++ = 'R';
						if (GetMenuState(m_menu, IDM_IGNORE_CASE, MF_BYCOMMAND) & MF_CHECKED)
							*args++ = 'I';
						if (GetMenuState(m_menu, IDM_INVERT, MF_BYCOMMAND) & MF_CHECKED)
							*args++ = 'V';
					}
					args += wsprintf(args, _T(" \""));
					args = EscapeArgument(args, text);
					args += wsprintf(args, _T("\""));
					if (hReadPipe == NULL)
						args += wsprintf(args, _T(" \"%s\""), m_path);
					if (HANDLE hProcess = Run(cmd, NULL, &hReadPipe, SW_SHOWMINNOACTIVE))
					{
						HCURSOR hCursor = SetCursor(LoadCursor(NULL, IDC_WAIT));
						char buffer[16];
						LineReader reader = hReadPipe;
						while (size_t len = reader.readLineAnsi(buffer, _countof(buffer) - 1, m_delimiter))
						{
							buffer[len] = '\0';
							int i = atoi(buffer) - 1;
							if (i >= 0 && i < n)
							{
								m_index[HIWORD(i)][LOWORD(i)].flags |= 0x01;
							}
							else
							{
								char extent[256];
								extent[buffer[len - 1] != m_delimiter ? reader.readLineAnsi(extent, _countof(extent) - 1, m_delimiter) : 0] = '\0';
								TCHAR text[280];
								wsprintf(text, _T("%hs%hs"), buffer, extent);
								MessageBox(m_hwnd, text, NULL, MB_ICONWARNING);
								break;
							}
							if (buffer[len - 1] != m_delimiter)
								reader.readLineAnsi(SIZE_MAX, m_delimiter);
						}
						CloseHandle(hReadPipe);
						WaitForSingleObject(hProcess, INFINITE);
						CloseHandle(hProcess);
						SetCursor(hCursor);
					}
					else
					{
						MessageBox(m_hwnd, cmd, NULL, MB_ICONWARNING);
					}
					SendMessage(m_hwndText, EM_SETMODIFY, 0, 0);
					InvalidateRect(m_hwndList, NULL, TRUE);
					SysFreeString(cmd);
				}
				SysFreeString(text);
			}
		}
		int i = ListView_GetNextItem(m_hwndList, -1, LVNI_FOCUSED);
		if (i == -1)
			i = 0;
		int j = i;
		do
		{
			i += direction;
			if (i >= n)
				i = 0;
			else if (i < 0)
				i = n - 1;
		} while ((m_index[HIWORD(i)][LOWORD(i)].flags & 1) == 0 && i != j);
		if (i != j)
			SetDlgItemInt(m_hwnd, IDC_LINE, i + 1, FALSE);
		else
			ListView_SetItemState(m_hwndList, -1, 0, LVIS_SELECTED);
	}
}

void MainWindow::DoStep(int direction, int shift)
{
	if (int n = ListView_GetItemCount(m_hwndList))
	{
		int i = GetDlgItemInt(m_hwnd, IDC_LINE, NULL, FALSE);
		i += direction;
		if (i > n)
			i = n;
		else if (i < 1)
			i = 1;
		TCHAR text[16];
		wsprintf(text, shift ? _T("%010u") : _T("%u"), i);
		SetDlgItemText(m_hwnd, IDC_LINE, text);
		int lower = 0;
		int upper = GetWindowTextLength(m_hwndLine);
		if (shift > 0 && shift <= upper)
		{
			lower = upper - shift;
			upper = lower + 1;
		}
		SendMessage(m_hwndLine, EM_SETSEL, lower, upper);
	}
}

int APIENTRY _tWinMain(HINSTANCE hInst, HINSTANCE, TCHAR* arg, int iCmdShow)
{
	hInstance = hInst;
	SetDllDirectory(_T(""));
	hCharsets = LoadLibraryEx(_T("character-sets.dll"), NULL, LOAD_LIBRARY_AS_DATAFILE);
	CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
	OleInitialize(NULL);
	InitCommonControls();
	GetModuleFileName(NULL, IniPath, _countof(IniPath));
	PathRenameExtension(IniPath, _T(".ini"));
	MainWindow(arg).Modal(hInstance, MAKEINTRESOURCEW(IDD_MAINWINDOW), NULL);
	OleUninitialize();
	CoUninitialize();
	return 0;
}
