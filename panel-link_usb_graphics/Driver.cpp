/*++

Copyright (c) Microsoft Corporation

Abstract:

    This module contains a sample implementation of an indirect display driver. See the included README.md file and the
    various TODO blocks throughout this file and all accompanying files for information on building a production driver.

    MSDN documentation on indirect displays can be found at https://msdn.microsoft.com/en-us/library/windows/hardware/mt761968(v=vs.85).aspx.

Environment:

    User Mode, UMDF

--*/

#include "Driver.h"
#include "Driver.tmh"


using namespace std;
using namespace Microsoft::IndirectDisp;
using namespace Microsoft::WRL;

extern "C" DRIVER_INITIALIZE DriverEntry;

EVT_WDF_DRIVER_DEVICE_ADD IddSampleDeviceAdd;


EVT_WDF_DEVICE_D0_ENTRY IddSampleDeviceD0Entry;
EVT_WDF_DEVICE_D0_EXIT IddSampleDeviceD0Exit;

EVT_IDD_CX_ADAPTER_INIT_FINISHED IddSampleAdapterInitFinished;
EVT_IDD_CX_ADAPTER_COMMIT_MODES IddSampleAdapterCommitModes;

EVT_IDD_CX_PARSE_MONITOR_DESCRIPTION IddSampleParseMonitorDescription;
EVT_IDD_CX_MONITOR_GET_DEFAULT_DESCRIPTION_MODES IddSampleMonitorGetDefaultModes;
EVT_IDD_CX_MONITOR_QUERY_TARGET_MODES IddSampleMonitorQueryModes;

EVT_IDD_CX_MONITOR_ASSIGN_SWAPCHAIN IddSampleMonitorAssignSwapChain;
EVT_IDD_CX_MONITOR_UNASSIGN_SWAPCHAIN IddSampleMonitorUnassignSwapChain;

#if 1
static unsigned char BeadaPanelScreenPreferedMode[256];

static int m_prefered_mode;
static int m_brightness;

//#define LOG(fmt, ...) TraceEvents((TRACE_LEVEL_INFORMATION), (TRACE_DRIVER), "HelloWorld")
#define LOG(fmt,...) do {;} while(0)

int getPanelInfo(WDFDEVICE device, STATUSLINK_INFO * info);
int getPanelTime(WDFDEVICE device, SYSTEMTIME * time);
int setPanelTime(WDFDEVICE device, SYSTEMTIME * time);
int setPanelBL(WDFDEVICE device, unsigned char value);

void HexDump(unsigned char *buf, int len, unsigned char *addr) {
	int i, j, k;
	char binstr[80];

	for (i = 0; i < len; i++) {
		if (0 == (i % 16)) {
			sprintf(binstr, "%08x -", i + (int)addr);
			sprintf(binstr, "%s %02x", binstr, (unsigned char)buf[i]);
		}
		else if (15 == (i % 16)) {
			sprintf(binstr, "%s %02x", binstr, (unsigned char)buf[i]);
			sprintf(binstr, "%s  ", binstr);
			for (j = i - 15; j <= i; j++) {
				sprintf(binstr, "%s%c", binstr, ('!' < buf[j] && buf[j] <= '~') ? buf[j] : '.');
			}
			TraceEvents(TRACE_LEVEL_INFORMATION, MYDRIVER_ALL_INFO, "%s\n", binstr);
		}
		else {
			sprintf(binstr, "%s %02x", binstr, (unsigned char)buf[i]);
		}
	}
	if (0 != (i % 16)) {
		k = 16 - (i % 16);
		for (j = 0; j < k; j++) {
			sprintf(binstr, "%s   ", binstr);
		}
		sprintf(binstr, "%s  ", binstr);
		k = 16 - k;
		for (j = i - k; j < i; j++) {
			sprintf(binstr, "%s%c", binstr, ('!' < buf[j] && buf[j] <= '~') ? buf[j] : '.');
		}
		TraceEvents(TRACE_LEVEL_INFORMATION, MYDRIVER_ALL_INFO, "%s\n", binstr);
	}
}
VOID EvtRequestWriteCompletionRoutine(
	_In_ WDFREQUEST                  Request,
	_In_ WDFIOTARGET                 Target,
	_In_ PWDF_REQUEST_COMPLETION_PARAMS CompletionParams,
	_In_ WDFCONTEXT                  Context);
NTSTATUS usbSendMsg(WDFUSBPIPE pipeHandle, PUCHAR msg, unsigned int * tsize);
NTSTATUS usbReadMsg(WDFUSBPIPE pipeHandle, PUCHAR msg, unsigned int * tsize);
NTSTATUS usbSendMsgAsync(urb_itm_t * urb, WDFUSBPIPE pipe, WDFREQUEST Request, WDFMEMORY Memory, WDFMEMORY_OFFSET * Offset);

#endif

struct IndirectDeviceContextWrapper {
    IndirectDeviceContext* pContext;
    //usb disp
    WDFUSBDEVICE                    UsbDevice;

    WDFUSBINTERFACE                 UsbInterface;

    WDFUSBPIPE                      BulkReadPipe;

    WDFUSBPIPE                      BulkWritePipe;
	WDFUSBPIPE                      BulkPLPipe;
	ULONG							UsbDeviceTraits;
	USB_DEVICE_DESCRIPTOR			UsbDeviceDescriptor;
	//

    void Cleanup() {
        delete pContext;
        pContext = nullptr;
    }
};

// This macro creates the methods for accessing an IndirectDeviceContextWrapper as a context for a WDF object
WDF_DECLARE_CONTEXT_TYPE(IndirectDeviceContextWrapper);

extern "C" BOOL WINAPI DllMain(
    _In_ HINSTANCE hInstance,
    _In_ UINT dwReason,
    _In_opt_ LPVOID lpReserved)
{
    UNREFERENCED_PARAMETER(hInstance);
    UNREFERENCED_PARAMETER(lpReserved);
    UNREFERENCED_PARAMETER(dwReason);

    return TRUE;
}

static void InitPreferedModeTable()
{
	// 1. 默认全部填 10
	std::fill(std::begin(BeadaPanelScreenPreferedMode),
		std::end(BeadaPanelScreenPreferedMode),
		10);

	// 2. 覆盖特殊值（只写真正有意义的 os_version）
	BeadaPanelScreenPreferedMode[0] = 10;
	BeadaPanelScreenPreferedMode[1] = 10;
	BeadaPanelScreenPreferedMode[2] = 6;
	BeadaPanelScreenPreferedMode[3] = 12;
	BeadaPanelScreenPreferedMode[4] = 9;
	BeadaPanelScreenPreferedMode[13] = 12;
	BeadaPanelScreenPreferedMode[14] = 9;
	BeadaPanelScreenPreferedMode[15] = 6;
	BeadaPanelScreenPreferedMode[16] = 6;
	BeadaPanelScreenPreferedMode[17] = 11;
	BeadaPanelScreenPreferedMode[18] = 11;
	BeadaPanelScreenPreferedMode[19] = 7;
	BeadaPanelScreenPreferedMode[20] = 8;
	BeadaPanelScreenPreferedMode[21] = 0;
	BeadaPanelScreenPreferedMode[22] = 2;
	BeadaPanelScreenPreferedMode[23] = 4;
	BeadaPanelScreenPreferedMode[24] = 0;
	BeadaPanelScreenPreferedMode[25] = 2;
	BeadaPanelScreenPreferedMode[26] = 4;

	// 最后一个特殊值
	BeadaPanelScreenPreferedMode[255] = 8;
}

_Use_decl_annotations_
extern "C" NTSTATUS DriverEntry(
    PDRIVER_OBJECT  pDriverObject,
    PUNICODE_STRING pRegistryPath
)
{
    WDF_DRIVER_CONFIG Config;
    NTSTATUS Status;

    WDF_OBJECT_ATTRIBUTES Attributes;

    //
    // Initialize WPP Tracing
    //
#if UMDF_VERSION_MAJOR == 2 && UMDF_VERSION_MINOR == 0
    WPP_INIT_TRACING(MYDRIVER_TRACING_ID);
#else
    WPP_INIT_TRACING(pDriverObject, pRegistryPath);
#endif

    WDF_OBJECT_ATTRIBUTES_INIT(&Attributes);

	TraceEvents(TRACE_LEVEL_WARNING, TRACE_DRIVER, "idd Driver BeadaPanel\n");

	InitPreferedModeTable();

    WDF_DRIVER_CONFIG_INIT(&Config,
                           IddSampleDeviceAdd
                          );

    Status = WdfDriverCreate(pDriverObject, pRegistryPath, &Attributes, &Config, WDF_NO_HANDLE);
    if(!NT_SUCCESS(Status)) {
#if UMDF_VERSION_MAJOR == 2 && UMDF_VERSION_MINOR == 0
        WPP_CLEANUP();
#else
        WPP_CLEANUP(pDriverObject);
#endif
        return Status;
    }

    return Status;
}

#if 1
_IRQL_requires_(PASSIVE_LEVEL)
NTSTATUS
SelectInterfaces(
    _In_ WDFDEVICE Device
)
/*++

Routine Description:

    This helper routine selects the configuration, interface and
    creates a context for every pipe (end point) in that interface.

Arguments:

    Device - Handle to a framework device

Return Value:

    NT status value

--*/
{
    WDF_USB_DEVICE_SELECT_CONFIG_PARAMS configParams;
    NTSTATUS                            status = STATUS_SUCCESS;
    //PDEVICE_CONTEXT                     pDeviceContext;
    auto* pDeviceContext = WdfObjectGet_IndirectDeviceContextWrapper(Device);
    WDFUSBPIPE                          pipe;
    WDF_USB_PIPE_INFORMATION            pipeInfo;
    UCHAR                               numberConfiguredPipes;
    WDFUSBINTERFACE                     usbInterface;

    PAGED_CODE();

    //pDeviceContext = GetDeviceContext(Device);

    WDF_USB_DEVICE_SELECT_CONFIG_PARAMS_INIT_SINGLE_INTERFACE(&configParams);

    usbInterface =
        WdfUsbTargetDeviceGetInterface(pDeviceContext->UsbDevice, 0);

    if(NULL == usbInterface) {
		TraceEvents(TRACE_LEVEL_ERROR, DBG_USB, "WdfUsbTargetDeviceGetInterface() return NULL\n");
		status = STATUS_UNSUCCESSFUL;
        return status;
    }

    configParams.Types.SingleInterface.ConfiguredUsbInterface =
        usbInterface;

    configParams.Types.SingleInterface.NumberConfiguredPipes =
        WdfUsbInterfaceGetNumConfiguredPipes(usbInterface);

    pDeviceContext->UsbInterface =
        configParams.Types.SingleInterface.ConfiguredUsbInterface;

    numberConfiguredPipes = configParams.Types.SingleInterface.NumberConfiguredPipes;

    //
    // Get pipe handle 1
    //
	WDF_USB_PIPE_INFORMATION_INIT(&pipeInfo);
	pipe = WdfUsbInterfaceGetConfiguredPipe(
		pDeviceContext->UsbInterface,
		1, //PipeIndex,
		&pipeInfo
	);

	if (WdfUsbPipeTypeBulk == pipeInfo.PipeType &&
		WdfUsbTargetPipeIsOutEndpoint(pipe)) {
		TraceEvents(TRACE_LEVEL_WARNING, DBG_USB, "BulkPLPipe Pipe is 0x%p\n", pipe);
		pDeviceContext->BulkPLPipe = pipe;

	}
	else {
		status = STATUS_INVALID_DEVICE_STATE;
		TraceEvents(TRACE_LEVEL_ERROR, DBG_USB, "Device is not configured properly, pipe 1 %p\n",
			pipe);

		return status;
	}

	//
	// Get pipe handle 2
	//
	WDF_USB_PIPE_INFORMATION_INIT(&pipeInfo);
	pipe = WdfUsbInterfaceGetConfiguredPipe(
		pDeviceContext->UsbInterface,
		2, //PipeIndex,
		&pipeInfo
	);
	if (WdfUsbPipeTypeBulk == pipeInfo.PipeType &&
		WdfUsbTargetPipeIsInEndpoint(pipe)) {
		TraceEvents(TRACE_LEVEL_WARNING, DBG_USB, "BulkInput Pipe is 0x%p\n", pipe);
		pDeviceContext->BulkReadPipe = pipe;

		//
		// Tell the framework that it's okay to read less than
		// MaximumPacketSize
		//
		WdfUsbTargetPipeSetNoMaximumPacketSizeCheck(pipe);
	}
	else {
		status = STATUS_INVALID_DEVICE_STATE;
		TraceEvents(TRACE_LEVEL_ERROR, DBG_USB, "Device is not configured properly, pipe 2 %p\n",
			pipe);

		return status;
	}

	//
	// Get pipe handle 3
	//
	WDF_USB_PIPE_INFORMATION_INIT(&pipeInfo);
	pipe = WdfUsbInterfaceGetConfiguredPipe(
		pDeviceContext->UsbInterface,
		3, //PipeIndex,
		&pipeInfo
	);

	if (WdfUsbPipeTypeBulk == pipeInfo.PipeType &&
		WdfUsbTargetPipeIsOutEndpoint(pipe)) {
		TraceEvents(TRACE_LEVEL_WARNING, DBG_USB, "BulkOutput Pipe is 0x%p\n", pipe);
		pDeviceContext->BulkWritePipe = pipe;
	}
	else {
		status = STATUS_INVALID_DEVICE_STATE;
		TraceEvents(TRACE_LEVEL_ERROR, DBG_USB, "Device is not configured properly, pipe 3 %p\n",
			pipe);

		return status;
	}

    return STATUS_SUCCESS;
}

int resetPanelPL(WDFDEVICE device)
{
	int ret;
	unsigned int len = MIN_Buffer_Size;
	unsigned char temp[MIN_Buffer_Size];
	auto* pContext = WdfObjectGet_IndirectDeviceContextWrapper(device);

	// send statuslink command
	ret = packageResetPLSL(temp, &len);
	if (!ret) {
		HexDump(temp, len, temp);
		ret = usbSendMsg(pContext->BulkWritePipe, temp, &len);

		// get statuslink response
		if (!ret) {
			;
		}
		else {
			TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "usbSendMsg() return:%x len=%d\n", ret, len);
		}
	}
	else {
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "packageResetPLSL() return:%d\n", ret);
	}

	return ret;
}

int resetPanelAN(WDFDEVICE device)
{
	int ret;
	unsigned int len = MIN_Buffer_Size;
	unsigned char temp[MIN_Buffer_Size];
	auto* pContext = WdfObjectGet_IndirectDeviceContextWrapper(device);

	// send statuslink command
	ret = packageResetANSL(temp, &len);
	if (!ret) {
		HexDump(temp, len, temp);
		ret = usbSendMsg(pContext->BulkWritePipe, temp, &len);

		// get statuslink response
		if (!ret) {
			;
		}
		else {
			TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "usbSendMsg() return:%x len=%d\n", ret, len);
		}
	}
	else {
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "packageResetANSL() return:%d\n", ret);
	}

	return ret;
}

int pushPanel(WDFDEVICE device)
{
	int ret;
	unsigned int len = MIN_Buffer_Size;
	unsigned char temp[MIN_Buffer_Size];
	auto* pContext = WdfObjectGet_IndirectDeviceContextWrapper(device);

	// send statuslink command
	ret = packagePushSL(temp, &len);
	if (!ret) {
		HexDump(temp, len, temp);
		ret = usbSendMsg(pContext->BulkWritePipe, temp, &len);

		// get statuslink response
		if (!ret) {
			;
		}
		else {
			TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "usbSendMsg() return:%x len=%d\n", ret, len);
		}
	}
	else {
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "packagePushSL() return:%d\n", ret);
	}

	return ret;
}

int getPanelInfo(WDFDEVICE device, STATUSLINK_INFO *info)
{
	int ret;
	unsigned int len = MIN_Buffer_Size;
	unsigned char temp[MIN_Buffer_Size];
	auto* pContext = WdfObjectGet_IndirectDeviceContextWrapper(device);

	// send statuslink command
	ret = packageGetInfoSL(temp, &len);
	if (!ret) {
		HexDump(temp, len, temp);

		ret = usbSendMsg(pContext->BulkWritePipe, temp, &len);

		// get statuslink response
		if (!ret) {
			len += sizeof(STATUSLINK_INFO);
			ret = usbReadMsg(pContext->BulkReadPipe, temp, &len);

			if (!ret) {
				HexDump(temp, len, temp);

				ret = depackGetInfoSL(temp, len, info);
			}
			else {
				TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "usbReadMsg() return:%x len=%d\n", ret, len);
			}
		}
		else {
			TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "usbSendMsg() return:%x len=%d\n", ret, len);
		}
	}
	else {
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "packageGetInfoSL() return:%d\n", ret);
	}

	return ret;
}

int getPanelTime(WDFDEVICE device, SYSTEMTIME *time)
{
	int ret;
	unsigned int len = MIN_Buffer_Size;
	unsigned char temp[MIN_Buffer_Size];
	auto* pContext = WdfObjectGet_IndirectDeviceContextWrapper(device);

	// send statuslink command
	ret = packageGetTimeSL(temp, &len);
	if (!ret) {
		HexDump(temp, len, temp);

		ret = usbSendMsg(pContext->BulkWritePipe, temp, &len);

		// get statuslink response
		if (!ret) {
			len += sizeof(SYSTEMTIME);
			ret = usbReadMsg(pContext->BulkReadPipe, temp, &len);

			if (!ret) {
				HexDump(temp, len, temp);

				ret = depackGetTimeSL(temp, len, time);
			}
			else {
				TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "usbReadMsg() return:%x\n", ret);
			}
		}
		else {
			TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "usbSendMsg() return:%x len=%d\n", ret, len);
		}
	}
	else {
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "packageGetTimeSL() return:%d\n", ret);
	}

	return ret;
}

int setPanelTime(WDFDEVICE device, SYSTEMTIME * time)
{
	int ret;
	unsigned int len = MIN_Buffer_Size;
	unsigned char temp[MIN_Buffer_Size];

	auto* pContext = WdfObjectGet_IndirectDeviceContextWrapper(device);

	ret = packageSetTimeSL(temp, &len, time);
	if (!ret) {
		HexDump(temp, len, temp);

		ret = usbSendMsg(pContext->BulkWritePipe, temp, &len);
	}
	else {
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "packageSetTimeSL() return:%d len=%d\n", ret, len);
	}

	return ret;
}

int setPanelBL(WDFDEVICE device, unsigned char value)
{
	int ret;
	unsigned int len = MIN_Buffer_Size;
	unsigned char temp[MIN_Buffer_Size];
	auto* pContext = WdfObjectGet_IndirectDeviceContextWrapper(device);

	ret = packageSetBLSL(temp, &len, value);
	if (!ret) {
		HexDump(temp, len, temp);

		ret = usbSendMsg(pContext->BulkWritePipe, temp, &len);
	}
	else {
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "packageSetBLSL() return:%d len=%d\n", ret, len);
	}


	return ret;
}

#endif

NTSTATUS
idd_usbdisp_evt_device_prepareHardware(
    WDFDEVICE Device,
    WDFCMRESLIST ResourceList,
    WDFCMRESLIST ResourceListTranslated
)
/*++

Routine Description:

    In this callback, the driver does whatever is necessary to make the
    hardware ready to use.  In the case of a USB device, this involves
    reading and selecting descriptors.

Arguments:

    Device - handle to a device

    ResourceList - handle to a resource-list object that identifies the
                   raw hardware resources that the PnP manager assigned
                   to the device

    ResourceListTranslated - handle to a resource-list object that
                             identifies the translated hardware resources
                             that the PnP manager assigned to the device

Return Value:

    NT status value

--*/
{
    NTSTATUS                            status;
 
    auto* pDeviceContext = WdfObjectGet_IndirectDeviceContextWrapper(Device);
    WDF_USB_DEVICE_INFORMATION          deviceInfo;
    ULONG                               waitWakeEnable;
	SYSTEMTIME							time;
	ULONG  length, valueType, value;
	DECLARE_CONST_UNICODE_STRING(valueName, L"LCDBrightness");
	WDFKEY  hKey;

    UNREFERENCED_PARAMETER(ResourceList);
    UNREFERENCED_PARAMETER(ResourceListTranslated);
    waitWakeEnable = FALSE;
    PAGED_CODE();

	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "--> EvtDevicePrepareHardware\n");

    //
    // Create a USB device handle so that we can communicate with the
    // underlying USB stack. The WDFUSBDEVICE handle is used to query,
    // configure, and manage all aspects of the USB device.
    // These aspects include device properties, bus properties,
    // and I/O creation and synchronization. We only create device the first
    // the PrepareHardware is called. If the device is restarted by pnp manager
    // for resource rebalance, we will use the same device handle but then select
    // the interfaces again because the USB stack could reconfigure the device on
    // restart.
    //
    if(pDeviceContext->UsbDevice == NULL) {

#if UMDF_VERSION_MINOR >= 25
        WDF_USB_DEVICE_CREATE_CONFIG createParams;

        WDF_USB_DEVICE_CREATE_CONFIG_INIT(&createParams,
                                          USBD_CLIENT_CONTRACT_VERSION_602);

        status = WdfUsbTargetDeviceCreateWithParameters(
                     Device,
                     &createParams,
                     WDF_NO_OBJECT_ATTRIBUTES,
                     &pDeviceContext->UsbDevice);

#else
        status = WdfUsbTargetDeviceCreate(Device,
                                          WDF_NO_OBJECT_ATTRIBUTES,
                                          &pDeviceContext->UsbDevice);


#endif

        if(!NT_SUCCESS(status)) {
			TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "WdfUsbTargetDeviceCreate() failed with Status code %x\n", status);
            return status;
        }
    }

    //
    // Retrieve USBD version information, port driver capabilites and device
    // capabilites such as speed, power, etc.
    //
    WDF_USB_DEVICE_INFORMATION_INIT(&deviceInfo);

    status = WdfUsbTargetDeviceRetrieveInformation(
                 pDeviceContext->UsbDevice,
                 &deviceInfo);
    if(NT_SUCCESS(status)) {

        waitWakeEnable = deviceInfo.Traits &
                         WDF_USB_DEVICE_TRAIT_REMOTE_WAKE_CAPABLE;

        //
        // Save these for use later.
        //
        pDeviceContext->UsbDeviceTraits = deviceInfo.Traits;
    } else  {
        pDeviceContext->UsbDeviceTraits = 0;
    }

	WdfUsbTargetDeviceGetDeviceDescriptor(
		pDeviceContext->UsbDevice,
		&pDeviceContext->UsbDeviceDescriptor);

#if 1
    status = SelectInterfaces(Device);
    if(!NT_SUCCESS(status)) {
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "SelectInterfaces() failed 0x%x\n", status);
        return status;
    }
#endif

	// get panel info, to determin prefered display mode
	status = getPanelInfo(Device, &pDeviceContext->pContext->info);
	if (status != 0) {
		return status;
	}

	if (pDeviceContext->pContext->info.firmware_version < 700) {
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "firmware_version:%d\n", pDeviceContext->pContext->info.firmware_version);
		return STATUS_INCOMPATIBLE_DRIVER_BLOCKED;
	}

	m_prefered_mode = BeadaPanelScreenPreferedMode[pDeviceContext->pContext->info.os_version];
	TraceEvents(TRACE_LEVEL_WARNING, TRACE_DEVICE, "m_prefered_mode:%d\n", m_prefered_mode);

	// set system time
	GetLocalTime(&time);
	setPanelTime(Device, &time);

	// get default brightness from system registry
	status = WdfDriverOpenParametersRegistryKey(WdfGetDriver(),
		GENERIC_READ,
		WDF_NO_OBJECT_ATTRIBUTES,
		&hKey
	);

	if (NT_SUCCESS(status)) {

		status = WdfRegistryQueryULong(hKey, &valueName, &value);

		if (NT_SUCCESS(status)) {
			// Vendor: can cache and use this values.
			m_brightness = value;
		}
		else {
			TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "WdfRegistryQueryULong() return:%x\n", status);

			m_brightness = 100;
			status = STATUS_SUCCESS;
		}

		WdfRegistryClose(hKey);
	}
	else {
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "WdfDriverOpenParametersRegistryKey() return:%x\n", status);

	}

	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "<-- EvtDevicePrepareHardware\n");

    return status;
}

#if 1

_Use_decl_annotations_
NTSTATUS IddSampleDeviceAdd(WDFDRIVER Driver, PWDFDEVICE_INIT pDeviceInit)
{

    NTSTATUS Status = STATUS_SUCCESS;
    WDF_PNPPOWER_EVENT_CALLBACKS PnpPowerCallbacks;

    UNREFERENCED_PARAMETER(Driver);

	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "-->IddSampleDeviceAdd()\n");

    // Register for power callbacks - in this sample only power-on is needed
    WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&PnpPowerCallbacks);
    PnpPowerCallbacks.EvtDeviceD0Entry = IddSampleDeviceD0Entry;
    PnpPowerCallbacks.EvtDevicePrepareHardware = idd_usbdisp_evt_device_prepareHardware;

    //
    // These two callbacks start and stop the wdfusb pipe continuous reader
    // as we go in and out of the D0-working state.
    //

    //PnpPowerCallbacks.EvtDeviceD0Entry = idd_usbdisp_evt_device_D0Entry;
    PnpPowerCallbacks.EvtDeviceD0Exit  = IddSampleDeviceD0Exit;
//   PnpPowerCallbacks.EvtDeviceSelfManagedIoFlush = idd_usbdisp_evt_device_SelfManagedIoFlush;
    WdfDeviceInitSetPnpPowerEventCallbacks(pDeviceInit, &PnpPowerCallbacks);
#if 1
    IDD_CX_CLIENT_CONFIG IddConfig;
    IDD_CX_CLIENT_CONFIG_INIT(&IddConfig);

    // If the driver wishes to handle custom IoDeviceControl requests, it's necessary to use this callback since IddCx
    // redirects IoDeviceControl requests to an internal queue. This sample does not need this.
    // IddConfig.EvtIddCxDeviceIoControl = IddSampleIoDeviceControl;

    IddConfig.EvtIddCxAdapterInitFinished = IddSampleAdapterInitFinished;

    IddConfig.EvtIddCxParseMonitorDescription = IddSampleParseMonitorDescription;
    IddConfig.EvtIddCxMonitorGetDefaultDescriptionModes = IddSampleMonitorGetDefaultModes;
    IddConfig.EvtIddCxMonitorQueryTargetModes = IddSampleMonitorQueryModes;
    IddConfig.EvtIddCxAdapterCommitModes = IddSampleAdapterCommitModes;
    IddConfig.EvtIddCxMonitorAssignSwapChain = IddSampleMonitorAssignSwapChain;
    IddConfig.EvtIddCxMonitorUnassignSwapChain = IddSampleMonitorUnassignSwapChain;

    Status = IddCxDeviceInitConfig(pDeviceInit, &IddConfig);
    if(!NT_SUCCESS(Status)) {
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "IddCxDeviceInitConfig() failed with Status code %x\n", Status);
		return Status;
    }
#endif
    WDF_OBJECT_ATTRIBUTES Attr;
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&Attr, IndirectDeviceContextWrapper);
#if 1
    Attr.EvtCleanupCallback = [](WDFOBJECT Object) {
        // Automatically cleanup the context when the WDF object is about to be deleted
        auto* pContext = WdfObjectGet_IndirectDeviceContextWrapper(Object);
        if(pContext) {
            pContext->Cleanup();
        }
    };
#endif
    WDFDEVICE Device = nullptr;
    Status = WdfDeviceCreate(&pDeviceInit, &Attr, &Device);
    if(!NT_SUCCESS(Status)) {
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "WdfDeviceCreate failed with Status code %x\n", Status);
        return Status;
    }
#if 1
    Status = IddCxDeviceInitialize(Device);

    // Create a new device context object and attach it to the WDF device object
    auto* pContext = WdfObjectGet_IndirectDeviceContextWrapper(Device);
    pContext->pContext = new IndirectDeviceContext(Device);
#endif
	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "<--IddSampleDeviceAdd()\n");

	return Status;
}
#endif

_Use_decl_annotations_
NTSTATUS IddSampleDeviceD0Entry(WDFDEVICE Device, WDF_POWER_DEVICE_STATE PreviousState)
{
    UNREFERENCED_PARAMETER(PreviousState);

	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "--> IddSampleDeviceD0Entry\n");

    // This function is called by WDF to start the device in the fully-on power state.

    auto* pContext = WdfObjectGet_IndirectDeviceContextWrapper(Device);
    pContext->pContext->InitAdapter();

	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "<-- IddSampleDeviceD0Entry\n");

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS IddSampleDeviceD0Exit(WDFDEVICE Device, WDF_POWER_DEVICE_STATE PreviousState)
{
	UNREFERENCED_PARAMETER(PreviousState);

	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "--> IddSampleDeviceD0Exit\n");

	// This function is called by WDF to start the device in the fully-on power state.

	auto* pContext = WdfObjectGet_IndirectDeviceContextWrapper(Device);
	pContext->pContext->m_fromSleepMode = 1;

	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "<-- IddSampleDeviceD0Exit\n");

	return STATUS_SUCCESS;
}

#pragma region Direct3DDevice

Direct3DDevice::Direct3DDevice(LUID AdapterLuid) : AdapterLuid(AdapterLuid)
{

}

Direct3DDevice::Direct3DDevice()
{

}

HRESULT Direct3DDevice::Init()
{
    // The DXGI factory could be cached, but if a new render adapter appears on the system, a new factory needs to be
    // created. If caching is desired, check DxgiFactory->IsCurrent() each time and recreate the factory if !IsCurrent.
    HRESULT hr = CreateDXGIFactory2(0, IID_PPV_ARGS(&DxgiFactory));
    if(FAILED(hr)) {
        return hr;
    }

    // Find the specified render adapter
    hr = DxgiFactory->EnumAdapterByLuid(AdapterLuid, IID_PPV_ARGS(&Adapter));
    if(FAILED(hr)) {
        return hr;
    }

    // Create a D3D device using the render adapter. BGRA support is required by the WHQL test suite.
    hr = D3D11CreateDevice(Adapter.Get(), D3D_DRIVER_TYPE_UNKNOWN, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr, 0, D3D11_SDK_VERSION, &Device, nullptr, &DeviceContext);
    if(FAILED(hr)) {
        // If creating the D3D device failed, it's possible the render GPU was lost (e.g. detachable GPU) or else the
        // system is in a transient state.
        return hr;
    }

    return S_OK;
}

#pragma endregion

#pragma region SwapChainProcessor

SwapChainProcessor::SwapChainProcessor(IDDCX_SWAPCHAIN hSwapChain, shared_ptr<Direct3DDevice> Device, WDFDEVICE  WdfDevice, HANDLE NewFrameEvent)
    : m_hSwapChain(hSwapChain), m_Device(Device), mp_WdfDevice(WdfDevice), m_hAvailableBufferEvent(NewFrameEvent)
{
    m_hTerminateEvent.Attach(CreateEvent(nullptr, FALSE, FALSE, nullptr));

    // Immediately create and run the swap-chain processing thread, passing 'this' as the thread parameter
    m_hThread.Attach(CreateThread(nullptr, 0, RunThread, this, 0, nullptr));
}

SwapChainProcessor::~SwapChainProcessor()
{
    // Alert the swap-chain processing thread to terminate
    SetEvent(m_hTerminateEvent.Get());

    if(m_hThread.Get()) {
        // Wait for the thread to terminate
        WaitForSingleObject(m_hThread.Get(), INFINITE);
    }
}

DWORD CALLBACK SwapChainProcessor::RunThread(LPVOID Argument)
{
    reinterpret_cast<SwapChainProcessor*>(Argument)->Run();
    return 0;
}

void SwapChainProcessor::Run()
{
    // For improved performance, make use of the Multimedia Class Scheduler Service, which will intelligently
    // prioritize this thread for improved throughput in high CPU-load scenarios.
    DWORD AvTask = 0;
    NTSTATUS status;
    int i = 0;
    purb_itm_t purb;
    HANDLE AvTaskHandle = AvSetMmThreadCharacteristicsW(L"Distribution", &AvTask);
    InitializeSListHead(&urb_list);

	TraceEvents(TRACE_LEVEL_INFORMATION, DBG_SWAPCHAIN, "init urb list\n");

    // Insert into the list.
#define MAX_URB_SIZE 2
    for(i = 1; i <= MAX_URB_SIZE; i++) {
        purb = (urb_itm_t *)_aligned_malloc(sizeof(urb_itm_t),
                                            MEMORY_ALLOCATION_ALIGNMENT);
        if(NULL == purb) {
			TraceEvents(TRACE_LEVEL_ERROR, DBG_SWAPCHAIN, "Memory allocation failed.\n");
            break;
        }
        status = WdfRequestCreate(
                     WDF_NO_OBJECT_ATTRIBUTES,
                     NULL,
                     &purb->Request
                 );

        if(!NT_SUCCESS(status)) {
			TraceEvents(TRACE_LEVEL_ERROR, DBG_SWAPCHAIN, "create request NG\n");
            break;//return status;
        }

        status = WdfMemoryCreate(
            WDF_NO_OBJECT_ATTRIBUTES,
            NonPagedPool,
            0,
            WDF_MEMORY_MAX_SIZE,
            &purb->Memory,
            NULL
        );

        if (!NT_SUCCESS(status)) {
			TraceEvents(TRACE_LEVEL_ERROR, DBG_SWAPCHAIN, "WdfMemoryCreate NG\n");
            break;// return status;
        }

        purb->id = i;
        purb->urb_list = &urb_list;
        InterlockedPushEntrySList(&urb_list,
                                  &(purb->node));
        curr_urb = purb;
    }


    RunCore();

    // Always delete the swap-chain object when swap-chain processing loop terminates in order to kick the system to
    // provide a new swap-chain if necessary.
    WdfObjectDelete((WDFOBJECT)m_hSwapChain);
    for(i = 1; i <= MAX_URB_SIZE; i++) {

        PSLIST_ENTRY 	pentry = InterlockedPopEntrySList(&urb_list);

        if(NULL == pentry) {
			TraceEvents(TRACE_LEVEL_ERROR, DBG_SWAPCHAIN, "List is empty.\n");
            break;
        }

        purb = (urb_itm_t *)pentry;
		TraceEvents(TRACE_LEVEL_INFORMATION, DBG_SWAPCHAIN, "id is %d\n", purb->id);

        // This example assumes that the SLIST_ENTRY structure is the
        // first member of the structure. If your structure does not
        // follow this convention, you must compute the starting address
        // of the structure before calling the free function.
        WdfObjectDelete(purb->Request);
        _aligned_free(pentry);

    }

    m_hSwapChain = nullptr;

    AvRevertMmThreadCharacteristics(AvTaskHandle);
}



long get_os_us(void)
{

    SYSTEMTIME time;

    GetSystemTime(&time);
    long time_ms = (time.wSecond * 1000) + time.wMilliseconds;

    return time_ms * 1000;



}


long get_system_us(void)
{
    return get_os_us();
}



long SwapChainProcessor::get_fps(void)
{
    fps_mgr_t * mgr = &fps_mgr;
    if(mgr->cur < FPS_STAT_MAX)//we ignore first loop and also ignore rollback case due to a long period
        return mgr->last_fps;//if <0 ,please ignore it
    else {
        long a = mgr->tb[(mgr->cur-1)%FPS_STAT_MAX];
        long b = mgr->tb[(mgr->cur)%FPS_STAT_MAX];
        long fps = (a - b) / (FPS_STAT_MAX - 1);
        fps = (1000000 * 10) / fps; //x10 for int
        mgr->last_fps = fps;
        return fps;
    }
}

void SwapChainProcessor::put_fps_data(long t)
{
    static int init = 0;
    fps_mgr_t * mgr = &fps_mgr;

    if(0 == init) {
        mgr->cur = 0;
        mgr->last_fps = -1;
        init = 1;
    }

    mgr->tb[mgr->cur%FPS_STAT_MAX] = t;
    mgr->cur++;//cur ptr to next

}

#if 1

VOID EvtRequestWriteCompletionRoutine(
    _In_ WDFREQUEST                  Request,
    _In_ WDFIOTARGET                 Target,
    _In_ PWDF_REQUEST_COMPLETION_PARAMS CompletionParams,
    _In_ WDFCONTEXT                  Context
)
/*++

Routine Description:

    This is the completion routine for writes
    If the irp completes with success, we check if we
    need to recirculate this irp for another stage of
    transfer.

Arguments:

    Context - Driver supplied context
    Device - Device handle
    Request - Request handle
    Params - request completion params

Return Value:
    None

--*/
{
    NTSTATUS    status;
    size_t      bytesWritten = 0;
    PWDF_USB_REQUEST_COMPLETION_PARAMS usbCompletionParams;

    UNREFERENCED_PARAMETER(Target);
	UNREFERENCED_PARAMETER(Request);

	urb_itm_t * urb = (urb_itm_t *)Context;
    // UNREFERENCED_PARAMETER(Context);

    status = CompletionParams->IoStatus.Status;

    //
    // For usb devices, we should look at the Usb.Completion param.
    //
    usbCompletionParams = CompletionParams->Parameters.Usb.Completion;

    bytesWritten =  usbCompletionParams->Parameters.PipeWrite.Length;

    if(NT_SUCCESS(status)) {
        ;

    } else {
		TraceEvents(TRACE_LEVEL_ERROR, DBG_USB, "Write failed: request Status 0x%x UsbdStatus 0x%x\n",
            status, usbCompletionParams->UsbdStatus);
    }




	TraceEvents(TRACE_LEVEL_INFORMATION, DBG_USB, "pipe:%p cpl urb id:%d bytesWritten:%d\n", urb->pipe, urb->id, bytesWritten);
    InterlockedPushEntrySList(urb->urb_list,
                              &(urb->node));

    return;
}

NTSTATUS usbSendMsg(WDFUSBPIPE pipeHandle, PUCHAR msg, unsigned int * tsize)
{
	ULONG  writeSize;
	NTSTATUS status;
	WDF_MEMORY_DESCRIPTOR  writeBufDesc;
	WDF_REQUEST_SEND_OPTIONS  syncReqOptions;

	WDF_REQUEST_SEND_OPTIONS_INIT(
		&syncReqOptions,
		0
	);
	WDF_REQUEST_SEND_OPTIONS_SET_TIMEOUT(
		&syncReqOptions,
		WDF_REL_TIMEOUT_IN_SEC(1)
	);

	writeSize = *tsize;


	WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(
		&writeBufDesc,
		msg,
		writeSize
	);

	status = WdfUsbTargetPipeWriteSynchronously(
		pipeHandle,
		NULL,
		&syncReqOptions,
		&writeBufDesc,
		&writeSize
	);

//	TraceEvents(TRACE_LEVEL_VERBOSE, DBG_USB, "WdfUsbTargetPipeWriteSynchronously() return: %x, size:%d\n", status, writeSize);
	*tsize = writeSize;
	return status;

}

NTSTATUS usbReadMsg(WDFUSBPIPE pipeHandle, PUCHAR msg, unsigned int * tsize)
{
	NTSTATUS status;
	WDF_MEMORY_DESCRIPTOR  readBufDesc;
	ULONG readSize = *tsize;
	WDF_REQUEST_SEND_OPTIONS  syncReqOptions;

	WDF_REQUEST_SEND_OPTIONS_INIT(
		&syncReqOptions,
		0
	);
	WDF_REQUEST_SEND_OPTIONS_SET_TIMEOUT(
		&syncReqOptions,
		WDF_REL_TIMEOUT_IN_SEC(1)
	);

	WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(
		&readBufDesc,
		msg,
		readSize
	);

	status = WdfUsbTargetPipeReadSynchronously(
		pipeHandle,
		NULL,
		&syncReqOptions,
		&readBufDesc,
		&readSize
	);

	*tsize = readSize;
//	TraceEvents(TRACE_LEVEL_VERBOSE, DBG_USB, "WdfUsbTargetPipeReadSynchronously() return: %x,read size:%d\n", status, readSize);

	return status;
}

NTSTATUS usbSendMsgAsync(urb_itm_t * urb, WDFUSBPIPE pipe, WDFREQUEST Request, WDFMEMORY Memory, WDFMEMORY_OFFSET * Offset)
{
	NTSTATUS status;

	status = WdfUsbTargetPipeFormatRequestForWrite(
		pipe,
		Request,
		Memory,
		Offset // Offsets
	);
	if (!NT_SUCCESS(status)) {
		TraceEvents(TRACE_LEVEL_ERROR, DBG_USB, "WdfUsbTargetPipeFormatRequestForWrite NG\n");
		goto Exit;
	}

	//
	// Set a CompletionRoutine callback function.
	//
	urb->pipe = pipe;
	WdfRequestSetCompletionRoutine(
		Request,
		EvtRequestWriteCompletionRoutine,
		urb
	);
	//
	// Send the request. If an error occurs, complete the request.
	//
	if (WdfRequestSend(
		Request,
		WdfUsbTargetPipeGetIoTarget(pipe),
		WDF_NO_SEND_OPTIONS
	) == FALSE) {
		status = WdfRequestGetStatus(Request);
		TraceEvents(TRACE_LEVEL_ERROR, DBG_USB, "WdfRequestSend NG %x\n", status);
		goto Exit;
	}
Exit:
	return status;

}


#endif

typedef unsigned __int32 uint32_t;
typedef unsigned __int16 uint16_t;
typedef unsigned __int8 uint8_t;

void SwapChainProcessor::RunCore()
{
	int ret;
	unsigned int len;
	urb_itm_t * purb;
	char fmtstr[256] = {0};
	PSLIST_ENTRY 	pentry;
	WDFMEMORY_OFFSET offset;
	SYSTEMTIME time;
	auto* pContext = WdfObjectGet_IndirectDeviceContextWrapper(this->mp_WdfDevice);

	// Get the DXGI device interface
    ComPtr<IDXGIDevice> DxgiDevice;
    HRESULT hr = m_Device->Device.As(&DxgiDevice);
    if(FAILED(hr)) {
		TraceEvents(TRACE_LEVEL_ERROR, DBG_SWAPCHAIN, "m_Device->Device.As() return:%x\n", hr);
		return;
    }

    IDARG_IN_SWAPCHAINSETDEVICE SetDevice = {};
    SetDevice.pDevice = DxgiDevice.Get();

    hr = IddCxSwapChainSetDevice(m_hSwapChain, &SetDevice);
    if(FAILED(hr)) {
		TraceEvents(TRACE_LEVEL_ERROR, DBG_SWAPCHAIN, "IddCxSwapChainSetDevice() return:%x\n", hr);
		return;
    }

	lastWidth = lastHeight = 0;
	hNewDesktopImage = NULL;

	//issue reset command downstream to clear Panel-Link receive buffer
	resetPanelPL(mp_WdfDevice);
    
	// wait until last status link command get executed
	getPanelTime(mp_WdfDevice, &time);

	// set backlight level
	setPanelBL(mp_WdfDevice, m_brightness);

    // Acquire and release buffers in a loop
    for(;;) {
        ComPtr<IDXGIResource> AcquiredBuffer;

        // Ask for the next buffer from the producer
        IDARG_OUT_RELEASEANDACQUIREBUFFER Buffer = {};
        hr = IddCxSwapChainReleaseAndAcquireBuffer(m_hSwapChain, &Buffer);

        // AcquireBuffer immediately returns STATUS_PENDING if no buffer is yet available
        if(hr == E_PENDING) {
            // We must wait for a new buffer
            HANDLE WaitHandles [] = {
                m_hAvailableBufferEvent,
                m_hTerminateEvent.Get()
            };
            DWORD WaitResult = WaitForMultipleObjects(ARRAYSIZE(WaitHandles), WaitHandles, FALSE, 32);
            if(WaitResult == WAIT_OBJECT_0 || WaitResult == WAIT_TIMEOUT) {
				TraceEvents(TRACE_LEVEL_VERBOSE, DBG_SWAPCHAIN, "WaitForMultipleObjects() return\n");

				// We have a new buffer, so try the AcquireBuffer again
                continue;
            } else if(WaitResult == WAIT_OBJECT_0 + 1) {
                // We need to terminate
				TraceEvents(TRACE_LEVEL_WARNING, DBG_SWAPCHAIN, "We need to terminate\n");
				break;
            } else {
                // The wait was cancelled or something unexpected happened
				TraceEvents(TRACE_LEVEL_WARNING, DBG_SWAPCHAIN, "The wait was cancelled or something unexpected happened\n");
				hr = HRESULT_FROM_WIN32(WaitResult);
                break;
            }
        } else if(SUCCEEDED(hr)) {
			TraceEvents(TRACE_LEVEL_INFORMATION, DBG_SWAPCHAIN, "--IddCxSwapChainReleaseAndAcquireBuffer() return with a new buffer\n");

			AcquiredBuffer.Attach(Buffer.MetaData.pSurface);

            // ==============================
            // TODO: Process the frame here
            //
            // This is the most performance-critical section of code in an IddCx driver. It's important that whatever
            // is done with the acquired surface be finished as quickly as possible. This operation could be:
            //  * a GPU copy to another buffer surface for later processing (such as a staging surface for mapping to CPU memory)
            //  * a GPU encode operation
            //  * a GPU VPBlt to another surface
            //  * a GPU custom compute shader encode operation
            // ==============================
            //LOG("-dxgi fid:%d dirty:%d\n",Buffer.MetaData.PresentationFrameNumber,Buffer.MetaData.DirtyRectCount);

#define RESET_OBJECT(obj) { if(obj) obj->Release(); obj = NULL; }
            ID3D11Texture2D *hAcquiredDesktopImage = NULL;
            hr = AcquiredBuffer->QueryInterface(__uuidof(ID3D11Texture2D), reinterpret_cast<void **>(&hAcquiredDesktopImage));
            if(FAILED(hr)) {
				TraceEvents(TRACE_LEVEL_ERROR, DBG_SWAPCHAIN, "--dxgi 2d NG\n");
                goto next;
            }

            // copy old description
            //
            D3D11_TEXTURE2D_DESC frameDescriptor;
            hAcquiredDesktopImage->GetDesc(&frameDescriptor);

            //
            // create a new staging buffer for cpu accessable, or only GPU can access
            //
			if (hNewDesktopImage == nullptr || lastWidth != frameDescriptor.Width || lastHeight != frameDescriptor.Height)
			{
				RESET_OBJECT(hNewDesktopImage); // 释放旧纹理

				D3D11_TEXTURE2D_DESC stagingDesc = frameDescriptor; // 复制原始描述符
				stagingDesc.Usage = D3D11_USAGE_STAGING;
				stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
				stagingDesc.BindFlags = 0;
				stagingDesc.MiscFlags = 0;
				stagingDesc.MipLevels = 1;
				stagingDesc.ArraySize = 1;
				stagingDesc.SampleDesc.Count = 1;

				hr = m_Device->Device->CreateTexture2D(&stagingDesc, NULL, &hNewDesktopImage);
				if (FAILED(hr)) {
					RESET_OBJECT(hAcquiredDesktopImage);
					hAcquiredDesktopImage = NULL;
					TraceEvents(TRACE_LEVEL_ERROR, DBG_SWAPCHAIN, "--dxgi create 2d NG\n");
					goto next;
				}
				TraceEvents(TRACE_LEVEL_INFORMATION, DBG_SWAPCHAIN, "Staging Texture (re)created: %dx%d\n", frameDescriptor.Width, frameDescriptor.Height);

				//
				// create staging buffer for map bits
				//
				hr = hNewDesktopImage->QueryInterface(__uuidof(IDXGISurface), (void**)(&hStagingSurf));

				if (FAILED(hr)) {
					TraceEvents(TRACE_LEVEL_ERROR, DBG_SWAPCHAIN, "--dxgi surf NG\n");
					goto next1;
				}
			}

            //
            // copy next staging buffer to new staging buffer
            //
            m_Device->DeviceContext->CopyResource(hNewDesktopImage, hAcquiredDesktopImage);

            //
            // copy bits to user space
            //
            DXGI_MAPPED_RECT mappedRect;
            hr = hStagingSurf->Map(&mappedRect, DXGI_MAP_READ);
		    if (FAILED(hr)) {

			    TraceEvents(TRACE_LEVEL_ERROR, DBG_SWAPCHAIN, "--dxgi map NG %x\n", hr);

			    // 尝试恢复 surface
			    hr = hNewDesktopImage->QueryInterface(__uuidof(IDXGISurface), (void**)&hStagingSurf);
				if (FAILED(hr)) {
					TraceEvents(TRACE_LEVEL_ERROR, DBG_SWAPCHAIN, "--dxgi surf NG\n");
					goto next1;
				}

				hr = hStagingSurf->Map(&mappedRect, DXGI_MAP_READ);
		    }

            if(SUCCEEDED(hr)) {

				//copy graphic data
				if (mappedRect.Pitch != frameDescriptor.Width * 4) {
					//LOGI("%s %d %d %d\n", __func__, __LINE__, frameDescriptor.Width, frameDescriptor.Height);
					for (int i = 0; i < frameDescriptor.Height; i++) {
						memcpy(&preBuff[frameDescriptor.Width * 4 * i], &(mappedRect.pBits[mappedRect.Pitch * i]), frameDescriptor.Width * 4);
					}
				}
				else
					memcpy(preBuff, mappedRect.pBits, frameDescriptor.Width * frameDescriptor.Height * 4);

				hStagingSurf->Unmap();

				//encode and issue urb
				pentry = InterlockedPopEntrySList(&urb_list);
				purb = (urb_itm_t*)pentry;
				if (NULL != purb) {
					len = WDF_MEMORY_MAX_SIZE;

					// we will start with a new tag, if frame dimensions changed
					if ((lastWidth != frameDescriptor.Width) || (lastHeight != frameDescriptor.Height)) {
							TraceEvents(TRACE_LEVEL_INFORMATION, DBG_SWAPCHAIN, "--issue PL start command, height=%d, width=%d, id=%d\n", frameDescriptor.Height, frameDescriptor.Width, purb->id);

							lastWidth = frameDescriptor.Width;
							lastHeight = frameDescriptor.Height;
							sprintf(fmtstr, "image/x-raw, format=BGR16, height=%d, width=%d, framerate=0/1", frameDescriptor.Height, frameDescriptor.Width);
							ret = packageStartPL((unsigned char*)WdfMemoryGetBuffer(purb->Memory, NULL), &len, fmtstr);
							if (!ret) {
								HexDump((unsigned char*)WdfMemoryGetBuffer(purb->Memory, NULL), len, (unsigned char*)WdfMemoryGetBuffer(purb->Memory, NULL));

								offset.BufferOffset = 0;
								offset.BufferLength = len;
								usbSendMsgAsync(purb, pContext->BulkPLPipe, purb->Request, purb->Memory, &offset);

								pentry = InterlockedPopEntrySList(&urb_list);
								purb = (urb_itm_t*)pentry;
								if (NULL == purb) {
									TraceEvents(TRACE_LEVEL_ERROR, DBG_SWAPCHAIN, "--purb handle for PL start is invalid\n");
									goto next1;
								}
							}
							else {
								TraceEvents(TRACE_LEVEL_ERROR, DBG_SWAPCHAIN, "--packageStartPL() return %d\n", ret);
								goto next1;
							}
					}

					len = WDF_MEMORY_MAX_SIZE;
					ret = convertBGR16((unsigned char*)postBuff,
							&len,
							preBuff,
							frameDescriptor.Width*4,
							frameDescriptor.Width,
							frameDescriptor.Height);
					if (!ret) {
							memcpy(WdfMemoryGetBuffer(purb->Memory, NULL), postBuff, len);

							TraceEvents(TRACE_LEVEL_INFORMATION, DBG_SWAPCHAIN, "--send graphic data, pitch=%d, height=%d, width=%d, size=%d, id=%d\n", mappedRect.Pitch, frameDescriptor.Height, frameDescriptor.Width, len, purb->id);
							offset.BufferOffset = 0;
							offset.BufferLength = len;
							usbSendMsgAsync(purb, pContext->BulkPLPipe, purb->Request, purb->Memory, &offset);
					}
					else {
							TraceEvents(TRACE_LEVEL_ERROR, DBG_SWAPCHAIN, "--convertBGR16() return %d\n", ret);
					}

				}
				else {
						TraceEvents(TRACE_LEVEL_ERROR, DBG_SWAPCHAIN, "--purb handle is invalid\n");
				}
next1:
				TraceEvents(TRACE_LEVEL_VERBOSE, DBG_SWAPCHAIN, "--continue\n");

            } else {

				TraceEvents(TRACE_LEVEL_ERROR, DBG_SWAPCHAIN, "--dxgi map NG %x\n", hr);

				// 尝试恢复 surface
				hNewDesktopImage->QueryInterface(__uuidof(IDXGISurface), (void**)&hStagingSurf);
            }


            RESET_OBJECT(hAcquiredDesktopImage);
next:

            AcquiredBuffer.Reset();
			hr = IddCxSwapChainFinishedProcessingFrame(m_hSwapChain);
			if (FAILED(hr)) {
				TraceEvents(TRACE_LEVEL_ERROR, DBG_SWAPCHAIN, "--IddCxSwapChainFinishedProcessingFrame() failed %x\n", hr);
				break;
			}

            // ==============================
            // TODO: Report frame statistics once the asynchronous encode/send work is completed
            //
            // Drivers should report information about sub-frame timings, like encode time, send time, etc.
            // ==============================
            // IddCxSwapChainReportFrameStatistics(m_hSwapChain, ...);
        } else {
            // The swap-chain was likely abandoned (e.g. DXGI_ERROR_ACCESS_LOST), so exit the processing loop
			TraceEvents(TRACE_LEVEL_ERROR, DBG_SWAPCHAIN, "The swap-chain was likely abandoned\n");

			break;
        }
    }

	RESET_OBJECT(hNewDesktopImage);
	hNewDesktopImage = NULL;
	RESET_OBJECT(hStagingSurf);
	hStagingSurf = NULL;

	// reset panel to force screen idle mode, when windows enters screen saving
	resetPanelAN(mp_WdfDevice);
}

#pragma endregion

#pragma region IndirectDeviceContext

const UINT64 MHZ = 1000000;
const UINT64 KHZ = 1000;

DISPLAYCONFIG_VIDEO_SIGNAL_INFO dispinfo(UINT32 h, UINT32 v)
{
    const UINT32 clock_rate = 60 * (v + 4) * (v + 4) + 1000;
    return {
        clock_rate,                                      // pixel clock rate [Hz]
        { clock_rate, v + 4 },                         // fractional horizontal refresh rate [Hz]
        { clock_rate, (v + 4) *(v + 4) },           // fractional vertical refresh rate [Hz]
        { h, v },                                    // (horizontal, vertical) active pixel resolution
        { h + 4, v + 4 },                         // (horizontal, vertical) total pixel resolution
        { { 255, 0 }},                                   // video standard and vsync divider
        DISPLAYCONFIG_SCANLINE_ORDERING_PROGRESSIVE
    };
}
// A list of modes exposed by the sample monitor EDID - FOR SAMPLE PURPOSES ONLY
const DISPLAYCONFIG_VIDEO_SIGNAL_INFO IndirectDeviceContext::s_KnownMonitorModes[] = {
	dispinfo(480, 1920),
	dispinfo(1920, 480),
	dispinfo(462, 1920),
	dispinfo(1920, 462),
	dispinfo(440, 1920),
	dispinfo(1920, 440),
	dispinfo(480, 1280),
	dispinfo(400, 1280),
	dispinfo(480, 854),
	dispinfo(480, 800),
	dispinfo(800, 480),
	dispinfo(480, 480),
	dispinfo(320, 480)
};

// This is a sample monitor EDID - FOR SAMPLE PURPOSES ONLY
const BYTE IndirectDeviceContext::s_KnownMonitorEdid[] = {
    /*  0x00,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0x00,0x79,0x5E,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xA6,0x01,0x03,0x80,0x28,
      0x1E,0x78,0x0A,0xEE,0x91,0xA3,0x54,0x4C,0x99,0x26,0x0F,0x50,0x54,0x20,0x00,0x00,0x01,0x01,0x01,0x01,0x01,0x01,
      0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0xA0,0x0F,0x20,0x00,0x31,0x58,0x1C,0x20,0x28,0x80,0x14,0x00,
      0x90,0x2C,0x11,0x00,0x00,0x1E,0x00,0x00,0x00,0x10,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
      0x00,0x00,0x00,0x00,0x00,0x10,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
      0x00,0x10,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x6E */

    0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x31, 0xD8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x05, 0x16, 0x01, 0x03, 0x6D, 0x32, 0x1C, 0x78, 0xEA, 0x5E, 0xC0, 0xA4, 0x59, 0x4A, 0x98, 0x25,
    0x20, 0x50, 0x54, 0x00, 0x00, 0x00, 0xD1, 0xC0, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x02, 0x3A, 0x80, 0x18, 0x71, 0x38, 0x2D, 0x40, 0x58, 0x2C,
    0x45, 0x00, 0xF4, 0x19, 0x11, 0x00, 0x00, 0x1E, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x4C, 0x69, 0x6E,
    0x75, 0x78, 0x20, 0x23, 0x30, 0x0A, 0x20, 0x20, 0x20, 0x20, 0x00, 0x00, 0x00, 0xFD, 0x00, 0x3B,
    0x3D, 0x42, 0x44, 0x0F, 0x00, 0x0A, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x00, 0x00, 0x00, 0xFC,
    0x00, 0x4C, 0x69, 0x6E, 0x75, 0x78, 0x20, 0x46, 0x48, 0x44, 0x0A, 0x20, 0x20, 0x20, 0x00, 0x05

};

IndirectDeviceContext::IndirectDeviceContext(_In_ WDFDEVICE WdfDevice) :
    m_WdfDevice(WdfDevice)
{
	m_fromSleepMode = 0;
}

IndirectDeviceContext::~IndirectDeviceContext()
{
    m_ProcessingThread.reset();
}

#define NUM_VIRTUAL_DISPLAYS 1

void IndirectDeviceContext::InitAdapter()
{
	// we will not re-init adapter when enter from sleep mode
	if (m_fromSleepMode)
		return;

    // ==============================
    // TODO: Update the below diagnostic information in accordance with the target hardware. The strings and version
    // numbers are used for telemetry and may be displayed to the user in some situations.
    //
    // This is also where static per-adapter capabilities are determined.
    // ==============================
    IDDCX_ADAPTER_CAPS AdapterCaps = {};
    AdapterCaps.Size = sizeof(AdapterCaps);

    // Declare basic feature support for the adapter (required)
    AdapterCaps.MaxMonitorsSupported = NUM_VIRTUAL_DISPLAYS;
    AdapterCaps.EndPointDiagnostics.Size = sizeof(AdapterCaps.EndPointDiagnostics);
    AdapterCaps.EndPointDiagnostics.GammaSupport = IDDCX_FEATURE_IMPLEMENTATION_NONE;
    AdapterCaps.EndPointDiagnostics.TransmissionType = IDDCX_TRANSMISSION_TYPE_WIRED_OTHER;

    // Declare your device strings for telemetry (required)
	// We may take those strings from USB Device Descriptors in future
    AdapterCaps.EndPointDiagnostics.pEndPointFriendlyName = L"BeadaPanel";
    AdapterCaps.EndPointDiagnostics.pEndPointManufacturerName = L"NXElec";
    AdapterCaps.EndPointDiagnostics.pEndPointModelName = L"BeadaPanel";

    // Declare your hardware and firmware versions (required)
    IDDCX_ENDPOINT_VERSION Version1 = {};
    Version1.Size = sizeof(Version1);
    Version1.MajorVer = info.firmware_version;
    AdapterCaps.EndPointDiagnostics.pFirmwareVersion = &Version1;

	IDDCX_ENDPOINT_VERSION Version2 = {};
	Version2.Size = sizeof(Version2);
	Version2.MajorVer = info.hardware_platform;
	AdapterCaps.EndPointDiagnostics.pHardwareVersion = &Version2;

    // Initialize a WDF context that can store a pointer to the device context object
    WDF_OBJECT_ATTRIBUTES Attr;
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&Attr, IndirectDeviceContextWrapper);

    IDARG_IN_ADAPTER_INIT AdapterInit = {};
    AdapterInit.WdfDevice = m_WdfDevice;
    AdapterInit.pCaps = &AdapterCaps;
    AdapterInit.ObjectAttributes = &Attr;

    // Start the initialization of the adapter, which will trigger the AdapterFinishInit callback later
    IDARG_OUT_ADAPTER_INIT AdapterInitOut;


    NTSTATUS Status = IddCxAdapterInitAsync(&AdapterInit, &AdapterInitOut);

    if(NT_SUCCESS(Status)) {
        // Store a reference to the WDF adapter handle
        m_Adapter = AdapterInitOut.AdapterObject;

        // Store the device context object into the WDF object context
        auto* pContext = WdfObjectGet_IndirectDeviceContextWrapper(AdapterInitOut.AdapterObject);
        pContext->pContext = this;
    }
	else {
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "IddCxAdapterInitAsync() return:%x\n", Status);
	}
}

void IndirectDeviceContext::FinishInit()
{
	for(unsigned int i = 0; i < NUM_VIRTUAL_DISPLAYS; i++) {
        CreateMonitor(i);
    }
}

void IndirectDeviceContext::CreateMonitor(unsigned int index)
{
    // ==============================
    // TODO: In a real driver, the EDID should be retrieved dynamically from a connected physical monitor. The EDID
    // provided here is purely for demonstration, as it describes only 640x480 @ 60 Hz and 800x600 @ 60 Hz. Monitor
    // manufacturers are required to correctly fill in physical monitor attributes in order to allow the OS to optimize
    // settings like viewing distance and scale factor. Manufacturers should also use a unique serial number every
    // single device to ensure the OS can tell the monitors apart.
    // ==============================

    WDF_OBJECT_ATTRIBUTES Attr;
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&Attr, IndirectDeviceContextWrapper);

    IDDCX_MONITOR_INFO MonitorInfo = {};
    MonitorInfo.Size = sizeof(MonitorInfo);
    MonitorInfo.MonitorType = DISPLAYCONFIG_OUTPUT_TECHNOLOGY_HDMI;
    MonitorInfo.ConnectorIndex = index;
    MonitorInfo.MonitorDescription.Size = sizeof(MonitorInfo.MonitorDescription);
    MonitorInfo.MonitorDescription.Type = IDDCX_MONITOR_DESCRIPTION_TYPE_EDID;

	// will get default modes from callback IddSampleMonitorGetDefaultModes()
	// We should use this method for BeadaPanel.
	// We have to avoid to index m_prefered_mode as a static variable
//	MonitorInfo.MonitorDescription.DataSize = 0;
//	MonitorInfo.MonitorDescription.pData = NULL;
	MonitorInfo.MonitorDescription.DataSize = sizeof(s_KnownMonitorEdid);
	MonitorInfo.MonitorDescription.pData = const_cast<BYTE*>(s_KnownMonitorEdid);

    // ==============================
    // TODO: The monitor's container ID should be distinct from "this" device's container ID if the monitor is not
    // permanently attached to the display adapter device object. The container ID is typically made unique for each
    // monitor and can be used to associate the monitor with other devices, like audio or input devices. In this
    // sample we generate a random container ID GUID, but it's best practice to choose a stable container ID for a
    // unique monitor or to use "this" device's container ID for a permanent/integrated monitor.
    // ==============================

    // Create a container ID
    CoCreateGuid(&MonitorInfo.MonitorContainerId);

    IDARG_IN_MONITORCREATE MonitorCreate = {};
    MonitorCreate.ObjectAttributes = &Attr;
    MonitorCreate.pMonitorInfo = &MonitorInfo;

    // Create a monitor object with the specified monitor descriptor
    IDARG_OUT_MONITORCREATE MonitorCreateOut;

	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "IddCxMonitorCreate\n");
    NTSTATUS Status = IddCxMonitorCreate(m_Adapter, &MonitorCreate, &MonitorCreateOut);
    if(NT_SUCCESS(Status)) {
        m_Monitor = MonitorCreateOut.MonitorObject;

        // Associate the monitor with this device context
        auto* pContext = WdfObjectGet_IndirectDeviceContextWrapper(MonitorCreateOut.MonitorObject);
        pContext->pContext = this;

        // Tell the OS that the monitor has been plugged in
        IDARG_OUT_MONITORARRIVAL ArrivalOut;
		TraceEvents(TRACE_LEVEL_WARNING, TRACE_DEVICE, "IddCxMonitorArrival\n");
        Status = IddCxMonitorArrival(m_Monitor, &ArrivalOut);
    }
}

void IndirectDeviceContext::AssignSwapChain(IDDCX_SWAPCHAIN SwapChain, LUID RenderAdapter, HANDLE NewFrameEvent)
{
    m_ProcessingThread.reset();

    auto Device = make_shared<Direct3DDevice>(RenderAdapter);
    LOG("AssignSwapChain\n");
    if(FAILED(Device->Init())) {
        // It's important to delete the swap-chain if D3D initialization fails, so that the OS knows to generate a new
        // swap-chain and try again.
        WdfObjectDelete(SwapChain);
    } else {
        // Create a new swap-chain processing thread
        m_ProcessingThread.reset(new SwapChainProcessor(SwapChain, Device, this->m_WdfDevice, NewFrameEvent));
    }
}

void IndirectDeviceContext::UnassignSwapChain()
{
    // Stop processing the last swap-chain
    LOG("UnassignSwapChain\n");
    m_ProcessingThread.reset();
}

#pragma endregion

#pragma region DDI Callbacks

_Use_decl_annotations_
NTSTATUS IddSampleAdapterInitFinished(IDDCX_ADAPTER AdapterObject, const IDARG_IN_ADAPTER_INIT_FINISHED* pInArgs)
{
    // This is called when the OS has finished setting up the adapter for use by the IddCx driver. It's now possible
    // to report attached monitors.
	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "--> IddSampleAdapterInitFinished\n");

    auto* pContext = WdfObjectGet_IndirectDeviceContextWrapper(AdapterObject);
    if(NT_SUCCESS(pInArgs->AdapterInitStatus)) {
        pContext->pContext->FinishInit();
    }

	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "<-- IddSampleAdapterInitFinished\n");

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS IddSampleAdapterCommitModes(IDDCX_ADAPTER AdapterObject, const IDARG_IN_COMMITMODES* pInArgs)
{
    UNREFERENCED_PARAMETER(AdapterObject);
    UNREFERENCED_PARAMETER(pInArgs);

    // For the sample, do nothing when modes are picked - the swap-chain is taken care of by IddCx

    // ==============================
    // TODO: In a real driver, this function would be used to reconfigure the device to commit the new modes. Loop
    // through pInArgs->pPaths and look for IDDCX_PATH_FLAGS_ACTIVE. Any path not active is inactive (e.g. the monitor
    // should be turned off).
    // ==============================

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS IddSampleParseMonitorDescription(const IDARG_IN_PARSEMONITORDESCRIPTION* pInArgs, IDARG_OUT_PARSEMONITORDESCRIPTION* pOutArgs)
{
	// ==============================
	// TODO: In a real driver, this function would be called to generate monitor modes for an EDID by parsing it. In
	// this sample driver, we hard-code the EDID, so this function can generate known modes.
	// ==============================
	pOutArgs->MonitorModeBufferOutputCount = 1;

	if (pInArgs->MonitorModeBufferInputCount < 1) {
		TraceEvents(TRACE_LEVEL_INFORMATION, DBG_DDI, "InputCount:%d size:%d\n", pInArgs->MonitorModeBufferInputCount, 1);

		// Return success if there was no buffer, since the caller was only asking for a count of modes
		return STATUS_SUCCESS;
	}
	else {
		TraceEvents(TRACE_LEVEL_INFORMATION, DBG_DDI, "InputCount:%d \n", pInArgs->MonitorModeBufferInputCount);

		// Copy the known modes to the output buffer
		// We should not use this method for BeadaPanel.
		// As a temp solution, we have to index to m_prefered_mode as a static variable
		pInArgs->pMonitorModes[0].Size = sizeof(IDDCX_MONITOR_MODE);
		pInArgs->pMonitorModes[0].Origin = IDDCX_MONITOR_MODE_ORIGIN_MONITORDESCRIPTOR;
		pInArgs->pMonitorModes[0].MonitorVideoSignalInfo = IndirectDeviceContext::s_KnownMonitorModes[m_prefered_mode];

		// Set the preferred mode as represented in the EDID
		pOutArgs->PreferredMonitorModeIdx = 0;

		return STATUS_SUCCESS;
	}
}


/// <summary>
/// Creates a target mode from the fundamental mode attributes.
/// </summary>
void CreateTargetMode(DISPLAYCONFIG_VIDEO_SIGNAL_INFO& Mode, UINT Width, UINT Height, UINT VSync)
{
    Mode.totalSize.cx = Mode.activeSize.cx = Width;
    Mode.totalSize.cy = Mode.activeSize.cy = Height;
    Mode.AdditionalSignalInfo.vSyncFreqDivider = 1;
    Mode.AdditionalSignalInfo.videoStandard = 255;
    Mode.vSyncFreq.Numerator = VSync;
    Mode.vSyncFreq.Denominator = Mode.hSyncFreq.Denominator = 1;
    Mode.hSyncFreq.Numerator = VSync * Height;
    Mode.scanLineOrdering = DISPLAYCONFIG_SCANLINE_ORDERING_PROGRESSIVE;
    Mode.pixelRate = VSync * Width * Height;
}

void CreateTargetMode(IDDCX_TARGET_MODE& Mode, UINT Width, UINT Height, UINT VSync)
{
    Mode.Size = sizeof(Mode);
    CreateTargetMode(Mode.TargetVideoSignalInfo.targetVideoSignalInfo, Width, Height, VSync);
}

void CreateDefaultMode(IDDCX_MONITOR_MODE& Mode, UINT Width, UINT Height, UINT VSync)
{
	Mode.Size = sizeof(Mode);
	CreateTargetMode(Mode.MonitorVideoSignalInfo, Width, Height, VSync);
}

_Use_decl_annotations_
NTSTATUS IddSampleMonitorGetDefaultModes(IDDCX_MONITOR MonitorObject, const IDARG_IN_GETDEFAULTDESCRIPTIONMODES* pInArgs, IDARG_OUT_GETDEFAULTDESCRIPTIONMODES* pOutArgs)
{
	UNREFERENCED_PARAMETER(MonitorObject);

	TraceEvents(TRACE_LEVEL_INFORMATION, DBG_DDI, "InputCount:%d\n",  pInArgs->DefaultMonitorModeBufferInputCount);

	vector<IDDCX_MONITOR_MODE> DefaultModes(1);

	// Create a set of modes supported for frame processing and scan-out. These are typically not based on the
	// monitor's descriptor and instead are based on the static processing capability of the device. The OS will
	// report the available set of modes for a given output as the intersection of monitor modes with target modes.

#if 0
	CreateTargetMode(TargetModes[0], 7680, 4320, 60);
	CreateTargetMode(TargetModes[1], 6016, 3384, 60);
	CreateTargetMode(TargetModes[2], 5120, 2880, 60);
	CreateTargetMode(TargetModes[3], 4096, 2560, 60);
	CreateTargetMode(TargetModes[4], 4096, 2304, 60);
	CreateTargetMode(TargetModes[5], 3840, 2400, 60);
	CreateTargetMode(TargetModes[6], 3840, 2160, 60);
	CreateTargetMode(TargetModes[7], 3200, 2400, 60);
	CreateTargetMode(TargetModes[8], 3200, 1800, 60);
	CreateTargetMode(TargetModes[9], 3008, 1692, 60);
	CreateTargetMode(TargetModes[10], 2880, 1800, 60);
	CreateTargetMode(TargetModes[11], 2880, 1620, 60);
	CreateTargetMode(TargetModes[12], 2560, 1600, 60);
	CreateTargetMode(TargetModes[13], 2560, 1440, 60);
	CreateTargetMode(TargetModes[14], 1920, 1440, 60);
	CreateTargetMode(TargetModes[15], 1920, 1200, 60);

	CreateTargetMode(TargetModes[16], 1920, 1080, 60);
	CreateTargetMode(TargetModes[17], 1600, 1024, 60);
	CreateTargetMode(TargetModes[18], 1680, 1050, 60);
	CreateTargetMode(TargetModes[19], 1600, 900, 60);
	CreateTargetMode(TargetModes[20], 1440, 900, 60);
	CreateTargetMode(TargetModes[21], 1400, 1050, 60);
	CreateTargetMode(TargetModes[22], 1366, 768, 60);
	CreateTargetMode(TargetModes[23], 1360, 768, 60);
	CreateTargetMode(TargetModes[24], 1280, 1024, 60);
	CreateTargetMode(TargetModes[25], 1280, 960, 60);
	CreateTargetMode(TargetModes[26], 1280, 800, 60);
	CreateTargetMode(TargetModes[27], 1280, 768, 60);
	CreateTargetMode(TargetModes[28], 1280, 720, 60);

#else
	CreateDefaultMode(DefaultModes[0], 800, 480, 60);

#endif

	if (pInArgs->DefaultMonitorModeBufferInputCount >= DefaultModes.size()) {
		TraceEvents(TRACE_LEVEL_INFORMATION, DBG_DDI, "InputCount:%d size:%d\n", pInArgs->DefaultMonitorModeBufferInputCount, DefaultModes.size());
		copy(DefaultModes.begin(), DefaultModes.end(), pInArgs->pDefaultMonitorModes);
	}
	pOutArgs->DefaultMonitorModeBufferOutputCount = (UINT)DefaultModes.size();
	pOutArgs->PreferredMonitorModeIdx = m_prefered_mode;

	return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS IddSampleMonitorQueryModes(IDDCX_MONITOR MonitorObject, const IDARG_IN_QUERYTARGETMODES* pInArgs, IDARG_OUT_QUERYTARGETMODES* pOutArgs)
{
	UNREFERENCED_PARAMETER(MonitorObject);

	vector<IDDCX_TARGET_MODE> TargetModes(14);

	// Create a set of modes supported for frame processing and scan-out. These are typically not based on the
	// monitor's descriptor and instead are based on the static processing capability of the device. The OS will
	// report the available set of modes for a given output as the intersection of monitor modes with target modes.
	CreateTargetMode(TargetModes[0], 480, 1920, 60);
	CreateTargetMode(TargetModes[1], 1920, 480, 60);
	CreateTargetMode(TargetModes[2], 462, 1920, 60);
	CreateTargetMode(TargetModes[3], 1920, 462, 60);
	CreateTargetMode(TargetModes[4], 440, 1920, 60);
	CreateTargetMode(TargetModes[5], 1920, 440, 60);
	CreateTargetMode(TargetModes[6], 480, 1280, 60);
	CreateTargetMode(TargetModes[7], 400, 1280, 60);
	CreateTargetMode(TargetModes[8], 480, 854, 60);
	CreateTargetMode(TargetModes[9], 480, 800, 60);
	CreateTargetMode(TargetModes[10], 800, 480, 60);
	CreateTargetMode(TargetModes[11], 480, 480, 60);
	CreateTargetMode(TargetModes[12], 320, 480, 60);

	pOutArgs->TargetModeBufferOutputCount = 1;

	if (pInArgs->TargetModeBufferInputCount >= 1) {
		TraceEvents(TRACE_LEVEL_INFORMATION, DBG_DDI, "InputCount:%d size:%d\n", pInArgs->TargetModeBufferInputCount, 1);

		pInArgs->pTargetModes[0] = TargetModes[m_prefered_mode];
	}
	else {
		TraceEvents(TRACE_LEVEL_INFORMATION, DBG_DDI, "InputCount:%d \n", pInArgs->TargetModeBufferInputCount);
	}

	return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS IddSampleMonitorAssignSwapChain(IDDCX_MONITOR MonitorObject, const IDARG_IN_SETSWAPCHAIN* pInArgs)
{
    auto* pContext = WdfObjectGet_IndirectDeviceContextWrapper(MonitorObject);

    LOG("IddSampleMonitorAssignSwapChain\n");
    pContext->pContext->AssignSwapChain(pInArgs->hSwapChain, pInArgs->RenderAdapterLuid, pInArgs->hNextSurfaceAvailable);
    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS IddSampleMonitorUnassignSwapChain(IDDCX_MONITOR MonitorObject)
{
    auto* pContext = WdfObjectGet_IndirectDeviceContextWrapper(MonitorObject);
    LOG("IddSampleMonitorUnassignSwapChain\n");
    pContext->pContext->UnassignSwapChain();
    return STATUS_SUCCESS;
}

#pragma endregion
