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

#include "Engine.h"

#include "RenderDeviceFactoryVk.h"

#include "BasicShaderSourceStreamFactory.h"

#include "mesh.h"

#include "TextureLoader.h"
#include "TextureUtilities.h"

/*
#include "MapHelper.h"

#include "util.h"
#include "noise.h"
#include "texture.h"
#include "StringTools.h"
*/

using namespace Diligent;

namespace Diligent
{
#if ENGINE_DLL
#   if D3D11_SUPPORTED
        GetEngineFactoryD3D11Type GetEngineFactoryD3D11 = nullptr;
#   endif
        
#   if D3D12_SUPPORTED
        GetEngineFactoryD3D12Type GetEngineFactoryD3D12 = nullptr;
#   endif

#   if GL_SUPPORTED
        GetEngineFactoryOpenGLType GetEngineFactoryOpenGL = nullptr;
#   endif

#   if VULKAN_SUPPORTED
        GetEngineFactoryVkType GetEngineFactoryVulkan = nullptr;
#   endif
#endif
}

namespace grx {


// Create Direct3D device and swap chain
void Engine::InitDevice(HWND hWnd, DeviceType DevType)
{
    SwapChainDesc SwapChainDesc;
    SwapChainDesc.SamplesCount = 1;
    SwapChainDesc.BufferCount = NUM_SWAP_CHAIN_BUFFERS;
    SwapChainDesc.ColorBufferFormat = TEX_FORMAT_RGBA8_UNORM_SRGB;
    SwapChainDesc.DepthBufferFormat = TEX_FORMAT_D32_FLOAT;
    SwapChainDesc.DefaultDepthValue = 0.f;

    std::vector<IDeviceContext*> ppContexts(mNumSubsets);

    switch (DevType)
    {
#if D3D11_SUPPORTED
        case DeviceType::D3D11:
        {
            EngineD3D11Attribs DeviceAttribs;
            DeviceAttribs.DebugFlags = (Uint32)EngineD3D11DebugFlags::VerifyCommittedShaderResources |
                                        (Uint32)EngineD3D11DebugFlags::VerifyCommittedResourceRelevance;

#if ENGINE_DLL
            if(!GetEngineFactoryD3D11)
                LoadGraphicsEngineD3D11(GetEngineFactoryD3D11);
#endif
            auto *pFactoryD3D11 = GetEngineFactoryD3D11();
            pFactoryD3D11->CreateDeviceAndContextsD3D11( DeviceAttribs, &mDevice, ppContexts.data(), mNumSubsets-1 );
            pFactoryD3D11->CreateSwapChainD3D11( mDevice, ppContexts[0], SwapChainDesc, FullScreenModeDesc{}, hWnd, &mSwapChain );
        }
        break;
#endif


#if D3D12_SUPPORTED
        case DeviceType::D3D12:
        {
            EngineD3D12Attribs Attribs;
            Attribs.GPUDescriptorHeapDynamicSize[0] = 65536*4;
            Attribs.GPUDescriptorHeapSize[0] = 65536; // For mutable mode
            Attribs.NumCommandsToFlushCmdList = 1024;
#ifndef _DEBUG
            Attribs.DynamicDescriptorAllocationChunkSize[0] = 8192;
#endif
#if ENGINE_DLL
            if(!GetEngineFactoryD3D12)
                LoadGraphicsEngineD3D12(GetEngineFactoryD3D12);
#endif
            auto *pFactoryD3D12 = GetEngineFactoryD3D12();
            pFactoryD3D12->CreateDeviceAndContextsD3D12( Attribs, &mDevice, ppContexts.data(), mNumSubsets-1 );
            pFactoryD3D12->CreateSwapChainD3D12( mDevice, ppContexts[0], SwapChainDesc, FullScreenModeDesc{}, hWnd, &mSwapChain );
        }
        break;
#endif


#if VULKAN_SUPPORTED
        case DeviceType::Vulkan:
        {
            EngineVkAttribs Attribs;
            Attribs.DynamicHeapSize = 256 * 1024 * 1024;
#if ENGINE_DLL
            if(!GetEngineFactoryVulkan)
                LoadGraphicsEngineVk(GetEngineFactoryVulkan);
#endif
            auto *pFactoryVk = GetEngineFactoryVk();
            pFactoryVk->CreateDeviceAndContextsVk( Attribs, &mDevice, ppContexts.data(), mNumSubsets-1 );
            pFactoryVk->CreateSwapChainVk( mDevice, ppContexts[0], SwapChainDesc, hWnd, &mSwapChain );

			//mSwapChain->GetDesc().
        }
        break;
#endif


#if GL_SUPPORTED
        case DeviceType::OpenGL:
        {
#if ENGINE_DLL
            if (GetEngineFactoryOpenGL == nullptr)
            {
                LoadGraphicsEngineOpenGL(GetEngineFactoryOpenGL);
            }
#endif
            EngineGLAttribs CreationAttribs;
            CreationAttribs.pNativeWndHandle = hWnd;
            GetEngineFactoryOpenGL()->CreateDeviceAndSwapChainGL(
                CreationAttribs, &mDevice, &mDeviceCtxt, SwapChainDesc, &mSwapChain);
        }
        break;
#endif


        default:
            LOG_ERROR_AND_THROW("Unknown device type");
        break;
    }

    if (DevType == DeviceType::D3D11 || DevType == DeviceType::D3D12 || DevType == DeviceType::Vulkan)
    {
        mDeviceCtxt.Attach(ppContexts[0]);
        mDeferredCtxt.resize(mNumSubsets-1);
        for(size_t ctx=0; ctx < mNumSubsets-1; ++ctx)
            mDeferredCtxt[ctx].Attach(ppContexts[1+ctx]);
    }
}

Engine::Engine(const Settings &settings, AsteroidsSimulation* pAst, GUI* gui, HWND hWnd, Diligent::DeviceType DevType)
	:
	m_pAst( pAst ),
    mGUI( gui )
{
    QueryPerformanceFrequency((LARGE_INTEGER*)&mPerfCounterFreq);

    //mNumSubsets = std::max(settings.numThreads,1);
    //mNumSubsets = std::min(settings.numThreads,32);

	mNumSubsets = 2;

    m_BindingMode = static_cast<BindingMode>(settings.resourceBindingMode);

    InitDevice(hWnd, DevType);
    
    mCmdLists.resize(mDeferredCtxt.size());
    mWorkerThreads.resize(mNumSubsets-1);
    for(auto &thread : mWorkerThreads)
    {
        thread = std::thread(WorkerThreadFunc, this, (Uint32)(&thread-mWorkerThreads.data()) );
    }

    const char *spriteFile = nullptr;
    switch(DevType)
    {
        case DeviceType::D3D11: spriteFile = "DiligentD3D11.dds"; break;
        case DeviceType::D3D12: spriteFile = "DiligentD3D12.dds"; break;
        case DeviceType::OpenGL: spriteFile = "DiligentGL.dds"; break;
        case DeviceType::Vulkan: spriteFile = "DiligentVk.dds"; break;
        default: UNEXPECTED("Unexpected device type");
    }
    mSprite.reset( new GUISprite(5, 10, 140, 50, spriteFile) );
    
    BasicShaderSourceStreamFactory BasicSSSFactory({"src"});

    std::vector<StateTransitionDesc> Barriers;
    mBackBufferWidth = mSwapChain->GetDesc().Width;
    mBackBufferHeight = mSwapChain->GetDesc().Height;
    // Create draw constant buffer
    {
        BufferDesc desc;
        desc.Name = "draw constant buffer (dynamic)";
        desc.Usage = USAGE_DYNAMIC;
        desc.uiSizeInBytes = (Uint32) sizeof(DrawConstantBuffer);
        desc.BindFlags = BIND_UNIFORM_BUFFER;
        desc.CPUAccessFlags = CPU_ACCESS_WRITE;
        mDevice->CreateBuffer(desc, BufferData(), &mDrawConstantBuffer);
        Barriers.emplace_back(mDrawConstantBuffer, RESOURCE_STATE_UNKNOWN, RESOURCE_STATE_CONSTANT_BUFFER, true);
    }

    // create pipeline state
    {
        PipelineStateDesc PSODesc;
        LayoutElement inputDesc[] = {
            LayoutElement( 0, 0, 3, VT_FLOAT32, false, 0, sizeof(Vertex)),
            LayoutElement( 1, 0, 3, VT_FLOAT32)
        };

        PSODesc.GraphicsPipeline.InputLayout.LayoutElements = inputDesc;
        PSODesc.GraphicsPipeline.InputLayout.NumElements = _countof(inputDesc);

        PSODesc.GraphicsPipeline.DepthStencilDesc.DepthFunc = COMPARISON_FUNC_GREATER_EQUAL;

        RefCntAutoPtr<IShader> vs, ps;
        {
            ShaderCreationAttribs attribs;
            attribs.Desc.DefaultVariableType = SHADER_VARIABLE_TYPE_STATIC;
            attribs.Desc.ShaderType = SHADER_TYPE_VERTEX;
            attribs.Desc.Name = "Asteroids VS";
            attribs.EntryPoint = "asteroid_vs";
            attribs.FilePath = "asteroid_vs.hlsl";
            attribs.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
            attribs.pShaderSourceStreamFactory = &BasicSSSFactory;
            attribs.UseCombinedTextureSamplers = true;
            mDevice->CreateShader(attribs, &vs);
            vs->GetShaderVariable("DrawConstantBuffer")->Set(mDrawConstantBuffer);
        }

        {
            ShaderCreationAttribs attribs;
            attribs.Desc.DefaultVariableType = m_BindingMode == BindingMode::Dynamic ? SHADER_VARIABLE_TYPE_DYNAMIC : SHADER_VARIABLE_TYPE_MUTABLE;
            attribs.Desc.ShaderType = SHADER_TYPE_PIXEL;
            attribs.Desc.Name = "Asteroids PS";
            attribs.EntryPoint = "asteroid_ps_d3d11";
            attribs.FilePath = "asteroid_ps_d3d11.hlsl";
            attribs.pShaderSourceStreamFactory = &BasicSSSFactory;
            attribs.UseCombinedTextureSamplers = true;
            attribs.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;

            StaticSamplerDesc samDesc;
            samDesc.Desc.MagFilter       = FILTER_TYPE_ANISOTROPIC;
            samDesc.Desc.MinFilter       = FILTER_TYPE_ANISOTROPIC;
            samDesc.Desc.MipFilter       = FILTER_TYPE_ANISOTROPIC;
            samDesc.Desc.AddressU        = TEXTURE_ADDRESS_WRAP;
            samDesc.Desc.AddressV        = TEXTURE_ADDRESS_WRAP;
            samDesc.Desc.AddressW        = TEXTURE_ADDRESS_WRAP;
            samDesc.Desc.MinLOD          =-D3D11_FLOAT32_MAX;
            samDesc.Desc.MaxLOD          = D3D11_FLOAT32_MAX;
            samDesc.Desc.MipLODBias      = 0.0f;
            samDesc.Desc.MaxAnisotropy   = TEXTURE_ANISO;
            samDesc.Desc.ComparisonFunc  = COMPARISON_FUNC_NEVER;
            samDesc.SamplerOrTextureName = "Tex";
            attribs.Desc.StaticSamplers  = &samDesc;
            attribs.Desc.NumStaticSamplers = 1;

            mDevice->CreateShader(attribs, &ps);
        }
        PSODesc.GraphicsPipeline.RTVFormats[0] = mSwapChain->GetDesc().ColorBufferFormat;
        PSODesc.GraphicsPipeline.NumRenderTargets = 1;
        PSODesc.GraphicsPipeline.DSVFormat = mSwapChain->GetDesc().DepthBufferFormat;
        PSODesc.GraphicsPipeline.PrimitiveTopology = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        PSODesc.Name = "Asteroids PSO";

        PSODesc.GraphicsPipeline.pVS = vs;
        PSODesc.GraphicsPipeline.pPS = ps;
        Uint32 NumSRBs = 0;
        if(m_BindingMode == BindingMode::Dynamic)
        {
            NumSRBs = mNumSubsets;
        }
        else if(m_BindingMode == BindingMode::Mutable)
        {
            PSODesc.SRBAllocationGranularity = 1024;
            NumSRBs = NUM_ASTEROIDS;
        }
        else if(m_BindingMode == BindingMode::TextureMutable)
        {
            PSODesc.SRBAllocationGranularity = NUM_UNIQUE_TEXTURES;
            NumSRBs = NUM_UNIQUE_TEXTURES;
        }
        mDevice->CreatePipelineState(PSODesc, &mAsteroidsPSO);
        mAsteroidsSRBs.resize(NumSRBs);
        for(size_t srb = 0; srb < mAsteroidsSRBs.size(); ++srb)
        {
            mAsteroidsPSO->CreateShaderResourceBinding(&mAsteroidsSRBs[srb], true);
        }
    }

  
    // Load textures
    {
        TextureLoadInfo loadInfo;
        loadInfo.IsSRGB = true;
        RefCntAutoPtr<ITexture> skybox;
        CreateTextureFromFile("starbox_1024.dds", loadInfo, mDevice, &skybox);
        mSkyboxSRV = skybox->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
        Barriers.emplace_back(skybox, RESOURCE_STATE_UNKNOWN, RESOURCE_STATE_SHADER_RESOURCE, true);
    }

    // Create skybox constant buffer
    {
        BufferDesc desc;
        desc.Name = "skybox constant buffer (dynamic)";
        desc.Usage = USAGE_DYNAMIC;
        desc.uiSizeInBytes = sizeof(SkyboxConstantBuffer);
        desc.BindFlags = BIND_UNIFORM_BUFFER;
        desc.CPUAccessFlags = CPU_ACCESS_WRITE;
        mDevice->CreateBuffer(desc, BufferData(), &mSkyboxConstantBuffer);
        Barriers.emplace_back(mSkyboxConstantBuffer, RESOURCE_STATE_UNKNOWN, RESOURCE_STATE_CONSTANT_BUFFER, true);
    }

    // create skybox pipeline state
    {
        LayoutElement inputDesc[] = {
            LayoutElement( 0, 0, 3, VT_FLOAT32),
            LayoutElement( 1, 0, 3, VT_FLOAT32)
        };

        RefCntAutoPtr<IShader> vs, ps;
        ShaderCreationAttribs attribs;
        attribs.Desc.DefaultVariableType = SHADER_VARIABLE_TYPE_STATIC;
        attribs.Desc.ShaderType = SHADER_TYPE_VERTEX;
        attribs.Desc.Name = "Skybox VS";
        attribs.EntryPoint = "skybox_vs";
        attribs.FilePath = "skybox_vs.hlsl";
        attribs.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
        attribs.UseCombinedTextureSamplers = true;
        attribs.pShaderSourceStreamFactory = &BasicSSSFactory;
        mDevice->CreateShader(attribs, &vs);
        vs->GetShaderVariable("SkyboxConstantBuffer")->Set(mSkyboxConstantBuffer);

        attribs.Desc.Name = "Skybox PS";
        attribs.EntryPoint = "skybox_ps";
        attribs.FilePath = "skybox_ps.hlsl";
        attribs.Desc.ShaderType = SHADER_TYPE_PIXEL;

        StaticSamplerDesc ssdesc;
        ssdesc.SamplerOrTextureName = "Skybox";
        ssdesc.Desc.MagFilter       = FILTER_TYPE_ANISOTROPIC;
        ssdesc.Desc.MinFilter       = FILTER_TYPE_ANISOTROPIC;
        ssdesc.Desc.MipFilter       = FILTER_TYPE_ANISOTROPIC;
        ssdesc.Desc.AddressU        = TEXTURE_ADDRESS_WRAP;
        ssdesc.Desc.AddressV        = TEXTURE_ADDRESS_WRAP;
        ssdesc.Desc.AddressW        = TEXTURE_ADDRESS_WRAP;
        ssdesc.Desc.MaxAnisotropy   = TEXTURE_ANISO;
        attribs.Desc.StaticSamplers = &ssdesc;
        attribs.Desc.NumStaticSamplers = 1;
        mDevice->CreateShader(attribs, &ps);
        ps->GetShaderVariable("Skybox")->Set(mSkyboxSRV);

        PipelineStateDesc PSODesc;
        PSODesc.Name = "Skybox PSO";
        PSODesc.GraphicsPipeline.InputLayout.LayoutElements = inputDesc;
        PSODesc.GraphicsPipeline.InputLayout.NumElements = _countof(inputDesc);

        PSODesc.GraphicsPipeline.DepthStencilDesc.DepthFunc = COMPARISON_FUNC_GREATER_EQUAL;
        PSODesc.GraphicsPipeline.pVS = vs;
        PSODesc.GraphicsPipeline.pPS = ps;

        PSODesc.GraphicsPipeline.RTVFormats[0] = mSwapChain->GetDesc().ColorBufferFormat;
        PSODesc.GraphicsPipeline.NumRenderTargets = 1;
        PSODesc.GraphicsPipeline.DSVFormat = mSwapChain->GetDesc().DepthBufferFormat;
        PSODesc.GraphicsPipeline.PrimitiveTopology = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        mDevice->CreatePipelineState(PSODesc, &mSkyboxPSO);
        mSkyboxPSO->CreateShaderResourceBinding(&mSkyboxSRB, true);
    }

    // Create sampler
    {
        SamplerDesc desc;
        desc.MagFilter      = FILTER_TYPE_ANISOTROPIC;
        desc.MinFilter      = FILTER_TYPE_ANISOTROPIC;
        desc.MipFilter      = FILTER_TYPE_ANISOTROPIC;
        desc.AddressU       = TEXTURE_ADDRESS_WRAP;
        desc.AddressV       = TEXTURE_ADDRESS_WRAP;
        desc.AddressW       = TEXTURE_ADDRESS_WRAP;
        desc.MinLOD         = -D3D11_FLOAT32_MAX;
        desc.MaxLOD         = D3D11_FLOAT32_MAX;
        desc.MipLODBias     = 0.0f;
        desc.MaxAnisotropy  = TEXTURE_ANISO;
        desc.ComparisonFunc = COMPARISON_FUNC_NEVER;
        mDevice->CreateSampler(desc, &mSamplerState);
    }

    CreateGUIResources();

    // Sprites and fonts
    {
        PipelineStateDesc PSODesc;
        LayoutElement inputDesc[] = {
            LayoutElement( 0, 0, 2, VT_FLOAT32),
            LayoutElement( 1, 0, 2, VT_FLOAT32)
        };
        PSODesc.GraphicsPipeline.InputLayout.LayoutElements = inputDesc;
        PSODesc.GraphicsPipeline.InputLayout.NumElements = _countof(inputDesc);

        auto &BlendState = PSODesc.GraphicsPipeline.BlendDesc;
        {
            // Premultiplied over blend
            BlendState.RenderTargets[0].BlendEnable = TRUE;
            BlendState.RenderTargets[0].SrcBlend    = BLEND_FACTOR_ONE;
            BlendState.RenderTargets[0].BlendOp     = BLEND_OPERATION_ADD;
            BlendState.RenderTargets[0].DestBlend   = BLEND_FACTOR_INV_SRC_ALPHA;
        }

        PSODesc.GraphicsPipeline.DepthStencilDesc.DepthEnable = false;

        PSODesc.GraphicsPipeline.RTVFormats[0] = mSwapChain->GetDesc().ColorBufferFormat;
        PSODesc.GraphicsPipeline.NumRenderTargets = 1;
        PSODesc.GraphicsPipeline.DSVFormat = mSwapChain->GetDesc().DepthBufferFormat;
        PSODesc.GraphicsPipeline.PrimitiveTopology = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        
        RefCntAutoPtr<IShader> sprite_vs, sprite_ps, font_ps;
        {
            ShaderCreationAttribs attribs;
            attribs.Desc.DefaultVariableType = SHADER_VARIABLE_TYPE_STATIC;
            attribs.Desc.ShaderType = SHADER_TYPE_VERTEX;
            attribs.Desc.Name = "Sprite VS";
            attribs.EntryPoint = "sprite_vs";
            attribs.FilePath = "sprite_vs.hlsl";
            attribs.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
            attribs.UseCombinedTextureSamplers = true;
            attribs.pShaderSourceStreamFactory = &BasicSSSFactory;
            mDevice->CreateShader(attribs, &sprite_vs);
        }

        {
            ShaderCreationAttribs attribs;
            attribs.Desc.DefaultVariableType = SHADER_VARIABLE_TYPE_DYNAMIC;
            attribs.Desc.ShaderType = SHADER_TYPE_PIXEL;
            attribs.Desc.Name = "Sprite PS";
            attribs.EntryPoint = "sprite_ps";
            attribs.FilePath = "sprite_ps.hlsl";
            attribs.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
            attribs.UseCombinedTextureSamplers = true;
            attribs.pShaderSourceStreamFactory = &BasicSSSFactory;
            mDevice->CreateShader(attribs, &sprite_ps);
        }

        {
            ShaderCreationAttribs attribs;
            attribs.Desc.DefaultVariableType = SHADER_VARIABLE_TYPE_STATIC;
            attribs.Desc.ShaderType = SHADER_TYPE_PIXEL;
            attribs.Desc.Name = "Font PS";
            attribs.EntryPoint = "font_ps";
            attribs.FilePath = "font_ps.hlsl";
            attribs.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
            attribs.UseCombinedTextureSamplers = true;
            attribs.pShaderSourceStreamFactory = &BasicSSSFactory;
            mDevice->CreateShader(attribs, &font_ps);
            font_ps->GetShaderVariable("Tex")->Set(mFontTextureSRV);
        }

        PSODesc.Name = "Sprite PSO";
        PSODesc.GraphicsPipeline.pVS = sprite_vs;
        PSODesc.GraphicsPipeline.pPS = sprite_ps;
        mDevice->CreatePipelineState(PSODesc, &mSpritePSO);
        mSpritePSO->CreateShaderResourceBinding(&mSpriteSRB, true);

        PSODesc.Name = "Font PSO";
        PSODesc.GraphicsPipeline.pPS = font_ps;
        mDevice->CreatePipelineState(PSODesc, &mFontPSO);
        mFontPSO->CreateShaderResourceBinding(&mFontSRB, true);
    }


    CreateMeshes();
    InitializeTextureData();
    if( m_BindingMode == BindingMode::Mutable )
    {
        for(size_t srb = 0; srb < NUM_ASTEROIDS; ++srb)
        {
            auto staticData = &m_pAst->StaticData()[srb];
            mAsteroidsSRBs[srb]->GetVariable(SHADER_TYPE_PIXEL, "Tex")->Set(mTextureSRVs[staticData->textureIndex]);
        }
    }
    else if( m_BindingMode == BindingMode::TextureMutable )
    {
        for(size_t srb = 0; srb < NUM_UNIQUE_TEXTURES; ++srb)
        {
            mAsteroidsSRBs[srb]->GetVariable(SHADER_TYPE_PIXEL, "Tex")->Set(mTextureSRVs[srb]);
        }
    }
    mDeviceCtxt->TransitionResourceStates(static_cast<Uint32>(Barriers.size()), Barriers.data());
}

Engine::~Engine()
{
    mDeviceCtxt->Flush();
    mDeviceCtxt->FinishFrame();

    mUpdateSubsetsSignal.Trigger(true, -1);

    for(auto &thread : mWorkerThreads)
    {
        thread.join();
    }
}



void Engine::ResizeSwapChain(HWND, unsigned int width, unsigned int height)
{
    mSwapChain->Resize(width, height);
    mBackBufferWidth = width;
    mBackBufferHeight = height;
}


void Engine::CreateMeshes()
{
	std::vector<StateTransitionDesc> Barriers;


	//*
    auto asteroidMeshes = m_pAst->Meshes();

    // create vertex buffer
    {
        BufferDesc desc;
        desc.Name = "Asteroid Meshes Vertex Buffer";
        desc.uiSizeInBytes = (Uint32)asteroidMeshes->vertices.size() * sizeof(asteroidMeshes->vertices[0]);
        desc.BindFlags = BIND_VERTEX_BUFFER;
        desc.Usage = USAGE_DEFAULT;

        BufferData data;
        data.pData = asteroidMeshes->vertices.data();
        data.DataSize = desc.uiSizeInBytes;

        mDevice->CreateBuffer(desc, data, &mVertexBuffer);
        Barriers.emplace_back(mVertexBuffer, RESOURCE_STATE_UNKNOWN, RESOURCE_STATE_VERTEX_BUFFER, true);
    }

    // create index buffer
    {
        BufferDesc desc;
        desc.Name = "Asteroid Meshes Index Buffer";
        desc.uiSizeInBytes = (Uint32)asteroidMeshes->indices.size() * sizeof(asteroidMeshes->indices[0]);
        desc.BindFlags = BIND_INDEX_BUFFER;
        desc.Usage = USAGE_DEFAULT;

        BufferData data;
        data.pData = asteroidMeshes->indices.data();
        data.DataSize = desc.uiSizeInBytes;

        mDevice->CreateBuffer(desc, data, &mIndexBuffer);
        Barriers.emplace_back(mIndexBuffer, RESOURCE_STATE_UNKNOWN, RESOURCE_STATE_INDEX_BUFFER, true);
    }
	//*/


    std::vector<SkyboxVertex> skyboxVertices;
    CreateSkyboxMesh(&skyboxVertices);


    // create skybox vertex buffer
    {
        BufferDesc desc;
        desc.Name = "Skybox Vertex Buffer";
        desc.uiSizeInBytes = (Uint32)skyboxVertices.size() * sizeof(skyboxVertices[0]);
        desc.BindFlags = BIND_VERTEX_BUFFER;
        desc.Usage = USAGE_DEFAULT;

        BufferData data;
        data.pData = skyboxVertices.data();
        data.DataSize = desc.uiSizeInBytes;

        mDevice->CreateBuffer(desc, data, &mSkyboxVertexBuffer);
        Barriers.emplace_back(mSkyboxVertexBuffer, RESOURCE_STATE_UNKNOWN, RESOURCE_STATE_VERTEX_BUFFER, true);
    }

    // create sprite vertex buffer (dynamic)
    {
        BufferDesc desc;
        desc.Name = "sprite vertex buffer (dynamic)";
        desc.uiSizeInBytes = (Uint32)MAX_SPRITE_VERTICES_PER_FRAME * sizeof(SpriteVertex);
        desc.BindFlags = BIND_VERTEX_BUFFER;
        desc.Usage = USAGE_DYNAMIC;
        desc.CPUAccessFlags = CPU_ACCESS_WRITE;
        
        mDevice->CreateBuffer(desc, BufferData(), &mSpriteVertexBuffer);
        Barriers.emplace_back(mSpriteVertexBuffer, RESOURCE_STATE_UNKNOWN, RESOURCE_STATE_VERTEX_BUFFER, true);
    }
    mDeviceCtxt->TransitionResourceStates(static_cast<Uint32>(Barriers.size()), Barriers.data());
}

void Engine::InitializeTextureData()
{
    TextureDesc textureDesc;
    textureDesc.Type			= RESOURCE_DIM_TEX_2D_ARRAY;
    textureDesc.Width           = TEXTURE_DIM;
    textureDesc.Height          = TEXTURE_DIM;
    textureDesc.ArraySize       = 3;
    textureDesc.MipLevels       = 0; // Full chain
    textureDesc.Format          = TEX_FORMAT_RGBA8_UNORM_SRGB;
    textureDesc.SampleCount     = 1;
    textureDesc.Usage           = USAGE_DEFAULT;
    textureDesc.BindFlags       = BIND_SHADER_RESOURCE;

    std::vector<StateTransitionDesc> Barriers;

	//*
    for (UINT t = 0; t < NUM_UNIQUE_TEXTURES; ++t) 
	{
        std::vector<TextureSubResData> subResData( size_t{textureDesc.ArraySize} * size_t{ m_pAst->GetTextureMipLevels() } );
        auto *texData = m_pAst->TextureData(t);
        for(size_t subRes=0; subRes < subResData.size(); ++subRes)
        {
            subResData[subRes].pData = texData[subRes].pSysMem;
            subResData[subRes].Stride = texData[subRes].SysMemPitch;
            subResData[subRes].DepthStride = texData[subRes].SysMemSlicePitch;
        }
        TextureData initData;
        initData.NumSubresources = (Uint32)subResData.size();
        initData.pSubResources = subResData.data();
        mDevice->CreateTexture(textureDesc, initData, &mTextures[t]);
        mTextureSRVs[t] = mTextures[t]->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
        mTextureSRVs[t]->SetSampler(mSamplerState);
        Barriers.emplace_back(mTextures[t], RESOURCE_STATE_UNKNOWN, RESOURCE_STATE_SHADER_RESOURCE, true);
    }

    mDeviceCtxt->TransitionResourceStates(static_cast<Uint32>(Barriers.size()), Barriers.data());
	//*/
}

void Engine::CreateGUIResources()
{
    auto font = mGUI->Font();
    TextureDesc textureDesc;
    textureDesc.Type = RESOURCE_DIM_TEX_2D;
    textureDesc.Format = TEX_FORMAT_R8_UNORM;
    textureDesc.Width = font->BitmapWidth();
    textureDesc.Height = font->BitmapHeight();
    textureDesc.MipLevels = 1;
    textureDesc.ArraySize = 1;
    textureDesc.BindFlags = BIND_SHADER_RESOURCE;
    textureDesc.Usage = USAGE_DEFAULT;

    TextureData initialData;
    TextureSubResData subresources[] = {
        TextureSubResData((void*)font->Pixels(), font->BitmapWidth())
    };
    initialData.pSubResources = subresources;
    initialData.NumSubresources = _countof(subresources);

    RefCntAutoPtr<ITexture> texture;
    mDevice->CreateTexture(textureDesc, initialData, &texture);
    mFontTextureSRV = texture->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
    mFontTextureSRV->SetSampler(mSamplerState);

    // Load any GUI sprite textures
    for (int i = -1; i < (int)mGUI->size(); ++i) {
        auto control = i >= 0 ? (*mGUI)[i] : mSprite.get();
        if (control->TextureFile().length() > 0 && mSpriteTextures.find(control->TextureFile()) == mSpriteTextures.end()) {
            std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;            
            auto path = Diligent::NarrowString(converter.from_bytes(control->TextureFile()).c_str());
            TextureLoadInfo loadInfo;
            loadInfo.IsSRGB = true;
            RefCntAutoPtr<ITexture> spriteTexture;
            CreateTextureFromFile(path.c_str(), loadInfo, mDevice, &spriteTexture);
            auto *pSRV = spriteTexture->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
            pSRV->SetSampler(mSamplerState);
            mSpriteTextures[control->TextureFile()] = pSRV;
        }
    }
}

static_assert(sizeof(IndexType) == 2, "Expecting 16-bit index buffer");

void Engine::WorkerThreadFunc(Engine *pThis, Diligent::Uint32 ThreadNum)
{
	cb::Profiler::ReqEnabled(true);
	cb::Profiler::Frame();

    for (;;)
    {
		{
			PROFILE_FN();
			// Wait for UpdateSubsets signal
			auto SignalledValue = pThis->mUpdateSubsetsSignal.Wait();
			if( SignalledValue < 0 )
				return;

			auto SubsetSize = NUM_ASTEROIDS / pThis->mNumSubsets;
			auto SubsetStart = SubsetSize * ( ThreadNum + 1 );
			auto & FrameAttribs = pThis->mFrameAttribs;

			pThis->m_pAst->Update( FrameAttribs.frameTime, FrameAttribs.camera->Eye(), *FrameAttribs.settings, SubsetStart, SubsetSize );

			// Increment number of completed threads
			++pThis->m_NumThreadsCompleted;


			// Wait for RenderSubsets signal
			pThis->mRenderSubsetsSignal.Wait();

			pThis->RenderSubset( 1 + ThreadNum, pThis->mDeferredCtxt[ThreadNum], *FrameAttribs.camera, SubsetStart, SubsetSize );

			RefCntAutoPtr<ICommandList> pCmdList;
			pThis->mCmdLists[ThreadNum].Release();
			pThis->mDeferredCtxt[ThreadNum]->FinishCommandList( &pThis->mCmdLists[ThreadNum] );

			// Increment number of completed threads
			++pThis->m_NumThreadsCompleted;

		}
		cb::Profiler::Frame();
    }
}

void Engine::RenderSubset(Diligent::Uint32 SubsetNum,
                             IDeviceContext *pCtx, 
                             const OrbitCamera& camera, 
                             Uint32 startIdx, 
                             Uint32 numAsteroids)
{
	PROFILE_FN();

    pCtx->SetRenderTargets(0, nullptr, nullptr, RESOURCE_STATE_TRANSITION_MODE_VERIFY);
    
    // Frame data
    auto staticAsteroidData = m_pAst->StaticData();
    auto dynamicAsteroidData = m_pAst->DynamicData();
    
    pCtx->SetPipelineState(mAsteroidsPSO);

    {
        IBuffer* ia_buffers[] = { mVertexBuffer };
        Uint32 ia_offsets[] = { 0 };
        pCtx->SetVertexBuffers(0, 1, ia_buffers, ia_offsets, RESOURCE_STATE_TRANSITION_MODE_VERIFY, SET_VERTEX_BUFFERS_FLAG_NONE);
        pCtx->SetIndexBuffer(mIndexBuffer, 0, RESOURCE_STATE_TRANSITION_MODE_VERIFY);
    }

    auto pVar = m_BindingMode == BindingMode::Dynamic ? mAsteroidsSRBs[SubsetNum]->GetVariable(SHADER_TYPE_PIXEL, "Tex") : nullptr;
    auto viewProjection = camera.ViewProjection();

	//*
    for (UINT drawIdx = startIdx; drawIdx < startIdx+numAsteroids; ++drawIdx)
    {
        auto staticData = &staticAsteroidData[drawIdx];
        auto dynamicData = &dynamicAsteroidData[drawIdx];

        {
            MapHelper<DrawConstantBuffer> drawConstants(pCtx, mDrawConstantBuffer, MAP_WRITE, MAP_FLAG_DISCARD);
            XMStoreFloat4x4(&drawConstants->mWorld,          dynamicData->world);
            XMStoreFloat4x4(&drawConstants->mViewProjection, viewProjection);
            drawConstants->mSurfaceColor = staticData->surfaceColor;
            drawConstants->mDeepColor    = staticData->deepColor;
        }

        if( m_BindingMode == BindingMode::Dynamic )
        {
			PROFILE_FN( _CommitShaderRes_Dynamic );
			pVar->Set(mTextureSRVs[staticData->textureIndex]);
            pCtx->CommitShaderResources(mAsteroidsSRBs[SubsetNum], RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        }
        else if( m_BindingMode == BindingMode::Mutable )
        {
			PROFILE_FN( _CommitShaderRes_Mutable );
			pCtx->CommitShaderResources(mAsteroidsSRBs[drawIdx], RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        }
        else if( m_BindingMode == BindingMode::TextureMutable )
        {
			PROFILE_FN( _CommitShaderRes_TexMutable );
			pCtx->CommitShaderResources(mAsteroidsSRBs[staticData->textureIndex], RESOURCE_STATE_TRANSITION_MODE_VERIFY);
        }

		{
			PROFILE_FN( _DrawAttribs );
			DrawAttribs attribs( dynamicData->indexCount, VT_UINT16, DRAW_FLAG_VERIFY_STATES );
			attribs.FirstIndexLocation = dynamicData->indexStart;
			attribs.BaseVertex = staticData->vertexStart;
			pCtx->Draw( attribs );
		}
    }
	//*/
}

void Engine::RenderBegin( float frameTime, const OrbitCamera& camera, const Settings& settings )
{
	mFrameAttribs.frameTime = frameTime;
	mFrameAttribs.camera = &camera;
	mFrameAttribs.settings = &settings;

	// Clear the render target
	float clearcol[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	mDeviceCtxt->SetRenderTargets( 0, nullptr, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION );
	mDeviceCtxt->ClearRenderTarget( nullptr, clearcol, RESOURCE_STATE_TRANSITION_MODE_VERIFY );
	mDeviceCtxt->ClearDepthStencil( nullptr, CLEAR_DEPTH_FLAG, 0.0f, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION );

	LONG64 currCounter;
	QueryPerformanceCounter( (LARGE_INTEGER*)& currCounter );
	mUpdateTicks = currCounter;



}
	

void Engine::RenderObjects( float frameTime, const OrbitCamera & camera, const Settings & settings )
{
	PROFILE_FN();

	LONG64 currCounter;

	auto SubsetSize = NUM_ASTEROIDS / ( settings.multithreadedRendering ? mNumSubsets : 1 );

	if( settings.multithreadedRendering )
	{
		m_NumThreadsCompleted = 0;
		mUpdateSubsetsSignal.Trigger( true );
	}

	m_pAst->Update( frameTime, camera.Eye(), settings, 0, SubsetSize );


	if( settings.multithreadedRendering )
	{
		PROFILE_FN( _Yield );
		// Wait for worker threads to finish
		while( m_NumThreadsCompleted < (int)mNumSubsets - 1 )
			std::this_thread::yield();
		// Reset mUpdateSubsetsSignal while all threads are waiting for mRenderSubsetsSignal
		mUpdateSubsetsSignal.Reset();
	}


	QueryPerformanceCounter( (LARGE_INTEGER*)& currCounter );
	mUpdateTicks = currCounter - mUpdateTicks;

	mRenderTicks = currCounter;

	if( settings.multithreadedRendering )
	{
		// Signal RenderSubsets
		m_NumThreadsCompleted = 0;
		mRenderSubsetsSignal.Trigger( true );
	}

	RenderSubset( 0, mDeviceCtxt, camera, 0, SubsetSize );

	if( settings.multithreadedRendering )
	{
		PROFILE_FN( _ExecCmdList );

		// Wait for worker threads to finish
		while( m_NumThreadsCompleted < (int)mNumSubsets - 1 )
			std::this_thread::yield();
		// Reset mRenderSubsetsSignal while all threads are waiting for mUpdateSubsetsSignal
		mRenderSubsetsSignal.Reset();

		for( auto& cmdList : mCmdLists )
		{
			mDeviceCtxt->ExecuteCommandList( cmdList );
			// Release command lists now to release all outstanding references
			// In d3d11 mode, command lists hold references to the swap chain's back buffer 
			// that cause swap chain resize to fail
			cmdList.Release();
		}
	}

	// Call FinishFrame() to release dynamic resources allocated by deferred contexts
	// IMPORTANT: we must wait until the command lists are submitted for execution
	// because FinishFrame() invalidates all dynamic resources
	{
		PROFILE_FN( _FinishFrame );

		for( auto& ctx : mDeferredCtxt )
			ctx->FinishFrame();
	}

	mDeviceCtxt->SetRenderTargets( 0, nullptr, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION );

	/*
	// Draw skybox
	{
		{
			MapHelper<SkyboxConstantBuffer> skyboxConstants( mDeviceCtxt, mSkyboxConstantBuffer, MAP_WRITE, MAP_FLAG_DISCARD );
			XMStoreFloat4x4( &skyboxConstants->mViewProjection, camera.ViewProjection() );
		}

		IBuffer* ia_buffers[] = { mSkyboxVertexBuffer };
		UINT ia_offsets[] = { 0 };
		mDeviceCtxt->SetVertexBuffers( 0, 1, ia_buffers, ia_offsets, RESOURCE_STATE_TRANSITION_MODE_VERIFY, SET_VERTEX_BUFFERS_FLAG_NONE );

		mDeviceCtxt->SetPipelineState( mSkyboxPSO );
		mDeviceCtxt->CommitShaderResources( mSkyboxSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION );

		DrawAttribs DrawAttrs( 6 * 6, DRAW_FLAG_VERIFY_STATES );
		mDeviceCtxt->Draw( DrawAttrs );
	}
	//*/

	/*
	// Draw sprites and fonts
	{
		// Fill in vertices (TODO: could move this vector to be a member - not a big deal)
		std::vector<UINT> controlVertices;
		controlVertices.reserve( mGUI->size() );

		{
			MapHelper<SpriteVertex> vertexBase( mDeviceCtxt, mSpriteVertexBuffer, MAP_WRITE, MAP_FLAG_DISCARD );
			auto vertexEnd = (SpriteVertex*)vertexBase;

			for( int i = -1; i < (int)mGUI->size(); ++i ) {
				auto control = i >= 0 ? ( *mGUI )[i] : mSprite.get();
				controlVertices.push_back( (UINT)( control->Draw( (float)mBackBufferWidth, (float)mBackBufferHeight, vertexEnd ) - vertexEnd ) );
				vertexEnd += controlVertices.back();
			}
		}

		IBuffer* ia_buffers[] = { mSpriteVertexBuffer };
		Uint32 ia_offsets[] = { 0 };
		mDeviceCtxt->SetVertexBuffers( 0, 1, ia_buffers, ia_offsets, RESOURCE_STATE_TRANSITION_MODE_VERIFY, SET_VERTEX_BUFFERS_FLAG_NONE );

		// Draw
		UINT vertexStart = 0;
		for( int i = -1; i < (int)mGUI->size(); ++i ) {
			auto control = i >= 0 ? ( *mGUI )[i] : mSprite.get();
			if( control->Visible() ) {
				if( control->TextureFile().length() == 0 ) { // Font
					mDeviceCtxt->SetPipelineState( mFontPSO );
					mDeviceCtxt->CommitShaderResources( mFontSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION );
				}
				else { // Sprite
					auto textureSRV = mSpriteTextures[control->TextureFile()];
					mDeviceCtxt->SetPipelineState( mSpritePSO );
					mSpriteSRB->GetVariable( SHADER_TYPE_PIXEL, "Tex" )->Set( textureSRV );
					mDeviceCtxt->CommitShaderResources( mSpriteSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION );
				}
				DrawAttribs DrawAttrs( controlVertices[1 + i], DRAW_FLAG_VERIFY_STATES );
				DrawAttrs.StartVertexLocation = vertexStart;
				mDeviceCtxt->Draw( DrawAttrs );
			}
			vertexStart += controlVertices[1 + i];
		}
	}
	//*/



}



void Engine::RenderEnd( float frameTime, const OrbitCamera& camera, const Settings& settings )
{

	LONG64 currCounter;



	QueryPerformanceCounter( (LARGE_INTEGER*)& currCounter );
	mRenderTicks = currCounter - mRenderTicks;

	mSwapChain->Present( settings.vsync ? 1 : 0 );
}

	


 
void Engine::GetPerfCounters(float &UpdateTime, float &RenderTime)
{
    UpdateTime = (float)mUpdateTicks/(float)mPerfCounterFreq;
    RenderTime = (float)mRenderTicks/(float)mPerfCounterFreq;
}

} // namespace grx
