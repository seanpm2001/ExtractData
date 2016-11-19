#include "StdAfx.h"
#include "../Image.h"
#include "Will.h"
#include "Utils/ArrayUtils.h"

/// Mounting
bool CWill::Mount(CArcFile* pclArc)
{
	if (lstrcmpi(pclArc->GetArcExten(), _T(".arc")) != 0)
		return false;

	bool is_match = false;

	static const char*	apszHeader[] =
	{
		"OGG", "WSC", "ANM", "MSK", "WIP", "TBL", "SCR"
	};

	for (DWORD i = 0; i < ArrayUtils::ArraySize(apszHeader); i++)
	{
		if (memcmp(&pclArc->GetHed()[4], apszHeader[i], 4) == 0)
		{
			is_match = true;
			break;
		}
	}

	if (!is_match)
	{
		return false;
	}

	// Get number of file formats
	DWORD dwFileFormats;
	pclArc->Read(&dwFileFormats, 4);

	// Get file format index
	DWORD          dwFormatIndexSize = 12 * dwFileFormats;
	YCMemory<BYTE> clmbtFormatIndex(dwFormatIndexSize);
	DWORD          dwFormatIndexPtr = 0;
	pclArc->Read(&clmbtFormatIndex[0], dwFormatIndexSize);

	// Get index size
	DWORD dwIndexSize = 0;
	for (DWORD i = 0; i < dwFileFormats; i++)
	{
		dwIndexSize += *(DWORD*)&clmbtFormatIndex[12 * i + 4] * 17;
	}

	// Get index
	YCMemory<BYTE> clmbtIndex(dwIndexSize);
	DWORD          dwIndexPtr = 0;
	pclArc->Read(&clmbtIndex[0], dwIndexSize);

	// Get file information
	std::vector<SFileInfo>	vcFileInfo;
	std::vector<SFileInfo>	vcMaskFileInfo;
	std::vector<SFileInfo>	vcNotMaskFileInfo;

	for (DWORD i = 0; i < dwFileFormats; i++)
	{
		// Get filetype extension
		TCHAR szFileExt[8];
		memcpy(szFileExt, &clmbtFormatIndex[dwFormatIndexPtr], 4);
		szFileExt[4] = '\0';
		::CharLower(szFileExt);

		// Get file information
		DWORD dwFiles = *(DWORD*)&clmbtFormatIndex[dwFormatIndexPtr + 4];

		for (DWORD j = 0; j < dwFiles; j++)
		{
			// Get file name
			char szFileTitle[16];
			memcpy(szFileTitle, &clmbtIndex[dwIndexPtr], 9);
			szFileTitle[9] = '\0';

			TCHAR szFileName[32];
			_stprintf(szFileName, _T("%s.%s"), szFileTitle, szFileExt);

			// Add information to the list
			SFileInfo stFileInfo;
			stFileInfo.name = szFileName;
			stFileInfo.sizeCmp = *(DWORD*)&clmbtIndex[dwIndexPtr + 9];
			stFileInfo.sizeOrg = stFileInfo.sizeCmp;
			stFileInfo.start = *(DWORD*)&clmbtIndex[dwIndexPtr + 13];
			stFileInfo.end = stFileInfo.start + stFileInfo.sizeCmp;

			if (lstrcmp(szFileExt, _T("msk")) == 0)
			{
				// Masked image
				vcMaskFileInfo.push_back(stFileInfo);
			}
			else
			{
				vcFileInfo.push_back(stFileInfo);
			}

			dwIndexPtr += 17;
		}

		dwFormatIndexPtr += 12;
	}

	// Sort by filename
	std::sort(vcFileInfo.begin(), vcFileInfo.end(), CArcFile::CompareForFileInfo);

	// Get file information from the mask image
	for (size_t i = 0; i < vcMaskFileInfo.size(); i++)
	{
		SFileInfo* pstsiMask = &vcMaskFileInfo[i];

		// Get the name of the file to be created
		TCHAR szTargetName[_MAX_FNAME];
		lstrcpy(szTargetName, pstsiMask->name);
		PathRenameExtension(szTargetName, _T(".wip"));

		// Getting file information to be created
		SFileInfo* pstsiTarget = nullptr;
		pstsiTarget = pclArc->SearchForFileInfo(vcFileInfo, szTargetName);
		if (pstsiTarget != nullptr)
		{
			// Definitely the mask image
			pstsiTarget->starts.push_back(pstsiMask->start);
			pstsiTarget->sizesCmp.push_back(pstsiMask->sizeCmp);
			pstsiTarget->sizesOrg.push_back(pstsiMask->sizeOrg);

			// Progress update
			pclArc->GetProg()->UpdatePercent(pstsiMask->sizeCmp);
		}
		else
		{
			// Is not a mask image

			vcNotMaskFileInfo.push_back(*pstsiMask);
		}
	}

	// Add to listview
	for (size_t i = 0; i < vcFileInfo.size(); i++)
	{
		pclArc->AddFileInfo(vcFileInfo[i]);
	}

	for (size_t i = 0; i < vcNotMaskFileInfo.size(); i++)
	{
		pclArc->AddFileInfo(vcNotMaskFileInfo[i]);
	}

	return true;
}

/// Decoding
bool CWill::Decode(CArcFile* pclArc)
{
	const SFileInfo* file_info = pclArc->GetOpenFileInfo();
	if (file_info->format != _T("WIP") && file_info->format != _T("MSK"))
		return false;

	// Read data
	DWORD          dwSrcSize = file_info->sizeCmp;
	YCMemory<BYTE> clmbtSrc(dwSrcSize);
	DWORD          dwSrcPtr = 0;
	pclArc->Read(&clmbtSrc[0], dwSrcSize);

	// Get number of files and number of colors
	WORD wFiles = *(WORD*)&clmbtSrc[4];
	WORD wBpp = *(WORD*)&clmbtSrc[6];
	dwSrcPtr += 8;

	// Get width and height
	std::vector<long>  vclWidth;
	std::vector<long>  vclHeight;
	std::vector<DWORD> vcdwSrcSize;

	for (WORD i = 0; i < wFiles; i++)
	{
		vclWidth.push_back(*(long*)&clmbtSrc[dwSrcPtr + 0]);
		vclHeight.push_back(*(long*)&clmbtSrc[dwSrcPtr + 4]);
		vcdwSrcSize.push_back(*(DWORD*)&clmbtSrc[dwSrcPtr + 20]);

		dwSrcPtr += 24;
	}

	// Is image mask there or not.
	BOOL bExistsMask = !file_info->starts.empty();

	// Get image mask
	DWORD              dwSrcSizeForMask = 0;
	YCMemory<BYTE>     clmbtSrcForMask;
	DWORD              dwSrcPtrForMask = 0;
	WORD               wFilesForMask = 0;
	WORD               wBppForMask = 0;
	std::vector<long>  vclWidthForMask;
	std::vector<long>  vclHeightForMask;
	std::vector<DWORD> vcdwSrcSizeForMask;

	if (bExistsMask)
	{
		// Image mask exists
		dwSrcSizeForMask = file_info->sizesCmp[0];
		dwSrcPtrForMask = 0;
		clmbtSrcForMask.resize(dwSrcSizeForMask);

		// Read image mask
		pclArc->SeekHed(file_info->starts[0]);
		pclArc->Read(&clmbtSrcForMask[0], dwSrcSizeForMask);

		// Get number of files and colors
		wFilesForMask = *(WORD*)&clmbtSrcForMask[4];
		wBppForMask = *(WORD*)&clmbtSrcForMask[6];
		dwSrcPtrForMask += 8;

		// Get width and height
		for (WORD i = 0; i < wFilesForMask; i++)
		{
			vclWidthForMask.push_back(*(long*)&clmbtSrcForMask[dwSrcPtrForMask + 0]);
			vclHeightForMask.push_back(*(long*)&clmbtSrcForMask[dwSrcPtrForMask + 4]);
			vcdwSrcSizeForMask.push_back(*(DWORD*)&clmbtSrcForMask[dwSrcPtrForMask + 20]);

			dwSrcPtrForMask += 24;
		}

		// Check to see if they have the same number of files
		bExistsMask = (wFiles == wFilesForMask);
	}

	// Output

	for (WORD i = 0; i < wFiles; i++)
	{
		// マスク画像の付加で変更されるため再取得
		WORD wBpp = *(WORD*)&clmbtSrc[6];

		// Ensure the output buffer exists
		DWORD          dwDstSize = vclWidth[i] * vclHeight[i] * (wBpp >> 3);
		YCMemory<BYTE> clmbtDst(dwDstSize);
		ZeroMemory(&clmbtDst[0], dwDstSize);
		BYTE* pbtDst = &clmbtDst[0];

		// Get pallet
		BYTE* pbtPallet = nullptr;

		if (wBpp == 8)
		{
			pbtPallet = &clmbtSrc[dwSrcPtr];
			dwSrcPtr += 1024;
		}

		// LZSS Decompression
		DecompLZSS(&clmbtDst[0], dwDstSize, &clmbtSrc[dwSrcPtr], vcdwSrcSize[i]);
		dwSrcPtr += vcdwSrcSize[i];

		// マスク画像を付加して32bit化
		DWORD          dwDstSizeFor32bit;
		YCMemory<BYTE> clmbtDstFor32bit;

		if (bExistsMask)
		{
			// Image mask exists
			DWORD dwDstSizeForMask = vclWidthForMask[i] * vclHeightForMask[i] * (wBppForMask >> 3);
			YCMemory<BYTE> clmbtDstForMask(dwDstSizeForMask);
			ZeroMemory(&clmbtDstForMask[0], dwDstSizeForMask);

			// Get pallet
			BYTE* pbtPalletForMask = nullptr;

			if (wBppForMask == 8)
			{
				pbtPalletForMask = &clmbtSrcForMask[dwSrcPtrForMask];
				dwSrcPtrForMask += 1024;
			}

			// LZSS Decompression
			DecompLZSS(&clmbtDstForMask[0], dwDstSizeForMask, &clmbtSrcForMask[dwSrcPtrForMask], vcdwSrcSizeForMask[i]);
			dwSrcPtrForMask += vcdwSrcSizeForMask[i];

			// Add mask to image
			dwDstSizeFor32bit = vclWidth[i] * vclHeight[i] * 4;
			clmbtDstFor32bit.resize(dwDstSizeFor32bit);
			if (AppendMask(&clmbtDstFor32bit[0], dwDstSizeFor32bit, &clmbtDst[0], dwDstSize, &clmbtDstForMask[0], dwDstSizeForMask))
			{
				// Success in adding the mask to the image
				wBpp = 32;
				dwDstSize = dwDstSizeFor32bit;
				pbtDst = &clmbtDstFor32bit[0];
			}
		}

		// Get file name
		TCHAR szFileExt[256];

		if (wFiles == 1)
		{
			// One file
			lstrcpy(szFileExt, _T(""));
		}
		else
		{
			// Two or more files
			_stprintf(szFileExt, _T("_%03u.bmp"), i);
		}

		// Request progress bar progress
		const bool progress = i == 0;

		// Output
		CImage clImage;
		clImage.Init(pclArc, vclWidth[i], vclHeight[i], wBpp, pbtPallet, 1024, szFileExt);
		clImage.WriteCompoBGRAReverse(pbtDst, dwDstSize, progress);
		clImage.Close();
	}

	return true;
}

/// LZSS Decompression
///
/// Parameters:
/// @param dst      Destination
/// @param dst_size Destination Size
/// @param src      Compressed data
/// @param src_size Compressed data size
///
void CWill::DecompLZSS(u8* dst, size_t dst_size, const u8* src, size_t src_size)
{
	size_t src_ptr = 0;
	size_t dst_ptr = 0;

	// Slide dictionary
	constexpr size_t dictionary_size = 4096;
	std::vector<u8> dictionary(dictionary_size);
	size_t dictionary_ptr = 1;

	while (src_ptr < src_size && dst_ptr < dst_size)
	{
		u8 flags = src[src_ptr++];

		for (size_t i = 0; i < 8 && src_ptr < src_size && dst_ptr < dst_size; i++)
		{
			if (flags & 1)
			{
				// Uncompressed data
				dst[dst_ptr++] = dictionary[dictionary_ptr++] = src[src_ptr++];
				dictionary_ptr &= dictionary_size - 1;
			}
			else
			{
				// Compressed data
				const u8 low = src[src_ptr++];
				const u8 high = src[src_ptr++];

				size_t back = ((low << 8) | high) >> 4;
				if (back == 0)
				{
					// Completed decompressing
					return;
				}

				// Get length from dictionary
				size_t length = (high & 0x0F) + 2;
				if (dst_ptr + length > dst_size)
				{
					// Exceeds the output buffer
					length = dst_size - dst_ptr;
				}

				// Enter data dictionary
				for (size_t j = 0; j < length; j++)
				{
					dst[dst_ptr++] = dictionary[dictionary_ptr++] = dictionary[back++];

					dictionary_ptr &= dictionary_size - 1;
					back &= dictionary_size - 1;
				}
			}

			flags >>= 1;
		}
	}
}

/// マスク画像を付加して32bit化する
///
/// Parameters:
/// @param dst       Destination
/// @param dst_size  Destination Size
/// @param src       24-bit data
/// @param src_size  24-bit data size
/// @param mask      8-bit data (mask)
/// @param mask_size 8-bit data size
///
bool CWill::AppendMask(u8* dst, size_t dst_size, const u8* src, size_t src_size, const u8* mask, size_t mask_size)
{
	// Make files
	memcpy(dst, src, src_size);
	memcpy(&dst[src_size], mask, mask_size);

	return true;
}
