struct EncodingInfo
{
	unsigned short id;
	unsigned short cp;
	char name[sizeof"windows-12345"];
	static EncodingInfo const *From(HMODULE module, const char *name)
	{
		return reinterpret_cast<EncodingInfo *>(GetProcAddress(module, name));
	}
	template<typename IMAGE_NT_HEADERS>
	static EncodingInfo const *From(HMODULE module, const char *name)
	{
		return reinterpret_cast<EncodingInfo *>(GetProcAddress<IMAGE_NT_HEADERS>(module, name));
	}
};
