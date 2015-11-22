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
#include <windows.h>
#include <wchar.h>
#include "Transcoder.h"

Transcoder::Transcoder(UINT codepage)
	: m_codepage(codepage)
	, m_handle(NULL)
	, m_output(NULL)
	, m_thread(NULL)
{
}

Transcoder::~Transcoder()
{
	if (m_handle != INVALID_HANDLE_VALUE)
	{
		CloseHandle(m_handle);
	}
	if (m_thread)
	{
		WaitForSingleObject(m_thread, INFINITE);
		CloseHandle(m_thread);
	}
	else if (m_output != NULL)
	{
		CloseHandle(m_output);
	}
}

HANDLE Transcoder::Open(LPCTSTR path)
{
	m_handle = CreateFile(path, FILE_GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
	HANDLE input = NULL;
	SECURITY_ATTRIBUTES sa = { sizeof sa, NULL, TRUE };
	if (!CreatePipe(&input, &m_output, &sa, 0))
		return NULL;
	m_thread = CreateThread(NULL, 0, StartThread, this, 0, NULL);
	return input;
}

DWORD Transcoder::Thread()
{
	WCHAR buf[0x1000 + 1];
	buf[0] = 0;
	DWORD count = 0;
	do
	{
		LPWSTR src = buf + 1;
		if (!ReadFile(m_handle, src, sizeof buf - sizeof(WCHAR), &count, NULL))
			break;
		WCHAR last = 0;
		int len = count;
		switch (m_codepage)
		{
		case 1201:
			_swab(reinterpret_cast<LPSTR>(buf + 1), reinterpret_cast<LPSTR>(buf + 1), count);
		case 1200:
			if (count >= sizeof(WCHAR))
			{
				count /= sizeof(WCHAR);
				last = buf[count];
				if (buf[0] >= 0xD800 && buf[0] <= 0xDBFF)
				{
					--src;
					++count;
				}
				if (last >= 0xD800 && last <= 0xDBFF)
				{
					--count;
				}
				CHAR tmp[0x3000];
				len = WideCharToMultiByte(CP_UTF8, 0, src, count, tmp, sizeof tmp, NULL, NULL);
				src = reinterpret_cast<LPWSTR>(tmp);
			}
			break;
		}
		if (len < 0 || !WriteFile(m_output, src, len, &count, NULL))
			break;
		buf[0] = last;
	} while (count != 0);
	CloseHandle(m_output);
	return 0;
}

DWORD WINAPI Transcoder::StartThread(LPVOID pv)
{
	return static_cast<Transcoder *>(pv)->Thread();
}
