cbuffer CBuf : register(b10)
{
	float4 materialColor;
};

TextureCube SkyMap;
SamplerState splr;

float4 main(float3 tc : Texcoord) : SV_Target
{
	return SkyMap.Sample(splr, normalize(tc)) * materialColor;
}

//RasterizerState MyCull {
//	FillMode = WireFrame;
//};
//
//technique11 main11
//{
//	pass p0
//	{
//		SetRasterizerState(MyCull);
//	}
//};