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
#include "util.h"

HANDLE Run(LPTSTR szCmdLine, LPCTSTR szDir, HANDLE *phReadPipe, WORD wShowWindow)
{
	STARTUPINFO si;
	ZeroMemory(&si, sizeof si);
	si.cb = sizeof si;
	si.dwFlags = STARTF_USESHOWWINDOW;
	si.wShowWindow = wShowWindow;
	BOOL bInheritHandles = FALSE;
	if (phReadPipe != NULL)
	{
		SECURITY_ATTRIBUTES sa = { sizeof sa, NULL, TRUE };
		si.hStdInput = *phReadPipe;
		if (!CreatePipe(phReadPipe, &si.hStdOutput, &sa, 0))
			return NULL;
		si.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
		si.hStdError = si.hStdOutput;
		bInheritHandles = TRUE;
	}
	PROCESS_INFORMATION pi;
	if (CreateProcess(NULL, szCmdLine, NULL, NULL,
		bInheritHandles, CREATE_NEW_CONSOLE, NULL, szDir, &si, &pi))
	{
		CloseHandle(pi.hThread);
	}
	else
	{
		pi.hProcess = NULL;
		if (phReadPipe != NULL)
			CloseHandle(*phReadPipe);
	}
	if (phReadPipe != NULL)
	{
		if (si.hStdInput != NULL)
			CloseHandle(si.hStdInput);
		CloseHandle(si.hStdOutput);
	}
	return pi.hProcess;
}

LPTSTR EscapeArgument(LPTSTR p, LPCTSTR q)
{
	do switch (*q)
	{
	case TEXT('"'):
	case TEXT('/'):
		*p++ = TEXT('\\');
	default:
		*p = *q;
	} while (*q++ && *p++);
	return p;
}

/**
 * @brief Remove prefix from the text.
 * @param [in] text Text to process.
 * @param [in] prefix Prefix to remove.
 * @return Text without the prefix.
 */
LPTSTR EatPrefix(LPCTSTR text, LPCTSTR prefix, BOOL strict)
{
	if (int len = lstrlen(prefix))
#ifdef NO_SHLWAPI_STRFCNS
		if ((strict ? _tcsncmp : _tcsnicmp)(text, prefix, len) == 0)
#else
		if (StrIsIntlEqual(strict, text, prefix, len))
#endif
			return const_cast<LPTSTR>(text + len);
	return NULL;
}

BSTR ExpandTabs(BSTR text, UINT tabwidth)
{
	UINT n = SysStringLen(text);
	UINT tabmask = tabwidth - 1;
	bool t = false;
	UINT w = 0;
	UINT i;
	for (i = 0; i < n; ++i)
	{
		if (text[i] == L'\t')
		{
			t = true;
			w |= tabmask;
		}
		++w;
	}
	if (t)
	{
		if (BSTR wide = SysAllocStringLen(NULL, w))
		{
			w = 0;
			for (i = 0; i < n; ++i)
			{
				WCHAR c = text[i];
				if (c == L'\t')
				{
					c = L' ';
					while ((w & tabmask) != tabmask)
						wide[w++] = c;
				}
				wide[w++] = c;
			}
			SysFreeString(text);
			text = wide;
		}
	}
	return text;
}

BSTR GetWindowText(HWND hwnd)
{
	BSTR text = NULL;
	if (UINT len = GetWindowTextLength(hwnd))
	{
		text = SysAllocStringLen(NULL, len);
		if (text != NULL)
		{
			GetWindowText(hwnd, text, len + 1);
		}
	}
	return text;
}

int CheckMenuInt(HMENU menu, int n, UINT id, int value)
{
	int i;
	while ((i = n) > 0 && GetMenuItemID(menu, --n) == id)
	{
		TCHAR text[16];
		GetMenuString(menu, n, text, _countof(text), MF_BYPOSITION);
		SHStripMneumonic(text);
		CheckMenuItem(menu, n, value == _ttoi(text) ? MF_BYPOSITION | MF_CHECKED : MF_BYPOSITION | MF_UNCHECKED);
	}
	return i;
}

int ParseMenuInt(HMENU menu, int n, UINT id)
{
	while (n > 0 && GetMenuItemID(menu, --n) == id)
	{
		if (GetMenuState(menu, n, MF_BYPOSITION) & MF_HILITE)
		{
			TCHAR text[16];
			GetMenuString(menu, n, text, _countof(text), MF_BYPOSITION);
			SHStripMneumonic(text);
			return _ttoi(text);
		}
	}
	return 0;
}

unsigned Unslash(char *string, unsigned codepage)
{
	char *p = string;
	char *q = p;
	char c;
	do
	{
		char *r = q + 1;
		switch (c = *q)
		{
		case '\\':
			switch (c = *r++)
			{
			case 'a':
				c = '\a';
				break;
			case 'b':
				c = '\b';
				break;
			case 'f':
				c = '\f';
				break;
			case 'n':
				c = '\n';
				break;
			case 'r':
				c = '\r';
				break;
			case 't':
				c = '\t';
				break;
			case 'v':
				c = '\v';
				break;
			case 'x':
				*p = (char)strtol(r, &q, 16);
				break;
			default:
				*p = (char)strtol(r - 1, &q, 8);
				break;
			}
			if (q >= r)
				break;
			// fall through
		default:
			*p = c;
			if ((*p & 0x80) && IsDBCSLeadByteEx(codepage, *p))
				*++p = *r++;
			q = r;
		}
		++p;
	} while (c != '\0');
	return static_cast<unsigned>(p - 1 - string);
}

unsigned RemoveLeadingZeros(char *string)
{
	char *p = string;
	char *q = p;
	char c = '?';
	char d = '\0';
	do
	{
		if (*q == '0' && (c < '0' || c > '9'))
		{
			d = *q++;
			continue;
		}
		c = *q++;
		if (d != '\0' && (c < '0' || c > '9'))
		{
			*p++ = d;
		}
		*p++ = c;
		d = '\0';
	} while (c != '\0');
	return static_cast<unsigned>(p - 1 - string);
}
