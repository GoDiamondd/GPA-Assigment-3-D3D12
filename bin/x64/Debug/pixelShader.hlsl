// Entry point: PSMain
// Target profile: ps_5_0

// -----------------------------------------------------------
// Constants
// -----------------------------------------------------------
// Slot 3 in Root Signature (Assumed based on previous binding logic)
cbuffer ShadingConstants : register(b3)
{
    int shadingModelId; // 5 = UNLIT (RenderViewFrustum), 0 = PROCEDURAL_GRID
    float3 padding;     // Align to 16-byte boundary just in case
};

// -----------------------------------------------------------
// Input Structure
// -----------------------------------------------------------
struct PS_INPUT
{
    float4 pos : SV_POSITION;       
    float3 f_worldVertex : POSITION0; 
    float3 f_viewVertex  : POSITION1; 
};

// -----------------------------------------------------------
// Helper Functions
// -----------------------------------------------------------

float4 WithFog(float4 color, float3 viewVertex)
{
    const float4 FOG_COLOR = float4(0.0, 0.0, 0.0, 1.0); // dark gray instead of black
    const float MAX_DIST = 150.0;
    const float MIN_DIST = 120.0;

    float dis = length(viewVertex);
    float fogFactor = (MAX_DIST - dis) / (MAX_DIST - MIN_DIST);
    fogFactor = clamp(fogFactor, 0.0f, 1.0f);
    fogFactor = fogFactor * fogFactor;

    return lerp(FOG_COLOR, color, fogFactor);
}


float GridFactor(float3 worldPos)
{
    const float2 HALF_LINE_WIDTH = float2(0.05f, 0.05f);
    
    // make it repeat
    float2 fr = frac(worldPos.xz); // GLSL fract -> HLSL frac
    
    // [0.0, 1.0) -> [-1.0, 1.0)
    fr = 2.0f * fr - 1.0f;
    
    // fwidth = abs(ddx) + abs(ddy)
    float2 uvDeriv = fwidth(worldPos.xz);
    
    const float2 lineAA = uvDeriv * 1.5f;
    
    // (approximate) constant pixel width
    float2 finalHalfLineWidth = HALF_LINE_WIDTH * lineAA;
    
    // prevent from too thickness
    finalHalfLineWidth = max(HALF_LINE_WIDTH, finalHalfLineWidth);
    
    const float2 SX = float2(0.0f, 0.0f);   
    const float2 s = smoothstep(SX - finalHalfLineWidth - lineAA, SX - finalHalfLineWidth,          fr);    
    const float2 m = smoothstep(SX + finalHalfLineWidth,          SX + finalHalfLineWidth + lineAA, fr);    
    const float2 res = s - m;
    
    // fade out the line based on the thickness
    const float2 fadeOutRes = res * clamp(HALF_LINE_WIDTH / finalHalfLineWidth, float2(0.0f, 0.0f), float2(1.0f, 1.0f));    
    
    // for the area that is usually has Moire patterns, fade out it
    const float2 deriv2 = clamp(uvDeriv, 0.0f, 1.0f);
    const float2 finalRes = lerp(fadeOutRes, HALF_LINE_WIDTH, deriv2);
    
    return lerp(finalRes.x, 1.0, finalRes.y);
}

// -----------------------------------------------------------
// Main Shader Function
// -----------------------------------------------------------

float4 PSMain(PS_INPUT input) : SV_TARGET
{
    // ShadingModelType::UNLIT corresponds to 5 in existing logic
    if (shadingModelId == 5)
    {
        return float4(1.0f, 0.0f, 0.0f, 1.0f); // RenderViewFrustum
    }
    else
    {
        // RenderGrid
        // in PSMain grid branch
        const float4 groundColor = float4(0.0f, 0.0f, 0.0f, 1.0f);
        const float4 gridColor = float4(0.7f, 0.7f, 0.7f, 1.0f);
        
        float gridF = GridFactor(input.f_worldVertex);
        float4 color = lerp(groundColor, gridColor, gridF);
        
        return WithFog(color, input.f_viewVertex);
        // inside PSMain, else branch (grid)
        //return float4(0.0f, 0.6f, 1.0f, 1.0f);
    }
}

// =======================
// Textured mesh path
// =======================
Texture2D g_tex0 : register(t0);
SamplerState g_samp0 : register(s0);

float4 PSMainTextured(float4 pos : SV_POSITION, float2 uv : TEXCOORD0) : SV_TARGET
{
    return g_tex0.Sample(g_samp0, uv);
}