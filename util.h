//#define NO_SHLWAPI_STRFCNS

#include <tchar.h>
#include <shlwapi.h>

#ifndef NO_SHLWAPI_STRFCNS
#define strcmp StrCmpA
#define wsccmp StrCmpW
#define strchr StrChrA
#define wcschr StrChrW
#define strrchr(s, c) StrRChrA(s, NULL, c)
#define wcsrchr(s, c) StrRChrW(s, NULL, c)
#define strspn StrSpnA
#define strcspn StrCSpnA
#define atoi StrToIntA
#define atol StrToIntA
#define _wtoi StrToIntW
#define _wtol StrToIntW
#define strlen lstrlenA
#define wcslen lstrlenW
#define _strlwr CharLowerA
#define _wcslwr CharLowerW
#endif

inline LPSTR TrimString(LPSTR s, LPCSTR t)
{
#ifdef NO_SHLWAPI_STRFCNS
	s = _strrev(s + strspn(s, t));
	s = _strrev(s + strspn(s, t));
#else
	StrTrimA(s, t);
#endif
	return s;
}

inline LPWSTR TrimString(LPWSTR s, LPCWSTR t)
{
#ifdef NO_SHLWAPI_STRFCNS
	s = _wcsrev(s + wcsspn(s, t));
	s = _wcsrev(s + wcsspn(s, t));
#else
	StrTrimW(s, t);
#endif
	return s;
}

template<class T>
LPCTSTR ParseInt(LPCTSTR p, T &value)
{
	if (p)
	{
		LPTSTR q = NULL;
		value = static_cast<T>(_tcstol(p, &q, 10));
		p = *q == _T(',') ? q + 1 : NULL;
	}
	return p;
}

class LogFont : public LOGFONT
{
public:
	LogFont()
	{
		ZeroMemory(this, sizeof *this);
	}
	void Parse(LPCTSTR buf)
	{
		buf = ParseInt(buf, lfHeight);
		buf = ParseInt(buf, lfWidth);
		buf = ParseInt(buf, lfEscapement);
		buf = ParseInt(buf, lfOrientation);
		buf = ParseInt(buf, lfWeight);
		buf = ParseInt(buf, lfItalic);
		buf = ParseInt(buf, lfUnderline);
		buf = ParseInt(buf, lfStrikeOut);
		buf = ParseInt(buf, lfCharSet);
		buf = ParseInt(buf, lfOutPrecision);
		buf = ParseInt(buf, lfClipPrecision);
		buf = ParseInt(buf, lfQuality);
		buf = ParseInt(buf, lfPitchAndFamily);
		// lstrcpyn() protects itself against NULL arguments
		lstrcpyn(lfFaceName, buf, LF_FACESIZE);
	}
	void Print(LPTSTR buf)
	{
		wsprintf(buf, _T("%ld,%ld,%ld,%ld,%ld,%u,%u,%u,%u,%u,%u,%u,%u,%s"),
			lfHeight, lfWidth, lfEscapement, lfOrientation, lfWeight,
			lfItalic, lfUnderline, lfStrikeOut, lfCharSet, lfOutPrecision,
			lfClipPrecision, lfQuality, lfPitchAndFamily, lfFaceName);
	}
};

class WindowPlacement : public WINDOWPLACEMENT
{
public:
	WindowPlacement()
	{
		length = sizeof(WINDOWPLACEMENT);
		flags = 0;
	}
	void Parse(LPCTSTR buf)
	{
		buf = ParseInt(buf, showCmd);
		buf = ParseInt(buf, ptMinPosition.x);
		buf = ParseInt(buf, ptMinPosition.y);
		buf = ParseInt(buf, ptMaxPosition.x);
		buf = ParseInt(buf, ptMaxPosition.y);
		buf = ParseInt(buf, rcNormalPosition.left);
		buf = ParseInt(buf, rcNormalPosition.top);
		buf = ParseInt(buf, rcNormalPosition.right);
		buf = ParseInt(buf, rcNormalPosition.bottom);
	}
	void Print(LPTSTR buf)
	{
		wsprintf(buf, _T("%u,%d,%d,%d,%d,%d,%d,%d,%d"),
			showCmd, ptMinPosition.x, ptMinPosition.y, ptMaxPosition.x, ptMaxPosition.y,
			rcNormalPosition.left, rcNormalPosition.top, rcNormalPosition.right, rcNormalPosition.bottom);
	}
};

template<size_t size, typename type = char, typename alignment = int>
struct pod
{
	alignment data[ size * sizeof(type) % sizeof(alignment) == 0 ?
					size * sizeof(type) / sizeof(alignment) : -1 ];
	operator type* () { return reinterpret_cast<type*>(data); }
	static void swap(void* p, void* q)
	{
		std::swap(*static_cast<pod*>(p), *static_cast<pod*>(q));
	}
};

inline char *NumToStr(UINT64 value, char *current = &pod<32>()[32])
{
	UINT32 group = 1 << 3 | 1 << 6 | 1 << 9 | 1 << 12 | 1 << 15 | 1 << 18;
	*--current = '\0';
	do
	{
		if (group & 1)
			*--current = ',';
		group >>= 1;
		*--current = static_cast<char>(value % 10) + '0';
		value /= 10;
	} while (value != 0);
	return current;
}

HANDLE Run(LPTSTR szCmdLine, LPCTSTR szDir, HANDLE *phReadPipe, WORD wShowWindow);
LPTSTR EscapeArgument(LPTSTR p, LPCTSTR q);
LPTSTR EatPrefix(LPCTSTR text, LPCTSTR prefix, BOOL strict = FALSE);
BSTR ExpandTabs(BSTR text, UINT tabwidth);
BSTR GetWindowText(HWND);

int CheckMenuInt(HMENU, int, UINT, int);
int ParseMenuInt(HMENU, int, UINT);

unsigned Unslash(char *string, unsigned codepage = CP_ACP);
unsigned RemoveLeadingZeros(char *string);

#define DECLARE_BIT_FIELD(name, bits) name : bits; static unsigned const static_##name##_bits = bits
#define MIN_BIT_FIELD_SIGNED(type, name) static_cast<signed>(1 << (type::static_##name##_bits - 1))
#define MAX_BIT_FIELD_SIGNED(type, name) static_cast<signed>((1 << (type::static_##name##_bits - 1)) - 1)
#define MAX_BIT_FIELD_UNSIGNED(type, name) static_cast<unsigned>((1 << type::static_##name##_bits) - 1)

template<typename T>
class CountingPointer
{
	unsigned count;
	T content;
public:
	CountingPointer(unsigned count = 0): count(count), content(0) { }
	operator unsigned() { return count; }
	CountingPointer &operator++(int) { ++count; return *this; }
	T &operator*() { return content; }
};

template<typename P, typename Q>
P ResolveRepetitionOperators(P p, Q q)
{
	wchar_t const *u = q;
	wchar_t const *v = 0;
	do switch (*q)
	{
	case '[':
		u = q;
	case ']':
		v = q;
		*p = *q;
		break;
	case '+':
		if (u < v)
		{
			wchar_t const *w = u;
			do 
			{
				*p++ = *w++;
			} while (w <= v);
			*p = '*';
			v = 0;
		}
		else
		{
			*p = '+';
		}
		break;
	case '^':
		if (u < v)
		{
			wchar_t const *w = u;
			do 
			{
				*p++ = *w++;
			} while (w < v);
			*p = *w;
		}
		else
		{
			*p = '^';
		}
		break;
	case '\\':
		*p++ = *q++;
	default:
		*p = *q;
		v = 0;
		break;
	} while (*q++ && *p++);
	return p;
}

template<typename IMAGE_NT_HEADERS>
LPVOID GetProcAddress(HMODULE hModule, LPCSTR lpProcName)
{
	BYTE *const pORG = reinterpret_cast<BYTE *>(reinterpret_cast<INT_PTR>(hModule) & ~1);
	if (pORG == reinterpret_cast<BYTE *>(hModule))
		return NULL; // failed to load or not loaded with LOAD_LIBRARY_AS_DATAFILE
	IMAGE_DOS_HEADER const *const pMZ = reinterpret_cast<IMAGE_DOS_HEADER const *>(pORG);
	IMAGE_NT_HEADERS const *const pPE = reinterpret_cast<IMAGE_NT_HEADERS const *>(pORG + pMZ->e_lfanew);
	if (pPE->FileHeader.SizeOfOptionalHeader != sizeof pPE->OptionalHeader)
		return NULL; // mismatched bitness
	IMAGE_SECTION_HEADER const *pSH = IMAGE_FIRST_SECTION(pPE);
	WORD nSH = pPE->FileHeader.NumberOfSections;
	DWORD offset = pPE->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress;
	// Search for the section which contains the offset at hand
	while ((pSH->VirtualAddress > offset || pSH->VirtualAddress + pSH->Misc.VirtualSize <= offset) && --nSH)
		++pSH;
	if (nSH == 0)
		return NULL; // section not found
	BYTE const *const pRD = pORG + pSH->PointerToRawData - pSH->VirtualAddress;
	IMAGE_EXPORT_DIRECTORY const *const pEAT = reinterpret_cast<IMAGE_EXPORT_DIRECTORY const *>(pRD + offset);
	ULONG const *const rgAddressOfNames = reinterpret_cast<ULONG const *>(pRD + pEAT->AddressOfNames);
	DWORD lower = 0;
	DWORD upper = pEAT->NumberOfNames;
	int const cbProcName = lstrlenA(lpProcName) + 1;
	while (lower < upper)
	{
		DWORD const match = (upper + lower) >> 1;
		int const cmp = memcmp(pRD + rgAddressOfNames[match], lpProcName, cbProcName);
		if (cmp >= 0)
			upper = match;
		if (cmp <= 0)
			lower = match + 1;
	}
	if (lower <= upper)
		return NULL; // name not found
	WORD const *const rgNameIndexes = reinterpret_cast<WORD const *>(pRD + pEAT->AddressOfNameOrdinals);
	DWORD const *rgEntyPoints = reinterpret_cast<DWORD const *>(pRD + pEAT->AddressOfFunctions);
	pSH = IMAGE_FIRST_SECTION(pPE);
	nSH = pPE->FileHeader.NumberOfSections;
	offset = rgEntyPoints[rgNameIndexes[upper]];
	// Search for the section which contains the offset at hand
	while ((pSH->VirtualAddress > offset || pSH->VirtualAddress + pSH->Misc.VirtualSize <= offset) && --nSH)
		++pSH;
	if (nSH == 0)
		return NULL; // section not found
	return pORG + pSH->PointerToRawData - pSH->VirtualAddress + offset;
}
