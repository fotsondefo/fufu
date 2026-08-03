#pragma once

#include "RenderMode.h"

namespace Fufu
{

	// Orthogonal to RenderMode (which governs display/accumulation, not the
	// algorithm): PathTracing does stochastic diffuse GI (noise that converges
	// over time), RayTracing is a classic ray tracer (Whitted) — deterministic
	// direct lighting, recursive reflections/refractions, no diffuse GI. No
	// noise, but no realistic indirect light either (just a slight flat ambient).
	// PathTracing/RayTracing : compute shader.
	// Forward  : direct PBR rasterization, per-fragment lighting.
	// Deferred : G-Buffer rasterization then fullscreen lighting.
	enum class RenderTechnique { PathTracing, RayTracing, Forward, Deferred };

	// None  : single sample at the pixel center, no smoothing — jagged edges
	//         but deterministic result and the cheapest option.
	// SSAA  : supersampling — different sub-pixel jitter at each `sample`
	//         of `samplesPerPixel`, averaged within the same frame (and further
	//         accumulated frame-to-frame in Accumulation mode). The historical
	//         renderer behavior before this setting was added.
	// TAA   : one sample per frame (sub-pixel jitter that varies each frame),
	//         smoothed by an exponential moving average with history —
	//         independent of RenderMode, so smooth even in Realtime where
	//         SSAA/None cannot accumulate.
	// FXAA  : no jitter (clean sample), post-process smoothing via luminance
	//         contrast detection on the final image — cheapest, requires no
	//         extra samples.
	enum class AAMode { None, SSAA, TAA, FXAA };

	// Tone mapping operator applied after each raster/compute pass.
	// None = linear HDR + gamma only (debug), Filmic = Uncharted 2 (gamma built in).
	enum class ToneMappingOperator { None, Reinhard, ACES, Filmic };

	struct RenderSettings
	{
		RenderMode      mode = RenderMode::Accumulation;
		RenderTechnique technique = RenderTechnique::PathTracing;
		AAMode     aaMode = AAMode::SSAA;
		int        maxBounces = 8;      // Maximum bounce depth
		int        samplesPerPixel = 1;      // Samples per pixel per frame
		int        maxAccumFrames = 2048;   // Accumulation limit before stopping
		float      exposure = 1.f;
		float      taaBlendFactor = 0.9f;
		bool       resetOnMove = true;
		ToneMappingOperator tonemapping = ToneMappingOperator::ACES;
		float               gamma       = 2.2f;
		float               shadowBias  = 0.005f;
		bool  bloomEnabled    = false;
		float bloomThreshold  = 1.0f;
		float bloomKnee       = 0.1f;
		float bloomStrength   = 0.04f;
		int   bloomIterations = 2;
		bool  ssaoEnabled   = false;
		int   ssaoSamples   = 32;
		float ssaoRadius    = 0.5f;
		float ssaoBias      = 0.025f;
		float ssaoStrength  = 1.0f;
		bool  dofEnabled    = false;
		float dofFocusDist  = 10.f;
		float dofFocusRange = 3.f;
		float dofMaxBlur    = 8.f;
		int   dofSamples    = 16;
		bool  volEnabled    = false;
		int   volSteps      = 32;
		float volDensity    = 0.02f;
		float volScattering = 0.5f;
		float volAnisotropy = 0.3f;
		float volAmbient    = 0.01f;
		float volMaxDist    = 100.f;
	};

}