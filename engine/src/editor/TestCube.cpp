#include "TestCube.h"

#include "Application.h"

#include "RenderDevice.h"
#include "SwapChain.h"
#include "MapHelper.hpp"
#include "BasicMath.hpp"

using namespace bt;
using namespace Diligent;

TestCube::TestCube() {
    // Pipeline state object encompasses configuration of all GPU stages

    auto app = gTheApp.get();
    auto engineFactory = app->GetEngineFactory();
    auto swapChain = app->GetSwapChain();
    auto renderDevice = app->GetRenderDevice();

    GraphicsPipelineStateCreateInfo PSOCreateInfo;

    // Pipeline state name is used by the engine to report issues.
    // It is always a good idea to give objects descriptive names.
    PSOCreateInfo.PSODesc.Name = "Cube PSO";

    // This is a graphics pipeline
    PSOCreateInfo.PSODesc.PipelineType = PIPELINE_TYPE_GRAPHICS;

    // clang-format off
    // This tutorial will render to a single render target
    PSOCreateInfo.GraphicsPipeline.NumRenderTargets = 1;
    // Set render target format which is the format of the swap chain's color buffer
    PSOCreateInfo.GraphicsPipeline.RTVFormats[0] = swapChain->GetDesc().ColorBufferFormat;
    // Set depth buffer format which is the format of the swap chain's back buffer
    PSOCreateInfo.GraphicsPipeline.DSVFormat = swapChain->GetDesc().DepthBufferFormat;
    // Primitive topology defines what kind of primitives will be rendered by this pipeline state
    PSOCreateInfo.GraphicsPipeline.PrimitiveTopology = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    // Cull back faces
    PSOCreateInfo.GraphicsPipeline.RasterizerDesc.CullMode = CULL_MODE_BACK;
    // Enable depth testing
    PSOCreateInfo.GraphicsPipeline.DepthStencilDesc.DepthEnable = True;
    // clang-format on

    ShaderCreateInfo ShaderCI;
    // Tell the system that the shader source code is in HLSL.
    // For OpenGL, the engine will convert this into GLSL under the hood.
    ShaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;

    // OpenGL backend requires emulated combined HLSL texture samplers (g_Texture + g_Texture_sampler combination)
    ShaderCI.UseCombinedTextureSamplers = true;

    // In this tutorial, we will load shaders from file. To be able to do that,
    // we need to create a shader source stream factory
    RefCntAutoPtr<IShaderSourceInputStreamFactory> pShaderSourceFactory;
    engineFactory->CreateDefaultShaderSourceStreamFactory(nullptr, &pShaderSourceFactory);
    ShaderCI.pShaderSourceStreamFactory = pShaderSourceFactory;
    // Create a vertex shader
    RefCntAutoPtr<IShader> pVS;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_VERTEX;
        ShaderCI.EntryPoint = "main";
        ShaderCI.Desc.Name = "Cube VS";
        ShaderCI.FilePath = "d:/assets/cube.vsh";
        renderDevice->CreateShader(ShaderCI, &pVS);
        // Create dynamic uniform buffer that will store our transformation matrix
        // Dynamic buffers can be frequently updated by the CPU
        BufferDesc CBDesc;
        CBDesc.Name = "VS constants CB";
        CBDesc.Size = sizeof(float4x4);
        CBDesc.Usage = USAGE_DYNAMIC;
        CBDesc.BindFlags = BIND_UNIFORM_BUFFER;
        CBDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
        renderDevice->CreateBuffer(CBDesc, nullptr, &m_VSConstants);
    }

    // Create a pixel shader
    RefCntAutoPtr<IShader> pPS;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_PIXEL;
        ShaderCI.EntryPoint = "main";
        ShaderCI.Desc.Name = "Cube PS";
        ShaderCI.FilePath = "d:/assets/cube.psh";
        renderDevice->CreateShader(ShaderCI, &pPS);
    }

    // clang-format off
    // Define vertex shader input layout
    LayoutElement LayoutElems[] =
        {
            // Attribute 0 - vertex position
            LayoutElement{0, 0, 3, VT_FLOAT32, False},
            // Attribute 1 - vertex color
            LayoutElement{1, 0, 4, VT_FLOAT32, False}
        };
    // clang-format on
    PSOCreateInfo.GraphicsPipeline.InputLayout.LayoutElements = LayoutElems;
    PSOCreateInfo.GraphicsPipeline.InputLayout.NumElements = _countof(LayoutElems);

    PSOCreateInfo.pVS = pVS;
    PSOCreateInfo.pPS = pPS;

    // Define variable type that will be used by default
    PSOCreateInfo.PSODesc.ResourceLayout.DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC;

    renderDevice->CreateGraphicsPipelineState(PSOCreateInfo, &m_pPSOCube);


    // Since we did not explcitly specify the type for 'Constants' variable, default
    // type (SHADER_RESOURCE_VARIABLE_TYPE_STATIC) will be used. Static variables never
    // change and are bound directly through the pipeline state object.
    m_pPSOCube->GetStaticVariableByName(SHADER_TYPE_VERTEX, "Constants")->Set(m_VSConstants);

    // Create a shader resource binding object and bind all static resources in it
    m_pPSOCube->CreateShaderResourceBinding(&m_pSRB, true);

    CreateVertexBuffer();
    CreateIndexBuffer();
}

static double CubeRotation = 0.f;
static double CubeRotationSpeed = 2.f;


void TestCube::Update(double CurrTime, double ElapsedTime) {
    if (gInputManager->IsKeyPressed(bt::input::Key::A)) {
        CubeRotation += ElapsedTime * CubeRotationSpeed;
    }
    if (gInputManager->IsKeyPressed(bt::input::Key::D)) {
        CubeRotation -= ElapsedTime * CubeRotationSpeed;
    }


    mCubeModelTransform = glm::translate(glm::mat4(1.0f), mLocation);

    mCubeModelTransform = glm::rotate(mCubeModelTransform, static_cast<float>(CubeRotation),
                                      glm::vec3(0.f, 1.f, 0.f));
}

void TestCube::DrawCube(const glm::mat4& ProjView) {
    auto immediateContext = gTheApp->GetImmediateContext();
    {
        // Map the buffer and write current world-view-projection matrix
        MapHelper<glm::mat4> CBConstants(immediateContext, m_VSConstants, MAP_WRITE, MAP_FLAG_DISCARD);

        auto PVW = ProjView * mCubeModelTransform;
        *CBConstants = glm::transpose(PVW);//(View * Proj).Transpose();//m_WorldViewProjMatrix.Transpose();
    }

    // Bind vertex and index buffers
    const Uint64 offset = 0;
    IBuffer *pBuffs[] = {m_CubeVertexBuffer};
    immediateContext->SetVertexBuffers(0, 1, pBuffs, &offset, RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
                                          SET_VERTEX_BUFFERS_FLAG_RESET);
    immediateContext->SetIndexBuffer(m_CubeIndexBuffer, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    // Set the pipeline state
    immediateContext->SetPipelineState(m_pPSOCube);
    // Commit shader resources. RESOURCE_STATE_TRANSITION_MODE_TRANSITION mode
    // makes sure that resources are transitioned to required states.
    immediateContext->CommitShaderResources(m_pSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    DrawIndexedAttribs DrawAttrs;     // This is an indexed draw call
    DrawAttrs.IndexType = VT_UINT32; // Index type
    DrawAttrs.NumIndices = 36;
    // Verify the state of vertex and index buffers
    DrawAttrs.Flags = DRAW_FLAG_VERIFY_ALL;
    immediateContext->DrawIndexed(DrawAttrs);
}

void TestCube::CreateVertexBuffer() {
    auto device = gTheApp->GetRenderDevice();
    // Layout of this structure matches the one we defined in the pipeline state
    struct Vertex {
      float3 pos;
      float4 color;
    };

    // clang-format off
    Vertex CubeVerts[8] =
        {
            {float3(-1, -1, -1), float4(1, 0, 0, 1)},
            {float3(-1, +1, -1), float4(0, 1, 0, 1)},
            {float3(+1, +1, -1), float4(0, 0, 1, 1)},
            {float3(+1, -1, -1), float4(1, 1, 1, 1)},

            {float3(-1, -1, +1), float4(1, 1, 0, 1)},
            {float3(-1, +1, +1), float4(0, 1, 1, 1)},
            {float3(+1, +1, +1), float4(1, 0, 1, 1)},
            {float3(+1, -1, +1), float4(0.2f, 0.2f, 0.2f, 1)},
        };
    // clang-format on

    // Create a vertex buffer that stores cube vertices
    BufferDesc VertBuffDesc;
    VertBuffDesc.Name = "Cube vertex buffer";
    VertBuffDesc.Usage = USAGE_IMMUTABLE;
    VertBuffDesc.BindFlags = BIND_VERTEX_BUFFER;
    VertBuffDesc.Size = sizeof(CubeVerts);
    BufferData VBData;
    VBData.pData = CubeVerts;
    VBData.DataSize = sizeof(CubeVerts);
    device->CreateBuffer(VertBuffDesc, &VBData, &m_CubeVertexBuffer);
}

void TestCube::CreateIndexBuffer() {
    auto device = gTheApp->GetRenderDevice();
    // clang-format off
    Uint32 Indices[] =
        {
            2, 0, 1, 2, 3, 0,
            4, 6, 5, 4, 7, 6,
            0, 7, 4, 0, 3, 7,
            1, 0, 4, 1, 4, 5,
            1, 5, 2, 5, 6, 2,
            3, 6, 7, 3, 2, 6
        };
    // clang-format on

    BufferDesc IndBuffDesc;
    IndBuffDesc.Name = "Cube index buffer";
    IndBuffDesc.Usage = USAGE_IMMUTABLE;
    IndBuffDesc.BindFlags = BIND_INDEX_BUFFER;
    IndBuffDesc.Size = sizeof(Indices);
    BufferData IBData;
    IBData.pData = Indices;
    IBData.DataSize = sizeof(Indices);
    device->CreateBuffer(IndBuffDesc, &IBData, &m_CubeIndexBuffer);
}

/*
void Application::CreateResources_Triangle() {
    // Pipeline state object encompasses configuration of all GPU stages

    GraphicsPipelineStateCreateInfo PSOCreateInfo;

    // Pipeline state name is used by the engine to report issues.
    // It is always a good idea to give objects descriptive names.
    PSOCreateInfo.PSODesc.Name = "Simple triangle PSO";

    // This is a graphics pipeline
    PSOCreateInfo.PSODesc.PipelineType = PIPELINE_TYPE_GRAPHICS;

    // clang-format off
    // This tutorial will render to a single render target
    PSOCreateInfo.GraphicsPipeline.NumRenderTargets = 1;
    // Set render target format which is the format of the swap chain's color buffer
    PSOCreateInfo.GraphicsPipeline.RTVFormats[0] = m_pSwapChain->GetDesc().ColorBufferFormat;
    // Use the depth buffer format from the swap chain
    PSOCreateInfo.GraphicsPipeline.DSVFormat = m_pSwapChain->GetDesc().DepthBufferFormat;
    // Primitive topology defines what kind of primitives will be rendered by this pipeline state
    PSOCreateInfo.GraphicsPipeline.PrimitiveTopology = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    // No back face culling for this tutorial
    PSOCreateInfo.GraphicsPipeline.RasterizerDesc.CullMode = CULL_MODE_NONE;
    // Disable depth testing
    PSOCreateInfo.GraphicsPipeline.DepthStencilDesc.DepthEnable = False;
    // clang-format on

    ShaderCreateInfo ShaderCI;
    // Tell the system that the shader source code is in HLSL.
    // For OpenGL, the engine will convert this into GLSL under the hood
    ShaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
    // OpenGL backend requires emulated combined HLSL texture samplers (g_Texture + g_Texture_sampler combination)
    ShaderCI.UseCombinedTextureSamplers = true;
    // Create a vertex shader
    RefCntAutoPtr<IShader> pVS;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_VERTEX;
        ShaderCI.EntryPoint = "main";
        ShaderCI.Desc.Name = "Triangle vertex shader";
        ShaderCI.Source = VSSource;
        m_pDevice->CreateShader(ShaderCI, &pVS);
    }

    // Create a pixel shader
    RefCntAutoPtr<IShader> pPS;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_PIXEL;
        ShaderCI.EntryPoint = "main";
        ShaderCI.Desc.Name = "Triangle pixel shader";
        ShaderCI.Source = PSSource;
        m_pDevice->CreateShader(ShaderCI, &pPS);
    }

    // Finally, create the pipeline state
    PSOCreateInfo.pVS = pVS;
    PSOCreateInfo.pPS = pPS;
    m_pDevice->CreateGraphicsPipelineState(PSOCreateInfo, &m_pPSOTriangle);
}

 void Application::DrawTriangle() {
  // Set the pipeline state in the immediate context
  m_pImmediateContext->SetPipelineState(m_pPSOTriangle);

  // Typically we should now call CommitShaderResources(), however shaders in this example don't
  // use any resources.

  DrawAttribs drawAttrs;
  drawAttrs.NumVertices = 3; // Render 3 vertices
  m_pImmediateContext->Draw(drawAttrs);
}


*/

