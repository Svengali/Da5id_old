// pch.h: This is a precompiled header file. 
// Files listed below are compiled only once, improving build performance for future builds. 
// This also affects IntelliSense performance, including code completion and many code browsing features.
// However, files listed here are ALL re-compiled if any one of them is updated between builds. 
// Do not add files that you will frequently be updating here (this negates the performance advantage).

#ifndef PCH_H
#define PCH_H

// TODO: add headers that you want to pre-compile here

// Windowsy

#define WIN32_LEAN_AND_MEAN
#include <sdkddkver.h>
#include <windows.h>
#include <windowsx.h>
#include <shellapi.h> // must be after windows.h
#include <ShellScalingApi.h>
#include <interactioncontext.h>
#include <mmsystem.h>



// C++y

#include <assert.h>
#include <d3d11.h>
#include <d3d12.h>

#include <directxmath.h>
#include <math.h>

// STL

#include <iostream>
#include <algorithm>
#include <vector>
#include <future>
#include <limits>
#include <random>
#include <locale>
#include <codecvt>
#include <map>
#include <vector>
#include <iostream>
#include <thread>


// Diligenty

#if D3D11_SUPPORTED
#include "RenderDeviceFactoryD3D11.h"
#endif

#if D3D12_SUPPORTED
#include "RenderDeviceFactoryD3D12.h"
#endif


#if GL_SUPPORTED
#include "RenderDeviceFactoryOpenGL.h"
#endif

#if VULKAN_SUPPORTED
#include "RenderDeviceFactoryVk.h"
#endif

#include "StringTools.h"
#include "MapHelper.h"





#undef min
#undef max

#endif //PCH_H
