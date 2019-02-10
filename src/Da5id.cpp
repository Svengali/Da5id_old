// Copyright 2014 Intel Corporation All Rights Reserved
//
// Intel makes no representations about the suitability of this software for any purpose.  
// THIS SOFTWARE IS PROVIDED ""AS IS."" INTEL SPECIFICALLY DISCLAIMS ALL WARRANTIES,
// EXPRESS OR IMPLIED, AND ALL LIABILITY, INCLUDING CONSEQUENTIAL AND OTHER INDIRECT DAMAGES,
// FOR THE USE OF THIS SOFTWARE, INCLUDING LIABILITY FOR INFRINGEMENT OF ANY PROPRIETARY
// RIGHTS, AND INCLUDING THE WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
// Intel does not assume any responsibility for any errors which may appear in this software
// nor any responsibility to update it.

#include "pch.h"



// MOVETO PCH
#include "vox/vox.h"




// LOCAL

#include "Engine.h"
#include "util.h"
#include <d3d11_4.h>

#include "imgui.h"

#include "imgui_impl_vulkan.h"
#include "imgui_impl_win32.h"

#include "vulkan\vulkan.h"


/**
#include "asteroids_d3d11.h"
#include "asteroids_d3d12.h"
#include "asteroids_DE.h"
#include "camera.h"
#include "gui.h"
//*/

///using namespace DirectX;



namespace {

	// Global demo state
	Settings gSettings;

	bool gUpdateWorkload = false;
	bool gd3d11Available = false;
	bool gd3d12Available = false;
	bool gVulkanAvailable = false;

	Settings::RenderMode gLastFrameRenderMode = static_cast<Settings::RenderMode>( -1 );

	OrbitCamera gCamera;

	ImGui_ImplVulkanH_WindowData g_WindowData;


	/**


	AsteroidsD3D11::Asteroids* gWorkloadD3D11 = nullptr;
	AsteroidsD3D12::Asteroids* gWorkloadD3D12 = nullptr;

	//*/

	IDXGIFactory2* gDXGIFactory = nullptr;

	GUI gGUI;
	GUIText* gFPSControl;

	grx::Engine*    gWorkloadDE = nullptr;

	enum
	{
		basicStyle = WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_VISIBLE,
		windowedStyle = basicStyle | WS_OVERLAPPEDWINDOW,
		fullscreenStyle = basicStyle
	};

	bool CheckDll( wchar_t const* dllName )
	{
		auto hModule = LoadLibrary( dllName );
		if( hModule == NULL ) {
			return false;
		}
		FreeLibrary( hModule );
		return true;
	}


	UINT SetupDPI()
	{
		// Just do system DPI awareness for now for simplicity... scale the 3D content
		SetProcessDpiAwareness( PROCESS_SYSTEM_DPI_AWARE );

		UINT dpiX = 0, dpiY;
		POINT pt = { 1, 1 };
		auto hMonitor = MonitorFromPoint( pt, MONITOR_DEFAULTTONEAREST );
		if( SUCCEEDED( GetDpiForMonitor( hMonitor, MDT_EFFECTIVE_DPI, &dpiX, &dpiY ) ) ) {
			return dpiX;
		}
		else {
			return 96; // default
		}
	}

	//*
	void ResetCameraView()
	{
		auto center = DirectX::XMVectorSet( 0.0f, -0.4f*SIM_DISC_RADIUS, 0.0f, 0.0f );
		auto radius = SIM_ORBIT_RADIUS + SIM_DISC_RADIUS + 10.f;
		auto minRadius = SIM_ORBIT_RADIUS - 3.0f * SIM_DISC_RADIUS;
		auto maxRadius = SIM_ORBIT_RADIUS + 3.0f * SIM_DISC_RADIUS;
		auto longAngle = 4.50f;
		auto latAngle = 1.45f;
		gCamera.View( center, radius, minRadius, maxRadius, longAngle, latAngle );
	}
	//*/

	void ToggleFullscreen( HWND hWnd )
	{
		static WINDOWPLACEMENT prevPlacement = { sizeof( prevPlacement ) };
		DWORD dwStyle = (DWORD)GetWindowLongPtr( hWnd, GWL_STYLE );
		if( ( dwStyle & windowedStyle ) == windowedStyle )
		{
			MONITORINFO mi = { sizeof( mi ) };
			if( GetWindowPlacement( hWnd, &prevPlacement ) &&
				GetMonitorInfo( MonitorFromWindow( hWnd, MONITOR_DEFAULTTOPRIMARY ), &mi ) )
			{
				SetWindowLong( hWnd, GWL_STYLE, fullscreenStyle );
				SetWindowPos( hWnd, HWND_TOP,
					mi.rcMonitor.left, mi.rcMonitor.top,
					mi.rcMonitor.right - mi.rcMonitor.left,
					mi.rcMonitor.bottom - mi.rcMonitor.top,
					SWP_NOOWNERZORDER | SWP_FRAMECHANGED );
			}
		}
		else {
			SetWindowLong( hWnd, GWL_STYLE, windowedStyle );
			SetWindowPlacement( hWnd, &prevPlacement );
			SetWindowPos( hWnd, NULL, 0, 0, 0, 0,
				SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER |
				SWP_NOOWNERZORDER | SWP_FRAMECHANGED );
		}
	}


	static void SetupVulkanWindowData( ImGui_ImplVulkanH_WindowData* wd, VkDevice dev, VkPhysicalDevice devPhysical, VkAllocationCallbacks* pAllocator, VkSurfaceKHR surface, int width, int height )
	{
		wd->Surface = surface;


		uint32_t queueFamily = (uint32_t)-1;

		// Select graphics queue family
		{
			uint32_t count;
			vkGetPhysicalDeviceQueueFamilyProperties( devPhysical, &count, NULL );
			VkQueueFamilyProperties* queues = (VkQueueFamilyProperties*)malloc( sizeof( VkQueueFamilyProperties ) * count );
			vkGetPhysicalDeviceQueueFamilyProperties( devPhysical, &count, queues );
			for( uint32_t i = 0; i < count; i++ )
				if( queues[i].queueFlags& VK_QUEUE_GRAPHICS_BIT )
				{
					queueFamily = i;
					break;
				}
			free( queues );
			assert( queueFamily != -1 );
		}


		// Check for WSI support
		VkBool32 res;
		vkGetPhysicalDeviceSurfaceSupportKHR( devPhysical, queueFamily, wd->Surface, &res );
		if( res != VK_TRUE )
		{
			fprintf( stderr, "Error no WSI support on physical device 0\n" );
			exit( -1 );
		}

		// Select Surface Format
		const VkFormat requestSurfaceImageFormat[] = { VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_B8G8R8_UNORM, VK_FORMAT_R8G8B8_UNORM };
		const VkColorSpaceKHR requestSurfaceColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
		wd->SurfaceFormat = ImGui_ImplVulkanH_SelectSurfaceFormat( devPhysical, wd->Surface, requestSurfaceImageFormat, (size_t)IM_ARRAYSIZE( requestSurfaceImageFormat ), requestSurfaceColorSpace );

		// Select Present Mode
#ifdef IMGUI_UNLIMITED_FRAME_RATE
		VkPresentModeKHR present_modes[] = { VK_PRESENT_MODE_MAILBOX_KHR, VK_PRESENT_MODE_IMMEDIATE_KHR, VK_PRESENT_MODE_FIFO_KHR };
#else
		VkPresentModeKHR present_modes[] = { VK_PRESENT_MODE_FIFO_KHR };
#endif
		wd->PresentMode = ImGui_ImplVulkanH_SelectPresentMode( devPhysical, wd->Surface, &present_modes[0], IM_ARRAYSIZE( present_modes ) );
		//printf("[vulkan] Selected PresentMode = %d\n", wd->PresentMode);

		// Create SwapChain, RenderPass, Framebuffer, etc.
		ImGui_ImplVulkanH_CreateWindowDataCommandBuffers( devPhysical, dev, queueFamily, wd, pAllocator );
		ImGui_ImplVulkanH_CreateWindowDataSwapChainAndFramebuffer( devPhysical, dev, wd, pAllocator, width, height );
	}



} // namespace






LRESULT CALLBACK WindowProc(
	HWND hWnd,
	UINT message,
	WPARAM wParam,
	LPARAM lParam )
{
	switch( message ) {
	case WM_DESTROY:
		/**
		// If all workloads are null, we are recreating the window
		// and should not post quit message
		if( gWorkloadD3D11 || gWorkloadD3D12 || gWorkloadDE )
			PostQuitMessage( 0 );
		//*/

		if( gWorkloadDE )
			PostQuitMessage( 0 );


		return 0;

	case WM_SIZE: {
		UINT ww = LOWORD( lParam );
		UINT wh = HIWORD( lParam );

		gSettings.windowWidth = (int)ww;
		gSettings.windowHeight = (int)wh;
		gSettings.renderWidth = (UINT)( double( gSettings.windowWidth )  * gSettings.renderScale );
		gSettings.renderHeight = (UINT)( double( gSettings.windowHeight ) * gSettings.renderScale );

		// Update camera projection
		if( gSettings.renderWidth != 0 && gSettings.renderHeight != 0 )
		{
			float aspect = (float)gSettings.renderWidth / (float)gSettings.renderHeight;
			gCamera.Projection( DirectX::XM_PIDIV2 * 0.8f * 3 / 2, aspect );
		}

		// Resize currently active swap chain
		switch( gSettings.mode )
		{
			/*
		case Settings::RenderMode::NativeD3D11:
			if( gWorkloadD3D11 && gSettings.renderWidth != 0 && gSettings.renderHeight != 0 )
				gWorkloadD3D11->ResizeSwapChain( gDXGIFactory, hWnd, gSettings.renderWidth, gSettings.renderHeight );
			break;

		case Settings::RenderMode::NativeD3D12:
			if( gWorkloadD3D12 && gSettings.renderWidth != 0 && gSettings.renderHeight != 0 )
				gWorkloadD3D12->ResizeSwapChain( gDXGIFactory, hWnd, gSettings.renderWidth, gSettings.renderHeight );
			break;
			*/

		case Settings::RenderMode::DiligentD3D11:
		case Settings::RenderMode::DiligentD3D12:
		case Settings::RenderMode::DiligentVulkan:
			if( gWorkloadDE )
				gWorkloadDE->ResizeSwapChain( hWnd, gSettings.renderWidth, gSettings.renderHeight );
			break;
		}

		return 0;
	}

	case WM_KEYDOWN:
		if( lParam & ( 1 << 30 ) ) {
			// Ignore repeats
			return 0;
		}
		switch( wParam ) {
		case VK_SPACE:
			gSettings.animate = !gSettings.animate;
			std::cout << "Animate: " << gSettings.animate << std::endl;
			return 0;

			/* Disabled for demo setup */
		case 'V':
			gSettings.vsync = !gSettings.vsync;
			std::cout << "Vsync: " << gSettings.vsync << std::endl;
			return 0;
		case 'M':
			gSettings.multithreadedRendering = !gSettings.multithreadedRendering;
			std::cout << "Multithreaded Rendering: " << gSettings.multithreadedRendering << std::endl;
			return 0;
		case 'I':
			gSettings.executeIndirect = !gSettings.executeIndirect;
			std::cout << "ExecuteIndirect Rendering: " << gSettings.executeIndirect << std::endl;
			return 0;
		case 'S':
			gSettings.submitRendering = !gSettings.submitRendering;
			std::cout << "Submit Rendering: " << gSettings.submitRendering << std::endl;
			return 0;
		case 'B':
			if( gSettings.mode == Settings::RenderMode::DiligentD3D12 || gSettings.mode == Settings::RenderMode::DiligentVulkan ) {
				gSettings.resourceBindingMode = ( gSettings.resourceBindingMode + 1 ) % 3;
				gUpdateWorkload = true;
			}
			return 0;

		case VK_ADD:
		case VK_OEM_PLUS:
			gSettings.numThreads = std::min( gSettings.numThreads + 1, 64 );
			gUpdateWorkload = true;
			return 0;
		case VK_SUBTRACT:
		case VK_OEM_MINUS:
			gSettings.numThreads = std::max( gSettings.numThreads - 1, 2 );
			gUpdateWorkload = true;
			return 0;

		/**
		case '1': gSettings.mode = gd3d11Available ? Settings::RenderMode::NativeD3D11 : gSettings.mode; return 0;
		case '2': gSettings.mode = gd3d12Available ? Settings::RenderMode::NativeD3D12 : gSettings.mode; return 0;
		case '3': gSettings.mode = gd3d11Available ? Settings::RenderMode::DiligentD3D11 : gSettings.mode; return 0;
		case '4': gSettings.mode = gd3d12Available ? Settings::RenderMode::DiligentD3D12 : gSettings.mode; return 0;
		case '5': gSettings.mode = gVulkanAvailable ? Settings::RenderMode::DiligentVulkan : gSettings.mode; return 0;
		//*/

		case VK_ESCAPE:
			SendMessage( hWnd, WM_CLOSE, 0, 0 );
			return 0;
		} // Switch on key code
		return 0;

	case WM_SYSKEYDOWN:
		if( lParam & ( 1 << 30 ) ) {
			// Ignore repeats
			return 0;
		}
		switch( wParam ) {
		case VK_RETURN:
			gSettings.windowed = !gSettings.windowed;
			ToggleFullscreen( hWnd );
			break;
		}
		return 0;

	case WM_MOUSEWHEEL: {
		auto delta = GET_WHEEL_DELTA_WPARAM( wParam );
		gCamera.ZoomRadius( -0.07f * delta );
		return 0;
	}

	case WM_POINTERDOWN:
	case WM_POINTERUPDATE:
	case WM_POINTERUP: {
		auto pointerId = GET_POINTERID_WPARAM( wParam );
		POINTER_INFO pointerInfo;
		if( GetPointerInfo( pointerId, &pointerInfo ) ) {
			if( message == WM_POINTERDOWN ) {
				// Compute pointer position in render units
				POINT p = pointerInfo.ptPixelLocation;
				ScreenToClient( hWnd, &p );
				RECT clientRect;
				GetClientRect( hWnd, &clientRect );
				p.x = p.x * gSettings.renderWidth / ( clientRect.right - clientRect.left );
				p.y = p.y * gSettings.renderHeight / ( clientRect.bottom - clientRect.top );

				//*
				auto guiControl = gGUI.HitTest( p.x, p.y );
				if( guiControl == gFPSControl ) {
					gSettings.lockFrameRate = !gSettings.lockFrameRate;
				}
				else { // Camera manipulation
					gCamera.AddPointer( pointerId );
				}
				//*/
			}

			// Otherwise send it to the camera controls
			gCamera.ProcessPointerFrames( pointerId, &pointerInfo );
			if( message == WM_POINTERUP ) gCamera.RemovePointer( pointerId );
		}
		return 0;
	}

	case WM_GETMINMAXINFO:
	{
		LPMINMAXINFO lpMMI = (LPMINMAXINFO)lParam;
		lpMMI->ptMinTrackSize.x = 320;
		lpMMI->ptMinTrackSize.y = 240;
		return 0;
	}

	default:
		return DefWindowProc( hWnd, message, wParam, lParam );
	}
}

//*
int InitWorkload( HWND hWnd, AsteroidsSimulation *pAst )
{
	switch( gSettings.mode )
	{
	/*
	case Settings::RenderMode::NativeD3D11:
		if( gd3d11Available ) {
			gWorkloadD3D11 = new AsteroidsD3D11::Asteroids( &asteroids, &gGUI, gSettings.warp );
		}

		gWorkloadD3D11->ResizeSwapChain( gDXGIFactory, hWnd, gSettings.renderWidth, gSettings.renderHeight );
		break;

	case Settings::RenderMode::NativeD3D12:
		if( gd3d12Available ) {
			// If requested, enumerate the warp adapter
			// TODO: Allow picking from multiple hardware adapters
			IDXGIAdapter1* adapter = nullptr;

			if( gSettings.warp ) {
				IDXGIFactory4* DXGIFactory4 = nullptr;
				if( FAILED( gDXGIFactory->QueryInterface( &DXGIFactory4 ) ) ) {
					fprintf( stderr, "error: WARP requires IDXGIFactory4 interface which is not present on this system!\n" );
					return -1;
				}

				auto hr = DXGIFactory4->EnumWarpAdapter( IID_PPV_ARGS( &adapter ) );
				DXGIFactory4->Release();

				if( FAILED( hr ) ) {
					fprintf( stderr, "error: WARP adapter not present on this system!\n" );
					return -1;
				}
			}

			gWorkloadD3D12 = new AsteroidsD3D12::Asteroids( &asteroids, &gGUI, gSettings.numThreads, adapter );
		}

		gWorkloadD3D12->ResizeSwapChain( gDXGIFactory, hWnd, gSettings.renderWidth, gSettings.renderHeight );
		break;

	case Settings::RenderMode::DiligentD3D11:
		gWorkloadDE = new AsteroidsDE::Asteroids( gSettings, &asteroids, &gGUI, hWnd, Diligent::DeviceType::D3D11 );
		break;

	case Settings::RenderMode::DiligentD3D12:
		gWorkloadDE = new AsteroidsDE::Asteroids( gSettings, &asteroids, &gGUI, hWnd, Diligent::DeviceType::D3D12 );
		break;
	*/

	case Settings::RenderMode::DiligentVulkan:
		gWorkloadDE = new grx::Engine( gSettings, pAst, &gGUI, hWnd, Diligent::DeviceType::Vulkan );
		break;
	}

	return 0;
}
//*/

void CreateDemoWindow( HWND& hWnd )
{
	RECT windowRect = { 100, 100, gSettings.windowWidth, gSettings.windowHeight };
	if( hWnd )
	{
		GetWindowRect( hWnd, &windowRect );
		DestroyWindow( hWnd );
	}
	else
	{
		AdjustWindowRect( &windowRect, windowedStyle, FALSE );
	}

	// create the window and store a handle to it
	hWnd = CreateWindowEx(
		WS_EX_APPWINDOW,
		L"AsteroidsDemoWndClass",
		L"Asteroids",
		windowedStyle,
		windowRect.left,
		windowRect.top,
		windowRect.right - windowRect.left,
		windowRect.bottom - windowRect.top,
		NULL,
		NULL,
		GetModuleHandle( NULL ),
		NULL );

	if( !gSettings.windowed ) {
		ToggleFullscreen( hWnd );
	}

	SetForegroundWindow( hWnd );
}

int main( int argc, char** argv )
{
#if defined(_DEBUG) || defined(DEBUG)
	_CrtSetReportMode( _CRT_WARN, _CRTDBG_MODE_FILE | _CRTDBG_MODE_DEBUG | _CRTDBG_MODE_WNDW );
	_CrtSetReportFile( _CRT_WARN, _CRTDBG_FILE_STDERR );


	_CrtSetDbgFlag( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );
#endif

	///gd3d11Available = CheckDll( "d3d11.dll" );
	///gd3d12Available = CheckDll( "d3d12.dll" );
#if VULKAN_SUPPORTED
	gVulkanAvailable = CheckDll( L"vulkan-1.dll" );
#endif

	// Must be done before any windowing-system-like things or else virtualization will kick in
	auto dpi = SetupDPI();
	// By default render at the lower resolution and scale up based on system settings
	gSettings.renderScale = 96.0 / double( dpi );

	// Scale default window size based on dpi
	gSettings.windowWidth *= dpi / 96;
	gSettings.windowHeight *= dpi / 96;

	gSettings.multithreadedRendering = true;


	gSettings.mode = Settings::RenderMode::Undefined;
	for( int a = 1; a < argc; ++a ) {
		if( _stricmp( argv[a], "-close_after" ) == 0 && a + 1 < argc ) {
			gSettings.closeAfterSeconds = atof( argv[++a] );
		}
		else if( _stricmp( argv[a], "-nod3d11" ) == 0 ) {
			///gd3d11Available = false;
		}
		else if( _stricmp( argv[a], "-singlethreaded" ) == 0 ) {
			gSettings.multithreadedRendering = false;
		}
		else if( _stricmp( argv[a], "-warp" ) == 0 ) {
			gSettings.warp = true;
		}
		else if( _stricmp( argv[a], "-nod3d12" ) == 0 ) {
			///gd3d12Available = false;
		}
		else if( _stricmp( argv[a], "-indirect" ) == 0 ) {
			gSettings.executeIndirect = true;
		}
		else if( _stricmp( argv[a], "-fullscreen" ) == 0 ) {
			gSettings.windowed = false;
		}
		else if( _stricmp( argv[a], "-window" ) == 0 && a + 2 < argc ) {
			gSettings.windowWidth = atoi( argv[++a] );
			gSettings.windowHeight = atoi( argv[++a] );
		}
		else if( _stricmp( argv[a], "-render_scale" ) == 0 && a + 1 < argc ) {
			gSettings.renderScale = atof( argv[++a] );
		}
		else if( _stricmp( argv[a], "-locked_fps" ) == 0 && a + 1 < argc ) {
			gSettings.lockedFrameRate = atoi( argv[++a] );
		}
		else if( _stricmp( argv[a], "-threads" ) == 0 && a + 1 < argc ) {
			gSettings.numThreads = atoi( argv[++a] );
		}
		else if( _stricmp( argv[a], "-d3d11" ) == 0 ) {
			gSettings.mode = Settings::RenderMode::DiligentD3D11;
		}
		else if( _stricmp( argv[a], "-d3d12" ) == 0 ) {
			///gSettings.mode = gd3d12Available ? Settings::RenderMode::DiligentD3D12 : Settings::RenderMode::Undefined;
		}
		else if( _stricmp( argv[a], "-vk" ) == 0 ) {
			gSettings.mode = gVulkanAvailable ? Settings::RenderMode::DiligentVulkan : Settings::RenderMode::Undefined;
		}
		else {
			fprintf( stderr, "error: unrecognized argument '%s'\n", argv[a] );
			fprintf( stderr, "usage: asteroids_d3d12 [options]\n" );
			fprintf( stderr, "options:\n" );
			fprintf( stderr, "  -close_after [seconds]\n" );
			fprintf( stderr, "  -nod3d11\n" );
			fprintf( stderr, "  -nod3d12\n" );
			fprintf( stderr, "  -fullscreen\n" );
			fprintf( stderr, "  -window [width] [height]\n" );
			fprintf( stderr, "  -render_scale [scale]\n" );
			fprintf( stderr, "  -locked_fps [fps]\n" );
			fprintf( stderr, "  -warp\n" );
			return -1;
		}
	}

	if( gSettings.numThreads == 0 )
	{
		gSettings.numThreads = std::max( std::thread::hardware_concurrency() - 1, 2u );
	}





	vox::testVox();






	//if (!d3d11Available && !d3d12Available) {
	//    fprintf(stderr, "error: neither D3D11 nor D3D12 available.\n");
	//    return -1;
	//}

	// DXGI Factory
	ThrowIfFailed( CreateDXGIFactory2( 0, IID_PPV_ARGS( &gDXGIFactory ) ) );


	// Setup GUI
	gFPSControl = gGUI.AddText( 150, 10 );


	AsteroidsSimulation asteroids( 1337, NUM_ASTEROIDS, NUM_UNIQUE_MESHES, MESH_MAX_SUBDIV_LEVELS, NUM_UNIQUE_TEXTURES );

	ResetCameraView();


	if( gSettings.mode == Settings::RenderMode::Undefined )
	{
		if( gVulkanAvailable )
			gSettings.mode = Settings::RenderMode::DiligentVulkan;
		else if( gd3d12Available )
			gSettings.mode = Settings::RenderMode::DiligentD3D12;
		else
			gSettings.mode = Settings::RenderMode::DiligentD3D11;
	}

	// init window class
	WNDCLASSEX windowClass;
	ZeroMemory( &windowClass, sizeof( WNDCLASSEX ) );
	windowClass.cbSize = sizeof( WNDCLASSEX );
	windowClass.style = CS_HREDRAW | CS_VREDRAW;
	windowClass.lpfnWndProc = WindowProc;
	windowClass.hInstance = GetModuleHandle( NULL );
	windowClass.hCursor = LoadCursor( NULL, IDC_ARROW );
	windowClass.lpszClassName = L"AsteroidsDemoWndClass";
	RegisterClassEx( &windowClass );

	HWND hWnd = NULL;

	// Initialize performance counters
	UINT64 perfCounterFreq = 0;
	UINT64 lastPerfCount = 0;
	QueryPerformanceFrequency( (LARGE_INTEGER*)&perfCounterFreq );
	QueryPerformanceCounter( (LARGE_INTEGER*)&lastPerfCount );

	// main loop
	double elapsedTime = 0.0;
	double frameTime = 0.0;
	POINTER_INFO pointerInfo = {};

	timeBeginPeriod( 1 );
	EnableMouseInPointer( TRUE );

	float filteredUpdateTime = 0.0f;
	float filteredRenderTime = 0.0f;
	float filteredFrameTime = 0.0f;
	for(;;)
	{
		MSG msg = {};
		while( PeekMessage( &msg, NULL, 0, 0, PM_REMOVE ) ) {
			if( msg.message == WM_QUIT ) {
				// Cleanup
				///delete gWorkloadD3D11;
				///delete gWorkloadD3D12;
				delete gWorkloadDE;
				SafeRelease( &gDXGIFactory );
				timeEndPeriod( 1 );
				EnableMouseInPointer( FALSE );
				return (int)msg.wParam;
			};

			TranslateMessage( &msg );
			DispatchMessage( &msg );
		}

		// If we swap to a new API we need to recreate swap chains
		if( gLastFrameRenderMode != gSettings.mode || gUpdateWorkload ) {
			// Delete workload first so that the window does not
			// post quit message to the queue
			///delete gWorkloadD3D11;
			///gWorkloadD3D11 = nullptr;
			///delete gWorkloadD3D12;
			///gWorkloadD3D12 = nullptr;
			delete gWorkloadDE;
			gWorkloadDE = nullptr;
			if( hWnd == NULL || gLastFrameRenderMode != gSettings.mode )
			{
				CreateDemoWindow( hWnd );



				const auto pDev = gWorkloadDE->mDevice.RawPtr();

				PROFILE(IMGUI_GENERATION);




				ImGui_ImplVulkanH_WindowData windowData;
				SetupVulkanWindowData( &windowData, nullptr, nullptr, nullptr, nullptr, gSettings.windowWidth, gSettings.windowHeight );

				ImGui::CreateContext();
				ImGuiIO& io = ImGui::GetIO(); (void)io;
				//io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // Enable Keyboard Controls

				// Setup Dear ImGui style
				ImGui::StyleColorsDark();
				//ImGui::StyleColorsClassic();

				// Setup Platform/Renderer bindings

				ImGui_ImplWin32_Init(hWnd);

				ImGui_ImplVulkan_InitInfo init_info = {};

				/*
				///init_info.Instance = g_Instance;
				init_info.PhysicalDevice = g_PhysicalDevice;
				init_info.Device = g_Device;
				///init_info.QueueFamily = g_QueueFamily;
				///init_info.Queue = g_Queue;
				init_info.PipelineCache = g_PipelineCache;
				init_info.DescriptorPool = g_DescriptorPool;
				init_info.Allocator = g_Allocator;
				init_info.CheckVkResultFn = check_vk_result;
				//*/


				ImGui_ImplVulkan_Init( &init_info, windowData.RenderPass );


			}



			InitWorkload( hWnd, &asteroids );

			gLastFrameRenderMode = gSettings.mode;
			gUpdateWorkload = false;
		}

		// Still need to process inertia even when no interaction is happening
		gCamera.ProcessInertia();

		// In D3D12 we'll wait on the GPU before taking the timestamp (more consistent)
		if( gSettings.mode == Settings::RenderMode::NativeD3D12 ) {
			///gWorkloadD3D12->WaitForReadyToRender();
		}

		// Get time delta
		UINT64 count;
		QueryPerformanceCounter( (LARGE_INTEGER*)&count );
		auto rawFrameTime = (double)( count - lastPerfCount ) / perfCounterFreq;
		elapsedTime += rawFrameTime;
		lastPerfCount = count;

		// Maintaining absolute time sync is not important in this demo so we can err on the "smoother" side
		double alpha = 0.2f;
		frameTime = alpha * rawFrameTime + ( 1.0f - alpha ) * frameTime;

		// Update GUI
		{
			const char *ModeStr = nullptr;
			const char *resBindModeStr = "";
			float updateTime = 0;
			float renderTime = 0;
			switch( gSettings.mode )
			{
			case Settings::RenderMode::NativeD3D11:
				ModeStr = "Native D3D11";
				///gWorkloadD3D11->GetPerfCounters( updateTime, renderTime );
				break;

			case Settings::RenderMode::NativeD3D12:
				ModeStr = "Native D3D12";
				///gWorkloadD3D12->GetPerfCounters( updateTime, renderTime );
				break;

			case Settings::RenderMode::DiligentD3D11:
				ModeStr = "Diligent D3D11";
				gWorkloadDE->GetPerfCounters( updateTime, renderTime );
				break;

			case Settings::RenderMode::DiligentD3D12:
			case Settings::RenderMode::DiligentVulkan:
				ModeStr = gSettings.mode == Settings::RenderMode::DiligentD3D12 ? "Diligent D3D12" : "Diligent Vk";
				gWorkloadDE->GetPerfCounters( updateTime, renderTime );
				switch( gSettings.resourceBindingMode )
				{
				case 0: resBindModeStr = "-d"; break;
				case 1: resBindModeStr = "-m"; break;
				case 2: resBindModeStr = "-tm"; break;
				}
				break;
			}

			float filterScale = 0.02f;
			filteredUpdateTime = filteredUpdateTime * ( 1.f - filterScale ) + filterScale * updateTime;
			filteredRenderTime = filteredRenderTime * ( 1.f - filterScale ) + filterScale * renderTime;
			filteredFrameTime = filteredFrameTime * ( 1.f - filterScale ) + filterScale * (float)frameTime;

			char buffer[256];
			sprintf_s( buffer, "Da5id %s%s (%dt) - %4.2f ms (%4.2f ms / %4.2f ms)", ModeStr, resBindModeStr, ( gSettings.multithreadedRendering ? gSettings.numThreads : 1 ),
				1000.f * filteredFrameTime, 1000.f * filteredUpdateTime, 1000.f * filteredRenderTime );

			SetWindowTextA( hWnd, buffer );

			/*
			if( gSettings.lockFrameRate ) {
				sprintf_s( buffer, "(Locked)" );
			}
			else {
				sprintf_s( buffer, "%.0f fps", 1.0f / filteredFrameTime );
			}
			*/
			gFPSControl->Text( buffer );
		}

		switch( gSettings.mode )
		{
		case Settings::RenderMode::NativeD3D11:
			///if( gWorkloadD3D11 )
			///	gWorkloadD3D11->Render( (float)frameTime, gCamera, gSettings );
			break;

		case Settings::RenderMode::NativeD3D12:
			///if( gWorkloadD3D12 )
			///	gWorkloadD3D12->Render( (float)frameTime, gCamera, gSettings );
			break;

		case Settings::RenderMode::DiligentD3D11:
		case Settings::RenderMode::DiligentD3D12:
		case Settings::RenderMode::DiligentVulkan:
			if( gWorkloadDE )
				gWorkloadDE->Render( (float)frameTime, gCamera, gSettings );
			break;
		}

		if( gSettings.lockFrameRate ) {

			UINT64 afterRenderCount;
			QueryPerformanceCounter( (LARGE_INTEGER*)&afterRenderCount );
			double renderTime = (double)( afterRenderCount - count ) / perfCounterFreq;

			double targetRenderTime = 1.0 / double( gSettings.lockedFrameRate );
			double deltaMs = ( targetRenderTime - renderTime ) * 1000.0;
			if( deltaMs > 1.0 ) {
				Sleep( (DWORD)deltaMs );
			}

		}

		// All done?
		if( gSettings.closeAfterSeconds > 0.0 && elapsedTime > gSettings.closeAfterSeconds ) {
			SendMessage( hWnd, WM_CLOSE, 0, 0 );
			break;
		}
	}

	// Shouldn't get here
	return 1;
}
