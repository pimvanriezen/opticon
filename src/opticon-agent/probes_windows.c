#include <stddef.h> // For size_t
#include <stdint.h>
#include <stdbool.h>

// Need to include ws2tcpip.h < winsock2.h before windows.h
#include <ws2tcpip.h> // For INET_ADDRSTRLEN, INET6_ADDRSTRLEN
#include <windows.h>
#include <pdh.h>
#include <pdhmsg.h>
#include <winioctl.h> // For VOLUME_DISK_EXTENTS
#include <psapi.h> // For EnumProcesses()
#include <wtsapi32.h> // For WTSEnumerateSessionsA()
#include <iphlpapi.h> // For GetAdaptersAddresses()
#include <versionhelpers.h> // For IsWindows7OrGreater()
#include <winternl.h> // For NtQuerySystemInformation()
#include <ntstatus.h> // For STATUS_SUCCESS
#include <intrin.h> // For __cpuid()
//#include <Objbase.h> // For COM?
// Need to include initguid.h before wuapi.h
#include <initguid.h> // For the macro DEFINE_GUID by which the guids are defined in wuapi.h (i.e. CLSID_UpdateSession)
#include <wuapi.h> // For IUpdateSession
#include <time.h> // For time()

// ----------------------------------------------------------------------------
// @note The RtlGetVersion function is declared in ntddk.h which is part of the DDK (Driver Development Kit) which apparently cannot work together with the 'user-mode' windows.h
// So we just manually declare a few things ourselves
//#include <ntddk.h> // For RtlGetVersion
//extern NTSYSAPI volatile CCHAR KeNumberProcessors;
//#define NTSTATUS LONG
NTSYSAPI NTSTATUS NTAPI RtlGetVersion(IN OUT PRTL_OSVERSIONINFOW lpVersionInformation);
// ----------------------------------------------------------------------------

#include <libopticon/var.h>
#include <libopticon/log.h>
#include "win/timeHelper.h"
#include "win/stringHelper.h"
#include "win/systemFirmwareInfo.h"
#include "opticon-agent.h"
#include "probes.h"




// For probe_omreport
//#include <libopticon/popen.h>
//#include <sys/select.h>
//#include <errno.h>

// ============================================================================

var *runprobe_hostname(probe *self) {
    (void)self;
    var *res = var_alloc();
    
    char computerName[256];
    DWORD computerNameSize = sizeof(computerName);
    //if (GetComputerNameExA(ComputerNameNetBIOS, (char*)&computerName, &computerNameSize) == false) {
    if (GetComputerNameExA(ComputerNameDnsHostname, (char*)&computerName, &computerNameSize) == false) {
        DWORD winerror = GetLastError();
        if (winerror == ERROR_MORE_DATA) log_error("Failed to get hostname: buffer too small");
        else log_error("Failed to get hostname: %" PRIx32, winerror);
        return res;
    }
    
    log_debug("probe_hostname: %s", computerName);
    var_set_str_forkey(res, "hostname", computerName);
    
    return res;
}

// ============================================================================

static uint32_t getGlobalMemoryInfo(MEMORYSTATUSEX *memoryStatus) {
    memoryStatus->dwLength = sizeof(MEMORYSTATUSEX);
    if (GlobalMemoryStatusEx(memoryStatus) == false) return GetLastError();
    
    return 0;
}

var *runprobe_meminfo(probe *self) {
    (void)self;
    uint32_t e;
    var *res = var_alloc();
    
    MEMORYSTATUSEX memoryStatus;
    if ((e = getGlobalMemoryInfo(&memoryStatus))) {
        log_error("Failed to get memory info: %" PRIx32, e);
        return res;
    }
    
    uint64_t totalMemory = memoryStatus.ullTotalPhys / 1024;
    uint64_t freeMemory = memoryStatus.ullAvailPhys / 1024;
    
    log_debug("probe_meminfo/total: %" PRIu64, totalMemory);
    log_debug("probe_meminfo/free: %" PRIu64, freeMemory);
    
    var *memDict = var_get_dict_forkey(res, "mem");
    var_set_int_forkey(memDict, "total", totalMemory);
    var_set_int_forkey(memDict, "free", freeMemory);
    
    
    void *pageFileInfoList = NULL;
    uint32_t pageFileInfoListBufferSize = 0;
    
    // Buffer size needed on an arbitrary desktop PC: 32 (This seems low, but the FileName.Buffer property is not inside our buffer, but a pointer to OS memory or something)
    // Loop because in between the query for the required buffer size, new page files may have have been activated (which means we need a larger buffer)
    // Also the code to call it manually twice is more confusing than the loop
    while (true) {
        ULONG pageFileInfoListBufferSizeRequired;
        NTSTATUS status = NtQuerySystemInformation(SystemPagefileInformation, pageFileInfoList, pageFileInfoListBufferSize, &pageFileInfoListBufferSizeRequired);
        if (status == STATUS_INFO_LENGTH_MISMATCH) {
            if (pageFileInfoList != NULL) free(pageFileInfoList);
            pageFileInfoListBufferSize = pageFileInfoListBufferSizeRequired;
            pageFileInfoList = malloc(pageFileInfoListBufferSize);
        }
        else if (status != STATUS_SUCCESS) {
            log_error("Failed to get page file information: %" PRIx32, status);
            if (pageFileInfoList != NULL) free(pageFileInfoList);
            return res;
        }
        else {
            break;
        }
    }
    
    uint64_t totalSwap = 0;
    SYSTEM_PAGEFILE_INFORMATION *pageFileInfo = (SYSTEM_PAGEFILE_INFORMATION *)pageFileInfoList;
    // @note We have no reliable way to determine if the returned list is actually empty, so we check for CurrentSize being 0
    if (pageFileInfo->CurrentSize != 0) {
        SYSTEM_INFO systemInfo;
        GetNativeSystemInfo(&systemInfo);
        
        while (true) {
            //printf("pagefile: %ls\n", pageFileInfo->FileName.Buffer);
            
            // @note Sizes are in pages (which are 4096 bytes on x86)
            // Convert Bytes to MB
            totalSwap += pageFileInfo->TotalUsed * systemInfo.dwPageSize / 1024 / 1024;
            
            if (pageFileInfo->NextEntryOffset == 0) break;
            pageFileInfo = (SYSTEM_PAGEFILE_INFORMATION *)((uint8_t *)pageFileInfo + pageFileInfo->NextEntryOffset);
        }
    }
    
    free(pageFileInfoList);
    
    log_debug("probe_meminfo/swap: %" PRIu64, totalSwap);
    var_set_int_forkey(memDict, "swap", totalSwap);
    
    return res;
}

// ============================================================================

// @note We have 2 ways to get cpu usage, one using GetSystemTimes and the other using Windows Performance Counters
// Usage from either method does not really correspond to the number reported by the task manager (can be a difference of 10%), but it's still the 'official' way to get it

typedef struct DCpuUsageProbeState {
    FILETIME previousIdleTime;
    FILETIME previousKernelAndIdleTime;
    FILETIME previousUserTime;
} DCpuUsageProbeState;

/*
typedef struct DCpuUsageProbeState {
    PDH_HQUERY query;
    PDH_HCOUNTER counter;
} DCpuUsageProbeState;
*/

static DCpuUsageProbeState cpuUsageProbeState;
static bool isCpuUsageProbeInitialized = false;

var *runprobe_cpuusage(probe *self) {
    (void)self;
    var *res = var_alloc();
    
    if (!isCpuUsageProbeInitialized) {
        if (GetSystemTimes(&cpuUsageProbeState.previousIdleTime, &cpuUsageProbeState.previousKernelAndIdleTime, &cpuUsageProbeState.previousUserTime) == false) {
            log_error("Failed to get cpu time: %" PRIx32, GetLastError());
            return res;
        }
        
        isCpuUsageProbeInitialized = true;
    }
    
    FILETIME idleTime;
    FILETIME kernelAndIdleTime;
    FILETIME userTime;
    if (GetSystemTimes(&idleTime, &kernelAndIdleTime, &userTime) == false) {
        log_error("Failed to get cpu time: %" PRIx32, GetLastError());
        return res;
    }
    
    uint64_t idleTicksSincePrevious = winFileTimeToTicks(idleTime) - winFileTimeToTicks(cpuUsageProbeState.previousIdleTime);
    uint64_t kernelAndIdleTicksSincePrevious = winFileTimeToTicks(kernelAndIdleTime) - winFileTimeToTicks(cpuUsageProbeState.previousKernelAndIdleTime);
    uint64_t userTicksSincePrevious = winFileTimeToTicks(userTime) - winFileTimeToTicks(cpuUsageProbeState.previousUserTime);
    
    double cpuUsagePercent = 0.0;
    uint64_t totalTicksSincePrevious = kernelAndIdleTicksSincePrevious + userTicksSincePrevious;
    if (totalTicksSincePrevious > 0) {
        // @note Multiply by 100 first to prevent dipping below 0 (so you could also use the result as an int)
        cpuUsagePercent = (double)(totalTicksSincePrevious - idleTicksSincePrevious) * 100.0 / totalTicksSincePrevious;
    }
    
    cpuUsageProbeState.previousIdleTime = idleTime;
    cpuUsageProbeState.previousKernelAndIdleTime = kernelAndIdleTime;
    cpuUsageProbeState.previousUserTime = userTime;
    
    log_debug("probe_cpuusage: %f", cpuUsagePercent);
    var_set_double_forkey(res, "pcpu", cpuUsagePercent);
    
    return res;
}


/*
var *runprobe_cpuusage(probe *self) {
    (void)self;
    var *res = var_alloc();
    
    PDH_STATUS status;
    
    if (!isCpuUsageProbeInitialized) {
        if ((status = PdhOpenQueryA(NULL, (DWORD_PTR)NULL, &cpuUsageProbeState.query)) != ERROR_SUCCESS) {
            log_error("Failed to create cpu usage query: %" PRIx32, status);
            return res;
        }
        
        if ((status = PdhAddCounterA(cpuUsageProbeState.query, "\\Processor Information(_Total)\\% Processor Time", (DWORD_PTR)NULL, &cpuUsageProbeState.counter)) != ERROR_SUCCESS) {
            log_error("Failed to add cpu usage query counter: %" PRIx32, status);
            return res;
        }
        
        // @note We can't get the value using PdhGetFormattedCounterValue after only a single collect (will result in PDH_INVALID_DATA), it needs at least 2 collects, so we jump start the first one here
        if ((status = PdhCollectQueryData(cpuUsageProbeState.query)) != ERROR_SUCCESS) {
            log_error("Failed to collect first cpu usage query data: %" PRIx32, status);
            return res;
        }
        
        isCpuUsageProbeInitialized = true;
        
        // @todo for graceful shutdown:
        //if ((status = PdhCloseQuery(query)) != ERROR_SUCCESS) {
        //  log_error("Failed to close cpu usage query: %" PRIx32, status);
        //}
    }
    
    if ((status = PdhCollectQueryData(cpuUsageProbeState.query)) != ERROR_SUCCESS) {
        log_error("Failed to collect cpu usage query data: %" PRIx32, status);
        return res;
    }
    
    PDH_FMT_COUNTERVALUE pdhValue;
    if ((status = PdhGetFormattedCounterValue(cpuUsageProbeState.counter, PDH_FMT_DOUBLE, NULL, &pdhValue)) != ERROR_SUCCESS) {
        log_error("Failed to format cpu usage query counter: %" PRIx32, status);
        return res;
    }
    double cpuUsagePercent = pdhValue.doubleValue;
    
    log_debug("probe_cpuusage: %f", cpuUsagePercent);
    var_set_double_forkey(res, "pcpu", cpuUsagePercent);
    
    return res;
}
*/

// ============================================================================

// The time interval in seconds between taking load counts, same as Linux
#define LOADAVG_INTERVAL 5

// We use an exponentially weighted moving average, just like Unix systems do
// https://en.wikipedia.org/wiki/Load_(computing)#Unix-style_load_calculation
//
// These constants serve as the damping factor and are calculated with
// 1 / exp(sampling interval in seconds / window size in seconds)
//
// This formula comes from linux's include/linux/sched/loadavg.h
// https://github.com/torvalds/linux/blob/345671ea0f9258f410eb057b9ced9cefbbe5dc78/include/linux/sched/loadavg.h#L20-L23

// @note These are based on a 5 second interval
#define LOADAVG_FACTOR_1MIN 0.9200444146293232478931553241
#define LOADAVG_FACTOR_5MIN 0.6592406302004437462547604110
#define LOADAVG_FACTOR_15MIN 0.2865047968601901003248854266

typedef struct DLoadAverageProbeState {
    PDH_HQUERY query;
    PDH_HCOUNTER counter;
    HANDLE waitHandle;
    double loadAverage1min;
    double loadAverage5min;
    double loadAverage15min;
} DLoadAverageProbeState;

static DLoadAverageProbeState loadAverageProbeState;
static bool isLoadAverageProbeInitialized = false;


__attribute__((force_align_arg_pointer))
static void CALLBACK onLoadavgUpdateCallback(void *customData, BOOLEAN hasTimedOut) {
    (void)customData;
    (void)hasTimedOut;
    
    PDH_STATUS status;
    
    PDH_FMT_COUNTERVALUE pdhValue;
    if ((status = PdhGetFormattedCounterValue(loadAverageProbeState.counter, PDH_FMT_DOUBLE, NULL, &pdhValue)) != ERROR_SUCCESS) {
        log_error("Failed to format load average query counter: %" PRIx32, status);
        return;
    }
    double currentLoad = pdhValue.doubleValue;
    
    // @todo maybe round to 2 decimals?
    loadAverageProbeState.loadAverage1min = loadAverageProbeState.loadAverage1min * LOADAVG_FACTOR_1MIN + currentLoad * (1.0 - LOADAVG_FACTOR_1MIN);
    loadAverageProbeState.loadAverage5min = loadAverageProbeState.loadAverage5min * LOADAVG_FACTOR_5MIN + currentLoad * (1.0 - LOADAVG_FACTOR_5MIN);
    loadAverageProbeState.loadAverage15min = loadAverageProbeState.loadAverage15min * LOADAVG_FACTOR_15MIN + currentLoad * (1.0 - LOADAVG_FACTOR_15MIN);
}

var *runprobe_loadavg(probe *self) {
    (void)self;
    var *res = var_alloc();
    
    PDH_STATUS status;
    
    if (!isLoadAverageProbeInitialized) {
        loadAverageProbeState.loadAverage1min = 0;
        loadAverageProbeState.loadAverage5min = 0;
        loadAverageProbeState.loadAverage15min = 0;
        
        if ((status = PdhOpenQueryA(NULL, (DWORD_PTR)NULL, &loadAverageProbeState.query)) != ERROR_SUCCESS) {
            log_error("Failed to create load average query: %" PRIx32, status);
            return res;
        }
        
        if ((status = PdhAddCounterA(loadAverageProbeState.query, "\\System\\Processor Queue Length", (DWORD_PTR)NULL, &loadAverageProbeState.counter)) != ERROR_SUCCESS) {
            log_error("Failed to add load average query counter: %" PRIx32, status);
            return res;
        }
        
        // @alternative If we want to manually probe at the probe's interval
        // @note We can't get the value using PdhGetFormattedCounterValue after only a single collect (will result in PDH_INVALID_DATA), it needs at least 2 collects, so we jump start the first one here
        //if ((status = PdhCollectQueryData(loadAverageProbeState.query)) != ERROR_SUCCESS) {
        //  log_error("Failed to collect first load average query data: %" PRIx32, status);
        //  return res;
        //}
        
        
        HANDLE signalEvent = CreateEventA(NULL, FALSE, FALSE, "LoadavgUpdateEvent");
        
        // Have the windows thread pool call us at an interval specified by LOADAVG_INTERVAL
        if ((status = PdhCollectQueryDataEx(loadAverageProbeState.query, LOADAVG_INTERVAL, signalEvent)) != ERROR_SUCCESS) {
            log_error("Failed to collect load average query data: %" PRIx32, status);
            return res;
        }
        
        // We can pass in custom data to the callback (but we don't because we use static state everywhere)
        if (RegisterWaitForSingleObject(&loadAverageProbeState.waitHandle, signalEvent, onLoadavgUpdateCallback, NULL, INFINITE, WT_EXECUTEDEFAULT) == false) {
            //DWORD winerror = GetLastError();
            log_error("Failed to register windows event callback: %" PRIx32, GetLastError());
            return res;
        }
        
        isLoadAverageProbeInitialized = true;
        
        // @todo for graceful shutdown:
        // @alternative If we want to manually probe at the probe's interval
        //if ((status = PdhCloseQuery(query)) != ERROR_SUCCESS) {
        //  log_error("Failed to close load average query: %" PRIx32, status);
        //}
        //
        //if (UnregisterWaitEx(loadAverageProbeState.waitHandle, NULL) == false) {
        //  DWORD winerror = GetLastError();
        //  log_error("Failed to unregister windows event callback: %" PRIx32, GetLastError());
        //  return;
        //}
    }
    
    // @alternative If we want to manually probe at the probe's interval
    // @todo we have to calculate the LOADAVG_FACTOR based on that probe interval
    
    //if ((status = PdhCollectQueryData(loadAverageProbeState.query)) != ERROR_SUCCESS) {
    //    log_error("Failed to collect load average query data");
    //    return res;
    //}
    //
    //PDH_FMT_COUNTERVALUE pdhValue;
    //if ((status = PdhGetFormattedCounterValue(loadAverageProbeState.counter, PDH_FMT_DOUBLE, NULL, &pdhValue)) != ERROR_SUCCESS) {
    //    log_error("Failed to format load average query counter: %" PRIx32, status);
    //    return res;
    //}
    //double currentLoad = pdhValue.doubleValue;
    //
    //loadAverageProbeState.loadAverage1min = loadAverageProbeState.loadAverage1min * LOADAVG_FACTOR_1MIN + currentLoad * \ (1.0 - LOADAVG_FACTOR_1MIN);
    //loadAverageProbeState.loadAverage5min = loadAverageProbeState.loadAverage5min * LOADAVG_FACTOR_5MIN + currentLoad * \ (1.0 - LOADAVG_FACTOR_5MIN);
    //loadAverageProbeState.loadAverage15min = loadAverageProbeState.loadAverage15min * LOADAVG_FACTOR_15MIN + currentLoad * \ (1.0 - LOADAVG_FACTOR_15MIN);
    
    
    log_debug("probe_loadavg: %f / %f / %f", loadAverageProbeState.loadAverage1min, loadAverageProbeState.loadAverage5min, loadAverageProbeState.loadAverage15min);
    
    var *loadavgArray = var_get_array_forkey(res, "loadavg");
    var_add_double(loadavgArray, loadAverageProbeState.loadAverage1min);
    var_add_double(loadavgArray, loadAverageProbeState.loadAverage5min);
    var_add_double(loadavgArray, loadAverageProbeState.loadAverage15min);
    
    return res;
}

// ============================================================================

typedef struct DNetProbeState {
    PDH_HQUERY query;
    PDH_HCOUNTER bytesReceivedCounter;
    PDH_HCOUNTER packetsReceivedCounter;
    PDH_HCOUNTER bytesSentCounter;
    PDH_HCOUNTER packetsSentCounter;
} DNetProbeState;

static DNetProbeState netProbeState;
static bool isNetProbeInitialized = false;

var *runprobe_net(probe *self) {
    (void)self;
    var *res = var_alloc();
    
    PDH_STATUS status;
    
    if (!isNetProbeInitialized) {
        if ((status = PdhOpenQueryA(NULL, (DWORD_PTR)NULL, &netProbeState.query)) != ERROR_SUCCESS) {
            log_error("Failed to create net query: %" PRIx32, status);
            return res;
        }
        
        if ((status = PdhAddCounterA(netProbeState.query, "\\Network Interface(*)\\Bytes Received/sec", (DWORD_PTR)NULL, &netProbeState.bytesReceivedCounter)) != ERROR_SUCCESS) {
            log_error("Failed to add net query counter: %" PRIx32, status);
            return res;
        }
        if ((status = PdhAddCounterA(netProbeState.query, "\\Network Interface(*)\\Packets Received/sec", (DWORD_PTR)NULL, &netProbeState.packetsReceivedCounter)) != ERROR_SUCCESS) {
            log_error("Failed to add net query counter: %" PRIx32, status);
            return res;
        }
        if ((status = PdhAddCounterA(netProbeState.query, "\\Network Interface(*)\\Bytes Sent/sec", (DWORD_PTR)NULL, &netProbeState.bytesSentCounter)) != ERROR_SUCCESS) {
            log_error("Failed to add net query counter: %" PRIx32, status);
            return res;
        }
        if ((status = PdhAddCounterA(netProbeState.query, "\\Network Interface(*)\\Packets Sent/sec", (DWORD_PTR)NULL, &netProbeState.packetsSentCounter)) != ERROR_SUCCESS) {
            log_error("Failed to add net query counter: %" PRIx32, status);
            return res;
        }
        
        // @note We can't get the value using PdhGetFormattedCounterValue after only a single collect (will result in PDH_INVALID_DATA), it needs at least 2 collects, so we jump start the first one here
        if ((status = PdhCollectQueryData(netProbeState.query)) != ERROR_SUCCESS) {
            log_error("Failed to collect first net query data: %" PRIx32, status);
            return res;
        }
        
        isNetProbeInitialized = true;
        
        // @todo for graceful shutdown:
        //if ((status = PdhCloseQuery(query)) != ERROR_SUCCESS) {
        //  log_error("Failed to close net query: %" PRIx32, status);
        //}
    }
    
    if ((status = PdhCollectQueryData(netProbeState.query)) != ERROR_SUCCESS) {
        log_error("Failed to collect net query data: %" PRIx32, status);
        return res;
    }
    
    var *netDict = var_get_dict_forkey(res, "net");
    
    PDH_FMT_COUNTERVALUE pdhValue;
    uint64_t value;
    
    // net/in_kbs
    if ((status = PdhGetFormattedCounterValue(netProbeState.bytesReceivedCounter, PDH_FMT_LARGE, NULL, &pdhValue)) != ERROR_SUCCESS) {
        log_error("Failed to format net query counter: %" PRIx32, status);
        return res;
    }
    // Convert Bytes to KB to kbit
    value = pdhValue.largeValue / 1024 * 8;
    log_debug("probe_net/in_kbs: %" PRIu64, value);
    var_set_int_forkey(netDict, "in_kbs", value);
    
    // net/in_pps
    if ((status = PdhGetFormattedCounterValue(netProbeState.packetsReceivedCounter, PDH_FMT_LARGE, NULL, &pdhValue)) != ERROR_SUCCESS) {
        log_error("Failed to format net query counter: %" PRIx32, status);
        return res;
    }
    value = pdhValue.largeValue;
    log_debug("probe_net/in_pps: %" PRIu64, value);
    var_set_int_forkey(netDict, "in_pps", value);
    
    // net/out_kbs
    if ((status = PdhGetFormattedCounterValue(netProbeState.bytesSentCounter, PDH_FMT_LARGE, NULL, &pdhValue)) != ERROR_SUCCESS) {
        log_error("Failed to format net query counter: %" PRIx32, status);
        return res;
    }
    // Convert Bytes to KB to kbit
    value = pdhValue.largeValue / 1024 * 8;
    log_debug("probe_net/out_kbs: %" PRIu64, value);
    var_set_int_forkey(netDict, "out_kbs", value);
    
    // net/out_pps
    if ((status = PdhGetFormattedCounterValue(netProbeState.packetsSentCounter, PDH_FMT_LARGE, NULL, &pdhValue)) != ERROR_SUCCESS) {
        log_error("Failed to format net query counter: %" PRIx32, status);
        return res;
    }
    value = pdhValue.largeValue;
    log_debug("probe_net/out_pps: %" PRIu64, value);
    var_set_int_forkey(netDict, "out_pps", value);
    
    return res;
}

// ============================================================================

typedef struct DDiskIoProbeState {
    PDH_HQUERY query;
    PDH_HCOUNTER readsCounter;
    PDH_HCOUNTER writesCounter;
    PDH_HCOUNTER waitCounter;
    //
    FILETIME previousIdleTime;
    FILETIME previousKernelAndIdleTime;
    FILETIME previousUserTime;
} DDiskIoProbeState;

static DDiskIoProbeState diskIoProbeState;
static bool isDiskIoProbeInitialized = false;

var *runprobe_io(probe *self) {
    (void)self;
    var *res = var_alloc();
    
    PDH_STATUS status;
    
    if (!isDiskIoProbeInitialized) {
        if ((status = PdhOpenQueryA(NULL, (DWORD_PTR)NULL, &diskIoProbeState.query)) != ERROR_SUCCESS) {
            log_error("Failed to format disk io query counter: %" PRIx32, status);
            return res;
        }
        
        if ((status = PdhAddCounterA(diskIoProbeState.query, "\\PhysicalDisk(_Total)\\Disk Reads/sec", (DWORD_PTR)NULL, &diskIoProbeState.readsCounter)) != ERROR_SUCCESS) {
            log_error("Failed to add disk io query counter: %" PRIx32, status);
            return res;
        }
        if ((status = PdhAddCounterA(diskIoProbeState.query, "\\PhysicalDisk(_Total)\\Disk Writes/sec", (DWORD_PTR)NULL, &diskIoProbeState.writesCounter)) != ERROR_SUCCESS) {
            log_error("Failed to add disk io query counter: %" PRIx32, status);
            return res;
        }
        
        if ((status = PdhAddCounterA(diskIoProbeState.query, "\\PhysicalDisk(_Total)\\Avg. Disk sec/Transfer", (DWORD_PTR)NULL, &diskIoProbeState.waitCounter)) != ERROR_SUCCESS) {
            log_error("Failed to add disk io query counter: %" PRIx32, status);
            return res;
        }
        
        // @note We can't get the value using PdhGetFormattedCounterValue after only a single collect (will result in PDH_INVALID_DATA), it needs at least 2 collects, so we jump start the first one here
        if ((status = PdhCollectQueryData(diskIoProbeState.query)) != ERROR_SUCCESS) {
            log_error("Failed to collect first disk io data");
            return res;
        }
        
        if (GetSystemTimes(&diskIoProbeState.previousIdleTime, &diskIoProbeState.previousKernelAndIdleTime, &diskIoProbeState.previousUserTime) == false) {
            log_error("Failed to get cpu time: %" PRIx32, GetLastError());
            return res;
        }
        
        isDiskIoProbeInitialized = true;
        
        // @todo for graceful shutdown:
        //if ((status = PdhCloseQuery(query)) != ERROR_SUCCESS) {
        //  log_error("Failed to close disk io query: %" PRIx32, status);
        //}
    }
    
    
    uint64_t totalTicksSincePrevious;
    
    {
        FILETIME idleTime;
        FILETIME kernelAndIdleTime;
        FILETIME userTime;
        if (GetSystemTimes(&idleTime, &kernelAndIdleTime, &userTime) == false) {
            log_error("Failed to get cpu time: %" PRIx32, GetLastError());
            return res;
        }
        
        //uint64_t idleTicksSincePrevious = winFileTimeToTicks(idleTime) - winFileTimeToTicks(diskIoProbeState.previousIdleTime);
        uint64_t kernelAndIdleTicksSincePrevious = winFileTimeToTicks(kernelAndIdleTime) - winFileTimeToTicks(diskIoProbeState.previousKernelAndIdleTime);
        uint64_t userTicksSincePrevious = winFileTimeToTicks(userTime) - winFileTimeToTicks(diskIoProbeState.previousUserTime);
        
        totalTicksSincePrevious = kernelAndIdleTicksSincePrevious + userTicksSincePrevious;
        
        diskIoProbeState.previousIdleTime = idleTime;
        diskIoProbeState.previousKernelAndIdleTime = kernelAndIdleTime;
        diskIoProbeState.previousUserTime = userTime;
    }
    
    
    if ((status = PdhCollectQueryData(diskIoProbeState.query)) != ERROR_SUCCESS) {
        log_error("Failed to collect disk io data: %" PRIx32, status);
        return res;
    }
    
    var *ioDict = var_get_dict_forkey(res, "io");
    
    PDH_FMT_COUNTERVALUE pdhValue;
    uint64_t value;
    uint64_t transfersPerSecond = 0;
    
    // io/rdops
    if ((status = PdhGetFormattedCounterValue(diskIoProbeState.readsCounter, PDH_FMT_LARGE, NULL, &pdhValue)) != ERROR_SUCCESS) {
        log_error("Failed to format disk io counter: %" PRIx32, status);
        return res;
    }
    value = pdhValue.largeValue;
    log_debug("probe_io/rdops: %" PRIu64, value);
    var_set_int_forkey(ioDict, "rdops", value);
    transfersPerSecond += value;
    
    // io/wrops
    if ((status = PdhGetFormattedCounterValue(diskIoProbeState.writesCounter, PDH_FMT_LARGE, NULL, &pdhValue)) != ERROR_SUCCESS) {
        log_error("Failed to format disk io counter: %" PRIx32, status);
        return res;
    }
    value = pdhValue.largeValue;
    log_debug("probe_io/wrops: %" PRIu64, value);
    var_set_int_forkey(ioDict, "wrops", value);
    transfersPerSecond += value;
    
    // io/pwait
    if (totalTicksSincePrevious > 0) {
        if ((status = PdhGetFormattedCounterValue(diskIoProbeState.waitCounter, PDH_FMT_DOUBLE, NULL, &pdhValue)) != ERROR_SUCCESS) {
            log_error("Failed to format disk io counter: %" PRIx32, status);
            return res;
        }
        // Average number of seconds it takes to get a response from a disk
        double averageWaitSecondsPerTransfer = pdhValue.doubleValue;
        
        uint64_t totalSeconds = totalTicksSincePrevious / WIN_TICKS_PER_SECOND;
        
        // @note Multiply by 100 first to prevent dipping below 0 (so you could also use the result as an int)
        //double waitPercent = averageWaitSecondsPerTransfer * 100.0 / totalSeconds;
        
        //uint64_t ioTransfers = transfersPerSecond * totalSeconds;
        //double waitSeconds = ioTransfers * averageWaitSecondsPerTransfer;
        //double waitPercent = waitSeconds / totalSeconds;
        
        double waitPercent = transfersPerSecond * averageWaitSecondsPerTransfer;
        log_debug("probe_io/pwait: %f", waitPercent);
        var_set_double_forkey(ioDict, "pwait", waitPercent);
    }
    else {
        var_set_double_forkey(ioDict, "pwait", 0);
    }
    
    return res;
}

// ============================================================================

var *runprobe_uptime(probe *self) {
    (void)self;
    var *res = var_alloc();
    
    // @alternative Windows Performance Counters: System Up Time
    
    uint64_t seconds;
    // @todo possibly available on earlier *server* systems (Windows Server 2008 R2)
    if (IsWindows7OrGreater()) {
        uint64_t ticks;
        if (QueryUnbiasedInterruptTime(&ticks) == false) {
            log_error("Failed to get uptime");
            return res;
        }
        
        seconds = ticks / WIN_TICKS_PER_SECOND;
    }
    else {
        seconds = GetTickCount64() / 1000;
    }
    
    log_debug("probe_uptime: %" PRIu64, seconds);
    var_set_int_forkey(res, "uptime", seconds);
    
    // Include agent uptime
    var *agentDict = var_get_dict_forkey(res, "agent");
    var_set_int_forkey(agentDict, "up", difftime(time(NULL), APP.starttime));
    
    return res;
}

// ============================================================================

static char hardwareName[100] = "";
static bool isHardwareNameSet = false;

var *runprobe_uname(probe *self) {
    (void)self;
    uint32_t e;
    var *res = var_alloc();
    
    var *osDict = var_get_dict_forkey(res, "os");
    log_debug("probe_uname/os/kernel: Windows");
    var_set_str_forkey(osDict, "kernel", "Windows");
    
    
    // @note GetVersionExA() has been deprecated (broken on purpose) by Microsoft since Windows 8 (it will always return version 6)
    //OSVERSIONINFOEX versionInfo;
    //versionInfo.dwOSVersionInfoSize = sizeof(versionInfo);
    //GetVersionExA((LPOSVERSIONINFOA)&versionInfo);
    
    // @note This is an undocumented function
    //WORD majorVersion;
    //DWORD minorVersion;
    //DWORD buildNumber;
    //RtlGetNtVersionNumbers(&majorVersion, &minorVersion, &buildNumber);
    //
    //printf("WIN: %" PRIu32 "\n", majorVersion);
    //printf("WIN: %" PRIu32 "\n", minorVersion);
    //printf("WIN: %" PRIu32 "\n", buildNumber | 0xF0000000); // This returns stuff in the high/low part or something, don't know
    
    
    // @note RtlGetVersion() is affected by the compatibility tab settings of the .exe
    RTL_OSVERSIONINFOEXW osVersionInfo;
    osVersionInfo.dwOSVersionInfoSize = sizeof(osVersionInfo);
    osVersionInfo.szCSDVersion[0] = '\0'; // Service pack version may not be initialized by RtlGetVersion(), so pre-initialize to empty string
    RtlGetVersion((PRTL_OSVERSIONINFOW)&osVersionInfo);
    
    // The largest size of the version string is: 4294967295.4294967295.4294967295 plus a null terminator
    char version[33];
    snprintf(version, sizeof(version), "%" PRIu32 ".%" PRIu32 ".%" PRIu32, (uint32_t)osVersionInfo.dwMajorVersion, (uint32_t)osVersionInfo.dwMinorVersion, (uint32_t)osVersionInfo.dwBuildNumber);
    log_debug("probe_uname/os/version: %s", version);
    var_set_str_forkey(osDict, "version", version);
    
    
    char servicePack[sizeof(osVersionInfo.szCSDVersion)];
    // Pass -1 for cchWideChar to tell it the input is a null terminated string
    WideCharToMultiByte(CP_UTF8, 0, osVersionInfo.szCSDVersion, -1, servicePack, sizeof(servicePack), NULL, NULL);
    
    
    char productName[100];
    DWORD productNameSizeOrWritten = sizeof(productName);
    LSTATUS status = RegGetValueA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", "ProductName", RRF_RT_REG_SZ, NULL, &productName, &productNameSizeOrWritten);
    if (status != ERROR_SUCCESS) {
        log_error("Failed to get OS name from registry: %" PRIx32, status);
        
        log_debug("probe_uname/os/distro: %s", servicePack);
        var_set_str_forkey(osDict, "distro", servicePack);
    }
    else {
        if (servicePack[0] != '\0') {
            // Concat service pack to productName
            char distro[100];
            snprintf(distro, sizeof(distro), "%s (%s)", productName, servicePack);
            log_debug("probe_uname/os/distro: %s", distro);
            var_set_str_forkey(osDict, "distro", distro);
        }
        else {
            log_debug("probe_uname/os/distro: %s", productName);
            var_set_str_forkey(osDict, "distro", productName);
        }
    }
    
    
    SYSTEM_INFO systemInfo;
    GetNativeSystemInfo(&systemInfo);
    
    char *processorArchitecture = "unknown";
    switch (systemInfo.wProcessorArchitecture) {
        case PROCESSOR_ARCHITECTURE_AMD64: {
            processorArchitecture = "x86_64";
            break;
        }
        case PROCESSOR_ARCHITECTURE_INTEL: {
            processorArchitecture = "x86";
            break;
        }
        case PROCESSOR_ARCHITECTURE_IA64: {
            processorArchitecture = "ia64";
            break;
        }
        case PROCESSOR_ARCHITECTURE_ARM64: {
            processorArchitecture = "arm64";
            break;
        }
        case PROCESSOR_ARCHITECTURE_ARM: {
            processorArchitecture = "arm";
            break;
        }
        default: {
            // processorArchitecture remains "unknown"
            break;
        }
    }
    
    log_debug("probe_uname/os/arch: %s", processorArchitecture);
    var_set_str_forkey(osDict, "arch", processorArchitecture);
    
    
    if (!isHardwareNameSet) {
        DSystemFirmwareInfo systemFirmwareInfo;
        if ((e = getSystemFirmwareInfoAlloc(&systemFirmwareInfo))) {
            log_error("Failed to get system firmware info: %" PRIx32, e);
        }
        else {
            isHardwareNameSet = true;
            
            char *hardwareManufacturer = systemFirmwareInfo.systemInfo.manufacturer;
            char *hardwareProductName = systemFirmwareInfo.systemInfo.productName;
            
            if (hardwareManufacturer != NULL && hardwareManufacturer[0] != '\0' && hardwareProductName != NULL && hardwareProductName[0] != '\0') {
                snprintf(hardwareName, sizeof(hardwareName), "%s %s", hardwareManufacturer, hardwareProductName);
            }
            else if (hardwareManufacturer != NULL && hardwareManufacturer[0] != '\0') {
                snprintf(hardwareName, sizeof(hardwareName), "%s", hardwareManufacturer);
            }
            else if (hardwareProductName != NULL && hardwareProductName[0] != '\0') {
                snprintf(hardwareName, sizeof(hardwareName), "%s", hardwareProductName);
            }
            
            freeSystemFirmwareInfo(&systemFirmwareInfo);
        }
    }
    
    if (hardwareName[0] != '\0') var_set_str_forkey(osDict, "hw", hardwareName);
    
    return res;
}

// ============================================================================

var *runprobe_df(probe *self) {
    var *skipdevices = var_get_array_forkey(self->options, "skip");
    var *matchdevices = var_get_array_forkey(self->options, "match");
    
    var *res = var_alloc();
    var *dfArray = var_get_array_forkey(res, "df");
    
    //char volumeRootName[MAX_PATH];
    //HANDLE findVolumeHandle = FindFirstVolumeA(volumeRootName, sizeof(volumeRootName));
    //if (findVolumeHandle == INVALID_HANDLE_VALUE) {
    //    log_error("Failed to find first volume: %" PRIx32, GetLastError());
    //    return res;
    //}
    //while (true) {
    //    if (GetDriveTypeA(volumeRootName) == DRIVE_FIXED) {
    //        printf("%s\n", volumeRootName);
    //        
    //        char pathNames[MAX_PATH + 1];
    //        // @todo possibly sizeof(pathNames) needs to be in TCHARS
    //        DWORD numberOfCharsWrittenOrRequired;
    //        if (GetVolumePathNamesForVolumeNameA(volumeRootName, pathNames, sizeof(pathNames), &numberOfCharsWrittenOrRequired) == false) {
    //            log_error("Failed to get volume path names: %" PRIx32, GetLastError());
    //        }
    //        if (numberOfCharsWrittenOrRequired > sizeof(pathNames)) {
    //        }
    //        
    //        
    //        // Each loop: skip over the single path plus the null terminator for the next single path
    //        for (char *singePathName = pathNames; *singePathName != '\0'; singePathName += strlen(singePathName) + 1) {
    //            printf("Path: %s\n", singePathName);
    //        }
    //    }
    //    
    //    
    //    if (FindNextVolumeA(findVolumeHandle, volumeRootName, sizeof(volumeRootName)) == false) {
    //        DWORD winerror = GetLastError();
    //        if (winerror == ERROR_NO_MORE_FILES) break;
    //        
    //        log_error("Failed to find next volume: %" PRIx32, winerror);
    //        break;
    //    }
    //}
    //if (FindVolumeClose(findVolumeHandle) == false) {
    //    log_error("Failed to close volume finder: %" PRIx32, GetLastError());
    //}
    
    char logicalDriveStrings[MAX_PATH];
    DWORD numberOfBytesWrittenOrRequired = GetLogicalDriveStringsA(sizeof(logicalDriveStrings), logicalDriveStrings);
    if (numberOfBytesWrittenOrRequired == 0) {
        log_error("Failed to get logical drive strings: %" PRIx32, GetLastError());
        return res;
    }
    
    // Each loop: skip over the single drive plus the null terminator for the next single drive
    for (char *singleLogicalDriveString = logicalDriveStrings; *singleLogicalDriveString != '\0'; singleLogicalDriveString += strlen(singleLogicalDriveString) + 1) {
        if (GetDriveTypeA(singleLogicalDriveString) != DRIVE_FIXED) continue;
        
        if (var_get_count(matchdevices) > 0 && !matchlist(singleLogicalDriveString, matchdevices)) continue;
        if (matchlist(singleLogicalDriveString, skipdevices)) continue;
        
        var *dfDict = var_add_dict(dfArray);
        
        ULARGE_INTEGER freeBytesAvailableToCaller;
        ULARGE_INTEGER totalNumberOfBytesAvailableToCaller;
        //ULARGE_INTEGER totalNumberOfFreeBytes;
        if (GetDiskFreeSpaceExA(singleLogicalDriveString, &freeBytesAvailableToCaller, &totalNumberOfBytesAvailableToCaller, NULL) == false) {
            log_error("Failed to get disk space: %" PRIx32, GetLastError());
        }
        else {
            uint64_t size = totalNumberOfBytesAvailableToCaller.QuadPart / 1024 / 1024;
            // @note Multiply by 100 first to prevent dipping below 0 (so you could also use the result as an int)
            double used = (double)(totalNumberOfBytesAvailableToCaller.QuadPart - freeBytesAvailableToCaller.QuadPart) * 100.0 / totalNumberOfBytesAvailableToCaller.QuadPart;
            log_debug("probe_df/size: %u", size);
            log_debug("probe_df/pused: %f", used);
            
            var_set_int_forkey(dfDict, "size", size);
            var_set_double_forkey(dfDict, "pused", used);
        }
        
        char volumeName[256];
        char fileSystemName[256];
        if (GetVolumeInformationA(singleLogicalDriveString, volumeName, sizeof(volumeName), NULL, NULL, NULL, fileSystemName, sizeof(fileSystemName)) == false) {
            log_error("Failed to get volume information: %" PRIx32, GetLastError());
            // Simply use the singleLogicalDriveString as the volume name
            strcpy(volumeName, singleLogicalDriveString);
        }
        else {
            size_t volumeNameLength = strlen(volumeName);
            if (volumeNameLength == 0) {
                strcpy(volumeName, singleLogicalDriveString);
            }
            else {
                size_t singleDriveStringLength = strlen(singleLogicalDriveString);
                // Create a string like: "My Volume Name (C:\)"
                // Extra size of of 4 to add are: space + ( + ) + null terminator
                uint8_t remainingLengthForVolumeName = sizeof(volumeName) - singleDriveStringLength - 4; // 4 already includes the null terminator
                if (remainingLengthForVolumeName == 0) {
                    strcpy(volumeName, singleLogicalDriveString);
                }
                else {
                    if (remainingLengthForVolumeName < volumeNameLength) {
                        // Only use the first remainingLengthForVolumeName chars of volumeName by terminating it 1 char after the remainingLengthForVolumeName'th char
                        volumeName[remainingLengthForVolumeName + 1] = '\0';
                    }
                    
                    char tmp[256];
                    snprintf(tmp, sizeof(tmp), "%s (%s)", volumeName, singleLogicalDriveString);
                    strcpy(volumeName, tmp);
                }
            }
            
            stringToLowerCase(fileSystemName);
            log_debug("probe_df/fs: %s", fileSystemName);
            var_set_str_forkey(dfDict, "fs", fileSystemName);
        }
        
        log_debug("probe_df/mount: %s", volumeName);
        var_set_str_forkey(dfDict, "mount", volumeName);
        
        // Get the physical disks associated with the logical drive / volume
        #define GET_MAX_NUMBER_OF_PHYSICAL_DISKS 16
        
        // Make a volume(?) path out of the drive letter that looks like this: "\\.\C:" (so prefixed with \\.\ and without a trailing backslash)
        //char driveVolumePath[7] = {'\\', '\\', '.', '\\', singleLogicalDriveString[0], ':', '\0'};
        char driveVolumePath[7] = "\\\\.\\C:";
        driveVolumePath[4] = singleLogicalDriveString[0];
        
        // A single logical drive can span multiple physical disks (e.g. software RAID)
        char physicalDisks[256] = "";
        // @note Apparently the FILE_SHARE_WRITE is needed to prevent access denied
        // @note the 0 instead of GENERIC_READ may prevent the need for administrator privileges
        HANDLE driveVolumeHandle = CreateFileA(driveVolumePath, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
        if (driveVolumeHandle == INVALID_HANDLE_VALUE) {
            log_error("Failed to open volume: %" PRIx32, GetLastError());
        }
        else {
            // Buffer of type VOLUME_DISK_EXTENTS plus extra capacity to hold GET_MAX_NUMBER_OF_PHYSICAL_DISKS of DISK_EXTENT structs
            uint8_t volumeDiskExtentsBuffer[sizeof(VOLUME_DISK_EXTENTS) + (sizeof(DISK_EXTENT) * GET_MAX_NUMBER_OF_PHYSICAL_DISKS)];
            
            DWORD numberOfBytesWritten;
            if (DeviceIoControl(driveVolumeHandle, IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS, NULL, 0, &volumeDiskExtentsBuffer, sizeof(volumeDiskExtentsBuffer), &numberOfBytesWritten, NULL) == false) {
                log_error("Failed to get physical disk info: %" PRIx32, GetLastError());
            }
            else {
                VOLUME_DISK_EXTENTS *volumeDiskExtents = (VOLUME_DISK_EXTENTS *)volumeDiskExtentsBuffer;
                for (size_t i = 0; i < volumeDiskExtents->NumberOfDiskExtents; ++i) {
                    // Concatenate "Disk diskNumber" into physicalDisks separated by a comma and a space
                    if (physicalDisks[0] == '\0') {
                        snprintf(physicalDisks, sizeof(physicalDisks), "Disk%" PRIu32, (uint32_t)volumeDiskExtents->Extents[i].DiskNumber);
                    }
                    else {
                        char tmp[256];
                        snprintf(tmp, sizeof(tmp), "%s, Disk%" PRIu32, physicalDisks, (uint32_t)volumeDiskExtents->Extents[i].DiskNumber);
                        strcpy(physicalDisks, tmp);
                    }
                }
            }
        }
        
        log_debug("probe_df/device: %s", physicalDisks);
        var_set_str_forkey(dfDict, "device", physicalDisks);
    }
    
    return res;
}

// ============================================================================

// @note We basically need to track all processes that have existed in between 2 top-probe rounds
// The number of active processes on an arbitrary win10 desktop PC is around 200 of which 100 of them are access denied.
// Also account for some additional processes that may be started & stopped in between 2 top-probe rounds
#define TRACK_MAX_NUMBER_OF_PROCESSES 250

typedef struct DTopProbeProcess {
    bool hasData;
    DWORD processId;
    uint64_t previousCpuUsageTicks;
    uint64_t cpuUsageTicks;
    double cpuUsagePercent;
    double memoryUsagePercent;
    char processName[256];
    char userName[256];
} DTopProbeProcess;

typedef struct DTopProbeState {
    FILETIME previousIdleTime;
    FILETIME previousKernelAndIdleTime;
    FILETIME previousUserTime;
    
    uint64_t previousCpuIdleTicks;
    uint64_t previousCpuUsageTicks;
    uint64_t cpuIdleTicks;
    uint64_t cpuUsageTicks;
    double cpuUsagePercent;
    bool isFirstSample;
    FILETIME previousSampleDate;
    DTopProbeProcess processList[TRACK_MAX_NUMBER_OF_PROCESSES];
} DTopProbeState;

static DTopProbeState topProbeState;
static bool isTopProbeInitialized = false;

#define E_TOPPROBE_NO_FREE_SLOT 0x162d8a68

static uint32_t getSlotIndexForProcess(size_t *outIndex, const DTopProbeProcess *process, const DTopProbeProcess *processList, size_t processListItemCount) {
    bool foundSLot = false;
    
    // Find a free slot
    for (size_t i = 0; i < processListItemCount; ++i) {
        const DTopProbeProcess *checkProcess = &processList[i];
        if (checkProcess->hasData) continue;
        
        // Free slot found
        foundSLot = true;
        *outIndex = i;
        break;
    }
    
    // If no free slot was found, check if this process is 'interesting' enough to bump an existing process
    
    // Check if its cpu usage is higher
    if (!foundSLot) {
        double largestDiff = 0;
        for (size_t i = 0; i < processListItemCount; ++i) {
            const DTopProbeProcess *checkProcess = &processList[i];
            
            double cpuUsageDiff = process->cpuUsagePercent - checkProcess->cpuUsagePercent;
            if (cpuUsageDiff > 0) {
                // Bump the old
                foundSLot = true;
                
                // Prefer to bump the least interesting
                if (cpuUsageDiff > largestDiff) {
                    largestDiff = cpuUsageDiff;
                    *outIndex = i;
                }
            }
        }
    }
    
    // Check if its cpu usage is equal, but memory is higher
    if (!foundSLot) {
        double largestDiff = 0;
        for (size_t i = 0; i < processListItemCount; ++i) {
            const DTopProbeProcess *checkProcess = &processList[i];
            
            double memoryUsageDiff = process->memoryUsagePercent - checkProcess->memoryUsagePercent;
            if (process->cpuUsagePercent == checkProcess->cpuUsagePercent && memoryUsageDiff > 0) {
                // Bump the old
                foundSLot = true;
                
                // Prefer to bump the least interesting
                if (memoryUsageDiff > largestDiff) {
                    largestDiff = memoryUsageDiff;
                    *outIndex = i;
                }
            }
        }
    }
    
    // Check if its cpu usage difference is insignificant (< 2% of total), but memory is significantly higher (> 10% of total)
    if (!foundSLot) {
        double largestDiff = 0;
        for (size_t i = 0; i < processListItemCount; ++i) {
            const DTopProbeProcess *checkProcess = &processList[i];
            
            double cpuUsageDiff = process->cpuUsagePercent - checkProcess->cpuUsagePercent;
            double memoryUsageDiff = process->memoryUsagePercent - checkProcess->memoryUsagePercent;
            if (cpuUsageDiff > -2.0 && memoryUsageDiff > 10.0) {
                // Bump the old
                foundSLot = true;
                
                // Prefer to bump the least interesting
                if (memoryUsageDiff > largestDiff) {
                    largestDiff = memoryUsageDiff;
                    *outIndex = i;
                }
            }
        }
    }
    
    if (!foundSLot) return E_TOPPROBE_NO_FREE_SLOT;
    
    return 0;
}

static uint32_t updateTopProbeCpuTicks(uint64_t *outTotalTicksSincePrevious) {
    FILETIME idleTime;
    FILETIME kernelAndIdleTime;
    FILETIME userTime;
    // GetSystemTimes includes time from all cores
    if (GetSystemTimes(&idleTime, &kernelAndIdleTime, &userTime) == false) {
        log_error("Failed to get cpu time: %" PRIx32, GetLastError());
        return GetLastError();
    }
    
    topProbeState.cpuIdleTicks = winFileTimeToTicks(idleTime);
    topProbeState.cpuUsageTicks = winFileTimeToTicks(kernelAndIdleTime) - winFileTimeToTicks(idleTime) + winFileTimeToTicks(userTime);
    
    if (topProbeState.isFirstSample) {
        topProbeState.previousCpuIdleTicks = topProbeState.cpuIdleTicks;
        topProbeState.previousCpuUsageTicks = topProbeState.cpuUsageTicks;
    }
    
    if (outTotalTicksSincePrevious != NULL) {
        uint64_t cpuUsageTicksSincePrevious = topProbeState.cpuUsageTicks - topProbeState.previousCpuUsageTicks;
        *outTotalTicksSincePrevious = (topProbeState.cpuIdleTicks - topProbeState.previousCpuIdleTicks) + cpuUsageTicksSincePrevious;
    }
    
    return 0;
}

static uint32_t sampleTopProbe() {
    uint32_t e;
    
    FILETIME currentSampleDate;
    GetSystemTimeAsFileTime(&currentSampleDate);
    
    uint64_t totalTicksSincePrevious;
    if ((e = updateTopProbeCpuTicks(&totalTicksSincePrevious))) {
        log_error("Failed to get cpu time: %" PRIx32, e);
        return e;
    }
    
    // Update cpu usage in case process doesn't exist anymore (and thus doesn't get updated in the loop below)
    if (totalTicksSincePrevious > 0) {
        for (size_t i = 0; i < TRACK_MAX_NUMBER_OF_PROCESSES; ++i) {
            DTopProbeProcess *process = &topProbeState.processList[i];
            if (!process->hasData) continue;
            
                
            process->cpuUsagePercent = 0.0;
            
            // Calculate cpu usage percentage
            uint64_t usageTicksSincePrevious = process->cpuUsageTicks - process->previousCpuUsageTicks;
            // @note Multiply by 100 first to prevent dipping below 0 (so you could also use the result as an int)
            process->cpuUsagePercent = (double)usageTicksSincePrevious * 100.0 / totalTicksSincePrevious;
        }
    }
    
    uint64_t totalMemory;
    {
        MEMORYSTATUSEX memoryStatus;
        if ((e = getGlobalMemoryInfo(&memoryStatus))) {
            log_error("Failed to get memory info: %" PRIx32, e);
            return e;
        }
        totalMemory = memoryStatus.ullTotalPhys;
    }
    
    // Get max 1024 live process ids
    DWORD processIds[1024];
    DWORD processIdsArrayBytesWritten;
    if (EnumProcesses(processIds, sizeof(processIds), &processIdsArrayBytesWritten) == false) {
        log_error("Failed to get processes: %" PRIx32, GetLastError());
        e = GetLastError();
        return e;
    }
    
    // Calculate how many process ids were returned
    size_t numberOfProcesses = processIdsArrayBytesWritten / sizeof(DWORD);
    for (size_t i = 0; i < numberOfProcesses; ++i) {
        DWORD processId = processIds[i];
        // Check if the id is not the system idle process (id 0x00000000)
        if (processId == 0) continue;
        
        HANDLE processHandle = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, false, processId);
        if (processHandle == NULL) {
            DWORD winerror = GetLastError();
            if (winerror == ERROR_ACCESS_DENIED) {
                // Access denied is expected for system processes and such
                // Ignore, we will just skip this process
            }
            else {
                log_error("Failed to open process: %" PRIx32, winerror);
            }
            
            // Skip this process
            continue;
        }
        
        DTopProbeProcess process;
        process.processId = processId;
        process.previousCpuUsageTicks = 0;
        process.cpuUsageTicks = 0;
        process.cpuUsagePercent = 0.0;
        process.memoryUsagePercent = 0.0;
        process.processName[0] = '\0';
        process.userName[0] = '\0';
        
        // Find this process in the process list
        DTopProbeProcess *foundProcess = NULL;
        for (size_t i = 0; i < TRACK_MAX_NUMBER_OF_PROCESSES; ++i) {
            DTopProbeProcess *checkProcess = &topProbeState.processList[i];
            if (!checkProcess->hasData) continue;
            if (checkProcess->processId != process.processId) continue;
            
            foundProcess = checkProcess;
            break;
        }
        
        // Get process's cpu times
        FILETIME creationDate;
        FILETIME exitDate;
        FILETIME kernelTime;
        FILETIME userTime;
        if (GetProcessTimes(processHandle, &creationDate, &exitDate, &kernelTime, &userTime) == false) {
            log_error("Failed to get cpu usage of process: %" PRIx32, GetLastError());
        }
        else {
            // Update cpu time directly after getting the process times for the most accurate process-cpu-usage calculation
            if ((e = updateTopProbeCpuTicks(&totalTicksSincePrevious))) {
                log_error("Failed to get cpu time: %" PRIx32, e);
                //return e;
            }
            
            process.cpuUsageTicks = winFileTimeToTicks(kernelTime) + winFileTimeToTicks(userTime);
            
            if (foundProcess != NULL) {
                // Update cpu usage of process already in list
                foundProcess->cpuUsageTicks = process.cpuUsageTicks;
                if (totalTicksSincePrevious > 0) {
                    // Calculate cpu usage percentage
                    uint64_t usageTicksSincePrevious = foundProcess->cpuUsageTicks - foundProcess->previousCpuUsageTicks;
                    // @note Multiply by 100 first to prevent dipping below 0 (so you could also use the result as an int)
                    foundProcess->cpuUsagePercent = (double)usageTicksSincePrevious * 100.0 / totalTicksSincePrevious;
                }
            }
            else if (topProbeState.isFirstSample || winFileTimeToTicks(creationDate) < winFileTimeToTicks(topProbeState.previousSampleDate)) {
                // Process could have been running before the first sample (or we couldn't access it before), so ignore all the previous cpu time
                process.previousCpuUsageTicks = process.cpuUsageTicks;
            }
            else {
                // Calculate cpu usage percentage (it's a new process since the last sample round, so all cpu time counts)
                // @note Multiply by 100 first to prevent dipping below 0 (so you could also use the result as an int)
                process.cpuUsagePercent = (double)process.cpuUsageTicks * 100.0 / totalTicksSincePrevious;
            }
        }
        
        //PROCESS_MEMORY_COUNTERS_EX processMemoryInfo;
        PROCESS_MEMORY_COUNTERS processMemoryInfo;
        if (GetProcessMemoryInfo(processHandle, &processMemoryInfo, sizeof(processMemoryInfo)) == false) {
            log_error("Failed to get memory info of process: %" PRIx32, GetLastError());
        }
        else {
            //WorkingSetSize
            //QuotaPagedPoolUsage
            //QuotaNonPagedPoolUsage
            // PrivateUsage is documented as: The Commit Charge value in bytes for this process. Commit Charge is the total amount of private memory that the memory manager has committed for a running process.
            // @note PrivateUsage is only available in PROCESS_MEMORY_COUNTERS_EX struct, but the function signature does not seem to take it (maybe we can simply cast the pointer in the argument)
            
            // @note Multiply by 100 first to prevent dipping below 0 (so you could also use the result as an int)
            process.memoryUsagePercent = (double)processMemoryInfo.WorkingSetSize * 100.0 / totalMemory;
            
            if (foundProcess != NULL) {
                // Update memory usage of process already in list
                foundProcess->memoryUsagePercent = process.memoryUsagePercent;
            }
        }
        
        
        if (foundProcess == NULL) {
            size_t addToProcessListIndex;
            if ((e = getSlotIndexForProcess(&addToProcessListIndex, &process, topProbeState.processList, TRACK_MAX_NUMBER_OF_PROCESSES))) {
                // No slot for process, skip
            }
            else {
                //HMODULE moduleHandle;
                //DWORD moduleArrayBytesWritten;
                //// Get a single module
                //if (EnumProcessModules(processHandle, &moduleHandle, sizeof(moduleHandle), &moduleArrayBytesWritten) == false) {
                //  log_error("Failed to get first module of process: %" PRIx32, GetLastError());
                //}
                //else {
                    
// @todo EnumWindows and GetWindowsText or WM_GETTEXT.
                    
                    //DWORD processNameBytesWritten = GetModuleBaseNameA(processHandle, moduleHandle, process.processName, sizeof(process.processName));
                    DWORD processNameBytesWritten = GetModuleBaseNameA(processHandle, NULL, process.processName, sizeof(process.processName));
                    if (processNameBytesWritten == 0) {
                        log_error("Failed to get name of process: %" PRIx32, GetLastError());
                    }
                //}
                
                HANDLE tokenHandle;
                if (OpenProcessToken(processHandle, TOKEN_QUERY, &tokenHandle) == false) {
                    log_error("Failed to open process for querying: %" PRIx32, GetLastError());
                }
                else {
                    // @note A TOKEN_USER struct holds a SID_AND_ATTRIBUTES struct which holds a pointer to a SID struct which is a variable-length struct
                    // This variable length struct is included in the buffer used by GetTokenInformation(), which has an unknown size up front
                    // On an arbitrary desktop PC, it's 44 bytes, so 1024 bytes should be enough for everyone
                    uint8_t userInfoBuffer[1024];
                    DWORD userInfoOfBytesWrittenOrRequired;
                    if (GetTokenInformation(tokenHandle, TokenUser, userInfoBuffer, sizeof(userInfoBuffer), &userInfoOfBytesWrittenOrRequired) == false) {
                        log_error("Failed to get user of process: %" PRIx32, GetLastError());
                    }
                    else {
                        TOKEN_USER *userInfo = (TOKEN_USER *)userInfoBuffer;
                        
                        DWORD userNameSizeOrRequired = sizeof(process.userName);
                        // @note The cchReferencedDomainName argument (domainSizeOrRequired) is required
                        char domain[256];
                        DWORD domainSizeOrRequired = sizeof(domain);
                        SID_NAME_USE accountType;
                        if (LookupAccountSidA(NULL, userInfo->User.Sid, process.userName, &userNameSizeOrRequired, domain, &domainSizeOrRequired, &accountType) == false) {
                            // @todo sidtostring
                            //log_error("Failed to get user info of user id: %d", userInfo->User.Sid);
                            log_error("Failed to get user info of user: %" PRIx32, GetLastError());
                        }
                    }
                    
                    CloseHandle(tokenHandle);
                }
                
                
                // Insert / overwrite new process into process list
                DTopProbeProcess *processInList = &topProbeState.processList[addToProcessListIndex];
                processInList->hasData = true;
                processInList->processId = process.processId;
                processInList->previousCpuUsageTicks = process.previousCpuUsageTicks;
                processInList->cpuUsageTicks = process.cpuUsageTicks;
                processInList->cpuUsagePercent = process.cpuUsagePercent;
                processInList->memoryUsagePercent = process.memoryUsagePercent;
                strcpy(processInList->processName, process.processName);
                strcpy(processInList->userName, process.userName);
            }
        }
        
        CloseHandle(processHandle);
    }
    
    
    uint64_t cpuUsageTicksSincePrevious = topProbeState.cpuUsageTicks - topProbeState.previousCpuUsageTicks;
    if (totalTicksSincePrevious > 0) {
        // @note Multiply by 100 first to prevent dipping below 0 (so you could also use the result as an int)
        topProbeState.cpuUsagePercent = (double)cpuUsageTicksSincePrevious * 100.0 / totalTicksSincePrevious;
    }
    else {
        topProbeState.cpuUsagePercent = 0.0;
    }
    
    
    topProbeState.isFirstSample = false;
    topProbeState.previousSampleDate = currentSampleDate;
    
    return 0;
}

static uint32_t resetTopProbeRound() {
    uint32_t e;
    
    topProbeState.previousCpuIdleTicks = topProbeState.cpuIdleTicks;
    topProbeState.previousCpuUsageTicks = topProbeState.cpuUsageTicks;
    
    
    // Get max 1024 live process ids
    DWORD processIds[1024];
    DWORD processIdsArrayBytesWritten;
    if (EnumProcesses(processIds, sizeof(processIds), &processIdsArrayBytesWritten) == false) {
        log_error("Failed to get processes: %" PRIx32, GetLastError());
        e = GetLastError();
        return e;
    }
    
    // Calculate how many process ids were returned
    size_t numberOfProcesses = processIdsArrayBytesWritten / sizeof(DWORD);
    
    for (size_t i = 0; i < TRACK_MAX_NUMBER_OF_PROCESSES; ++i) {
        DTopProbeProcess *process = &topProbeState.processList[i];
        if (!process->hasData) continue;
        
        // Check if previously sampled process still exists
        bool foundProcess = false;
        for (size_t j = 0; j < numberOfProcesses; ++j) {
            if (process->processId != processIds[j]) continue;
            
            foundProcess = true;
            break;
        }
        
        if (!foundProcess) {
            // Clear process that doesn't exist anymore
            process->hasData = false;
        }
        else {
            // Reset process cpu times
            process->previousCpuUsageTicks = process->cpuUsageTicks;
            process->cpuUsagePercent = 0.0;
        }
    }
    
    return 0;
}

__attribute__((force_align_arg_pointer))
static void CALLBACK onTopTimerCallback(void *customData, BOOLEAN hasTimedOut) {
    (void)customData;
    (void)hasTimedOut;
    
    sampleTopProbe();
}

var *runprobe_top(probe *self) {
    (void)self;
    uint32_t e;
    var *res = var_alloc();
    
    if (!isTopProbeInitialized) {
        topProbeState.isFirstSample = true;
        
        // Init array
        for (size_t i = 0; i < TRACK_MAX_NUMBER_OF_PROCESSES; ++i) {
            topProbeState.processList[i].hasData = false;
        }
        
        isTopProbeInitialized = true;
        
        
        // Sample at higher frequency than the probe itself (10 second interval), so we also include processes that may not exist anymore at the time of the probe round
        HANDLE timer;
        // We can pass in custom data to the callback (but we don't because we use static state everywhere)
        CreateTimerQueueTimer(&timer, NULL, onTopTimerCallback, NULL, 10000, 10000, WT_EXECUTEDEFAULT);
    }
    
    if ((e = sampleTopProbe())) return res;
    
    log_debug("probe_top/pcpu: %f", topProbeState.cpuUsagePercent);
    var_set_double_forkey(res, "pcpu", topProbeState.cpuUsagePercent);
    
    
    // 'Sort/shorten' the full process list into an array of the 16 most 'interesting' processes
    #define RETURN_MAX_NUMBER_OF_PROCESSES 16
    
    // Init array
    DTopProbeProcess returnProcessList[RETURN_MAX_NUMBER_OF_PROCESSES];
    for (size_t i = 0; i < RETURN_MAX_NUMBER_OF_PROCESSES; ++i) {
        returnProcessList[i].hasData = false;
    }
    
    // @todo make sure sample thread is not sampling while we're gathering?
    for (size_t i = 0; i < TRACK_MAX_NUMBER_OF_PROCESSES; ++i) {
        DTopProbeProcess *process = &topProbeState.processList[i];
        if (!process->hasData) continue;
        
        size_t addToProcessListIndex;
        if ((e = getSlotIndexForProcess(&addToProcessListIndex, process, returnProcessList, RETURN_MAX_NUMBER_OF_PROCESSES))) {
            // No slot for process, skip
        }
        else {
            // Insert / overwrite into return process list
            DTopProbeProcess *processInList = &returnProcessList[addToProcessListIndex];
            processInList->hasData = true;
            processInList->processId = process->processId;
            processInList->cpuUsagePercent = process->cpuUsagePercent;
            processInList->memoryUsagePercent = process->memoryUsagePercent;
            strcpy(processInList->processName, process->processName);
            strcpy(processInList->userName, process->userName);
            // ignore other properties, we don't return them
        }
    }
    
    // Create var dicts for the processes we want to return
    var *topArray = var_get_array_forkey(res, "top");
    for (size_t i = 0; i < RETURN_MAX_NUMBER_OF_PROCESSES; ++i) {
        DTopProbeProcess *process = &returnProcessList[i];
        if (!process->hasData) continue;
        
        var *processDict = var_add_dict(topArray);
        var_set_int_forkey(processDict, "pid", process->processId);
        var_set_double_forkey(processDict, "pcpu", process->cpuUsagePercent);
        var_set_double_forkey(processDict, "pmem", process->memoryUsagePercent);
        var_set_str_forkey(processDict, "name", process->processName);
        var_set_str_forkey(processDict, "user", process->userName);
        
        log_debug("probe_top/top: %s (%f)", process->processName, process->cpuUsagePercent);
    }
    
    resetTopProbeRound();
    
    return res;
}

// ============================================================================

var *runprobe_proc(probe *self) {
    (void)self;
    var *res = var_alloc();
    
    var *procDict = var_get_dict_forkey(res, "proc");
    
    void *processInfoList = NULL;
    uint32_t processInfoListBufferSize = 0;
    
    // Buffer size needed on an arbitrary desktop PC: 263832
    // Loop because in between the query for the required buffer size, new processes may have started (which means we need a larger buffer)
    while (true) {
        ULONG processInfoListBufferSizeRequired;
        NTSTATUS status = NtQuerySystemInformation(SystemProcessInformation, processInfoList, processInfoListBufferSize, &processInfoListBufferSizeRequired);
        if (status == STATUS_INFO_LENGTH_MISMATCH) {
            if (processInfoList != NULL) free(processInfoList);
            // Include some headroom in the buffer to allocate for new processes that may have started
            processInfoListBufferSize = processInfoListBufferSizeRequired + 4096;
            processInfoList = malloc(processInfoListBufferSize);
        }
        else if (status != STATUS_SUCCESS) {
            log_error("Failed to get process information: %" PRIx32, status);
            if (processInfoList != NULL) free(processInfoList);
            return res;
        }
        else {
            break;
        }
    }
    
    uint64_t runningThreadCount = 0;
    uint64_t totalThreadCount = 0;
    SYSTEM_PROCESS_INFORMATION *processInfo = (SYSTEM_PROCESS_INFORMATION *)processInfoList;
    // @note We have no reliable way to determine if the returned list is actually empty, but if our process runs this code, it cannot be empty anway
    while (true) {
        totalThreadCount += processInfo->NumberOfThreads;
        
        // The first SYSTEM_THREAD_INFORMATION structure sits after the SYSTEM_PROCESS_INFORMATION structure
        SYSTEM_THREAD_INFORMATION *thread = (SYSTEM_THREAD_INFORMATION *)((uint8_t *)processInfo + sizeof(SYSTEM_PROCESS_INFORMATION));
        for (DWORD i = 0; i < processInfo->NumberOfThreads; ++i) {
            //thread->ThreadState == Waiting && thread->WaitReason == Suspended
            if (thread->ThreadState == StateRunning) ++runningThreadCount;
            
            // Move pointer to the next thread
            ++thread;
        }
        
        if (processInfo->NextEntryOffset == 0) break;
        processInfo = (SYSTEM_PROCESS_INFORMATION *)((uint8_t *)processInfo + processInfo->NextEntryOffset);
    }
    
    free(processInfoList);
    
    log_debug("probe_proc/run: %u", runningThreadCount);
    log_debug("probe_proc/total: %u", totalThreadCount);
    var_set_int_forkey(procDict, "run", runningThreadCount);
    var_set_int_forkey(procDict, "total", totalThreadCount);
    
    return res;
}

// ============================================================================

var *runprobe_who(probe *self) {
    (void)self;
    var *res = var_alloc();
    var *whoArray = var_get_array_forkey(res, "who");
    
    
    // Alternative to WTSEnumerateSessionsA is LsaEnumerateLogonSessions (which may also return local logon sessions, but we still need WTSQuerySessionInformationA to get the ip)
    
    WTS_SESSION_INFO *sessionInfos;
    DWORD numberOfSessionInfos;
    if (WTSEnumerateSessionsA(WTS_CURRENT_SERVER_HANDLE, 0, 1, &sessionInfos, &numberOfSessionInfos) == false) {
        log_error("Failed to get logged on users: %" PRIx32, GetLastError());
        return res;
    }
    
    for (size_t i = 0; i < numberOfSessionInfos; ++i) {
        WTS_SESSION_INFO sessionInfo = sessionInfos[i];
        
        if (sessionInfo.State != WTSActive) continue;
        
        var *userDict = var_add_dict(whoArray);
        log_debug("probe_who/tty: %s", sessionInfo.pWinStationName);
        var_set_str_forkey(userDict, "tty", sessionInfo.pWinStationName);
        
        DWORD numberOfBytesReturned;
        
        WTSINFOA *sessionInfo2;
        if (WTSQuerySessionInformationA(WTS_CURRENT_SERVER_HANDLE, sessionInfo.SessionId, WTSSessionInfo, (LPSTR *)&sessionInfo2, &numberOfBytesReturned) == false) {
            log_error("Failed to get logged on user info: %" PRIx32, GetLastError());
        }
        else {
            log_debug("probe_who/user: %s", sessionInfo2->UserName);
            var_set_str_forkey(userDict, "user", sessionInfo2->UserName);
            
            WTSFreeMemory(sessionInfo2);
        }
        
        // Query for RDP info (if the session is a local "Console" login, the function still succeeds, but clientInfo->ClientAddressFamily will be 0)
        WTSCLIENTA *clientInfo;
        if (WTSQuerySessionInformationA(WTS_CURRENT_SERVER_HANDLE, sessionInfo.SessionId, WTSClientInfo, (LPSTR *)&clientInfo, &numberOfBytesReturned) == false) {
            log_error("Failed to get logged on user rdp client info: %" PRIx32, GetLastError());
        }
        else {
            if (clientInfo->ClientAddressFamily != 0) {
                //log_debug("probe_who/client: %s", clientInfo->UserName);
                //var_set_str_forkey(userDict, "client", clientInfo->UserName);
            }
            
            // The documentation says the following:
            // ---
            // For the ClientAddressFamily member, AF_INET (IPv4) will return in string format, for example "127.0.0.1". AF_INET6 (IPv6) will return in binary form.
            // For an address family AF_INET: Address contains the IPV4 address of the client as a null-terminated string.
            // For a family AF_INET6: Address contains the IPV6 address of the client as raw byte values.
            // ---
            // However it appears the IPv4 address is also stored as byte values
            // So we convert it to a string ourselves
            if (clientInfo->ClientAddressFamily == AF_INET) {
                char ipString[INET_ADDRSTRLEN];
                // @note inet_ntop() doesn't seem to give the correct string, maybe because it expects network byte order?
                //inet_ntop(AF_INET, clientInfo->ClientAddress, ipString, sizeof(ipString));
                snprintf(ipString, sizeof(ipString), "%d.%d.%d.%d", clientInfo->ClientAddress[0], clientInfo->ClientAddress[1], clientInfo->ClientAddress[2], clientInfo->ClientAddress[3]);
                
                log_debug("probe_who/remote: %s", ipString);
                var_set_str_forkey(userDict, "remote", ipString);
            }
            else if (clientInfo->ClientAddressFamily == AF_INET6) {
                char ipString[INET6_ADDRSTRLEN];
                inet_ntop(AF_INET6, clientInfo->ClientAddress, ipString, sizeof(ipString));
                
                log_debug("probe_who/remote: %s", ipString);
                var_set_str_forkey(userDict, "remote", ipString);
            }
            else {
                var_set_str_forkey(userDict, "remote", "");
            }
            
            WTSFreeMemory(clientInfo);
        }
    }
    
    WTSFreeMemory(sessionInfos);
    
    
    return res;
}

// ============================================================================

var *runprobe_localip(probe *self) {
    (void)self;
    uint32_t e;
    var *res = var_alloc();
    
    // Buffer size needed on an arbitrary desktop PC: 16383
    DWORD adapterListSizeRequired;
    e = GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_INCLUDE_PREFIX | GAA_FLAG_INCLUDE_GATEWAYS, NULL, NULL, &adapterListSizeRequired);
    if (e != ERROR_BUFFER_OVERFLOW) {
        log_error("Failed to get adapter list size required: %" PRIx32, e);
        return res;
    }
    
    IP_ADAPTER_ADDRESSES *adapterList = malloc(adapterListSizeRequired);
    e = GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_INCLUDE_PREFIX | GAA_FLAG_INCLUDE_GATEWAYS, NULL, adapterList, &adapterListSizeRequired);
    if (e != ERROR_SUCCESS) {
        log_error("Failed to get adapter list: %" PRIx32, e);
        free(adapterList);
        return res;
    }
    
    var *agentDict = var_get_dict_forkey(res, "agent");
    for (IP_ADAPTER_ADDRESSES *adapter = adapterList; adapter != NULL; adapter = adapter->Next) {
        if (adapter->FirstUnicastAddress == NULL) continue;
        if (adapter->FirstGatewayAddress == NULL) continue;
        
        for (IP_ADAPTER_UNICAST_ADDRESS_LH *unicastAddress = adapter->FirstUnicastAddress; unicastAddress != NULL; unicastAddress = unicastAddress->Next) {
            if (unicastAddress->Address.lpSockaddr->sa_family == AF_INET) {
                struct sockaddr_in *socketAddress = (struct sockaddr_in *)(unicastAddress->Address.lpSockaddr);
                char ipString[INET_ADDRSTRLEN];
                if (inet_ntop(AF_INET, &(socketAddress->sin_addr), ipString, sizeof(ipString)) == NULL) {
                    log_error("Failed to convert ip into string: %" PRIx32, WSAGetLastError());
                    continue;
                }
                
                log_debug("probe_localip/ip: %s", ipString);
                var_set_str_forkey(agentDict, "ip", ipString);
                
                break;
            }
            else if (unicastAddress->Address.lpSockaddr->sa_family == AF_INET6) {
                struct sockaddr_in6 *socketAddress = (struct sockaddr_in6 *)(unicastAddress->Address.lpSockaddr);
                char ipString[INET6_ADDRSTRLEN];
                if (inet_ntop(AF_INET6, &(socketAddress->sin6_addr), ipString, sizeof(ipString)) == NULL) {
                    log_error("Failed to convert ip into string: %" PRIx32, WSAGetLastError());
                    continue;
                }
                
                log_debug("probe_localip/ip: %s", ipString);
                var_set_str_forkey(agentDict, "ip", ipString);
                
                // @note Continue after an ipv6 ip address is found to see if there is also an ipv4 address (which we prefer and will overwrite the one we set here)
            }
        }
        
        break;
    }
    
    free(adapterList);
    
    return res;
}

// ============================================================================

/**
 *  @param      outProcessorBrand       Minimum 48 bytes
 *  @returns    void
**/
void getCpuidProcessorBrand(char *outProcessorBrand) {
    // @note __cpuid returns the info for the processor the function is executed on, so not 100% correct in case of multiple processors
    
    // https://learn.microsoft.com/en-us/cpp/intrinsics/cpuid-cpuidex?view=msvc-170
    // https://en.wikipedia.org/wiki/CPUID#EAX=80000002h,80000003h,80000004h:_Processor_Brand_String
    
    // cpuInfo holds the 4 ints returned in the EAX, EBX, ECX and EDX registers
    // For the cpuid functions 0x80000002, 0x80000003 and 0x80000004 we get the processor brand string as 16 bytes (chars) per function in those registers
    int cpuInfo[4] = {0};
    
    // 0x80000000 is the function to determine the highest supported extended cpuid function
    __cpuid(cpuInfo, 0x80000000);
    unsigned int highestExtendedCpuidFunction = cpuInfo[0];
    // Cap at 0x80000004 (we don't need to read higher than that)
    if (highestExtendedCpuidFunction > 0x80000004) highestExtendedCpuidFunction = 0x80000004;
    
    // Processor brand string is 48 byte null terminated (3 x 16 bytes)
    //char processorBrand[48];
    memset(outProcessorBrand, 0, sizeof(outProcessorBrand));
    for (int i = 0x80000002; i <= highestExtendedCpuidFunction; ++i) {
        // We could also pass outProcessorBrand + offset instead of cpuInfo
        __cpuid(cpuInfo, i);
        if (i == 0x80000002) memcpy(outProcessorBrand, cpuInfo, sizeof(cpuInfo));
        else if  (i == 0x80000003) memcpy(outProcessorBrand + 16, cpuInfo, sizeof(cpuInfo));
        else if  (i == 0x80000004) memcpy(outProcessorBrand + 32, cpuInfo, sizeof(cpuInfo));
    }
}

var *runprobe_cpu(probe *self) {
    // Some (not all) hyper-v machines report processor version (model) as "None", so we use cpuid as a fallback (though that is x86 specific)
    // The only other reliable way to get this information seems to be WMI (Win32_Processor)
    // Processor brand string is 48 byte null terminated (3 x 16 bytes)
    static char processorBrand[48] = {'\0'};
    
    (void)self;
    uint32_t e;
    var *res = var_alloc();
    
    DSystemFirmwareInfo systemFirmwareInfo;
    if ((e = getSystemFirmwareInfoAlloc(&systemFirmwareInfo))) {
        log_error("Failed to get system firmware info: %" PRIx32, e);
        return res;
    }
    
    
    var *cpuArray = var_get_array_forkey(res, "cpu");
    for (uint8_t i = 0; i < 16; ++i) {
        DProcessorInfo processorInfo = systemFirmwareInfo.processorInfos[i];
        if (!processorInfo.hasData) continue;
        if (!processorInfo.isSocketPopulated) continue;
        
        var *cpuDict = var_add_dict(cpuArray);
        
        if (processorInfo.version == NULL) {
            log_debug("probe_cpu/cpu/model: %s", "");
            var_set_str_forkey(cpuDict, "model", "");
        }
        else if (strcmp(processorInfo.version, "None") == 0) {
            if (processorBrand[0] == '\0') getCpuidProcessorBrand(processorBrand);
            log_debug("probe_cpu/cpu/model: %s", processorBrand);
            var_set_str_forkey(cpuDict, "model", processorBrand);
        }
        else {
            log_debug("probe_cpu/cpu/model: %s", processorInfo.version);
            var_set_str_forkey(cpuDict, "model", processorInfo.version);
        }
        
        log_debug("probe_cpu/cpu/count: %u", processorInfo.coreEnabled);
        var_set_int_forkey(cpuDict, "count", processorInfo.coreEnabled);
    }
    
    freeSystemFirmwareInfo(&systemFirmwareInfo);
    
    return res;
}

// ============================================================================
/*
void omreport_chassis(var *healthArray) {
    //Example output of "omreport chassis":
    //Health
    //
    //Main System Chassis
    //
    //SEVERITY : COMPONENT
    //Ok       : Fans
    //Ok       : Intrusion
    //Ok       : Memory
    //Ok       : Power Supplies
    //Ok       : Power Management
    //Ok       : Processors
    //Ok       : Temperatures
    //Ok       : Voltages
    //Ok       : Hardware Log
    //Ok       : Batteries
    //
    //For further help, type the command followed by -?
    
    FILE *proc = popen_safe("omreport chassis", "r");
    if (proc == NULL) {
        log_warn("probe_omreport: Could not open 'omreport chassis': %s", strerror(errno));
        return;
    }
    
    log_debug("probe_omreport: Called 'omreport chassis'");
    
    const uint8_t FINDING_COMPONENTS = 0;
    const uint8_t PROCESSING_COMPONENTS = 1;
    const uint8_t DONE = 2;
    
    uint8_t state = FINDING_COMPONENTS;
    while (true) {
        char line[256] = "";
        if (fgets(line, sizeof(line), proc) == NULL) break;
        if (line[0] == '\0') continue;
        
        // Strip line endings
        line[strcspn(line, "\r\n")] = '\0';
        
        if (state == FINDING_COMPONENTS) {
            if (strcmp(line, "SEVERITY : COMPONENT") == 0) {
                state = PROCESSING_COMPONENTS;
            }
        }
        else if (state == PROCESSING_COMPONENTS) {
            if (line[0] == '\0') {
                // Don't break but wait until the process is done (i.e. keep reading the output to prevent "The process tried to write to a nonexistent pipe.")
                state = DONE;
            }
            else {
                // Find space
                char *found;
                found = strchr(line, ' ');
                if (found != NULL) {
                    // Terminate line at found char to copy status
                    *found = '\0';
                    char status[256];
                    strcpy(status, line);
                    // Move past the found char
                    ++found;
                    // Find colon
                    found = strchr(found, ':');
                    if (found != NULL) {
                        // Move past the colon
                        ++found;
                        if (found[0] == ' ') {
                            // Move past the space
                            ++found;
                            
                            var *healthComponentDict = var_add_dict(healthArray);
                            var_set_str_forkey(healthComponentDict, "id", found);
                            var_set_str_forkey(healthComponentDict, "s", status);
                            
                            log_debug("probe_omreport/id: %s", found);
                            log_debug("probe_omreport/s: %s", status);
                        }
                    }
                }
                
            }
        }
    }
    int pret = pclose_safe(proc);
    int status = WEXITSTATUS(pret);
    log_debug("probe_omreport: chassis complete %i %i", status, pret);
}

bool strstarts(const char *str, const char *prefix) {
     return strncmp(str, prefix, strlen(prefix)) == 0;
}

void omreport_storage(var *healthArray) {
    //Example output of "omreport storage controller":
    // Controller  PERC H730P Mini(Embedded)
    //
    //Controller
    //ID                                            : 0
    //Status                                        : Ok
    //Name                                          : PERC H730P Mini
    //Slot ID                                       : Embedded
    //State                                         : Ready
    //Firmware Version                              : 25.5.9.0001
    //Minimum Required Firmware Version             : Not Applicable
    //Driver Version                                : 6.604.06.00
    //Minimum Required Driver Version               : Not Applicable
    //Storport Driver Version                       : 10.0.20348.740
    //Minimum Required Storport Driver Version      : Not Applicable
    //Number of Connectors                          : 2
    //Rebuild Rate                                  : 30%
    //BGI Rate                                      : 30%
    //Check Consistency Rate                        : 30%
    //Reconstruct Rate                              : 30%
    //Alarm State                                   : Not Applicable
    //Cluster Mode                                  : Not Applicable
    //SCSI Initiator ID                             : Not Applicable
    //Cache Memory Size                             : 2048 MB
    //Patrol Read Mode                              : Auto
    //Patrol Read State                             : Stopped
    //Patrol Read Rate                              : 30%
    //Patrol Read Iterations                        : 74
    //Abort Check Consistency on Error              : Disabled
    //Allow Revertible Hot Spare and Replace Member : Enabled
    //Load Balance                                  : Not Applicable
    //Auto Replace Member on Predictive Failure     : Disabled
    //Redundant Path view                           : Not Applicable
    //CacheCade Capable                             : Not Applicable
    //Persistent Hot Spare                          : Disabled
    //Encryption Capable                            : Yes
    //Encryption Key Present                        : No
    //Encryption Mode                               : None
    //Preserved Cache                               : Not Applicable
    //Spin Down Unconfigured Drives                 : Disabled
    //Spin Down Hot Spares                          : Disabled
    //Spin Down Configured Drives                   : Disabled
    //Automatic Disk Power Saving (Idle C)          : Disabled
    //Start Time (HH:MM)                            : Not Applicable
    //Time Interval for Spin Up (in Hours)          : Not Applicable
    //T10 Protection Information Capable            : No
    //Non-RAID HDD Disk Cache Policy                : Unchanged
    //Current Controller Mode                       : RAID
    
    FILE *proc = popen_safe("omreport storage controller", "r");
    if (proc == NULL) {
        log_warn("probe_omreport: Could not open omreport storage controller: %s", strerror(errno));
        return;
    }
    
    log_debug("probe_omreport: Called 'omreport storage controller'");
    
    const uint8_t FINDING_STATUS = 0;
    const uint8_t DONE = 1;
    
    uint8_t state = FINDING_STATUS;
    while (true) {
        char line[256] = "";
        if (fgets(line, sizeof(line), proc) == NULL) break;
        if (line[0] == '\0') continue;
        
        // Strip line endings
        line[strcspn(line, "\r\n")] = '\0';
        
        if (state == FINDING_STATUS) {
            if (strstarts(line, "Status")) {
                char *found;
                // Find colon
                found = strchr(line, ':');
                if (found != NULL) {
                    // Move past the colon
                    ++found;
                    if (found[0] == ' ') {
                        // Move past the space
                        ++found;
                        
                        var *healthComponentDict = var_add_dict(healthArray);
                        var_set_str_forkey(healthComponentDict, "id", "Storage");
                        var_set_str_forkey(healthComponentDict, "s", found);
                        
                        log_debug("probe_omreport/id: Storage");
                        log_debug("probe_omreport/s: %s", found);
                    }
                }
                
                // Don't break but wait until the process is done (i.e. keep reading the output to prevent "The process tried to write to a nonexistent pipe.")
                state = DONE;
            }
        }
    }
    int pret = pclose_safe(proc);
    int status = WEXITSTATUS(pret);
    log_debug("probe_omreport: storage controller complete %i %i", status, pret);
}

var *runprobe_omreport(probe *self) {
    (void)self;
    var *res = var_alloc();
    
    var *healthArray = var_get_array_forkey(res, "health");
    omreport_chassis(healthArray);
    omreport_storage(healthArray);
    
    return res;
}
*/
// ============================================================================

typedef struct DUpdatesProbeState {
    time_t lastOnlineSearchTime;
} DUpdatesProbeState;

static DUpdatesProbeState updatesProbeState;
static bool isUpdatesProbeInitialized = false;

var *runprobe_updates(probe *self) {
    (void)self;
    uint32_t e;
    var *res = var_alloc();
    
    bool searchOnline = false;
    
    if (!isUpdatesProbeInitialized) {
        e = CoInitializeEx(NULL, COINIT_MULTITHREADED);
        if (FAILED(e)) {
            log_error("Failed to initialize COM: %" PRIx32, e);
            return res;
        }
        
        updatesProbeState.lastOnlineSearchTime = time(NULL);
        
        isUpdatesProbeInitialized = true;
        
        searchOnline = true;
    }
    
    
    // @note Alternative is IUpdate2::get_RebootRequired
    ISystemInformation *systemInformation = NULL;
    e = CoCreateInstance(&CLSID_SystemInformation, NULL, CLSCTX_INPROC_SERVER, &IID_ISystemInformation, (void **)&systemInformation);
    if (FAILED(e)) {
        log_error("Failed to create system information: %" PRIx32, e);
        return res;
    }
    
    VARIANT_BOOL isRebootRequired;
    systemInformation->lpVtbl->get_RebootRequired(systemInformation, &isRebootRequired);
    
    
    // Search online once every 24 hours
    time_t now = time(NULL);
    if (difftime(now, updatesProbeState.lastOnlineSearchTime) > 86400) {
        updatesProbeState.lastOnlineSearchTime = now;
        searchOnline = true;
    }
    
    IUpdateSession *updateSession = NULL;
    e = CoCreateInstance(&CLSID_UpdateSession, NULL, CLSCTX_INPROC_SERVER, &IID_IUpdateSession, (void **)&updateSession);
    if (FAILED(e)) {
        log_error("Failed to create update session: %" PRIx32, e);
        return res;
    }
    
    IUpdateSearcher *updateSearcher = NULL;
    e = updateSession->lpVtbl->CreateUpdateSearcher(updateSession, &updateSearcher);
    if (FAILED(e)) {
        log_error("Failed to create update searcher: %" PRIx32, e);
        updateSession->lpVtbl->Release(updateSession);
        return res;
    }
    
    updateSearcher->lpVtbl->put_Online(updateSearcher, searchOnline);
    
    // https://learn.microsoft.com/en-us/windows/win32/api/wuapi/nf-wuapi-iupdatesearcher-search
    // DeploymentAction 1 is should be installed (and will be installed automatically if auto updates are enabled)
    // DeploymentAction 4 is optional
    // Not sure how to properly distinguish optional driver updates and a "quality" software update such as "Cumulative Update Preview for Windows 10 Version 22H2"
    // except that the latter has AutoSelectOnWebSites true
    // The search criteria do somehow accept "IsAssigned" which is not a property on IUpdate, but is documented as:
    // "Finds updates that are intended for deployment by Automatic Updates."
    
    // @note If we don't explicitely include "DeploymentAction = *" it won't show updates with DeploymentAction 4
    BSTR criteria = SysAllocString(L"IsInstalled = 0 AND IsHidden = 0 AND DeploymentAction = * AND IsAssigned = 1");
    ISearchResult *searchResult = NULL;
    e = updateSearcher->lpVtbl->Search(updateSearcher, criteria, &searchResult);
    SysFreeString(criteria);
    if (FAILED(e)) {
        log_error("Failed to search for updates: %" PRIx32, e);
        updateSearcher->lpVtbl->Release(updateSearcher);
        updateSession->lpVtbl->Release(updateSession);
        return res;
    }
    
    var *pkglArray = var_get_array_forkey(res, "pkgl");
    
    IUpdateCollection *updateCollection;
    searchResult->lpVtbl->get_Updates(searchResult, &updateCollection);
    LONG updateCount;
    updateCollection->lpVtbl->get_Count(updateCollection, &updateCount);
    for (size_t i = 0; i < updateCount && i < 10; ++i) {
        IUpdate *update = NULL;
        updateCollection->lpVtbl->get_Item(updateCollection, i, &update);
        
        var *updateDict = var_add_dict(pkglArray);
        
        BSTR nameWide;
        update->lpVtbl->get_Title(update, &nameWide);
        //printf("%ls\n", nameWide);
        
        char nameChar[256];
        // Pass -1 for cchWideChar to tell it the input is a null terminated string
        WideCharToMultiByte(CP_UTF8, 0, nameWide, -1, nameChar, sizeof(nameChar), NULL, NULL);
        //printf("%s\n", nameChar);
        
        SysFreeString(nameWide);
        
        log_debug("probe_updates/id: %s", nameChar);
        var_set_str_forkey(updateDict, "id", nameChar);
        
        DATE publishDate;
        update->lpVtbl->get_LastDeploymentChangeTime(update, &publishDate);
        
        
        BSTR dateWide;
        // Assume LOCALE_INVARIANT will always use the format mm/dd/yyyy
        e = VarBstrFromDate(publishDate, LOCALE_INVARIANT, 0, &dateWide);
        if (FAILED(e)) {
            log_error("Failed to convert update publish date to string: %" PRIx32, e);
        }
        else {
            //printf("%ls\n", dateWide);
            
            char dateChar[11]; // 10 chars plus a null terminator
            // Pass -1 for cchWideChar to tell it the input is a null terminated string
            WideCharToMultiByte(CP_UTF8, 0, dateWide, -1, dateChar, sizeof(dateChar), NULL, NULL);
            //printf("%s\n", dateChar);
            
            SysFreeString(dateWide);
            
            // Convert format from mm/dd/yyyy to yyyy-mm-dd
            char date[11]; // 10 chars plus a null terminator
            date[0] = dateChar[6];
            date[1] = dateChar[7];
            date[2] = dateChar[8];
            date[3] = dateChar[9];
            date[4] = '-';
            date[5] = dateChar[0];
            date[6] = dateChar[1];
            date[7] = '-';
            date[8] = dateChar[3];
            date[9] = dateChar[4];
            date[10] = '\0';
            
            //printf("%s\n", date);
            
            log_debug("probe_updates/v: %s", date);
            var_set_str_forkey(updateDict, "v", date);
        }
        
        //UpdateType updateType;
        //update->lpVtbl->get_Type(update, &updateType);
        //if (updateType == utDriver) {
        //    printf("DRIVER\n");
        //}
        
        //IUpdate5::get_AutoDownload
        
        //IWindowsDriverUpdate *driverUpdate = NULL;
        //e = update->lpVtbl->QueryInterface(update, &IID_IWindowsDriverUpdate, (void **)&driverUpdate);
        //if (SUCCEEDED(e)) {
        //    //printf("DRIVER\n");
        //    driverUpdate->lpVtbl->Release(driverUpdate);
        //}
        
        update->lpVtbl->Release(update);
    }
    updateCollection->lpVtbl->Release(updateCollection);
    
    searchResult->lpVtbl->Release(searchResult);
    updateSearcher->lpVtbl->Release(updateSearcher);
    updateSession->lpVtbl->Release(updateSession);
    
    var *pkgmDict = var_get_dict_forkey(res, "pkgm");
    var_set_int_forkey(pkgmDict, "inq", updateCount);
    var_set_int_forkey(pkgmDict, "reboot", isRebootRequired ? 1 : 0);
    
    return res;
}

// ============================================================================

builtinfunc BUILTINS[] = {
    {"probe_cpu", runprobe_cpu},
    {"probe_version", runprobe_version},
    {"probe_hostname", runprobe_hostname},
    {"probe_meminfo", runprobe_meminfo},
    //{"probe_cpuusage", runprobe_cpuusage}, // @note Cpu usage is calculated in the top-probe
    {"probe_loadavg", runprobe_loadavg},
    {"probe_net", runprobe_net},
    {"probe_io", runprobe_io},
    {"probe_uptime", runprobe_uptime},
    {"probe_uname", runprobe_uname},
    {"probe_df", runprobe_df},
    {"probe_top", runprobe_top},
    {"probe_proc", runprobe_proc},
    {"probe_who", runprobe_who},
    {"probe_localip", runprobe_localip},
    //{"probe_omreport", runprobe_omreport}, // @note The omreport probe is made 'pluginnable' as an extprobe
    {"probe_updates", runprobe_updates},
    {NULL, NULL}
};