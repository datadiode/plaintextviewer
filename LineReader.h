/**
 * @brief A reader that reads textual data line by line from a file.
 */
class LineReader
{
public:
	enum Encoding { NONE = 0x00, GUESS = 0x01, ANSI = 0xEE, UTF8 = 0xEF, UCS2BE = 0xFE, UCS2LE = 0xFF };
	LineReader(HANDLE handle)
		: m_handle(handle), m_index(0), m_ahead(0)
	{
	}
	size_t readBom(Encoding &, Encoding guess = NONE);
	size_t readLineAnsi(size_t limit, char eol = '\n');
	size_t readLineAnsi(char *buffer, size_t limit, char eol = '\n');
	size_t peekLineAnsi(size_t index, char *buffer, size_t limit, char eol = '\n');
	size_t readLineWide(size_t limit, wchar_t eol = L'\n');
private:
	HANDLE const m_handle;
	size_t m_index;
	size_t m_ahead;
	BYTE m_chunk[0x8000];
	LineReader &LineReader::operator=(const LineReader &);
};

