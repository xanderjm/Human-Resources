// Renderer-neutral material helper, not a compiled runtime pipeline.
// Inputs: unit world-space normal and unit direction toward camera.
// Return straight-alpha colour. Use alpha blend, depth test, no depth writes.
// Draw one visible sphere surface; cull backfaces to avoid double opacity.
float4 HumanResourcesShieldRim(float3 normalWS, float3 toCameraWS, float3 tint, float opacity)
{
    float rim = pow(1.0 - saturate(abs(dot(normalize(normalWS), normalize(toCameraWS)))), 3.5);
    return float4(tint, saturate(opacity * (0.025 + rim * 0.55)));
}
