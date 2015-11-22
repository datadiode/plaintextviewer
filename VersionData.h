/**
 * This code has been released into the public domain and may be used
 * for any purposes whatsoever without acknowledgment.
 */
class VersionData
{
private:
	// Disallow construction
	VersionData();
public:
	WORD   wLength;
	WORD   wValueLength;
	WORD   wType;
	WCHAR  szKey[1];
	const WCHAR *Data() const
	{
		if (this)
		{
			const WCHAR *p = szKey + lstrlenW(szKey) + 1;
			return (const WCHAR *)((INT_PTR)p + 3 & ~3);
		}
		return 0;
	}
	const VersionData *First() const
	{
		if (this)
		{
			return (const VersionData *)((INT_PTR)Data() + wValueLength + 3 & ~3);
		}
		return 0;
	}
	const VersionData *Next() const
	{
		if (this)
		{
			return (const VersionData *)((INT_PTR)this + wLength + 3 & ~3);
		}
		return 0;
	}
	const VersionData *Find(LPCWSTR szKey) const
	{
		const VersionData *p = First();
		while (p < Next())
		{
			if (szKey == 0 || lstrcmpW(szKey, p->szKey) == 0)
				return p;
			p = p->Next();
		}
		return 0;
	}
	static const VersionData *Load(HMODULE hModule = 0, LPCTSTR lpszRes = MAKEINTRESOURCE(VS_VERSION_INFO))
	{
		HRSRC hRes = FindResource(hModule, lpszRes, RT_VERSION);
		return (const VersionData *)LoadResource(hModule, hRes);
	}
};
