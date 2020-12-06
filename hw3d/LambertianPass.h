#pragma once
#include "RenderQueuePass.h"
#include "Job.h"
#include <vector>
#include "Sink.h"
#include "Source.h"
#include "Stencil.h"
#include "Camera.h"
#include "DepthStencil.h"
#include "ShadowCameraCBuf.h"
#include "ShadowSampler.h"
#include "Blender.h"

class Graphics;

namespace Rgph
{
	class LambertianPass : public RenderQueuePass
	{
	public:
		LambertianPass( Graphics& gfx,std::string name )
			:
			RenderQueuePass( std::move( name ) ),
			pShadowCBuf{ std::make_shared<Bind::ShadowCameraCBuf>(gfx, 5u) }
		{
			using namespace Bind;
			AddBind( pShadowCBuf );
			RegisterSink( DirectBufferSink<RenderTarget>::Make( "renderTarget",renderTarget ) );
			RegisterSink( DirectBufferSink<DepthStencil>::Make( "depthStencil",depthStencil ) );
			AddBindSink<Bindable>( "shadowMap" );
			AddBindSink<Bindable>( "shadowControl" );
			AddBindSink<Bindable>( "shadowSampler" );
			AddBindSink<Bindable>("cubeMapBlurIn");
			AddBindSink<Bindable>("cubeMapMipIn");
			AddBindSink<ShaderInputRenderTarget>("planeBRDFLUTIn");
			RegisterSource( DirectBufferSource<RenderTarget>::Make( "renderTarget",renderTarget ) );
			RegisterSource( DirectBufferSource<DepthStencil>::Make( "depthStencil",depthStencil ) );
			AddBind( Stencil::Resolve( gfx,Stencil::Mode::Off ) );
			AddBind(Blender::Resolve(gfx, false));
		}
		void BindMainCamera( const Camera& cam ) noexcept
		{
			pMainCamera = &cam;
		}
		void BindShadowCamera( const Camera& cam ) noexcept
		{
			pShadowCBuf->SetCamera( &cam );
		}
		void Execute( Graphics& gfx ) const noxnd override
		{
			assert( pMainCamera );
			pShadowCBuf->Update( gfx );
			pMainCamera->BindToGraphics( gfx );
			RenderQueuePass::Execute( gfx );
		}
	private:
		std::shared_ptr<Bind::ShadowCameraCBuf> pShadowCBuf;
		const Camera* pMainCamera = nullptr;
	};
}