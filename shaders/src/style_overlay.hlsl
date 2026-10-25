struct Output { float4 position:SV_Position;float2 uv:TEXCOORD0; };
Output vertex_main(uint id:SV_VertexID) {
  const float2 p[6]={float2(0,0),float2(1,0),float2(0,1),float2(0,1),float2(1,0),float2(1,1)};
  Output o;o.position=float4(p[id]*float2(2,-2)+float2(-1,1),0,1);o.uv=p[id];return o;
}
Texture2D<float4> image:register(t0,space2);
SamplerState image_sampler:register(s0,space2);
float4 fragment_main(Output i):SV_Target0 {float4 c=image.Sample(image_sampler,i.uv);return float4(pow(c.rgb,1./2.2),c.a);}
