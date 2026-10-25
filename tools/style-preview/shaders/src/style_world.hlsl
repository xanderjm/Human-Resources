struct Input {
  float3 position : TEXCOORD0;
  float3 normal : TEXCOORD1;
  float2 uv : TEXCOORD2;
  float4 color : TEXCOORD3;
  float2 flags : TEXCOORD4;
};
struct Output {
  float4 position : SV_Position;
  float3 normal : TEXCOORD0;
  float2 uv : TEXCOORD1;
  float4 color : TEXCOORD2;
  float4 light : TEXCOORD3;
  float2 flags : TEXCOORD4;
  float4 texture_style : TEXCOORD5;
  float3 to_camera : TEXCOORD6;
};
struct InstanceData {
  row_major float4x4 model;
  float4 tint;
  float4 params;
  float4 texture_style;
};
cbuffer Scene : register(b0, space1) {
  row_major float4x4 camera;
  row_major float4x4 light_camera;
  float4 animation;
};
cbuffer Instances : register(b1, space1) {
  InstanceData instances[64];
};
float3 effect_palette_to_linear(float3 color) {
  color=saturate(color);
  return lerp(color/12.92, pow((color+.055)/1.055,2.4), step(.04045,color));
}
float3 effect_linear_to_display(float3 color) {
  color=saturate(color);
  return lerp(color*12.92, 1.055*pow(color,1./2.4)-.055, step(.0031308,color));
}
Output vertex_main(Input i, uint instance_id : SV_InstanceID) {
  InstanceData data=instances[instance_id];
  float3 p=i.position;
  float3 n=i.normal;
  if(i.flags.y>0.5) {
    p.y-=0.35;
    float c=cos(animation.x),s=sin(animation.x);
    p=float3(c*p.x+s*p.z,p.y,-s*p.x+c*p.z);
    n=float3(c*n.x+s*n.z,n.y,-s*n.x+c*n.z);
    p.y+=0.35;
  }
  float4 world=mul(data.model,float4(p,1));
  Output o;
  o.position=mul(camera,world);
  o.normal=normalize(mul((float3x3)data.model,n));
  o.uv=i.uv*data.params.zw;
  o.color=i.color*data.tint;
  // Explicit procedural effects use the same sRGB-authored palette as the UI.
  // Vertex/material factors remain linear; alpha remains coverage.
  if(data.params.y>0.5)
    o.color.rgb=i.color.rgb*effect_palette_to_linear(data.tint.rgb);
  o.light=mul(light_camera,world);
  // Orthographic camera row 2 is the viewing axis (sign is irrelevant to rim).
  o.to_camera=normalize(-camera[2].xyz);
  o.texture_style=data.texture_style;
  // Keep imported material emission separate from the procedural palette.
  o.flags=float2(i.flags.x,data.params.y);
  return o;
}
Texture2D<float4> albedo : register(t0, space2);
SamplerState albedo_sampler : register(s0, space2);
Texture2D<float> shadow_map : register(t1, space2);
SamplerComparisonState shadow_sampler : register(s1, space2);
float4 fragment_main(Output i) : SV_Target0 {
  float4 surface=albedo.Sample(albedo_sampler,i.uv);
  surface.rgb=lerp(i.texture_style.rgb,surface.rgb,saturate(i.texture_style.a));
  surface*=i.color;
  // Unity unlit response preserves mint/coral; the old 1.55 boost clipped them.
  // Return before shadow sampling without changing the opaque material path.
  if(i.flags.y>0.5) {
    if(i.flags.y>1.5) {
      float facing=dot(normalize(i.normal),normalize(i.to_camera));
      if(facing<=0) discard; // One visible sphere surface; no double opacity.
      float rim=pow(1-saturate(abs(facing)),3.5);
      surface.a*=saturate(.025+rim*.55);
    }
    return float4(effect_linear_to_display(surface.rgb),surface.a);
  }
  float3 normal=normalize(i.normal);
  float3 sun=normalize(float3(27,43,-25));
  float diffuse=max(0,dot(normal,sun));
  float3 projected=i.light.xyz/i.light.w;
  float2 uv=projected.xy*float2(.5,-.5)+.5;
  float shadow=0;
  float bias=max(.00035,.0011*(1-diffuse));
  [unroll] for(int y=-1;y<=1;y++) [unroll] for(int x=-1;x<=1;x++) {
    shadow+=1-shadow_map.SampleCmpLevelZero(shadow_sampler,uv+float2(x,y)/2048.,projected.z-bias);
  }
  shadow/=9;
  if(any(uv<0)||any(uv>1)) shadow=0;
  // Broad warm key, cool ambient and retained painted values.
  float3 illumination=float3(.39,.43,.52)+float3(.67,.61,.49)*diffuse*(1-shadow*.78);
  float3 rgb=surface.rgb*lerp(illumination,float3(1.55,1.55,1.55),saturate(i.flags.x));
  rgb=pow(max(rgb,0),1./2.2);
  return float4(saturate(rgb),surface.a);
}
