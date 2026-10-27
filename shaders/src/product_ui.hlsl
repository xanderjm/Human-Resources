// CPU owner: presentation::ScreenQuad (four float4 values, 64 bytes).
struct Quad { float4 rect; float4 uv; float4 color; float4 presentation; };
cbuffer Screen : register(b0,space1) { Quad quads[128]; };
struct Output {
 float4 position:SV_Position;
 float2 uv:TEXCOORD0;
 float4 color:TEXCOORD1;
 float2 local:TEXCOORD2;
 nointerpolation float4 shape:TEXCOORD3;
 nointerpolation float page:TEXCOORD4;
};
Output vertex_main(uint vertex:SV_VertexID,uint instance:SV_InstanceID) {
 const float2 p[6]={float2(0,0),float2(1,0),float2(0,1),float2(0,1),float2(1,0),float2(1,1)};
 Quad q=quads[instance]; Output o;
 float2 position_xy=q.rect.xy+p[vertex]*q.rect.zw;
 position_xy+=float2(p[vertex].y*q.presentation.y,p[vertex].x*q.presentation.z);
 o.position=float4(position_xy*float2(2,-2)+float2(-1,1),0,1);
 o.uv=q.uv.xy+p[vertex]*q.uv.zw; o.color=q.color;
 o.local=p[vertex]; o.shape=q.uv; o.page=q.presentation.x; return o;
}
Texture2D<float4> image:register(t0,space2);
SamplerState image_sampler:register(s0,space2);
Texture2D<float4> enamel:register(t1,space2);
SamplerState enamel_sampler:register(s1,space2);
float4 fragment_main(Output i):SV_Target0 {
 if(i.shape.z<0) {
  float2 extent=i.shape.xy*.5; float radius=-i.shape.z;
  float2 d=abs((i.local-.5)*i.shape.xy)-(extent-radius);
  float distance=length(max(d,0))+min(max(d.x,d.y),0)-radius;
  return float4(i.color.rgb,i.color.a*(1-smoothstep(-.6,.6,distance)));
 }
 float4 sampled=i.page>.5?enamel.Sample(enamel_sampler,i.uv):image.Sample(image_sampler,i.uv);
 return float4(pow(max(sampled.rgb,0),1./2.2)*i.color.rgb,sampled.a*i.color.a);
}
