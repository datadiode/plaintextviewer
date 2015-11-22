/*
 * Copyright (c) 2011 Jochen Neubeck
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
#pragma once

class Subclass
{
public:
	template<UINT id>
	class DlgItem
	{
		LONG_PTR m_super;
	public:
		operator LONG_PTR() const { return m_super; }
	protected:
		DlgItem(): m_super(0) { }
		HWND Init(HWND hwnd)
		{
			hwnd = GetDlgItem(hwnd, id);
			SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LPARAM>(this));
			m_super = SetWindowLongPtr(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(WndProc));
			return hwnd;
		}
	private:
		virtual LRESULT DoMsg(DlgItem &, HWND, UINT, WPARAM, LPARAM) = 0;
		static LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
		{
			DlgItem *pThis = reinterpret_cast<DlgItem *>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
			return pThis->DoMsg(*pThis, hwnd, message, wParam, lParam);
		}
	};

	Subclass() { }
	virtual ~Subclass() { }

	INT_PTR Modal(HINSTANCE hInstance, LPCWSTR lpTemplateName, HWND hWndParent)
	{
		return DialogBoxParamW(hInstance, lpTemplateName, hWndParent, ModalDlgProc, reinterpret_cast<LPARAM>(this));
	}

	HWND Create(HINSTANCE hInstance, LPCWSTR lpTemplateName, HWND hWndParent)
	{
		HWND hwnd = CreateDialogParamW(hInstance, lpTemplateName, hWndParent, ModelessDlgProc, reinterpret_cast<LPARAM>(this));
		if (hwnd == NULL)
			delete this; // don't leak memory in case window creation fails
		return hwnd;
	}

protected:
	LONG_PTR SubclassWndProc(HWND hwnd)
	{
		SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LPARAM>(this));
		return SetWindowLongPtr(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(WndProc));
	}

	LONG_PTR SubclassDlgProc(HWND hwnd)
	{
		SetWindowLongPtr(hwnd, DWLP_DLGPROC, 0);
		return SubclassWndProc(hwnd);
	}

	static LRESULT Default(LRESULT super, HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
	{
		return CallWindowProc(reinterpret_cast<WNDPROC>(super), hwnd, message, wParam, lParam);
	}

private:
	virtual LRESULT DoMsg(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) = 0;

	static INT_PTR CALLBACK ModalDlgProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
	{
		LRESULT lResult = 0;
		if (message == WM_INITDIALOG)
			SetWindowLongPtr(hwnd, GWLP_USERDATA, lParam);
		if (Subclass *p = reinterpret_cast<Subclass *>(GetWindowLongPtr(hwnd, GWLP_USERDATA)))
			lResult = p->DoMsg(hwnd, message, wParam, lParam);
		return lResult;
	}

	static INT_PTR CALLBACK ModelessDlgProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
	{
		LRESULT lResult = ModalDlgProc(hwnd, message, wParam, lParam);
		if (message == WM_DESTROY)
			delete reinterpret_cast<Subclass *>(SetWindowLongPtr(hwnd, GWLP_USERDATA, 0));
		return lResult;
	}

	static LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
	{
		LRESULT lResult = 0;
		if (Subclass *p = reinterpret_cast<Subclass *>(GetWindowLongPtr(hwnd, GWLP_USERDATA)))
			lResult = p->DoMsg(hwnd, message, wParam, lParam);
		if (message == WM_DESTROY && lResult != 0)
		{
			SetWindowLongPtr(hwnd, GWLP_WNDPROC, lResult);
			delete reinterpret_cast<Subclass *>(SetWindowLongPtr(hwnd, GWLP_USERDATA, 0));
			lResult = Default(lResult, hwnd, message, wParam, lParam);
		}
		return lResult;
	}

	Subclass(const Subclass &); // instances are non-copyable
	Subclass &operator=(const Subclass &); // instances are non-assignable
};
