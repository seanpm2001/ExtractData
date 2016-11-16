#include "stdafx.h"
#include "Yotsunoha.h"

/// Determine if decryption is possible
///
/// @param archive Archive
///
bool CYotsunoha::OnCheckDecrypt(CArcFile* archive)
{
	return CheckTpm("AAF0A99EAF4018CB1AA5E0D9065C2239");
}

/// Initialization of the decryption process
///
/// @param archive Archive
///
DWORD CYotsunoha::OnInitDecrypt(CArcFile* archive)
{
	const SFileInfo* file_info = archive->GetOpenFileInfo();

	// Decryption key
	return ((file_info->key >> 8) & 0xFF) ^ (file_info->key & 0xFF);
}

/// Decryption Process
///
/// @param target      Data to be decoded
/// @param target_size Data size
/// @param offset      Location of data to be decoded
/// @param decrypt_key Decryption key
///
DWORD CYotsunoha::OnDecrypt(BYTE* target, DWORD target_size, DWORD offset, DWORD decrypt_key)
{
	// Decryption
	BYTE byte_decrypt_key = (BYTE)decrypt_key;

	for (size_t i = 0; i < target_size; i++)
	{
		target[i] ^= byte_decrypt_key;
	}

	return target_size;
}
