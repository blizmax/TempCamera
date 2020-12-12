#include "Projection.h"
#include "imgui/imgui.h"
#include "Graphics.h"

Projection::Projection(Graphics& gfx, float width, float height, float nearZ, float farZ, bool isPerspective)
	:
	width( width ),
	height( height ),
	nearZ( nearZ ),
	farZ( farZ ),
	frust(gfx, width, height, nearZ, farZ, isPerspective),
	homeWidth( width ),homeHeight( height ),homeNearZ( nearZ ),homeFarZ( farZ ),
	isPerspective(isPerspective)
{}

DirectX::XMMATRIX Projection::GetMatrix() const
{
	if(isPerspective)
		return DirectX::XMMatrixPerspectiveLH( width,height,nearZ,farZ );
	else
		return DirectX::XMMatrixOrthographicLH(width, height, nearZ, farZ);
}

void Projection::RenderWidgets( Graphics& gfx )
{
	bool dirty = false;
	const auto dcheck = [&dirty]( bool d ) { dirty = dirty || d; };

	if (isPerspective)
	{
		ImGui::Text("Projection");
		dcheck(ImGui::SliderFloat("Width", &width, 0.01f, 4.0f, "%.2f", 1.5f));
		dcheck(ImGui::SliderFloat("Height", &height, 0.01f, 4.0f, "%.2f", 1.5f));
		dcheck(ImGui::SliderFloat("Near Z", &nearZ, 0.01f, farZ - 0.01f, "%.2f", 4.0f));
		dcheck(ImGui::SliderFloat("Far Z", &farZ, nearZ + 0.01f, 400.0f, "%.2f", 4.0f));
	}
	else
	{
		ImGui::Text("Projection");
		dcheck(ImGui::SliderFloat("Width", &width, 0.01f, 4096.f, "%.2f", 1.0f));
		dcheck(ImGui::SliderFloat("Height", &height, 0.01f, 4096.0f, "%.2f", 1.0f));
		dcheck(ImGui::SliderFloat("Near Z", &nearZ, 0.01f, farZ - 0.01f, "%.2f", 1.0f));
		dcheck(ImGui::SliderFloat("Far Z", &farZ, nearZ + 0.01f, 400.0f, "%.2f", 1.0f));
	}

	if( dirty )
	{
		frust.SetVertices( gfx,width,height,nearZ,farZ );
	}
}

void Projection::SetPos( DirectX::XMFLOAT3 pos )
{
	frust.SetPos( pos );
}

void Projection::SetRotation( DirectX::XMFLOAT3 rot )
{
	frust.SetRotation( rot );
}

void Projection::Submit( size_t channel ) const
{
	frust.Submit( channel );
}

void Projection::LinkTechniques( Rgph::RenderGraph& rg )
{
	frust.LinkTechniques( rg );
}

void Projection::Reset( Graphics& gfx )
{
	width = homeWidth;
	height = homeHeight;
	nearZ = homeNearZ;
	farZ = homeFarZ;
	frust.SetVertices( gfx,width,height,nearZ,farZ );	
}

void Projection::SetProjection(float _width, float _height, float _nearZ, float _farZ)
{
	width = _width;
	height = _height;
	nearZ = _nearZ;
	farZ = _farZ;
}
