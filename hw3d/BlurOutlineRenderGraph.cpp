#include "BlurOutlineRenderGraph.h"
#include "BufferClearPass.h"
#include "LambertianPass.h"
#include "OutlineDrawingPass.h"
#include "OutlineMaskGenerationPass.h"
#include "Source.h"
#include "HorizontalBlurPass.h"
#include "VerticalBlurPass.h"
#include "BlurOutlineDrawingPass.h"
#include "WireframePass.h"
#include "ShadowMappingPass.h"
#include "DynamicConstant.h"
#include "imgui/imgui.h"
#include "ChiliMath.h"
#include "EnvironmentPass.h"
#include "PreCalSimpleCube.h"
#include "WaterPre.h"
#include "LambertianPass_Water.h"
#include "WaterCaustics.h"

namespace Rgph
{
	BlurOutlineRenderGraph::BlurOutlineRenderGraph( Graphics& gfx )
		:
		RenderGraph( gfx ),
		prg(gfx)
	{
		AddGlobalSource(DirectBindableSource<Bind::RenderTarget>::Make("cubeMap", prg.pPreCalSimpleCube));
		AddGlobalSource(DirectBindableSource<Bind::RenderTarget>::Make("cubeMapBlur", prg.pPreCalBlurCube));
		AddGlobalSource(DirectBindableSource<Bind::RenderTarget>::Make("cubeMapMip", prg.pPreCalMipCube));
		AddGlobalSource(DirectBindableSource<Bind::RenderTarget>::Make("planeBRDFLUT", prg.pPreCalLUTPlane));
		{
			auto pass = std::make_unique<BufferClearPass>( "clearRT" );
			pass->SetSinkLinkage( "buffer","$.backbuffer" );
			AppendPass( std::move( pass ) );
		}
		{
			auto pass = std::make_unique<BufferClearPass>( "clearDS" );
			pass->SetSinkLinkage( "buffer","$.masterDepth" );
			AppendPass( std::move( pass ) );
		}
		{
			auto pass = std::make_unique<ShadowMappingPass>( gfx,"shadowMap" );
			AppendPass( std::move( pass ) );
		}
		{
			auto pass = std::make_unique<LambertianPass>( gfx,"lambertian" );
			pass->SetSinkLinkage( "shadowMap","shadowMap.map" );
			pass->SetSinkLinkage("cubeMapBlurIn", "$.cubeMapBlur");
			pass->SetSinkLinkage("cubeMapMipIn", "$.cubeMapMip");
			pass->SetSinkLinkage("planeBRDFLUTIn", "$.planeBRDFLUT");
			pass->SetSinkLinkage( "renderTarget","clearRT.buffer" );
			pass->SetSinkLinkage( "depthStencil","clearDS.buffer" );
			AppendPass( std::move( pass ) );
		}
		{
			auto pass = std::make_unique<EnvironmentPass>(gfx, "environment");
			pass->SetSinkLinkage("cubeMapIn", "$.cubeMap");
			pass->SetSinkLinkage("renderTarget", "lambertian.renderTarget");
			pass->SetSinkLinkage("depthStencil", "lambertian.depthStencil");
			AppendPass(std::move(pass));
		}
		{
			Dcb::RawLayout l;
			l.Add<Dcb::Float4>("amplitude");
			l.Add<Dcb::Float4>("wavespeed");
			l.Add<Dcb::Float4>("wavelength");
			l.Add<Dcb::Float4>("omega");
			l.Add<Dcb::Float4>("Q");
			l.Add<Dcb::Float4>("directionX");
			l.Add<Dcb::Float4>("directionZ");
			Dcb::Buffer buf{ std::move(l) };
			buf["amplitude"] = dx::XMFLOAT4{ 0.071f,0.032f,0.048f,0.063f };
			buf["wavespeed"] = dx::XMFLOAT4{ 0.097f,0.258f,0.179f,0.219f };
			buf["wavelength"] = dx::XMFLOAT4{ 0.887f,0.774f,0.790f,0.844f };
			buf["omega"] = dx::XMFLOAT4{ 0.0f,0.0f,0.0f,0.0f };
			buf["Q"] = dx::XMFLOAT4{ 1.0f,0.871f,0.935f,0.844f };
			buf["directionX"] = dx::XMFLOAT4{ 0.0f,0.113f,0.306f,0.281f };
			buf["directionZ"] = dx::XMFLOAT4{ 0.629f,0.081f,0.484f,0.156f };
			
			waterFlow = std::make_shared<Bind::CachingVertexConstantBufferEx>(gfx, buf, 10u);
			AddGlobalSource(DirectBindableSource<Bind::CachingVertexConstantBufferEx>::Make("waterFlow", waterFlow));
		}
		{
			auto pass = std::make_unique<WaterPrePass>(gfx, "waterPre", gfx.GetWidth(), gfx.GetWidth());
			pass->SetSinkLinkage("waterFlow", "$.waterFlow");
			pass->SetSinkLinkage("renderTarget", "environment.renderTarget");
			pass->SetSinkLinkage("depthStencil", "environment.depthStencil");
			AppendPass(std::move(pass));
		}
		{
			auto pass = std::make_unique<WaterCaustics>(gfx, "waterCaustic", gfx.GetWidth(), gfx.GetWidth());
			AppendPass(std::move(pass));
		}
		{
			auto pass = std::make_unique<LambertianPass_Water>(gfx, "water");
			pass->SetSinkLinkage("shadowMap", "shadowMap.map");
			pass->SetSinkLinkage("cubeMapBlurIn", "$.cubeMapBlur");
			pass->SetSinkLinkage("cubeMapMipIn", "$.cubeMapMip");
			pass->SetSinkLinkage("planeBRDFLUTIn", "$.planeBRDFLUT");
			//pass->SetSinkLinkage("waterPreMap", "waterPre.waterPreOut");
			pass->SetSinkLinkage("waterCausticMap", "waterCaustic.waterCausticOut");
			pass->SetSinkLinkage("renderTarget", "waterPre.renderTarget");
			pass->SetSinkLinkage("depthStencil", "waterPre.depthStencil");
			AppendPass(std::move(pass));
		}
		{
			auto pass = std::make_unique<OutlineMaskGenerationPass>( gfx,"outlineMask" );
			pass->SetSinkLinkage( "depthStencil","water.depthStencil" );
			AppendPass( std::move( pass ) );
		}

		// setup blur constant buffers
		{
			{
				Dcb::RawLayout l;
				l.Add<Dcb::Integer>( "nTaps" );
				l.Add<Dcb::Array>( "coefficients" );
				l["coefficients"].Set<Dcb::Float>( maxRadius * 2 + 1 );
				Dcb::Buffer buf{ std::move( l ) };
				blurKernel = std::make_shared<Bind::CachingPixelConstantBufferEx>( gfx,buf,5u );
				SetKernelGauss( radius,sigma );
				AddGlobalSource( DirectBindableSource<Bind::CachingPixelConstantBufferEx>::Make( "blurKernel",blurKernel ) );
			}
			{
				Dcb::RawLayout l;
				l.Add<Dcb::Bool>( "isHorizontal" );
				Dcb::Buffer buf{ std::move( l ) };
				blurDirection = std::make_shared<Bind::CachingPixelConstantBufferEx>( gfx,buf,6u );
				AddGlobalSource( DirectBindableSource<Bind::CachingPixelConstantBufferEx>::Make( "blurDirection",blurDirection ) );
			}
		}

		{
			auto pass = std::make_unique<BlurOutlineDrawingPass>( gfx,"outlineDraw",gfx.GetWidth(),gfx.GetHeight() );
			AppendPass( std::move( pass ) );
		}
		{
			auto pass = std::make_unique<HorizontalBlurPass>( "horizontal",gfx,gfx.GetWidth(),gfx.GetHeight() );
			pass->SetSinkLinkage( "scratchIn","outlineDraw.scratchOut" );
			pass->SetSinkLinkage( "kernel","$.blurKernel" );
			pass->SetSinkLinkage( "direction","$.blurDirection" );
			AppendPass( std::move( pass ) );
		}
		{
			auto pass = std::make_unique<VerticalBlurPass>( "vertical",gfx );
			pass->SetSinkLinkage( "renderTarget","water.renderTarget" );
			pass->SetSinkLinkage( "depthStencil","outlineMask.depthStencil" );
			pass->SetSinkLinkage( "scratchIn","horizontal.scratchOut" );
			pass->SetSinkLinkage( "kernel","$.blurKernel" );
			pass->SetSinkLinkage( "direction","$.blurDirection" );
			AppendPass( std::move( pass ) );
		}
		{
			auto pass = std::make_unique<WireframePass>( gfx,"wireframe" );
			pass->SetSinkLinkage( "renderTarget","vertical.renderTarget" );
			pass->SetSinkLinkage( "depthStencil","vertical.depthStencil" );
			AppendPass( std::move( pass ) );
		}
		SetSinkTarget( "backbuffer","wireframe.renderTarget" );
		Finalize();
	}

	void BlurOutlineRenderGraph::SetKernelGauss( int radius,float sigma ) noxnd
	{
		assert( radius <= maxRadius );
		auto k = blurKernel->GetBuffer();
		const int nTaps = radius * 2 + 1;
		k["nTaps"] = nTaps;
		float sum = 0.0f;
		for( int i = 0; i < nTaps; i++ )
		{
			const auto x = float( i - radius );
			const auto g = gauss( x,sigma );
			sum += g;
			k["coefficients"][i] = g;
		}
		for( int i = 0; i < nTaps; i++ )
		{
			k["coefficients"][i] = (float)k["coefficients"][i] / sum;
		}
		blurKernel->SetBuffer( k );
	}
	
	void BlurOutlineRenderGraph::SetKernelBox( int radius ) noxnd
	{
		assert( radius <= maxRadius );
		auto k = blurKernel->GetBuffer();
		const int nTaps = radius * 2 + 1;
		k["nTaps"] = nTaps;
		const float c = 1.0f / nTaps;
		for( int i = 0; i < nTaps; i++ )
		{
			k["coefficients"][i] = c;
		}
		blurKernel->SetBuffer( k );
	}
	
	void BlurOutlineRenderGraph::RenderWidgets( Graphics& gfx )
	{
		if( ImGui::Begin( "Kernel" ) )
		{
			bool filterChanged = false;
			{
				const char* items[] = { "Gauss","Box" };
				static const char* curItem = items[0];
				if( ImGui::BeginCombo( "Filter Type",curItem ) )
				{
					for( int n = 0; n < std::size( items ); n++ )
					{
						const bool isSelected = (curItem == items[n]);
						if( ImGui::Selectable( items[n],isSelected ) )
						{
							filterChanged = true;
							curItem = items[n];
							if( curItem == items[0] )
							{
								kernelType = KernelType::Gauss;
							}
							else if( curItem == items[1] )
							{
								kernelType = KernelType::Box;
							}
						}
						if( isSelected )
						{
							ImGui::SetItemDefaultFocus();
						}
					}
					ImGui::EndCombo();
				}
			}

			bool radChange = ImGui::SliderInt( "Radius",&radius,0,maxRadius );
			bool sigChange = ImGui::SliderFloat( "Sigma",&sigma,0.1f,10.0f );
			if( radChange || sigChange || filterChanged )
			{
				if( kernelType == KernelType::Gauss )
				{
					SetKernelGauss( radius,sigma );
				}
				else if( kernelType == KernelType::Box )
				{
					SetKernelBox( radius );
				}
			}
		}
		ImGui::End();
	}
	void Rgph::BlurOutlineRenderGraph::DumpShadowMap( Graphics & gfx,const std::string & path )
	{
		dynamic_cast<ShadowMappingPass&>(FindPassByName( "shadowMap" )).DumpShadowMap( gfx,path );
	}
	void Rgph::BlurOutlineRenderGraph::BindMainCamera( Camera& cam )
	{
		dynamic_cast<EnvironmentPass&>(FindPassByName("environment")).BindMainCamera(cam);
		dynamic_cast<LambertianPass&>(FindPassByName( "lambertian" )).BindMainCamera( cam );
		dynamic_cast<LambertianPass&>(FindPassByName("water")).BindMainCamera(cam);
	}
	void Rgph::BlurOutlineRenderGraph::BindShadowCamera( Camera& cam )
	{
		dynamic_cast<ShadowMappingPass&>(FindPassByName( "shadowMap" )).BindShadowCamera( cam );
		dynamic_cast<LambertianPass&>(FindPassByName( "lambertian" )).BindShadowCamera( cam );
		dynamic_cast<LambertianPass&>(FindPassByName("water")).BindShadowCamera(cam);
	}
}
