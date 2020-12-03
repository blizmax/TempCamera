#pragma once
#include "PreCubeCalculatePass.h"

namespace dx = DirectX;

class Graphics;
namespace Bind
{
	class PixelShader;
	class RenderTarget;
}

namespace Rgph
{
	class PreCalSimpleCube : public PreCubeCalculatePass
	{
	public:
		PreCalSimpleCube(std::string name, Graphics& gfx, unsigned int fullWidth, unsigned int fullHeight, const std::string& path);
		void Execute(Graphics& gfx) const noxnd override;
		void DumpShadowMap(Graphics& gfx, const std::string& path) const;
	public:
		std::shared_ptr<Bind::ShaderInputRenderTarget> pPreCalSimpleCube;
		dx::XMMATRIX viewmatrix[6] =
		{
			dx::XMMatrixLookAtLH({ 0.0f,0.0f,0.0f }, { 1.0f,0.0f,0.0f }, { 0.0f,1.0f,0.0f }),
			dx::XMMatrixLookAtLH({ 0.0f,0.0f,0.0f }, { -1.0f,0.0f,0.0f }, { 0.0f,1.0f,0.0f }),
			dx::XMMatrixLookAtLH({ 0.0f,0.0f,0.0f }, { 0.0f,1.0f,0.0f }, { 0.0f,0.0f,-1.0f }),
			dx::XMMatrixLookAtLH({ 0.0f,0.0f,0.0f }, { 0.0f,-1.0f,0.0f }, { 0.0f,0.0f,1.0f }),
			dx::XMMatrixLookAtLH({ 0.0f,0.0f,0.0f }, { 0.0f,0.0f,1.0f }, { 0.0f,1.0f,0.0f }),
			dx::XMMatrixLookAtLH({ 0.0f,0.0f,0.0f }, { 0.0f,0.0f,-1.0f }, { 0.0f,1.0f,0.0f })
		};
	};
}