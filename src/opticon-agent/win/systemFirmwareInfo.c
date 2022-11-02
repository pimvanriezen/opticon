#include <stddef.h> // For size_t
#include <stdint.h>
#include <stdbool.h>

#include <windows.h>

//#include <stdlib.h> // For malloc

#include "systemFirmwareInfo.h"

// @tmp
//#include <inttypes.h>
//#include <stdio.h>

// https://docs.microsoft.com/en-us/windows/win32/api/sysinfoapi/nf-sysinfoapi-getsystemfirmwaretable
typedef struct DRawSmBiosData {
	BYTE Used20CallingMethod;
	BYTE SMBIOSMajorVersion;
	BYTE SMBIOSMinorVersion;
	BYTE DmiRevision;
	DWORD Length;
	BYTE SMBIOSTableData[];
} DRawSmBiosData;

// SMBIOS Structure header as described at:
// https://www.dmtf.org/sites/default/files/standards/documents/DSP0134_3.3.0.pdf (paragraph 6.1.2)
typedef struct DSmBiosStructureHeader {
	BYTE type;
	// Specifies the length of the formatted area of the structure, starting at the Type field (so including itself).
	// @note: The length of the structure's string-set is not included, so the start of the string-set can be found at: (char *)biosStructureHeader + biosStructureHeader->length
	BYTE length;
	WORD handle;
} DSmBiosStructureHeader;


static void getUuidFromBiosTableData(uint8_t *outByteArray16, const BYTE *biosTableData, int16_t version) {
	// Although RFC4122 recommends network byte order for all fields, the PC industry (including the ACPI, UEFI, and Microsoft specifications)
	// has consistently used little-endian byte encoding for the first three fields: time_low, time_mid, time_hi_and_version.
	// Supposedly this was only done/described as of version 2.6 of the SMBIOS specification? (though the text seems to indicate the industry did this all along).
	// https://www.dmtf.org/sites/default/files/standards/documents/DSP0134_3.3.0.pdf (paragraph 7.2.1)
	if (version >= 0x0206) {
		outByteArray16[0] = biosTableData[3];
		outByteArray16[1] = biosTableData[2];
		outByteArray16[2] = biosTableData[1];
		outByteArray16[3] = biosTableData[0];
		outByteArray16[4] = biosTableData[5];
		outByteArray16[5] = biosTableData[4];
		outByteArray16[6] = biosTableData[7];
		outByteArray16[7] = biosTableData[6];
	}
	else {
		outByteArray16[0] = biosTableData[0];
		outByteArray16[1] = biosTableData[1];
		outByteArray16[2] = biosTableData[2];
		outByteArray16[3] = biosTableData[3];
		outByteArray16[4] = biosTableData[4];
		outByteArray16[5] = biosTableData[5];
		outByteArray16[6] = biosTableData[6];
		outByteArray16[7] = biosTableData[7];
	}
	
	outByteArray16[8] = biosTableData[8];
	outByteArray16[9] = biosTableData[9];
	outByteArray16[10] = biosTableData[10];
	outByteArray16[11] = biosTableData[11];
	outByteArray16[12] = biosTableData[12];
	outByteArray16[13] = biosTableData[13];
	outByteArray16[14] = biosTableData[14];
	outByteArray16[15] = biosTableData[15];
}

static char *getStringFromBiosStringSet(char *stringSet, BYTE stringNumber) {
	// Check for no string value specified
	if (stringNumber == 0) return NULL;
	
	// When the formatted portion of an SMBIOS structure references a string, it does so by specifying a non-zero string number within the structure's string-set
	while (stringNumber > 1 && *stringSet != 0) {
		// Skip over each string in the set until we reach the given stringNumber
		stringSet += strlen(stringSet);
		// Skip over null terminator
		++stringSet;
		// Move on to next string number
		--stringNumber;
	}
	// Check for bad index
	if (*stringSet == 0) return NULL;
	
	// Sanitize ASCII
	size_t length = strlen(stringSet);
	for (size_t i = 0; i < length; ++i) {
		if (stringSet[i] < 32 || stringSet[i] == 127) stringSet[i] = '.';
	}
	
	return stringSet;
}


/**
 *	@param		outSystemFirmwareInfo
 *	@param		outRawBufferSize			Size of the provided buffer
 *	@returns	0 for success
 *	@returns	Windows API error code
**/
uint32_t getSystemFirmwareInfoRequiredBufferSize(size_t *outRawBufferSize) {
	// @note Using hex notation instead of multi-character character constant to avoid compiler warning
	//const DWORD firmwareTableProviderSignature = 'RSMB';
	const DWORD firmwareTableProviderSignature = 0x52534d42;
	
	// You can get the required buffer size by passing a NULL buffer
	UINT numberOfBytesWrittenOrRequired = GetSystemFirmwareTable(firmwareTableProviderSignature, 0, NULL, 0);
	if (numberOfBytesWrittenOrRequired == 0) return GetLastError();
	
	*outRawBufferSize = numberOfBytesWrittenOrRequired;
	
	return 0;
}

void initSystemFirmwareInfo(DSystemFirmwareInfo *outSystemFirmwareInfo) {
	outSystemFirmwareInfo->biosInfo.vendor = "";
	outSystemFirmwareInfo->biosInfo.version = "";
	outSystemFirmwareInfo->biosInfo.releaseDate = "";
	outSystemFirmwareInfo->biosInfo.embeddedControllerMajorRelease = 0;
	outSystemFirmwareInfo->biosInfo.embeddedControllerMinorRelease = 0;
	
	outSystemFirmwareInfo->systemInfo.manufacturer = "";
	outSystemFirmwareInfo->systemInfo.productName = "";
	outSystemFirmwareInfo->systemInfo.version = "";
	outSystemFirmwareInfo->systemInfo.serialNumber = "";
	for (uint8_t i = 0; i < 16; ++i) {
		outSystemFirmwareInfo->systemInfo.uuid[i] = 0;
	}
	outSystemFirmwareInfo->systemInfo.skuNumber = "";
	outSystemFirmwareInfo->systemInfo.family = "";
}

/**
 *	@param		outSystemFirmwareInfo
 *	@param		rawBuffer				Caller provided buffer (buffer size needed on a win10 HP laptop is 1686, on a win10 HP desktop 2760, on a win2008 vm 17315)
 *	@param		rawBufferSize			Size of the provided buffer
 *	@returns	0 for success
 *	@returns	E_SYSTEMFIRMWAREINFO_BUFFER_TOO_SMALL
 *	@returns	Windows API error code
**/
uint32_t getSystemFirmwareInfo(DSystemFirmwareInfo *outSystemFirmwareInfo, void *rawBuffer, size_t rawBufferSize) {
	initSystemFirmwareInfo(outSystemFirmwareInfo);
	
	// @todo DSystemFirmwareInfo could also have fixed size char arrays with some sane max lengths (instead of a passed in raw buffer)? (check what data the win2008 vm has in its bios?)
	outSystemFirmwareInfo->rawBuffer = rawBuffer;
	
	// @note Using hex notation instead of multi-character character constant to avoid compiler warning
	//const DWORD firmwareTableProviderSignature = 'RSMB';
	const DWORD firmwareTableProviderSignature = 0x52534d42;
	
	UINT numberOfBytesWrittenOrRequired = GetSystemFirmwareTable(firmwareTableProviderSignature, 0, rawBuffer, rawBufferSize);
	if (numberOfBytesWrittenOrRequired == 0) return GetLastError();
	if (numberOfBytesWrittenOrRequired > rawBufferSize) {
		// @todo additional failure data (numberOfBytesWrittenOrRequired)
		return E_SYSTEMFIRMWAREINFO_BUFFER_TOO_SMALL;
	}
	
	DRawSmBiosData *biosData = (DRawSmBiosData *)rawBuffer;
	
	BYTE *biosTableData = biosData->SMBIOSTableData;
	while (biosTableData < biosData->SMBIOSTableData + biosData->Length) {
		DSmBiosStructureHeader *biosStructureHeader = (DSmBiosStructureHeader *)biosTableData;
		
		// @todo describe why this should break (probably something to do with the end marker)
		if (biosStructureHeader->length < 4) break;
		
		// Text strings associated with a given SMBIOS structure are appended directly after the formatted portion of the structure
		char *stringSet = (char *)biosTableData;
		// Skip over the formatted area
		stringSet += biosStructureHeader->length;
		
		// @todo biosStructureHeader->type 1 should only exist once, don't know about type 0, but most likely also only once
		
		if (biosStructureHeader->type == 0) {
			outSystemFirmwareInfo->biosInfo.vendor = getStringFromBiosStringSet(stringSet, biosTableData[0x4]);
			outSystemFirmwareInfo->biosInfo.version = getStringFromBiosStringSet(stringSet, biosTableData[0x5]);
			outSystemFirmwareInfo->biosInfo.releaseDate = getStringFromBiosStringSet(stringSet, biosTableData[0x8]);
			
			outSystemFirmwareInfo->biosInfo.embeddedControllerMajorRelease = biosTableData[0x16];
			outSystemFirmwareInfo->biosInfo.embeddedControllerMinorRelease = biosTableData[0x17];
		}
		else if (biosStructureHeader->type == 1) {
			// An SMBIOS implementation is associated with a single system instance and contains one and only one System Information (Type 1) structure
			// https://www.dmtf.org/sites/default/files/standards/documents/DSP0134_3.3.0.pdf (paragraph 7.2)
			
			outSystemFirmwareInfo->systemInfo.manufacturer = getStringFromBiosStringSet(stringSet, biosTableData[0x4]);
			outSystemFirmwareInfo->systemInfo.productName = getStringFromBiosStringSet(stringSet, biosTableData[0x5]);
			outSystemFirmwareInfo->systemInfo.version = getStringFromBiosStringSet(stringSet, biosTableData[0x6]);
			outSystemFirmwareInfo->systemInfo.serialNumber = getStringFromBiosStringSet(stringSet, biosTableData[0x7]);
			
			// UUID is at offset 0x08
			getUuidFromBiosTableData(outSystemFirmwareInfo->systemInfo.uuid, biosTableData + 0x8, biosData->SMBIOSMajorVersion * 0x100 + biosData->SMBIOSMinorVersion);
			
			outSystemFirmwareInfo->systemInfo.skuNumber = getStringFromBiosStringSet(stringSet, biosTableData[0x19]);
			outSystemFirmwareInfo->systemInfo.family = getStringFromBiosStringSet(stringSet, biosTableData[0x1a]);
		}
		
		// Skip over formatted area
		biosTableData += biosStructureHeader->length;
		// Skip over unformatted area of the structure (end marker is 0x0000, so 2 consecutive 0 bytes)
		while (biosTableData < biosData->SMBIOSTableData + biosData->Length && (biosTableData[0] != 0 || biosTableData[1] != 0)) {
			++biosTableData;
		}
		// Skip over the 2 end marker bytes
		biosTableData += 2;
	}
	
	return 0;
}

uint32_t getSystemFirmwareInfoAlloc(DSystemFirmwareInfo *outSystemFirmwareInfo) {
	uint32_t e;
	
	size_t rawBufferSize = 0;
	if ((e = getSystemFirmwareInfoRequiredBufferSize(&rawBufferSize))) return e;
	
    //void *rawBuffer = malloc(rawBufferSize);
	//if (rawBuffer == NULL) return E_SYSTEMFIRMWAREINFO_ALLOC_FAILED;
	HANDLE heap = GetProcessHeap();
	if (heap == NULL) return GetLastError();
	void *rawBuffer = HeapAlloc(heap, 0, rawBufferSize);
	if (rawBuffer == NULL) return GetLastError();
	
    if ((e = getSystemFirmwareInfo(outSystemFirmwareInfo, rawBuffer, rawBufferSize))) return e;
    
    return 0;
}

uint32_t freeSystemFirmwareInfo(DSystemFirmwareInfo *systemFirmwareInfo) {
	//free(systemFirmwareInfo->rawBuffer);
	HANDLE heap = GetProcessHeap();
	if (heap == NULL) return GetLastError();
	if (HeapFree(heap, 0, systemFirmwareInfo->rawBuffer) == false) return GetLastError();
	
	return 0;
}