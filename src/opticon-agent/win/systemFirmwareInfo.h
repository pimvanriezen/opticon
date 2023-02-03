#pragma once

#include <stddef.h> // For size_t
#include <stdint.h>
#include <stdbool.h>

#define E_SYSTEMFIRMWAREINFO_BUFFER_TOO_SMALL 0x4df6e519
//#define E_SYSTEMFIRMWAREINFO_ALLOC_FAILED 0xb5820921

// @note All strings can be NULL
typedef struct DBiosInfo {
	char *vendor;
	char *version;
	//WORD startingAddressSegment;
	// Date in either mm/dd/yy or mm/dd/yyyy format. If the year portion of the string is two digits, the year is assumed to be 19yy.
	// @note The mm/dd/yyyy format is required for SMBIOS version 2.3 and later.
	char *releaseDate;
	//BYTE romSize;
	//QWORD characteristics;
	//? characteristicsExtensionBytes; (zero or more bytes)
	//BYTE majorRelease; This is already included in the DRawSmBiosData?
	//BYTE minorRelease;
	uint8_t embeddedControllerMajorRelease; // 0xff if the system does not have field upgradeable embedded controller firmware
	uint8_t embeddedControllerMinorRelease; // 0xff if the system does not have field upgradeable embedded controller firmware
	//WORD extendedBiosRomSize;
} DBiosInfo;

// @note All strings can be NULL
typedef struct DSystemInfo {
	char *manufacturer;
	char *productName;
	char *version;
	char *serialNumber;
	// If the uuid is all 1's, the uuid is not present in the system, but it can be set.
	// If the value is all 0's, the uuid is not present in the system.
	uint8_t uuid[16];
	//enum? wakeupType;
	char *skuNumber;
	char *family;
} DSystemInfo;

typedef struct DProcessorInfo {
	bool hasData;
	char *version; // This is actually the model name e.g. "Intel(R) Core(TM) i5-5300U CPU @ 2.30GHz"
	bool isSocketPopulated;
	uint16_t coreCount; // v2.5+ (not available in windows 2008)
	uint16_t coreEnabled; // v2.5+
	uint16_t threadCount; // v2.5+
} DProcessorInfo;

typedef struct DSystemFirmwareInfo {
	void *rawBuffer;
	DBiosInfo biosInfo;
	DSystemInfo systemInfo;
	DProcessorInfo processorInfos[16];
} DSystemFirmwareInfo;


/**
 *	@param		outSystemFirmwareInfo
 *	@param		outRawBufferSize			Size of the provided buffer
 *	@returns	0 for success
 *	@returns	Windows API error code
**/
uint32_t getSystemFirmwareInfoRequiredBufferSize(size_t *outRawBufferSize);

/**
 *	@param		outSystemFirmwareInfo
 *	@param		rawBuffer				Caller provided buffer (buffer size needed on a win10 HP laptop is 1686, on a win10 HP desktop 2760, on a win2008 vm 17315)
 *	@param		rawBufferSize			Size of the provided buffer
 *	@returns	0 for success
 *	@returns	E_SYSTEMFIRMWAREINFO_BUFFER_TOO_SMALL
 *	@returns	Windows API error code
**/
uint32_t getSystemFirmwareInfo(DSystemFirmwareInfo *outSystemFirmwareInfo, void *rawBuffer, size_t rawBufferSize);

uint32_t getSystemFirmwareInfoAlloc(DSystemFirmwareInfo *outSystemFirmwareInfo);
uint32_t freeSystemFirmwareInfo(DSystemFirmwareInfo *systemFirmwareInfo);