#pragma once

#include <cstdint>

namespace Fufu
{
	// Full bloom pipeline (bright extract + Gaussian blur + composite).
	// All intermediate passes run at half resolution to reduce cost. The output
	// (composite) is full-resolution linear HDR, ready to be consumed by
	// ToneMappingPass.
	//
	// Internal sequence:
	//   HDR source  →  BrightPass (½ res)
	//               →  [BlurH → BlurV] × iterations  (ping-pong ½ res)
	//               →  BloomComposite (full res, additive)
	class BloomPass
	{
	public:
		void init(int width, int height);
		void shutdown();
		void resize(int width, int height);

		// Executes the bloom pipeline and returns the composite texture handle.
		// hdrTex    : full-resolution linear HDR texture (source and destination).
		// threshold : luminance cutoff        (e.g. 1.0)
		// knee      : threshold softness      (e.g. 0.1)
		// strength  : additive intensity      (e.g. 0.04)
		// iterations: H+V blur passes         (1..4)
		uint32_t execute(uint32_t hdrTex,
		                 uint32_t quadVAO,
		                 int width, int height,
		                 float threshold, float knee,
		                 float strength, int iterations);

		uint32_t getOutputTexture() const { return m_CompositeTex; }

	private:
		void createResources(int width, int height);

		uint32_t m_BrightProgram    = 0;
		uint32_t m_BlurProgram      = 0;
		uint32_t m_CompositeProgram = 0;

		// Half-resolution textures (bright + 2 ping-pong for blur)
		uint32_t m_BrightTex  = 0;
		uint32_t m_PingTex    = 0;
		uint32_t m_PongTex    = 0;

		// Full-resolution texture (final composite)
		uint32_t m_CompositeTex = 0;

		uint32_t m_BrightFBO    = 0;
		uint32_t m_PingFBO      = 0;
		uint32_t m_PongFBO      = 0;
		uint32_t m_CompositeFBO = 0;
	};
}
