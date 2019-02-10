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

#include <algorithm>
#include <hash_map>


//
#include "cblib/Link.h"
#include "cblib/Vec3.h"
#include "cblib/Vec3i.h"
#include "cblib/Frame3.h"
#include "cblib/CRC.h"
#include "cblib/Reflection.h"
#include "cblib/AxialBox.h"
#include "cblib/profiler.h"


//Source level compiled in dependencies
#include "tinyxml/tinyxml.h"
#include "util/profile/profile.h"
#include "util/concurrentqueue.h"
#include "task/thread_pool.h"
#include "task/ctpl.h"
//#include "async++.h"
#include "math/units.h"
#include "math/units64.h"
#include "util/index.h"
#include "util/id.h"
#include "util/expected_lite.h"
#include "util/optional.h"
#include "util/markable.h"
#include "util/lvalue_ref.h"
#include "util/not_null.h"
#include "util/out_param.h"
#include "util/tagged_bool.h"
#include "util/util.h"

#include "util/String.h"
#include "util/Symbol.h"

#include "util/Reflection.h"
#include "util/XMLReader.h"

#include "util/Clock.h"


#include "util/Serialization.h"
#include "util/Config.h"


// Mostly frozen project includes
#include "ent/entityId.h"

typedef  uint8_t u8;
typedef   int8_t i8;
typedef uint16_t u16;
typedef  int16_t i16;
typedef uint32_t u32;
typedef  int32_t i32;
typedef uint64_t u64;
typedef  int64_t i64;
typedef  float   f32;
typedef  double  f64;

//
//#include "ent/ent.h"
//#include "df/com.h"
//#include "map/map.h"



#include "async++.h"




#endif //PCH_H
