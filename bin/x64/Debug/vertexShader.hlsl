// Entry point: VSMain
// Target profile: vs_5_0

// -----------------------------------------------------------
// Constant Buffers (Root Constants)
// -----------------------------------------------------------
// Matches RendererBase::init Root Parameters:
// b0 -> View Matrix
// b1 -> Projection Matrix
// b2 -> Model Matrix

cbuffer ViewConstants : register(b0)
{
    matrix viewMat;
};

cbuffer ProjConstants : register(b1)
{
    matrix projMat;
};

cbuffer ModelConstants : register(b2)
{
    matrix modelMat;
};

// -----------------------------------------------------------
// Input / Output Structures
// -----------------------------------------------------------

struct VS_INPUT
{
    // Corresponds to "POSITION" in D3D12_INPUT_ELEMENT_DESC
    float3 v_vertex : POSITION; 
};

struct VS_OUTPUT
{
    float4 pos : SV_POSITION;       // Output Position (System Value)
    float3 f_worldVertex : POSITION0; // Semantic for Pixel Shader
    float3 f_viewVertex  : POSITION1; // Semantic for Pixel Shader
};

// -----------------------------------------------------------
// Main Shader Function
// -----------------------------------------------------------
VS_OUTPUT VSMain(VS_INPUT input)
{
    VS_OUTPUT output;

    // Convert input to homogenous coordinate
    float4 pos4 = float4(input.v_vertex, 1.0f);

    // Matrix Transforms
    // Note: D3D12 mul(vector, matrix) is used here because GLM data 
    // is column-major but HLSL defaults to row-major reading. 
    // This effectively treats the matrix as transposed, which is 
    // mathematically correct for column-major data if we swap operands.
    
    // 1. Model Space -> World Space
    //float4 worldVertex = mul(pos4, modelMat);
    
    // 2. World Space -> View Space
    //float4 viewVertex = mul(worldVertex, viewMat);
    // 3. View Space -> Clip Space (Position Output)
    //output.pos = mul(viewVertex, projMat);
    //output.pos = float4(input.v_vertex.xy * 0.05, 0.0f, 1.0f);

    // Pass intermediate vectors to Pixel Shader
    //output.f_worldVertex = worldVertex.xyz;
    //output.f_viewVertex = viewVertex.xyz;
    float4 worldVertex = mul(modelMat, pos4);
    float4 viewVertex = mul(viewMat, worldVertex);
    float4 clipVertex = mul(projMat, viewVertex);

    output.pos = clipVertex;
    output.f_worldVertex = worldVertex.xyz;
    output.f_viewVertex = viewVertex.xyz;

    return output;
}

// =======================
// Textured mesh path
// =======================

cbuffer ViewConstants_T : register(b0)
{
    column_major float4x4 viewMat_T;
};
cbuffer ProjConstants_T : register(b1)
{
    column_major float4x4 projMat_T;
};
cbuffer ModelConstants_T : register(b2)
{
    column_major float4x4 modelMat_T;
};

struct VS_INPUT_TEX
{
    float3 v_vertex : POSITION;
    float2 v_uv : TEXCOORD0;
};

struct VS_OUTPUT_TEX
{
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD0;
};

VS_OUTPUT_TEX VSMainTextured(VS_INPUT_TEX input)
{
    VS_OUTPUT_TEX o;
    float4 w = mul(modelMat_T, float4(input.v_vertex, 1.0f));
    float4 v = mul(viewMat_T, w);
    o.pos = mul(projMat_T, v);
    o.uv = input.v_uv;
    return o;
}