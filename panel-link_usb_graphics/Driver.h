#pragma once

#define NOMINMAX
#include <windows.h>
#include <bugcodes.h>
#include <wudfwdm.h>
#include <wdf.h>
#include <iddcx.h>

#include <dxgi1_5.h>
#include <d3d11_2.h>
#include <avrt.h>
#include <wrl.h>

#include <memory>
#include <vector>

#include "usbdi.h"
#include <wdf.h>
#include <wdfusb.h>
#include "EncodeRGB.h"
#include "StatusLinkProtocol.h"
#include "PanelLinkProtocol.h"
#include "Trace.h"

namespace Microsoft
{
namespace WRL
{
namespace Wrappers
{
// Adds a wrapper for thread handles to the existing set of WRL handle wrapper classes
typedef HandleT<HandleTraits::HANDLENullTraits> Thread;
}

#define PIXEL_32BIT
typedef uint32_t  pixel_type_t;
#define FB_DISP_DEFAULT_PIXEL_BITS  32
}
}

#define DISP_MAX_HEIGHT 1920
#define DISP_MAX_WIDTH  1080
#define WDF_MEMORY_MAX_SIZE 1920*1080*2

typedef struct {
    SLIST_ENTRY node;
    WDFUSBPIPE pipe;
    int id;
    PSLIST_HEADER urb_list;
    WDFREQUEST Request;
	WDFMEMORY Memory;
} urb_itm_t, *purb_itm_t;

#define FPS_STAT_MAX 6

typedef struct {
    long tb[FPS_STAT_MAX];
    int cur;
    long last_fps;
} fps_mgr_t;

namespace Microsoft
{


namespace IndirectDisp
{
/// <summary>
/// Manages the creation and lifetime of a Direct3D render device.
/// </summary>
struct Direct3DDevice {
    Direct3DDevice(LUID AdapterLuid);
    Direct3DDevice();
    HRESULT Init();

    LUID AdapterLuid;
    Microsoft::WRL::ComPtr<IDXGIFactory5> DxgiFactory;
    Microsoft::WRL::ComPtr<IDXGIAdapter1> Adapter;
    Microsoft::WRL::ComPtr<ID3D11Device> Device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> DeviceContext;
};

/// <summary>
/// Manages a thread that consumes buffers from an indirect display swap-chain object.
/// </summary>
class SwapChainProcessor
{
public:
    SwapChainProcessor(IDDCX_SWAPCHAIN hSwapChain, std::shared_ptr<Direct3DDevice> Device, WDFDEVICE      WdfDevice, HANDLE NewFrameEvent);
    ~SwapChainProcessor();

private:


    static DWORD CALLBACK RunThread(LPVOID Argument);

    void Run();
    void RunCore();
    long get_fps(void);
    void put_fps_data(long t);
public:
    IDDCX_SWAPCHAIN m_hSwapChain;
    std::shared_ptr<Direct3DDevice> m_Device;
    WDFDEVICE  mp_WdfDevice;
	unsigned int lastWidth;
	unsigned int lastHeight;
    ID3D11Texture2D* hNewDesktopImage;
    IDXGISurface* hStagingSurf;
    uint8_t		preBuff[DISP_MAX_HEIGHT * DISP_MAX_WIDTH * 4];
    uint8_t		postBuff[DISP_MAX_HEIGHT * DISP_MAX_WIDTH * 4];
    fps_mgr_t fps_mgr ;

    SLIST_HEADER urb_list;
    urb_itm_t * curr_urb;
    HANDLE m_hAvailableBufferEvent;
    Microsoft::WRL::Wrappers::Thread m_hThread;
    Microsoft::WRL::Wrappers::Event m_hTerminateEvent;

};

/// <summary>
/// Provides a sample implementation of an indirect display driver.
/// </summary>
class IndirectDeviceContext
{
public:
    IndirectDeviceContext(_In_ WDFDEVICE WdfDevice);
    virtual ~IndirectDeviceContext();

    void InitAdapter();
    void FinishInit();

    void CreateMonitor(unsigned int index);
	void AssignSwapChain(IDDCX_SWAPCHAIN SwapChain, LUID RenderAdapter, HANDLE NewFrameEvent);
    void UnassignSwapChain();

	STATUSLINK_INFO	info;

	//flag to indicate if wake from sleep mode
	int m_fromSleepMode;

protected:

    WDFDEVICE m_WdfDevice;
    IDDCX_ADAPTER m_Adapter;
    IDDCX_MONITOR m_Monitor;
    IDDCX_MONITOR m_Monitor2;

    std::unique_ptr<SwapChainProcessor> m_ProcessingThread;

public:
    static const DISPLAYCONFIG_VIDEO_SIGNAL_INFO s_KnownMonitorModes[];
    static const BYTE s_KnownMonitorEdid[];
};
}
}
