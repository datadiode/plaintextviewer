class Transcoder
{
	UINT m_codepage;
	HANDLE m_handle;
	HANDLE m_output;
	HANDLE m_thread;
public:
	Transcoder(UINT codepage);
	~Transcoder();
	HANDLE Open(LPCTSTR path);
private:
	DWORD Thread();
	static DWORD WINAPI StartThread(LPVOID);
};
