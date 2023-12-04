#pragma once
#pragma GCC system_header

// @note We're extending this system header
#include_next <wuapi.h>







#ifndef __IWindowsDriverUpdate_FWD_DEFINED__
#define __IWindowsDriverUpdate_FWD_DEFINED__
typedef interface IWindowsDriverUpdate IWindowsDriverUpdate;
#ifdef __cplusplus
interface IWindowsDriverUpdate;
#endif /* __cplusplus */
#endif














/*****************************************************************************
 * IWindowsDriverUpdate interface
 */
#ifndef __IWindowsDriverUpdate_INTERFACE_DEFINED__
#define __IWindowsDriverUpdate_INTERFACE_DEFINED__

DEFINE_GUID(IID_IWindowsDriverUpdate, 0xb383cd1a, 0x5ce9, 0x4504, 0x9f,0x63, 0x76,0x4b,0x12,0x36,0xf1,0x91);
#if defined(__cplusplus) && !defined(CINTERFACE)
MIDL_INTERFACE("b383cd1a-5ce9-4504-9f63-764b1236f191")
IWindowsDriverUpdate : public IDispatch
{
    //get_DeviceProblemNumber
	//get_DeviceStatus
	//get_DriverClass
	//get_DriverHardwareID
	//get_DriverManufacturer
	//get_DriverModel
	//get_DriverProvider
	//get_DriverVerDate

};
#ifdef __CRT_UUID_DECL
__CRT_UUID_DECL(IWindowsDriverUpdate, 0xb383cd1a, 0x5ce9, 0x4504, 0x9f,0x63, 0x76,0x4b,0x12,0x36,0xf1,0x91)
#endif
#else
typedef struct IWindowsDriverUpdateVtbl {
    BEGIN_INTERFACE

    /*** IUnknown methods ***/
    HRESULT (STDMETHODCALLTYPE *QueryInterface)(
        IWindowsDriverUpdate *This,
        REFIID riid,
        void **ppvObject);

    ULONG (STDMETHODCALLTYPE *AddRef)(
        IWindowsDriverUpdate *This);

    ULONG (STDMETHODCALLTYPE *Release)(
        IWindowsDriverUpdate *This);

    /*** IDispatch methods ***/
    HRESULT (STDMETHODCALLTYPE *GetTypeInfoCount)(
        IWindowsDriverUpdate *This,
        UINT *pctinfo);

    HRESULT (STDMETHODCALLTYPE *GetTypeInfo)(
        IWindowsDriverUpdate *This,
        UINT iTInfo,
        LCID lcid,
        ITypeInfo **ppTInfo);

    HRESULT (STDMETHODCALLTYPE *GetIDsOfNames)(
        IWindowsDriverUpdate *This,
        REFIID riid,
        LPOLESTR *rgszNames,
        UINT cNames,
        LCID lcid,
        DISPID *rgDispId);

    HRESULT (STDMETHODCALLTYPE *Invoke)(
        IWindowsDriverUpdate *This,
        DISPID dispIdMember,
        REFIID riid,
        LCID lcid,
        WORD wFlags,
        DISPPARAMS *pDispParams,
        VARIANT *pVarResult,
        EXCEPINFO *pExcepInfo,
        UINT *puArgErr);

    /*** IUpdate methods ***/
    HRESULT (STDMETHODCALLTYPE *get_Title)(
        IWindowsDriverUpdate *This,
        BSTR *retval);

    HRESULT (STDMETHODCALLTYPE *get_AutoSelectOnWebSites)(
        IWindowsDriverUpdate *This,
        VARIANT_BOOL *retval);

    HRESULT (STDMETHODCALLTYPE *get_BundledUpdates)(
        IWindowsDriverUpdate *This,
        IUpdateCollection **retval);

    HRESULT (STDMETHODCALLTYPE *get_CanRequireSource)(
        IWindowsDriverUpdate *This,
        VARIANT_BOOL *retval);

    HRESULT (STDMETHODCALLTYPE *get_Categories)(
        IWindowsDriverUpdate *This,
        ICategoryCollection **retval);

    HRESULT (STDMETHODCALLTYPE *get_Deadline)(
        IWindowsDriverUpdate *This,
        VARIANT *retval);

    HRESULT (STDMETHODCALLTYPE *get_DeltaCompressedContentAvailable)(
        IWindowsDriverUpdate *This,
        VARIANT_BOOL *retval);

    HRESULT (STDMETHODCALLTYPE *get_DeltaCompressedContentPreferred)(
        IWindowsDriverUpdate *This,
        VARIANT_BOOL *retval);

    HRESULT (STDMETHODCALLTYPE *get_Description)(
        IWindowsDriverUpdate *This,
        BSTR *retval);

    HRESULT (STDMETHODCALLTYPE *get_EulaAccepted)(
        IWindowsDriverUpdate *This,
        VARIANT_BOOL *retval);

    HRESULT (STDMETHODCALLTYPE *get_EulaText)(
        IWindowsDriverUpdate *This,
        BSTR *retval);

    HRESULT (STDMETHODCALLTYPE *get_HandlerID)(
        IWindowsDriverUpdate *This,
        BSTR *retval);

    HRESULT (STDMETHODCALLTYPE *get_Identity)(
        IWindowsDriverUpdate *This,
        IUpdateIdentity **retval);

    HRESULT (STDMETHODCALLTYPE *get_Image)(
        IWindowsDriverUpdate *This,
        IImageInformation **retval);

    HRESULT (STDMETHODCALLTYPE *get_InstallationBehavior)(
        IWindowsDriverUpdate *This,
        IInstallationBehavior **retval);

    HRESULT (STDMETHODCALLTYPE *get_IsBeta)(
        IWindowsDriverUpdate *This,
        VARIANT_BOOL *retval);

    HRESULT (STDMETHODCALLTYPE *get_IsDownloaded)(
        IWindowsDriverUpdate *This,
        VARIANT_BOOL *retval);

    HRESULT (STDMETHODCALLTYPE *get_IsHidden)(
        IWindowsDriverUpdate *This,
        VARIANT_BOOL *retval);

    HRESULT (STDMETHODCALLTYPE *put_IsHidden)(
        IWindowsDriverUpdate *This,
        VARIANT_BOOL value);

    HRESULT (STDMETHODCALLTYPE *get_IsInstalled)(
        IWindowsDriverUpdate *This,
        VARIANT_BOOL *retval);

    HRESULT (STDMETHODCALLTYPE *get_IsMandatory)(
        IWindowsDriverUpdate *This,
        VARIANT_BOOL *retval);

    HRESULT (STDMETHODCALLTYPE *get_IsUninstallable)(
        IWindowsDriverUpdate *This,
        VARIANT_BOOL *retval);

    HRESULT (STDMETHODCALLTYPE *get_Languages)(
        IWindowsDriverUpdate *This,
        IStringCollection **retval);

    HRESULT (STDMETHODCALLTYPE *get_LastDeploymentChangeTime)(
        IWindowsDriverUpdate *This,
        DATE *retval);

    HRESULT (STDMETHODCALLTYPE *get_MaxDownloadSize)(
        IWindowsDriverUpdate *This,
        DECIMAL *retval);

    HRESULT (STDMETHODCALLTYPE *get_MinDownloadSize)(
        IWindowsDriverUpdate *This,
        DECIMAL *retval);

    HRESULT (STDMETHODCALLTYPE *get_MoreInfoUrls)(
        IWindowsDriverUpdate *This,
        IStringCollection **retval);

    HRESULT (STDMETHODCALLTYPE *get_MsrcSeverity)(
        IWindowsDriverUpdate *This,
        BSTR *retval);

    HRESULT (STDMETHODCALLTYPE *get_RecommendedCpuSpeed)(
        IWindowsDriverUpdate *This,
        LONG *retval);

    HRESULT (STDMETHODCALLTYPE *get_RecommendedHardDiskSpace)(
        IWindowsDriverUpdate *This,
        LONG *retval);

    HRESULT (STDMETHODCALLTYPE *get_RecommendedMemory)(
        IWindowsDriverUpdate *This,
        LONG *retval);

    HRESULT (STDMETHODCALLTYPE *get_ReleaseNotes)(
        IWindowsDriverUpdate *This,
        BSTR *retval);

    HRESULT (STDMETHODCALLTYPE *get_SecurityBulletinIDs)(
        IWindowsDriverUpdate *This,
        IStringCollection **retval);

    HRESULT (STDMETHODCALLTYPE *get_SupersededUpdateIDs)(
        IWindowsDriverUpdate *This,
        IStringCollection **retval);

    HRESULT (STDMETHODCALLTYPE *get_SupportUrl)(
        IWindowsDriverUpdate *This,
        BSTR *retval);

    HRESULT (STDMETHODCALLTYPE *get_Type)(
        IWindowsDriverUpdate *This,
        UpdateType *retval);

    HRESULT (STDMETHODCALLTYPE *get_UninstallationNotes)(
        IWindowsDriverUpdate *This,
        BSTR *retval);

    HRESULT (STDMETHODCALLTYPE *get_UninstallationBehavior)(
        IWindowsDriverUpdate *This,
        IInstallationBehavior **retval);

    HRESULT (STDMETHODCALLTYPE *get_UninstallationSteps)(
        IWindowsDriverUpdate *This,
        IStringCollection **retval);

    HRESULT (STDMETHODCALLTYPE *get_KBArticleIDs)(
        IWindowsDriverUpdate *This,
        IStringCollection **retval);

    HRESULT (STDMETHODCALLTYPE *AcceptEula)(
        IWindowsDriverUpdate *This);

    HRESULT (STDMETHODCALLTYPE *get_DeploymentAction)(
        IWindowsDriverUpdate *This,
        DeploymentAction *retval);

    HRESULT (STDMETHODCALLTYPE *CopyFromCache)(
        IWindowsDriverUpdate *This,
        BSTR path,
        VARIANT_BOOL toExtractCabFiles);

    HRESULT (STDMETHODCALLTYPE *get_DownloadPriority)(
        IWindowsDriverUpdate *This,
        DownloadPriority *retval);

    HRESULT (STDMETHODCALLTYPE *get_DownloadContents)(
        IWindowsDriverUpdate *This,
        IUpdateDownloadContentCollection **retval);
    
    /*** IWindowsDriverUpdate methods ***/
    //

    END_INTERFACE
} IWindowsDriverUpdateVtbl;

interface IWindowsDriverUpdate {
    CONST_VTBL IWindowsDriverUpdateVtbl* lpVtbl;
};

#ifdef COBJMACROS
#ifndef WIDL_C_INLINE_WRAPPERS
/*** IUnknown methods ***/
#define IWindowsDriverUpdate_QueryInterface(This,riid,ppvObject) (This)->lpVtbl->QueryInterface(This,riid,ppvObject)
#define IWindowsDriverUpdate_AddRef(This) (This)->lpVtbl->AddRef(This)
#define IWindowsDriverUpdate_Release(This) (This)->lpVtbl->Release(This)
/*** IDispatch methods ***/
#define IWindowsDriverUpdate_GetTypeInfoCount(This,pctinfo) (This)->lpVtbl->GetTypeInfoCount(This,pctinfo)
#define IWindowsDriverUpdate_GetTypeInfo(This,iTInfo,lcid,ppTInfo) (This)->lpVtbl->GetTypeInfo(This,iTInfo,lcid,ppTInfo)
#define IWindowsDriverUpdate_GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId) (This)->lpVtbl->GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)
#define IWindowsDriverUpdate_Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr) (This)->lpVtbl->Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)
/*** IUpdate methods ***/
#define IWindowsDriverUpdate_get_Title(This,retval) (This)->lpVtbl->get_Title(This,retval)
#define IWindowsDriverUpdate_get_AutoSelectOnWebSites(This,retval) (This)->lpVtbl->get_AutoSelectOnWebSites(This,retval)
#define IWindowsDriverUpdate_get_BundledUpdates(This,retval) (This)->lpVtbl->get_BundledUpdates(This,retval)
#define IWindowsDriverUpdate_get_CanRequireSource(This,retval) (This)->lpVtbl->get_CanRequireSource(This,retval)
#define IWindowsDriverUpdate_get_Categories(This,retval) (This)->lpVtbl->get_Categories(This,retval)
#define IWindowsDriverUpdate_get_Deadline(This,retval) (This)->lpVtbl->get_Deadline(This,retval)
#define IWindowsDriverUpdate_get_DeltaCompressedContentAvailable(This,retval) (This)->lpVtbl->get_DeltaCompressedContentAvailable(This,retval)
#define IWindowsDriverUpdate_get_DeltaCompressedContentPreferred(This,retval) (This)->lpVtbl->get_DeltaCompressedContentPreferred(This,retval)
#define IWindowsDriverUpdate_get_Description(This,retval) (This)->lpVtbl->get_Description(This,retval)
#define IWindowsDriverUpdate_get_EulaAccepted(This,retval) (This)->lpVtbl->get_EulaAccepted(This,retval)
#define IWindowsDriverUpdate_get_EulaText(This,retval) (This)->lpVtbl->get_EulaText(This,retval)
#define IWindowsDriverUpdate_get_HandlerID(This,retval) (This)->lpVtbl->get_HandlerID(This,retval)
#define IWindowsDriverUpdate_get_Identity(This,retval) (This)->lpVtbl->get_Identity(This,retval)
#define IWindowsDriverUpdate_get_Image(This,retval) (This)->lpVtbl->get_Image(This,retval)
#define IWindowsDriverUpdate_get_InstallationBehavior(This,retval) (This)->lpVtbl->get_InstallationBehavior(This,retval)
#define IWindowsDriverUpdate_get_IsBeta(This,retval) (This)->lpVtbl->get_IsBeta(This,retval)
#define IWindowsDriverUpdate_get_IsDownloaded(This,retval) (This)->lpVtbl->get_IsDownloaded(This,retval)
#define IWindowsDriverUpdate_get_IsHidden(This,retval) (This)->lpVtbl->get_IsHidden(This,retval)
#define IWindowsDriverUpdate_put_IsHidden(This,value) (This)->lpVtbl->put_IsHidden(This,value)
#define IWindowsDriverUpdate_get_IsInstalled(This,retval) (This)->lpVtbl->get_IsInstalled(This,retval)
#define IWindowsDriverUpdate_get_IsMandatory(This,retval) (This)->lpVtbl->get_IsMandatory(This,retval)
#define IWindowsDriverUpdate_get_IsUninstallable(This,retval) (This)->lpVtbl->get_IsUninstallable(This,retval)
#define IWindowsDriverUpdate_get_Languages(This,retval) (This)->lpVtbl->get_Languages(This,retval)
#define IWindowsDriverUpdate_get_LastDeploymentChangeTime(This,retval) (This)->lpVtbl->get_LastDeploymentChangeTime(This,retval)
#define IWindowsDriverUpdate_get_MaxDownloadSize(This,retval) (This)->lpVtbl->get_MaxDownloadSize(This,retval)
#define IWindowsDriverUpdate_get_MinDownloadSize(This,retval) (This)->lpVtbl->get_MinDownloadSize(This,retval)
#define IWindowsDriverUpdate_get_MoreInfoUrls(This,retval) (This)->lpVtbl->get_MoreInfoUrls(This,retval)
#define IWindowsDriverUpdate_get_MsrcSeverity(This,retval) (This)->lpVtbl->get_MsrcSeverity(This,retval)
#define IWindowsDriverUpdate_get_RecommendedCpuSpeed(This,retval) (This)->lpVtbl->get_RecommendedCpuSpeed(This,retval)
#define IWindowsDriverUpdate_get_RecommendedHardDiskSpace(This,retval) (This)->lpVtbl->get_RecommendedHardDiskSpace(This,retval)
#define IWindowsDriverUpdate_get_RecommendedMemory(This,retval) (This)->lpVtbl->get_RecommendedMemory(This,retval)
#define IWindowsDriverUpdate_get_ReleaseNotes(This,retval) (This)->lpVtbl->get_ReleaseNotes(This,retval)
#define IWindowsDriverUpdate_get_SecurityBulletinIDs(This,retval) (This)->lpVtbl->get_SecurityBulletinIDs(This,retval)
#define IWindowsDriverUpdate_get_SupersededUpdateIDs(This,retval) (This)->lpVtbl->get_SupersededUpdateIDs(This,retval)
#define IWindowsDriverUpdate_get_SupportUrl(This,retval) (This)->lpVtbl->get_SupportUrl(This,retval)
#define IWindowsDriverUpdate_get_Type(This,retval) (This)->lpVtbl->get_Type(This,retval)
#define IWindowsDriverUpdate_get_UninstallationNotes(This,retval) (This)->lpVtbl->get_UninstallationNotes(This,retval)
#define IWindowsDriverUpdate_get_UninstallationBehavior(This,retval) (This)->lpVtbl->get_UninstallationBehavior(This,retval)
#define IWindowsDriverUpdate_get_UninstallationSteps(This,retval) (This)->lpVtbl->get_UninstallationSteps(This,retval)
#define IWindowsDriverUpdate_get_KBArticleIDs(This,retval) (This)->lpVtbl->get_KBArticleIDs(This,retval)
#define IWindowsDriverUpdate_AcceptEula(This) (This)->lpVtbl->AcceptEula(This)
#define IWindowsDriverUpdate_get_DeploymentAction(This,retval) (This)->lpVtbl->get_DeploymentAction(This,retval)
#define IWindowsDriverUpdate_CopyFromCache(This,path,toExtractCabFiles) (This)->lpVtbl->CopyFromCache(This,path,toExtractCabFiles)
#define IWindowsDriverUpdate_get_DownloadPriority(This,retval) (This)->lpVtbl->get_DownloadPriority(This,retval)
#define IWindowsDriverUpdate_get_DownloadContents(This,retval) (This)->lpVtbl->get_DownloadContents(This,retval)
/*** IWindowsDriverUpdate methods ***/
//
#else
/*** IUnknown methods ***/
static __WIDL_INLINE HRESULT IWindowsDriverUpdate_QueryInterface(IWindowsDriverUpdate* This,REFIID riid,void **ppvObject) {
    return This->lpVtbl->QueryInterface(This,riid,ppvObject);
}
static __WIDL_INLINE ULONG IWindowsDriverUpdate_AddRef(IWindowsDriverUpdate* This) {
    return This->lpVtbl->AddRef(This);
}
static __WIDL_INLINE ULONG IWindowsDriverUpdate_Release(IWindowsDriverUpdate* This) {
    return This->lpVtbl->Release(This);
}
/*** IDispatch methods ***/
static __WIDL_INLINE HRESULT IWindowsDriverUpdate_GetTypeInfoCount(IWindowsDriverUpdate* This,UINT *pctinfo) {
    return This->lpVtbl->GetTypeInfoCount(This,pctinfo);
}
static __WIDL_INLINE HRESULT IWindowsDriverUpdate_GetTypeInfo(IWindowsDriverUpdate* This,UINT iTInfo,LCID lcid,ITypeInfo **ppTInfo) {
    return This->lpVtbl->GetTypeInfo(This,iTInfo,lcid,ppTInfo);
}
static __WIDL_INLINE HRESULT IWindowsDriverUpdate_GetIDsOfNames(IWindowsDriverUpdate* This,REFIID riid,LPOLESTR *rgszNames,UINT cNames,LCID lcid,DISPID *rgDispId) {
    return This->lpVtbl->GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId);
}
static __WIDL_INLINE HRESULT IWindowsDriverUpdate_Invoke(IWindowsDriverUpdate* This,DISPID dispIdMember,REFIID riid,LCID lcid,WORD wFlags,DISPPARAMS *pDispParams,VARIANT *pVarResult,EXCEPINFO *pExcepInfo,UINT *puArgErr) {
    return This->lpVtbl->Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr);
}
/*** IUpdate methods ***/
static __WIDL_INLINE HRESULT IWindowsDriverUpdate_get_Title(IWindowsDriverUpdate* This,BSTR *retval) {
    return This->lpVtbl->get_Title(This,retval);
}
static __WIDL_INLINE HRESULT IWindowsDriverUpdate_get_AutoSelectOnWebSites(IWindowsDriverUpdate* This,VARIANT_BOOL *retval) {
    return This->lpVtbl->get_AutoSelectOnWebSites(This,retval);
}
static __WIDL_INLINE HRESULT IWindowsDriverUpdate_get_BundledUpdates(IWindowsDriverUpdate* This,IUpdateCollection **retval) {
    return This->lpVtbl->get_BundledUpdates(This,retval);
}
static __WIDL_INLINE HRESULT IWindowsDriverUpdate_get_CanRequireSource(IWindowsDriverUpdate* This,VARIANT_BOOL *retval) {
    return This->lpVtbl->get_CanRequireSource(This,retval);
}
static __WIDL_INLINE HRESULT IWindowsDriverUpdate_get_Categories(IWindowsDriverUpdate* This,ICategoryCollection **retval) {
    return This->lpVtbl->get_Categories(This,retval);
}
static __WIDL_INLINE HRESULT IWindowsDriverUpdate_get_Deadline(IWindowsDriverUpdate* This,VARIANT *retval) {
    return This->lpVtbl->get_Deadline(This,retval);
}
static __WIDL_INLINE HRESULT IWindowsDriverUpdate_get_DeltaCompressedContentAvailable(IWindowsDriverUpdate* This,VARIANT_BOOL *retval) {
    return This->lpVtbl->get_DeltaCompressedContentAvailable(This,retval);
}
static __WIDL_INLINE HRESULT IWindowsDriverUpdate_get_DeltaCompressedContentPreferred(IWindowsDriverUpdate* This,VARIANT_BOOL *retval) {
    return This->lpVtbl->get_DeltaCompressedContentPreferred(This,retval);
}
static __WIDL_INLINE HRESULT IWindowsDriverUpdate_get_Description(IWindowsDriverUpdate* This,BSTR *retval) {
    return This->lpVtbl->get_Description(This,retval);
}
static __WIDL_INLINE HRESULT IWindowsDriverUpdate_get_EulaAccepted(IWindowsDriverUpdate* This,VARIANT_BOOL *retval) {
    return This->lpVtbl->get_EulaAccepted(This,retval);
}
static __WIDL_INLINE HRESULT IWindowsDriverUpdate_get_EulaText(IWindowsDriverUpdate* This,BSTR *retval) {
    return This->lpVtbl->get_EulaText(This,retval);
}
static __WIDL_INLINE HRESULT IWindowsDriverUpdate_get_HandlerID(IWindowsDriverUpdate* This,BSTR *retval) {
    return This->lpVtbl->get_HandlerID(This,retval);
}
static __WIDL_INLINE HRESULT IWindowsDriverUpdate_get_Identity(IWindowsDriverUpdate* This,IUpdateIdentity **retval) {
    return This->lpVtbl->get_Identity(This,retval);
}
static __WIDL_INLINE HRESULT IWindowsDriverUpdate_get_Image(IWindowsDriverUpdate* This,IImageInformation **retval) {
    return This->lpVtbl->get_Image(This,retval);
}
static __WIDL_INLINE HRESULT IWindowsDriverUpdate_get_InstallationBehavior(IWindowsDriverUpdate* This,IInstallationBehavior **retval) {
    return This->lpVtbl->get_InstallationBehavior(This,retval);
}
static __WIDL_INLINE HRESULT IWindowsDriverUpdate_get_IsBeta(IWindowsDriverUpdate* This,VARIANT_BOOL *retval) {
    return This->lpVtbl->get_IsBeta(This,retval);
}
static __WIDL_INLINE HRESULT IWindowsDriverUpdate_get_IsDownloaded(IWindowsDriverUpdate* This,VARIANT_BOOL *retval) {
    return This->lpVtbl->get_IsDownloaded(This,retval);
}
static __WIDL_INLINE HRESULT IWindowsDriverUpdate_get_IsHidden(IWindowsDriverUpdate* This,VARIANT_BOOL *retval) {
    return This->lpVtbl->get_IsHidden(This,retval);
}
static __WIDL_INLINE HRESULT IWindowsDriverUpdate_put_IsHidden(IWindowsDriverUpdate* This,VARIANT_BOOL value) {
    return This->lpVtbl->put_IsHidden(This,value);
}
static __WIDL_INLINE HRESULT IWindowsDriverUpdate_get_IsInstalled(IWindowsDriverUpdate* This,VARIANT_BOOL *retval) {
    return This->lpVtbl->get_IsInstalled(This,retval);
}
static __WIDL_INLINE HRESULT IWindowsDriverUpdate_get_IsMandatory(IWindowsDriverUpdate* This,VARIANT_BOOL *retval) {
    return This->lpVtbl->get_IsMandatory(This,retval);
}
static __WIDL_INLINE HRESULT IWindowsDriverUpdate_get_IsUninstallable(IWindowsDriverUpdate* This,VARIANT_BOOL *retval) {
    return This->lpVtbl->get_IsUninstallable(This,retval);
}
static __WIDL_INLINE HRESULT IWindowsDriverUpdate_get_Languages(IWindowsDriverUpdate* This,IStringCollection **retval) {
    return This->lpVtbl->get_Languages(This,retval);
}
static __WIDL_INLINE HRESULT IWindowsDriverUpdate_get_LastDeploymentChangeTime(IWindowsDriverUpdate* This,DATE *retval) {
    return This->lpVtbl->get_LastDeploymentChangeTime(This,retval);
}
static __WIDL_INLINE HRESULT IWindowsDriverUpdate_get_MaxDownloadSize(IWindowsDriverUpdate* This,DECIMAL *retval) {
    return This->lpVtbl->get_MaxDownloadSize(This,retval);
}
static __WIDL_INLINE HRESULT IWindowsDriverUpdate_get_MinDownloadSize(IWindowsDriverUpdate* This,DECIMAL *retval) {
    return This->lpVtbl->get_MinDownloadSize(This,retval);
}
static __WIDL_INLINE HRESULT IWindowsDriverUpdate_get_MoreInfoUrls(IWindowsDriverUpdate* This,IStringCollection **retval) {
    return This->lpVtbl->get_MoreInfoUrls(This,retval);
}
static __WIDL_INLINE HRESULT IWindowsDriverUpdate_get_MsrcSeverity(IWindowsDriverUpdate* This,BSTR *retval) {
    return This->lpVtbl->get_MsrcSeverity(This,retval);
}
static __WIDL_INLINE HRESULT IWindowsDriverUpdate_get_RecommendedCpuSpeed(IWindowsDriverUpdate* This,LONG *retval) {
    return This->lpVtbl->get_RecommendedCpuSpeed(This,retval);
}
static __WIDL_INLINE HRESULT IWindowsDriverUpdate_get_RecommendedHardDiskSpace(IWindowsDriverUpdate* This,LONG *retval) {
    return This->lpVtbl->get_RecommendedHardDiskSpace(This,retval);
}
static __WIDL_INLINE HRESULT IWindowsDriverUpdate_get_RecommendedMemory(IWindowsDriverUpdate* This,LONG *retval) {
    return This->lpVtbl->get_RecommendedMemory(This,retval);
}
static __WIDL_INLINE HRESULT IWindowsDriverUpdate_get_ReleaseNotes(IWindowsDriverUpdate* This,BSTR *retval) {
    return This->lpVtbl->get_ReleaseNotes(This,retval);
}
static __WIDL_INLINE HRESULT IWindowsDriverUpdate_get_SecurityBulletinIDs(IWindowsDriverUpdate* This,IStringCollection **retval) {
    return This->lpVtbl->get_SecurityBulletinIDs(This,retval);
}
static __WIDL_INLINE HRESULT IWindowsDriverUpdate_get_SupersededUpdateIDs(IWindowsDriverUpdate* This,IStringCollection **retval) {
    return This->lpVtbl->get_SupersededUpdateIDs(This,retval);
}
static __WIDL_INLINE HRESULT IWindowsDriverUpdate_get_SupportUrl(IWindowsDriverUpdate* This,BSTR *retval) {
    return This->lpVtbl->get_SupportUrl(This,retval);
}
static __WIDL_INLINE HRESULT IWindowsDriverUpdate_get_Type(IWindowsDriverUpdate* This,UpdateType *retval) {
    return This->lpVtbl->get_Type(This,retval);
}
static __WIDL_INLINE HRESULT IWindowsDriverUpdate_get_UninstallationNotes(IWindowsDriverUpdate* This,BSTR *retval) {
    return This->lpVtbl->get_UninstallationNotes(This,retval);
}
static __WIDL_INLINE HRESULT IWindowsDriverUpdate_get_UninstallationBehavior(IWindowsDriverUpdate* This,IInstallationBehavior **retval) {
    return This->lpVtbl->get_UninstallationBehavior(This,retval);
}
static __WIDL_INLINE HRESULT IWindowsDriverUpdate_get_UninstallationSteps(IWindowsDriverUpdate* This,IStringCollection **retval) {
    return This->lpVtbl->get_UninstallationSteps(This,retval);
}
static __WIDL_INLINE HRESULT IWindowsDriverUpdate_get_KBArticleIDs(IWindowsDriverUpdate* This,IStringCollection **retval) {
    return This->lpVtbl->get_KBArticleIDs(This,retval);
}
static __WIDL_INLINE HRESULT IWindowsDriverUpdate_AcceptEula(IWindowsDriverUpdate* This) {
    return This->lpVtbl->AcceptEula(This);
}
static __WIDL_INLINE HRESULT IWindowsDriverUpdate_get_DeploymentAction(IWindowsDriverUpdate* This,DeploymentAction *retval) {
    return This->lpVtbl->get_DeploymentAction(This,retval);
}
static __WIDL_INLINE HRESULT IWindowsDriverUpdate_CopyFromCache(IWindowsDriverUpdate* This,BSTR path,VARIANT_BOOL toExtractCabFiles) {
    return This->lpVtbl->CopyFromCache(This,path,toExtractCabFiles);
}
static __WIDL_INLINE HRESULT IWindowsDriverUpdate_get_DownloadPriority(IWindowsDriverUpdate* This,DownloadPriority *retval) {
    return This->lpVtbl->get_DownloadPriority(This,retval);
}
static __WIDL_INLINE HRESULT IWindowsDriverUpdate_get_DownloadContents(IWindowsDriverUpdate* This,IUpdateDownloadContentCollection **retval) {
    return This->lpVtbl->get_DownloadContents(This,retval);
}
/*** IWindowsDriverUpdate methods ***/
//
#endif
#endif

#endif


#endif  /* __IWindowsDriverUpdate_INTERFACE_DEFINED__ */