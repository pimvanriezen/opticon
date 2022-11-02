#include <stddef.h> // For size_t
#include <stdint.h>
#include <stdbool.h>
#include <inttypes.h> // For PRIx32

#include <string.h>
#include <windows.h>
#include <libopticon/thread.h>
#include <libopticon/log.h>
#include "winService.h"

// @temp
//#include <stdio.h>

#define OPTICON_WIN_SERVICE_NAME "opticon-agent"

uint32_t installWindowsService(bool startAfterInstall) {
    char exefilePath[MAX_PATH];
    if (GetModuleFileNameA(NULL, exefilePath, sizeof(exefilePath)) == 0) {
        log_error("Failed to get executable file path: %" PRIx32, GetLastError());
        return 1;
    }
    
    SC_HANDLE serviceManager;
    if ((serviceManager = OpenSCManagerA(NULL, NULL, SC_MANAGER_ALL_ACCESS)) == NULL) {
        DWORD winerror = GetLastError();
        if (winerror == ERROR_ACCESS_DENIED) log_error("Failed to open service manager: access denied (try running with administrator privileges)");
        else log_error("Failed to open service manager: %" PRIx32, GetLastError());
        return 1;
    }
    
    //strcat(exefilePath, " --test");
    
    SC_HANDLE service;
    if ((service = CreateServiceA(serviceManager, OPTICON_WIN_SERVICE_NAME, "Opticon Agent", SERVICE_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS, SERVICE_AUTO_START, SERVICE_ERROR_NORMAL, exefilePath,
        NULL,   // No load ordering group
        NULL,   // No tag identifier
        NULL,   // No dependencies
        NULL,   // LocalSystem account
        NULL    // No password
    )) == NULL) {
        DWORD winerror = GetLastError();
        if (winerror == ERROR_DUPLICATE_SERVICE_NAME || winerror == ERROR_SERVICE_EXISTS) log_error("Failed to create service: service already exists");
        else log_error("Failed to create service: %" PRIx32, GetLastError());
        
        if (CloseServiceHandle(serviceManager) == false) {
            log_error("Failed to close service manager: %" PRIx32, GetLastError());
        }
        
        return 1;
    }
    log_info("Service installed");
    
    SC_ACTION failureActions[] = {
        {SC_ACTION_RESTART, 60000},
        {SC_ACTION_RESTART, 60000},
        {SC_ACTION_RESTART, 60000}
    };
    SERVICE_FAILURE_ACTIONSA failureActionInfo;
    failureActionInfo.dwResetPeriod = 86400;
    failureActionInfo.lpRebootMsg = NULL;
    failureActionInfo.lpCommand = NULL;
    failureActionInfo.cActions = 3;
    failureActionInfo.lpsaActions = failureActions;
    if (ChangeServiceConfig2A(service, SERVICE_CONFIG_FAILURE_ACTIONS, &failureActionInfo) == false) {
        log_error("Failed to configure service: %" PRIx32, GetLastError());
    }
    else {
        log_info("Service configured");
    }
    
    if (CloseServiceHandle(serviceManager) == false) {
        log_error("Failed to close service manager: %" PRIx32, GetLastError());
    }
    
    if (startAfterInstall) {
        if (StartServiceA(service, 0, NULL) == false) {
            log_error("Failed to start service: %" PRIx32, GetLastError());
        }
        else {
            log_info("Service Started");
        }
    }
    
    if (CloseServiceHandle(service) == false) {
        log_error("Failed to close service: %" PRIx32, GetLastError());
    }
    
    return 0;
}

uint32_t uninstallWindowsService() {
    SC_HANDLE serviceManager;
    if ((serviceManager = OpenSCManagerA(NULL, NULL, SC_MANAGER_ALL_ACCESS)) == NULL) {
        DWORD winerror = GetLastError();
        if (winerror == ERROR_ACCESS_DENIED) log_error("Failed to open service manager: access denied (try running with administrator privileges)");
        else log_error("Failed to open service manager: %" PRIx32, GetLastError());
        return 1;
    }
    
    SC_HANDLE service;
    if ((service = OpenServiceA(serviceManager, OPTICON_WIN_SERVICE_NAME, SERVICE_ALL_ACCESS)) == NULL) {
        DWORD winerror = GetLastError();
        if (winerror != ERROR_SERVICE_DOES_NOT_EXIST) {
            log_error("Failed to open service: %" PRIx32, winerror);
            
            if (CloseServiceHandle(serviceManager) == false) {
                log_error("Failed to close service manager: %" PRIx32, GetLastError());
            }
            
            return 1;
        }
        else {
            if (CloseServiceHandle(serviceManager) == false) {
                log_error("Failed to close service manager: %" PRIx32, GetLastError());
            }
            
            log_info("Service was not installed");
            return 0;
        }
    }
    
    SERVICE_STATUS serviceStatus;
    if (ControlService(service, SERVICE_CONTROL_STOP, &serviceStatus) == false) {
        if (serviceStatus.dwCurrentState != SERVICE_STOPPED) {
            log_error("Failed to stop service: %" PRIx32, GetLastError());
            return 1;
        }
        // Ignore if the service was already stopped
    }
    
    if (CloseServiceHandle(serviceManager) == false) {
        log_error("Failed to close service manager: %" PRIx32, GetLastError());
    }
    
    if (DeleteService(service) == false) {
        log_error("Failed to delete service: %" PRIx32, GetLastError());
        
        if (CloseServiceHandle(service) == false) {
            log_error("Failed to close service: %" PRIx32, GetLastError());
        }
        
        return 1;
    }
    log_info("Service uninstalled");
    
    if (CloseServiceHandle(service) == false) {
        log_error("Failed to close service: %" PRIx32, GetLastError());
    }
    
    return 0;
}



static int appArgc;
static const char **appArgv;
static FAppMain appMainFunction;

static SERVICE_STATUS serviceStatus;
static SERVICE_STATUS_HANDLE serviceStatusHandle;
static HANDLE serviceStopEvent = NULL;

static thread *workerThread = NULL;


static void workerThreadMain(thread *self) {
    (void)self;
    
    char exefilePath[MAX_PATH];
    if (GetModuleFileNameA(NULL, exefilePath, sizeof(exefilePath)) == 0) {
        log_error("Failed to get executable file path: %" PRIx32, GetLastError());
    }
    else {
        
        char *backslash = strrchr(exefilePath, '\\');
        backslash[1] = '\0';
        
        //FILE *f = fopen("C:\\TEST.txt", "a");
        //fprintf(f, "exe: %s", exefilePath);
        //fflush(f);
        
        if (SetCurrentDirectory(exefilePath) == false) {
            log_error("Failed to set current directory: %" PRIx32, GetLastError());
        }
    }
    
    appMainFunction(appArgc, appArgv);
}

static void onWorkerThreadCancel(thread *self) {
    (void)self;
    
    // @todo stop the daemon_main loop
}

static void reportServiceStatus(DWORD dwCurrentState, DWORD dwWin32ExitCode, DWORD dwWaitHint) {
    static DWORD dwCheckPoint = 1;
    
    serviceStatus.dwCurrentState = dwCurrentState;
    serviceStatus.dwWin32ExitCode = dwWin32ExitCode;
    serviceStatus.dwWaitHint = dwWaitHint;
    
    if (dwCurrentState == SERVICE_START_PENDING) serviceStatus.dwControlsAccepted = 0;
    else serviceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP;
    
    if (dwCurrentState == SERVICE_RUNNING || dwCurrentState == SERVICE_STOPPED) serviceStatus.dwCheckPoint = 0;
    else serviceStatus.dwCheckPoint = dwCheckPoint++;
    
    SetServiceStatus(serviceStatusHandle, &serviceStatus);
}

__attribute__((force_align_arg_pointer))
static DWORD WINAPI serviceControlHandler(DWORD controlCode, DWORD eventType, void *eventData, void *customData) {
    (void)eventType;
    (void)eventData;
    (void)customData;
    
    switch (controlCode) {
        case SERVICE_CONTROL_STOP: {
            reportServiceStatus(SERVICE_STOP_PENDING, NO_ERROR, 0);
            
            SetEvent(serviceStopEvent);
            reportServiceStatus(serviceStatus.dwCurrentState, NO_ERROR, 0);
            
            return NO_ERROR;
            break;
        }
        case SERVICE_CONTROL_INTERROGATE: {
            return NO_ERROR;
            break;
        }
        //SERVICE_CONTROL_SHUTDOWN?
        //SERVICE_CONTROL_PAUSE?
    }
    
    return ERROR_CALL_NOT_IMPLEMENTED;
}

static void WINAPI serviceMain(DWORD serviceArgc, LPTSTR *serviceArgv) {
    (void)serviceArgc;
    (void)serviceArgv;
    
    serviceStatusHandle = RegisterServiceCtrlHandlerExA(OPTICON_WIN_SERVICE_NAME, serviceControlHandler, NULL);
    if(serviceStatusHandle == 0) {
        log_error("Failed to register service control handler: %" PRIx32, GetLastError());
        return;
    }
    
    serviceStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    serviceStatus.dwServiceSpecificExitCode = 0;
    
    // Report initial status to the SCM
    reportServiceStatus(SERVICE_START_PENDING, NO_ERROR, 3000);
    
    serviceStopEvent = CreateEvent(NULL, true, false, NULL);
    if (serviceStopEvent == NULL) {
        reportServiceStatus(SERVICE_STOPPED, NO_ERROR, 0);
        return;
    }
    
    // Report running status when initialization is complete
    reportServiceStatus(SERVICE_RUNNING, NO_ERROR, 0);
    
    // Start worker thread
    workerThread = thread_create(workerThreadMain, onWorkerThreadCancel);
    //if (!workerThread->isrunning) {
        // @todo stop service
    //}
    
    WaitForSingleObject(serviceStopEvent, INFINITE);
    //log_debug("Stop service");
    
    // @todo Graceful shutdown: stop worker thread does not seem to work, maybe other probe threads are still running
    // (for now we just force an exit of the whole process instead)
    
    // Stop worker thread
    //thread_cancel(workerThread);
    //workerThread = NULL;
    
    //log_debug("Service stopped");
    
    
    if (CloseHandle(serviceStopEvent) == false) {
        log_error("Failed to close event handle: %" PRIx32, GetLastError());
    }
    
    reportServiceStatus(SERVICE_STOPPED, NO_ERROR, 0);
    
    // Non-graceful shutdown
    ExitProcess(0);
}

uint32_t mainWithServiceSupport(int argc, const char *argv[], FAppMain mainFunction) {
    appArgc = argc;
    appArgv = argv;
    appMainFunction = mainFunction;
    
    SERVICE_TABLE_ENTRY dispatchTable[] = {
        {OPTICON_WIN_SERVICE_NAME, (LPSERVICE_MAIN_FUNCTION)serviceMain},
        {NULL, NULL}
    };
    
    if (StartServiceCtrlDispatcherA(dispatchTable) == false) {
        DWORD winerror = GetLastError();
        if (winerror != ERROR_FAILED_SERVICE_CONTROLLER_CONNECT) {
            log_error("Failed to run as a service: %" PRIx32, GetLastError());
            return 1; // Mimic the error return value of the mainFunction
        }
        else {
            // Application was not started as a service
            return mainFunction(argc, argv);
        }
    }
    
    return 0;
}