struct EncodingInfo
{
	unsigned short cp;
	char name[sizeof"windows-12345"];
	static EncodingInfo const *From(HMODULE module, const char *name)
	{
		return reinterpret_cast<EncodingInfo *>(GetProcAddress(module, name));
	}
};
