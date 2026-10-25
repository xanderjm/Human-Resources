struct VertexOutput {
  float4 position : SV_Position;
  float4 color : TEXCOORD0;
};

cbuffer UiDraw : register(b0, space1) {
  float4 draw_rect;
  float4 viewport;
  float4 draw_color;
};

VertexOutput vertex_main(uint vertex_id : SV_VertexID) {
  const float2 positions[6] = {
      float2(0.0, 0.0),
      float2(1.0, 0.0),
      float2(0.0, 1.0),
      float2(0.0, 1.0),
      float2(1.0, 0.0),
      float2(1.0, 1.0),
  };
  const float2 pixel = draw_rect.xy + positions[vertex_id] * draw_rect.zw;
  float2 normalized = pixel / viewport.xy * 2.0 - 1.0;
  normalized.y = -normalized.y;
  VertexOutput output;
  output.position = float4(normalized, 0.0, 1.0);
  output.color = draw_color;
  return output;
}

float4 fragment_main(VertexOutput input) : SV_Target0 {
  return input.color;
}
