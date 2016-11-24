#include "StdAfx.h"
#include "SearchBase.h"

CSearchBase::CSearchBase()
{
	Init();
}

CSearchBase::~CSearchBase()
{
}

void CSearchBase::Init()
{
	m_offset = 0;
	m_num_files = 1;
}

void CSearchBase::Init(SOption* option)
{
	Init();
	OnInit(option);
}

void CSearchBase::InitPattern(const void* pattern, u32 size, u32 num)
{
	memcpy(m_pattern[num].pattern, pattern, size);
	m_pattern[num].size = size;
}

// Enter the header
void CSearchBase::InitHed(const void* pattern, u32 size)
{
	InitPattern(pattern, size, 0);
}
// Enter the footer
void CSearchBase::InitFot(const void* pattern, u32 size)
{
	InitPattern(pattern, size, 1);
}

bool CSearchBase::CmpMem(const u8* data, const u8* pattern, size_t size) const
{
	return std::equal(data, data + size, pattern, [](u8 data_byte, u8 pattern_byte) {
		return pattern_byte == '*' || data_byte == pattern_byte;
	});
}

bool CSearchBase::Search(const u8* data, u32 search_size)
{
	for (int offset = 0; offset <= static_cast<int>(search_size); offset++)
	{
		if (CmpHed(&data[offset]))
		{
			SetOffset(offset);
			return true;
		}
	}

	return false;
}

bool CSearchBase::SearchFot(CArcFile* archive)
{
	const u32 footer_size = GetFotSize();
	while (true)
	{
		std::array<u8, SEARCH_BUFFER_SIZE> buffer;
		const u32 read_size = archive->Read(buffer.data(), buffer.size());
		
		if (read_size < footer_size)
		{
			archive->GetProg()->UpdatePercent(read_size);
			break;
		}

		const u32 search_size = read_size - footer_size;
		for (int i = 0; i <= (int)search_size; i++)
		{
			if (CmpFot(&buffer[i]))
			{
				// read_size - i -- Moves back to position found by i, has moved to the footer file and proceeds from the footer_size
				archive->Seek(-((int)read_size-i - (int)footer_size), FILE_CURRENT);
				archive->GetProg()->UpdatePercent(i+footer_size);
				return true;
			}
		}
		archive->GetProg()->UpdatePercent(search_size);
		archive->Seek(-((int)footer_size-1), FILE_CURRENT);
	}
	return false;
}
