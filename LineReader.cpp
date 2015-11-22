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
#include <windows.h>
#include <wchar.h>
#include "LineReader.h"

size_t LineReader::readBom(Encoding &encoding, Encoding guess)
{
	DWORD bytes;
	m_ahead = ReadFile(m_handle, m_chunk, sizeof m_chunk, &bytes, NULL) ? bytes : 0;
	if (m_ahead >= 2)
	{
		if (m_chunk[0] == 0xFF && m_chunk[1] == 0xFE || m_chunk[0] == 0xFE && m_chunk[1] == 0xFF)
			m_index = 2;
		else if (m_ahead >= 3 && m_chunk[0] == 0xEF && m_chunk[1] == 0xBB && m_chunk[2] == 0xBF)
			m_index = 3;
		else if (guess == GUESS)
		{
			if (m_chunk[0] != 0x00 && m_chunk[1] == 0x00)
				guess = UCS2LE;
			else if (m_chunk[0] == 0x00 && m_chunk[1] != 0x00)
				guess = UCS2BE;
		}
	}
	m_ahead -= m_index;
	encoding = m_index ? static_cast<Encoding>(m_chunk[0]) : guess;
	return m_index;
}

size_t LineReader::readLineAnsi(size_t limit, char eol)
{
	size_t count = 0;
	do 
	{
		size_t n = count;
		size_t delta = limit - n;
		if (delta > m_ahead)
			delta = m_ahead;
		count = n + delta;
		BYTE *lower = m_chunk + m_index;
		if (BYTE *upper = static_cast<BYTE *>(memchr(m_chunk + m_index, eol, delta)))
		{
			++upper;
			delta = upper - lower;
			limit = n + delta;
			count = limit; // causes loop termination
		}
		if (count == limit)
		{
			m_index += delta;
			m_ahead -= delta;
			break;
		}
		m_index = 0;
		DWORD bytes;
		m_ahead = ReadFile(m_handle, m_chunk, sizeof m_chunk, &bytes, NULL) ? bytes : 0;
	} while (m_ahead != 0);
	return count;
}

size_t LineReader::readLineWide(size_t limit, wchar_t eol)
{
	size_t count = 0;
	do 
	{
		size_t n = count;
		size_t delta = limit - n;
		if (delta > m_ahead)
			delta = m_ahead;
		count = n + delta;
		BYTE *lower = m_chunk + m_index;
		if (BYTE *upper = reinterpret_cast<BYTE *>(wmemchr(
			reinterpret_cast<wchar_t *>(m_chunk + m_index), eol, delta / sizeof(wchar_t))))
		{
			upper += sizeof(wchar_t);
			delta = upper - lower;
			limit = n + delta;
			count = limit; // causes loop termination
		}
		if (count == limit)
		{
			m_index += delta;
			m_ahead -= delta;
			break;
		}
		m_index = 0;
		DWORD bytes;
		m_ahead = ReadFile(m_handle, m_chunk, sizeof m_chunk, &bytes, NULL) ? bytes : 0;
	} while (m_ahead != 0);
	return count;
}

size_t LineReader::readLineAnsi(char *buffer, size_t limit, char eol)
{
	size_t count = 0;
	do 
	{
		size_t n = count;
		size_t delta = limit - n;
		if (delta > m_ahead)
			delta = m_ahead;
		count = n + delta;
		char *lower = buffer + n;
		if (char *upper = static_cast<char *>(_memccpy(lower, m_chunk + m_index, eol, delta)))
		{
			delta = upper - lower;
			limit = n + delta;
			count = limit; // causes loop termination
		}
		if (count == limit)
		{
			m_index += delta;
			m_ahead -= delta;
			break;
		}
		m_index = 0;
		DWORD bytes;
		m_ahead = ReadFile(m_handle, m_chunk, sizeof m_chunk, &bytes, NULL) ? bytes : 0;
	} while (m_ahead != 0);
	return count;
}

size_t LineReader::peekLineAnsi(size_t index, char *buffer, size_t limit, char eol)
{
	size_t ahead = m_ahead - index;
	if (ahead > limit)
		ahead = limit;
	char *upper = static_cast<char *>(_memccpy(buffer, m_chunk + index, eol, ahead));
	return upper ? upper - buffer : 0;
}
