RasterizerState rs
	{
	CullMode=None;
	};

matrix View;
matrix Projection;
matrix World;

Texture2D txDiffuse;
SamplerState samLinear
	{
	Filter = MIN_MAG_MIP_LINEAR;
	AddressU = Wrap;
	AddressV = Wrap;
	};

struct VS_INPUT
	{
	float4 Pos : POSITION;
	float2 Tex : TEXCOORD;
	};

struct PS_INPUT
	{
	float4 Pos : SV_POSITION;
	float2 Tex : TEXCOORD0;
	};


//--------------------------------------------------------------------------------------
// Vertex Shader
//--------------------------------------------------------------------------------------
PS_INPUT VS( VS_INPUT input )
	{
	PS_INPUT output = (PS_INPUT)0;
	output.Pos = mul( input.Pos, World );
	output.Pos = mul( output.Pos, View );
	output.Pos = mul( output.Pos, Projection );
	output.Tex = input.Tex;

	return output;
	}


//--------------------------------------------------------------------------------------
// Pixel Shader
//--------------------------------------------------------------------------------------
float4 PS( PS_INPUT input) : SV_Target
	{
	return txDiffuse.Sample( samLinear, input.Tex ) ;
	}


//--------------------------------------------------------------------------------------
technique11 Render
	{
	pass P0
		{
		SetVertexShader( CompileShader( vs_4_0, VS() ) );
		SetGeometryShader( NULL );
		SetPixelShader( CompileShader( ps_4_0, PS() ) );
		SetRasterizerState(rs); 
		}
	}

