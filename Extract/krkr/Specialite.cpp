#include "stdafx.h"
#include "Specialite.h"

/// Determine if decryption is possible
///
/// @param archive Archive
///
bool CSpecialite::OnCheckDecrypt(CArcFile* archive)
{
	return CheckTpm("F78FF15C0BD396080BCDF431AED59211");
}

/// Initialization of the decryption process
///
/// @param archive Archive
///
DWORD CSpecialite::OnInitDecrypt(CArcFile* archive)
{
	return 0xAF;
}

/// Decryption Process
///
/// @param target      Data to be decoded
/// @param target_size Data size
/// @param offset      Location of data to be decoded
/// @param decrypt_key Decryption key
///
DWORD CSpecialite::OnDecrypt(BYTE* target, DWORD target_size, DWORD offset, DWORD decrypt_key)
{
	BYTE byte_decrypt_key = (BYTE)decrypt_key;

	for (size_t i = 0; i < target_size; i++)
	{
		target[i] ^= byte_decrypt_key;
		target[i] += 1;
	}

	return target_size;
}
