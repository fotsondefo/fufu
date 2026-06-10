#include "depch.h"
#include "Project/Scene.h"
#include "Renderer/Renderer.h"
#include "Project/Components.h"
#include "Project/Assets/AssetManager.h"

namespace Fufu 
{

	// ----------------------------------------------------------------
	// Compute shader — path tracing complet
	// GI + shadows + reflections + réfraction
	// ----------------------------------------------------------------

	static const char* s_ComputeSrc = R"(
#version 430 core
layout(local_size_x = 16, local_size_y = 16) in;

layout(rgba32f, binding = 0) uniform image2D u_Output;
layout(rgba32f, binding = 1) uniform image2D u_Accum;

struct Triangle {
    vec4 v0, v1, v2;
    vec4 n0, n1, n2;
    vec2 uv0, uv1, uv2;
    int  materialIndex;
    float _pad;
};

struct Material {
    vec4  albedo;
    float metallic;
    float roughness;
    float emissive;
    float ior;
    int   albedoTexIdx;
    float _pad[3];
};

layout(std430, binding = 2) readonly buffer TriangleBuffer { Triangle triangles[]; };
layout(std430, binding = 3) readonly buffer MaterialBuffer { Material materials[]; };

layout(std140, binding = 4) uniform CameraBlock {
    vec4  camPosition;
    vec4  camForward;
    vec4  camRight;
    vec4  camUp;
    float camFov;
    float camAspect;
    float camNear;
    float _pad;
};

layout(std140, binding = 5) uniform FrameBlock {
    int   frameIndex;
    int   maxBounces;
    int   samplesPerPixel;
    float exposure;
    int   width;
    int   height;
    int   triangleCount;
    int   materialCount;
};

// ---- RNG (PCG hash) ----
uint pcg(inout uint state) {
    state = state * 747796405u + 2891336453u;
    uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return (word >> 22u) ^ word;
}
float rand(inout uint seed) {
    return float(pcg(seed)) / 4294967296.0;
}

// ---- Géométrie ----
struct Ray { vec3 origin; vec3 dir; };

struct HitInfo {
    float t;
    vec3  normal;
    vec2  uv;
    int   materialIndex;
    bool  hit;
};

HitInfo intersectTriangle(Ray ray, Triangle tri) {
    HitInfo info;
    info.hit = false;

    vec3 v0 = tri.v0.xyz, v1 = tri.v1.xyz, v2 = tri.v2.xyz;
    vec3 e1 = v1 - v0, e2 = v2 - v0;
    vec3 h  = cross(ray.dir, e2);
    float a = dot(e1, h);
    if (abs(a) < 1e-6) return info;

    float f = 1.0 / a;
    vec3  s = ray.origin - v0;
    float u = f * dot(s, h);
    if (u < 0.0 || u > 1.0) return info;

    vec3  q = cross(s, e1);
    float v = f * dot(ray.dir, q);
    if (v < 0.0 || u + v > 1.0) return info;

    float t = f * dot(e2, q);
    if (t < 1e-4) return info;

    float w = 1.0 - u - v;
    info.hit           = true;
    info.t             = t;
    info.normal        = normalize(w * tri.n0.xyz + u * tri.n1.xyz + v * tri.n2.xyz);
    info.uv            = w * tri.uv0 + u * tri.uv1 + v * tri.uv2;
    info.materialIndex = tri.materialIndex;
    return info;
}

HitInfo intersectScene(Ray ray) {
    HitInfo closest;
    closest.hit = false;
    closest.t   = 1e30;

    for (int i = 0; i < triangleCount; ++i) {
        HitInfo h = intersectTriangle(ray, triangles[i]);
        if (h.hit && h.t < closest.t)
            closest = h;
    }
    return closest;
}

// ---- Sampling ----
vec3 cosineHemisphere(vec3 normal, inout uint seed) {
    float u1 = rand(seed), u2 = rand(seed);
    float r   = sqrt(u1);
    float phi = 6.28318530718 * u2;
    vec3  t   = normalize(abs(normal.x) > 0.9 ? vec3(0,1,0) : vec3(1,0,0));
    vec3  bt  = cross(normal, t);
    t         = cross(bt, normal);
    return normalize(r * cos(phi) * t + r * sin(phi) * bt + sqrt(1.0 - u1) * normal);
}

vec3 reflect(vec3 v, vec3 n) { return v - 2.0 * dot(v, n) * n; }

vec3 refract(vec3 v, vec3 n, float ior, inout bool tir) {
    float cosI = -dot(v, n);
    float k    = 1.0 - ior * ior * (1.0 - cosI * cosI);
    tir = k < 0.0;
    return tir ? vec3(0) : normalize(ior * v + (ior * cosI - sqrt(k)) * n);
}

// Fresnel Schlick
float fresnel(float cosTheta, float f0) {
    return f0 + (1.0 - f0) * pow(1.0 - cosTheta, 5.0);
}

// ---- Path tracing ----
vec3 tracePath(Ray ray, inout uint seed) {
    vec3 radiance    = vec3(0.0);
    vec3 throughput  = vec3(1.0);

    for (int bounce = 0; bounce < maxBounces; ++bounce) {
        HitInfo hit = intersectScene(ray);

        if (!hit.hit) {
            // Environnement simple — ciel gradient
            float t   = 0.5 * (ray.dir.y + 1.0);
            vec3  sky = mix(vec3(1.0), vec3(0.5, 0.7, 1.0), t);
            radiance += throughput * sky;
            break;
        }

        Material mat = materials[hit.materialIndex];
        vec3 albedo  = mat.albedo.rgb;
        vec3 N       = hit.normal;

        // Émission
        if (mat.emissive > 0.0) {
            radiance += throughput * albedo * mat.emissive;
            break;
        }

        float cosTheta = max(dot(-ray.dir, N), 0.0);
        float f0       = mix(0.04, 1.0, mat.metallic);
        float F        = fresnel(cosTheta, f0);

        // Choix stochastique : réflexion spéculaire / diffuse / réfraction
        float specProb = mix(F, 1.0, mat.metallic);
        float refrProb = (mat.albedo.a < 1.0) ? (1.0 - specProb) * (1.0 - mat.albedo.a) : 0.0;
        float r        = rand(seed);

        if (r < specProb) {
            // Réflexion spéculaire (GGX simplifié via roughness)
            vec3 roughDir = cosineHemisphere(N, seed);
            vec3 specDir  = reflect(ray.dir, N);
            ray.dir    = normalize(mix(specDir, roughDir, mat.roughness * mat.roughness));
            ray.origin = hit.t * ray.dir + ray.origin + N * 1e-4;
            throughput *= albedo;
        }
        else if (r < specProb + refrProb) {
            // Réfraction (verre)
            bool tir;
            float eta = dot(ray.dir, N) < 0.0 ? (1.0 / mat.ior) : mat.ior;
            vec3  refractDir = refract(ray.dir, N, eta, tir);
            ray.dir    = tir ? reflect(ray.dir, N) : refractDir;
            ray.origin = hit.t * ray.dir + ray.origin - N * 1e-4;
            throughput *= albedo;
        }
        else {
            // Diffuse (Lambertian)
            ray.dir    = cosineHemisphere(N, seed);
            ray.origin = hit.t * ray.dir + ray.origin + N * 1e-4;
            throughput *= albedo * (1.0 - mat.metallic);
        }

        // Russian roulette
        float p = max(throughput.r, max(throughput.g, throughput.b));
        if (bounce > 3 && rand(seed) > p) break;
        throughput /= max(p, 1e-4);
    }

    return radiance;
}

void main() {
    ivec2 coord = ivec2(gl_GlobalInvocationID.xy);
    if (coord.x >= width || coord.y >= height) return;

    uint seed = uint(coord.x + coord.y * width) ^ uint(frameIndex * 2654435761u);

    vec3 color = vec3(0.0);
    for (int s = 0; s < samplesPerPixel; ++s) {
        // Jitter sub-pixel pour anti-aliasing
        vec2 jitter = vec2(rand(seed), rand(seed)) - 0.5;
        vec2 uv     = (vec2(coord) + jitter) / vec2(width, height) * 2.0 - 1.0;
        uv.x *= camAspect;

        float scale = tan(camFov * 0.5);
        vec3  dir   = normalize(
            camForward.xyz +
            uv.x * scale * camRight.xyz +
            uv.y * scale * camUp.xyz
        );

        Ray ray;
        ray.origin = camPosition.xyz;
        ray.dir    = dir;
        color += tracePath(ray, seed);
    }
    color /= float(samplesPerPixel);

    // Accumulation
    vec4 prev  = imageLoad(u_Accum, coord);
    vec4 accum = (frameIndex == 0)
        ? vec4(color, 1.0)
        : vec4(prev.rgb + color, prev.a + 1.0);
    imageStore(u_Accum, coord, accum);

    // Tone mapping ACES + exposition
    vec3 result = accum.rgb / accum.a * exposure;
    result = (result * (2.51 * result + 0.03)) / (result * (2.43 * result + 0.59) + 0.14);
    result = pow(clamp(result, 0.0, 1.0), vec3(1.0 / 2.2));

    imageStore(u_Output, coord, vec4(result, 1.0));
}
)";

	// ----------------------------------------------------------------
	// Blit vertex + fragment (quad plein écran)
	// ----------------------------------------------------------------

	static const char* s_BlitVertSrc = R"(
#version 430 core
layout(location = 0) in vec2 a_Pos;
layout(location = 1) in vec2 a_UV;
out vec2 v_UV;
void main() {
    v_UV        = a_UV;
    gl_Position = vec4(a_Pos, 0.0, 1.0);
}
)";

	static const char* s_BlitFragSrc = R"(
#version 430 core
in  vec2      v_UV;
out vec4      fragColor;
uniform sampler2D u_Texture;
void main() {
    fragColor = texture(u_Texture, v_UV);
}
)";

	// ----------------------------------------------------------------
	// Init / Shutdown
	// ----------------------------------------------------------------

	void Renderer::init(int width, int height)
	{
		m_Width = width;
		m_Height = height;

		createTextures();
		createShaders();
		createSSBOs();
		createQuad();

		FUFU_INFO("Renderer initialized ({}x{})", width, height);
	}

	void Renderer::shutdown()
	{
		glDeleteTextures(1, &m_OutputTexture);
		glDeleteTextures(1, &m_AccumTexture);
		glDeleteProgram(m_ComputeProgram);
		glDeleteProgram(m_BlitProgram);
		glDeleteBuffers(1, &m_TriangleSSBO);
		glDeleteBuffers(1, &m_MaterialSSBO);
		glDeleteBuffers(1, &m_CameraUBO);
		glDeleteBuffers(1, &m_FrameDataUBO);
		glDeleteVertexArrays(1, &m_QuadVAO);
		glDeleteBuffers(1, &m_QuadVBO);
	}

	void Renderer::createTextures()
	{
		auto makeTexture = [&](uint32_t& id)
		{
			glGenTextures(1, &id);
			glBindTexture(GL_TEXTURE_2D, id);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F,
				m_Width, m_Height, 0, GL_RGBA, GL_FLOAT, nullptr);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		};

		makeTexture(m_OutputTexture);
		makeTexture(m_AccumTexture);
	}

	void Renderer::createShaders()
	{
		// Compute
		uint32_t cs = compileShader(GL_COMPUTE_SHADER, s_ComputeSrc);
		m_ComputeProgram = linkProgram({ cs });
		glDeleteShader(cs);

		// Blit
		uint32_t vs = compileShader(GL_VERTEX_SHADER, s_BlitVertSrc);
		uint32_t fs = compileShader(GL_FRAGMENT_SHADER, s_BlitFragSrc);
		m_BlitProgram = linkProgram({ vs, fs });
		glDeleteShader(vs);
		glDeleteShader(fs);
	}

	void Renderer::createSSBOs()
	{
		glGenBuffers(1, &m_TriangleSSBO);
		glGenBuffers(1, &m_MaterialSSBO);
		glGenBuffers(1, &m_CameraUBO);
		glGenBuffers(1, &m_FrameDataUBO);
	}

	void Renderer::createQuad()
	{
		float quad[] = {
			// pos       uv
			-1.f, -1.f,  0.f, 0.f,
			 1.f, -1.f,  1.f, 0.f,
			 1.f,  1.f,  1.f, 1.f,
			-1.f, -1.f,  0.f, 0.f,
			 1.f,  1.f,  1.f, 1.f,
			-1.f,  1.f,  0.f, 1.f,
		};

		glGenVertexArrays(1, &m_QuadVAO);
		glGenBuffers(1, &m_QuadVBO);
		glBindVertexArray(m_QuadVAO);
		glBindBuffer(GL_ARRAY_BUFFER, m_QuadVBO);
		glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
		glBindVertexArray(0);
	}

	// ----------------------------------------------------------------
	// Upload scène
	// ----------------------------------------------------------------

	void Renderer::uploadSceneData(Scene& scene)
	{
		m_Triangles.clear();
		m_Materials.clear();

		// Map UUID material ? index GPU
		std::unordered_map<uint32_t, int> matIndexMap;

		scene.each<TransformComponent, MeshComponent, MaterialComponent>(
			[&](entt::entity /*e*/,
				TransformComponent& transform,
				MeshComponent& meshComp,
				MaterialComponent& matComp)
		{
			// Material
			int matIdx = static_cast<int>(m_Materials.size());
			GPUMaterial gpuMat;
			gpuMat.albedo = matComp.albedo;
			gpuMat.metallic = matComp.metallic;
			gpuMat.roughness = matComp.roughness;
			gpuMat.emissive = matComp.emissive;
			gpuMat.ior = 1.5f;
			gpuMat.albedoTexIdx = -1;
			m_Materials.push_back(gpuMat);

			// Mesh
			auto meshAsset = AssetManager::get().getMesh(meshComp.meshPath);
			if (!meshAsset) return;

			glm::mat4 model = transform.getTransform();
			glm::mat3 normalMat = glm::transpose(glm::inverse(glm::mat3(model)));

			for (const auto& sub : meshAsset->getSubMeshes())
			{
				for (size_t i = 0; i + 2 < sub.indices.size(); i += 3)
				{
					const Vertex& a = sub.vertices[sub.indices[i]];
					const Vertex& b = sub.vertices[sub.indices[i + 1]];
					const Vertex& c = sub.vertices[sub.indices[i + 2]];

					GPUTriangle tri;
					glm::vec4 tv = model * glm::vec4(a.position.x, a.position.y, a.position.z, 1.f);
					tri.v0 = glm::vec4(tv.x, tv.y, tv.z, 0.f);

					tv = model * glm::vec4(b.position.x, b.position.y, b.position.z, 1.f);
					tri.v1 = glm::vec4(tv.x, tv.y, tv.z, 0.f);
					
					tv = model * glm::vec4(c.position.x, c.position.y, c.position.z, 1.f);
					tri.v2 = glm::vec4(tv.x, tv.y, tv.z, 0.f);
					
					tri.n0 = glm::vec4(normalMat * a.normal, 0.f);
					tri.n1 = glm::vec4(normalMat * b.normal, 0.f);
					tri.n2 = glm::vec4(normalMat * c.normal, 0.f);
					tri.uv0 = a.uv;
					tri.uv1 = b.uv;
					tri.uv2 = c.uv;
					tri.materialIndex = matIdx;
					m_Triangles.push_back(tri);
				}
			}
		}
		);

		// Upload triangles
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_TriangleSSBO);
		glBufferData(GL_SHADER_STORAGE_BUFFER,
			m_Triangles.size() * sizeof(GPUTriangle),
			m_Triangles.data(), GL_DYNAMIC_DRAW);

		// Upload materials
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_MaterialSSBO);
		glBufferData(GL_SHADER_STORAGE_BUFFER,
			m_Materials.size() * sizeof(GPUMaterial),
			m_Materials.data(), GL_DYNAMIC_DRAW);

		glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

		FUFU_TRACE("Scene uploaded: {} triangles, {} materials",
			m_Triangles.size(), m_Materials.size());
	}

	// ----------------------------------------------------------------
	// Render
	// ----------------------------------------------------------------

	void Renderer::renderScene(Scene& scene)
	{
		// Upload scène si changée
		if (sceneNeedsUpdate(scene))
		{
			uploadSceneData(scene);
			if (m_Settings.resetOnMove)
				resetAccumulation();
		}

		// Caméra
		Entity cam = scene.getPrimaryCamera();
		if (!cam) return;

		auto& camTransform = cam.getComponent<TransformComponent>();
		auto& camComponent = cam.getComponent<CameraComponent>();

		glm::mat4 view = glm::inverse(camTransform.getTransform());
		glm::vec3 forward = glm::normalize(-glm::vec3(view[0][2], view[1][2], view[2][2]));
		glm::vec3 right = glm::normalize(glm::vec3(view[0][0], view[1][0], view[2][0]));
		glm::vec3 up = glm::normalize(glm::vec3(view[0][1], view[1][1], view[2][1]));

		GPUCamera gpuCam;
		gpuCam.position = glm::vec4(camTransform.position.x, camTransform.position.y, camTransform.position.z, 1.f);
		gpuCam.forward = glm::vec4(forward, 0.f);
		gpuCam.right = glm::vec4(right, 0.f);
		gpuCam.up = glm::vec4(up, 0.f);
		gpuCam.fov = camComponent.fov;
		gpuCam.aspectRatio = static_cast<float>(m_Width) / static_cast<float>(m_Height);
		gpuCam.nearPlane = camComponent.nearPlane;

		glBindBuffer(GL_UNIFORM_BUFFER, m_CameraUBO);
		glBufferData(GL_UNIFORM_BUFFER, sizeof(GPUCamera), &gpuCam, GL_DYNAMIC_DRAW);

		// Frame data
		GPUFrameData frameData;
		frameData.frameIndex = (m_Settings.mode == RenderMode::Accumulation)
			? m_FrameIndex : 0;
		frameData.maxBounces = m_Settings.maxBounces;
		frameData.samplesPerPixel = m_Settings.samplesPerPixel;
		frameData.exposure = m_Settings.exposure;
		frameData.width = m_Width;
		frameData.height = m_Height;
		frameData.triangleCount = static_cast<int>(m_Triangles.size());
		frameData.materialCount = static_cast<int>(m_Materials.size());

		glBindBuffer(GL_UNIFORM_BUFFER, m_FrameDataUBO);
		glBufferData(GL_UNIFORM_BUFFER, sizeof(GPUFrameData), &frameData, GL_DYNAMIC_DRAW);

		computePass();
		blitPass();

		// Incrémenter uniquement en accumulation et si pas à la limite
		if (m_Settings.mode == RenderMode::Accumulation &&
			m_FrameIndex < m_Settings.maxAccumFrames)
			++m_FrameIndex;
	}

	void Renderer::computePass()
	{
		glUseProgram(m_ComputeProgram);

		// Textures image
		glBindImageTexture(0, m_OutputTexture, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F);
		glBindImageTexture(1, m_AccumTexture, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F);

		// Buffers
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, m_TriangleSSBO);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, m_MaterialSSBO);
		glBindBufferBase(GL_UNIFORM_BUFFER, 4, m_CameraUBO);
		glBindBufferBase(GL_UNIFORM_BUFFER, 5, m_FrameDataUBO);

		// Dispatch — groupes de 16x16
		int gx = (m_Width + 15) / 16;
		int gy = (m_Height + 15) / 16;
		glDispatchCompute(gx, gy, 1);

		// Barrière avant lecture par le fragment shader
		glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
	}

	void Renderer::blitPass()
	{
		glUseProgram(m_BlitProgram);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, m_OutputTexture);
		glUniform1i(glGetUniformLocation(m_BlitProgram, "u_Texture"), 0);

		glBindVertexArray(m_QuadVAO);
		glDrawArrays(GL_TRIANGLES, 0, 6);
		glBindVertexArray(0);
	}

	// ----------------------------------------------------------------
	// Resize / Reset
	// ----------------------------------------------------------------

	void Renderer::resize(int width, int height)
	{
		m_Width = width;
		m_Height = height;

		glDeleteTextures(1, &m_OutputTexture);
		glDeleteTextures(1, &m_AccumTexture);
		createTextures();
		resetAccumulation();
	}

	void Renderer::resetAccumulation()
	{
		m_FrameIndex = 0;
	}

	bool Renderer::sceneNeedsUpdate(Scene& /*scene*/)
	{
		// Pour l'instant on upload à chaque frame.
		// Tu peux ajouter un système de version sur Scene plus tard
		// pour ne re-uploader que si des composants ont changé.
		return true;
	}

	// ----------------------------------------------------------------
	// Compilation shaders
	// ----------------------------------------------------------------

	uint32_t Renderer::compileShader(uint32_t type, const std::string& source)
	{
		uint32_t shader = glCreateShader(type);
		const char* src = source.c_str();
		glShaderSource(shader, 1, &src, nullptr);
		glCompileShader(shader);

		int success;
		glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
		if (!success)
		{
			char log[1024];
			glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
			FUFU_ERROR("Shader compile error:\n{}", log);
		}
		return shader;
	}

	uint32_t Renderer::linkProgram(std::initializer_list<uint32_t> shaders)
	{
		uint32_t program = glCreateProgram();
		for (uint32_t s : shaders)
			glAttachShader(program, s);
		glLinkProgram(program);

		int success;
		glGetProgramiv(program, GL_LINK_STATUS, &success);
		if (!success)
		{
			char log[1024];
			glGetProgramInfoLog(program, sizeof(log), nullptr, log);
			FUFU_ERROR("Program link error:\n{}", log);
		}
		return program;
	}

}