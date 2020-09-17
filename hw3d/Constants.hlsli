cbuffer TransformCBuf : register(b0)//VS, PS
{
	matrix matrix_MVP;
	matrix matrix_MV;
	matrix matrix_V;
	matrix matrix_P;
	matrix matrixmatrix_VP;
	matrix matrixmatrix_T_MV;
	matrix matrix_IT_MV;
	matrix matrix_M2W;
	matrix matrix_W2M;
};

cbuffer CameraCBuf : register(b1)//PS
{
	float3 cameraPos;
	float3 cameraDir;
};