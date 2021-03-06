#include "TestCube.h"
#include "Cube.h"
#include "BindableCommon.h"
#include "ConstantBuffersEx.h"
#include "imgui/imgui.h"
#include "DynamicConstant.h"
#include "TechniqueProbe.h"
#include "TransformCbufScaling.h"
#include "Channels.h"

TestCube::TestCube( Graphics& gfx,float size )
{
	using namespace Bind;
	namespace dx = DirectX;

	auto model = Cube::MakeIndependentTBN();
	model.Transform( dx::XMMatrixScaling( size,size,size ) );
	model.SetTBNIndependentFlat();
	const auto geometryTag = "$cube." + std::to_string( size );
	pVertices = VertexBuffer::Resolve( gfx,geometryTag,model.vertices );
	pIndices = IndexBuffer::Resolve( gfx,geometryTag,model.indices );
	pTopology = Topology::Resolve( gfx,D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST );
	
	auto tcb = std::make_shared<TransformCbufScaling>(gfx);

	{
		Technique shade( "Shade",Chan::main );
		{
			Step only( "lambertian" );

			only.AddBindable( Texture::Resolve( gfx,"Images\\brickwall.jpg" ) );
			only.AddBindable(Texture::Resolve(gfx, "Images\\brickwall_normal.jpg", 2u));
			only.AddBindable( Sampler::Resolve( gfx ) );

			auto pvs = VertexShader::Resolve( gfx,"PhongDifNrm_VS.cso" );
			only.AddBindable( InputLayout::Resolve( gfx,model.vertices.GetLayout(),*pvs ) );
			only.AddBindable( std::move( pvs ) );

			only.AddBindable( PixelShader::Resolve( gfx,"PhongDifNrm_PS.cso" ) );
			
			Dcb::RawLayout lay;
			lay.Add<Dcb::Float3>( "specularColor" );
			lay.Add<Dcb::Float>( "specularWeight" );
			lay.Add<Dcb::Float>( "specularGloss" );
			lay.Add<Dcb::Bool>("useNormalMap");
			lay.Add<Dcb::Float>("normalMapWeight");
			auto buf = Dcb::Buffer( std::move( lay ) );
			buf["specularColor"] = dx::XMFLOAT3{ 1.0f,1.0f,1.0f };
			buf["specularWeight"] = 0.1f;
			buf["specularGloss"] = 20.0f;
			buf["useNormalMap"] = true;
			buf["normalMapWeight"] = 1.0f;
			only.AddBindable( std::make_shared<Bind::CachingPixelConstantBufferEx>( gfx,buf,10u ) );


			only.AddBindable( Rasterizer::Resolve( gfx,false ) );

			only.AddBindable( tcb );

			shade.AddStep( std::move( only ) );
		}
		AddTechnique( std::move( shade ) );
	}
	{
		Technique outline("Outline", Chan::main, false);
		{
			Step mask( "outlineMask" );

			// TODO: better sub-layout generation tech for future consideration maybe
			mask.AddBindable( InputLayout::Resolve( gfx,model.vertices.GetLayout(),*VertexShader::Resolve( gfx,"Solid_VS.cso" ) ) );

			mask.AddBindable( std::move( tcb ) );

			// TODO: might need to specify rasterizer when doubled-sided models start being used

			outline.AddStep( std::move( mask ) );
		}
		{
			Step draw( "outlineDraw" );

			Dcb::RawLayout lay;
			lay.Add<Dcb::Float4>( "color" );
			auto buf = Dcb::Buffer( std::move( lay ) );
			buf["color"] = DirectX::XMFLOAT4{ 1.0f,0.4f,0.4f,1.0f };
			draw.AddBindable( std::make_shared<Bind::CachingPixelConstantBufferEx>( gfx,buf,10u ) );

			// TODO: better sub-layout generation tech for future consideration maybe
			draw.AddBindable( InputLayout::Resolve( gfx,model.vertices.GetLayout(),*VertexShader::Resolve( gfx,"Solid_VS.cso" ) ) );
			
			draw.AddBindable( std::make_shared<TransformCbufScaling>( gfx ) );

			// TODO: might need to specify rasterizer when doubled-sided models start being used

			outline.AddStep( std::move( draw ) );
		}
		AddTechnique( std::move( outline ) );
	}
	// shadow map technique
	{
		Technique map{ "ShadowMap",Chan::shadow,true };
		{
			Step draw( "shadowMap" );

			// TODO: better sub-layout generation tech for future consideration maybe
			draw.AddBindable( InputLayout::Resolve( gfx,model.vertices.GetLayout(),*VertexShader::Resolve( gfx,"Solid_VS.cso" ) ) );

			draw.AddBindable( std::make_shared<TransformCbuf>( gfx ) );

			// TODO: might need to specify rasterizer when doubled-sided models start being used

			map.AddStep( std::move( draw ) );
		}
		AddTechnique( std::move( map ) );
	}
}

void TestCube::SetPos( DirectX::XMFLOAT3 pos ) noexcept
{
	this->pos = pos;
}

void TestCube::SetRotation( float roll,float pitch,float yaw ) noexcept
{
	this->roll = roll;
	this->pitch = pitch;
	this->yaw = yaw;
}

DirectX::XMMATRIX TestCube::GetTransformXM() const noexcept
{
	return DirectX::XMMatrixRotationRollPitchYaw(pitch, yaw, roll) *
		DirectX::XMMatrixTranslation(pos.x, pos.y, pos.z);
}

void TestCube::SpawnControlWindow( Graphics& gfx,const char* name ) noexcept
{
	if( ImGui::Begin( name ) )
	{
		ImGui::Text( "Position" );
		ImGui::SliderFloat( "X",&pos.x,-80.0f,80.0f,"%.1f" );
		ImGui::SliderFloat( "Y",&pos.y,-80.0f,80.0f,"%.1f" );
		ImGui::SliderFloat( "Z",&pos.z,-80.0f,80.0f,"%.1f" );
		ImGui::Text( "Orientation" );
		ImGui::SliderAngle( "Pitch",&pitch,-180.0f,180.0f );
		ImGui::SliderAngle( "Yaw",&yaw,-180.0f,180.0f );
		ImGui::SliderAngle("Roll", &roll, -180.0f, 180.0f);

		class Probe : public TechniqueProbe
		{
		public:
			void OnSetTechnique() override
			{
				using namespace std::string_literals;
				ImGui::TextColored( { 0.4f,1.0f,0.6f,1.0f },pTech->GetName().c_str() );
				bool active = pTech->IsActive();
				ImGui::Checkbox( ("Tech Active##"s + std::to_string( techIdx )).c_str(),&active );
				pTech->SetActiveState( active );
			}
			bool OnVisitBuffer( Dcb::Buffer& buf ) override
			{
				namespace dx = DirectX;
				float dirty = false;
				const auto dcheck = [&dirty]( bool changed ) {dirty = dirty || changed;};
				auto tag = [tagScratch = std::string{},tagString = "##" + std::to_string( bufIdx )]
				( const char* label ) mutable
				{
					tagScratch = label + tagString;
					return tagScratch.c_str();
				};

				if( auto v = buf["scale"]; v.Exists() )
				{
					dcheck( ImGui::SliderFloat( tag( "Scale" ),&v,0.0f,2.0f,"%.3f",1.0f ) );
				}
				if( auto v = buf["color"]; v.Exists() )
				{
					dcheck( ImGui::ColorPicker4( tag( "Color" ),reinterpret_cast<float*>(&static_cast<dx::XMFLOAT4&>(v)) ) );
				}
				if( auto v = buf["specularWeight"]; v.Exists() )
				{
					dcheck( ImGui::SliderFloat( tag( "Spec. Intens." ),&v,0.0f,1.0f ) );
				}
				if( auto v = buf["specularGloss"]; v.Exists() )
				{
					dcheck( ImGui::SliderFloat( tag( "Glossiness" ),&v,1.0f,100.0f,"%.1f",1.5f ) );
				}
				if (auto v = buf["useNormalMap"]; v.Exists())
				{
					dcheck(ImGui::Checkbox(tag("Normal Map Enable"), &v));
				}
				if (auto v = buf["normalMapWeight"]; v.Exists())
				{
					dcheck(ImGui::SliderFloat(tag("Normal Map Weight"), &v, 0.0f, 1.0f, "%.1f"));
				}
				return dirty;
			}
		} probe;

		Accept( probe );
	}
	ImGui::End();
}
