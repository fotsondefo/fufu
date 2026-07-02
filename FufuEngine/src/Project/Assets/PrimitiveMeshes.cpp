#include "depch.h"
#include "Project/Assets/PrimitiveMeshes.h"
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

namespace Fufu
{
	SubMesh PrimitiveMeshes::makeCube()
	{
		SubMesh mesh;
		mesh.name = "Cube";

		struct Face { glm::vec3 normal, u, v; };
		const Face faces[6] = {
			{ { 0.f,  0.f,  1.f}, { 1.f, 0.f,  0.f}, {0.f, 1.f,  0.f} }, // +Z
			{ { 0.f,  0.f, -1.f}, {-1.f, 0.f,  0.f}, {0.f, 1.f,  0.f} }, // -Z
			{ { 1.f,  0.f,  0.f}, { 0.f, 0.f, -1.f}, {0.f, 1.f,  0.f} }, // +X
			{ {-1.f,  0.f,  0.f}, { 0.f, 0.f,  1.f}, {0.f, 1.f,  0.f} }, // -X
			{ { 0.f,  1.f,  0.f}, { 1.f, 0.f,  0.f}, {0.f, 0.f, -1.f} }, // +Y
			{ { 0.f, -1.f,  0.f}, { 1.f, 0.f,  0.f}, {0.f, 0.f,  1.f} }, // -Y
		};

		const glm::vec2 uvs[4] = { {0.f, 0.f}, {1.f, 0.f}, {1.f, 1.f}, {0.f, 1.f} };

		for (const Face& f : faces)
		{
			uint32_t base = static_cast<uint32_t>(mesh.vertices.size());
			glm::vec3 corners[4] = {
				f.normal - f.u - f.v,
				f.normal + f.u - f.v,
				f.normal + f.u + f.v,
				f.normal - f.u + f.v,
			};

			for (int i = 0; i < 4; ++i)
			{
				Vertex vert;
				vert.position = corners[i];
				vert.normal = f.normal;
				vert.uv = uvs[i];
				vert.tangent = glm::normalize(f.u);
				mesh.vertices.push_back(vert);
			}

			mesh.indices.push_back(base + 0);
			mesh.indices.push_back(base + 1);
			mesh.indices.push_back(base + 2);
			mesh.indices.push_back(base + 0);
			mesh.indices.push_back(base + 2);
			mesh.indices.push_back(base + 3);
		}

		return mesh;
	}

	SubMesh PrimitiveMeshes::makePlane()
	{
		SubMesh mesh;
		mesh.name = "Plane";

		const glm::vec3 positions[4] = {
			{-1.f, 0.f, -1.f}, { 1.f, 0.f, -1.f}, { 1.f, 0.f,  1.f}, {-1.f, 0.f,  1.f},
		};
		const glm::vec2 uvs[4] = { {0.f, 0.f}, {1.f, 0.f}, {1.f, 1.f}, {0.f, 1.f} };

		for (int i = 0; i < 4; ++i)
		{
			Vertex v;
			v.position = positions[i];
			v.normal = { 0.f, 1.f, 0.f };
			v.uv = uvs[i];
			v.tangent = { 1.f, 0.f, 0.f };
			mesh.vertices.push_back(v);
		}

		mesh.indices = { 0, 1, 2, 0, 2, 3 };
		return mesh;
	}

	SubMesh PrimitiveMeshes::makeSphere(int rings, int segments)
	{
		SubMesh mesh;
		mesh.name = "Sphere";

		for (int ring = 0; ring <= rings; ++ring)
		{
			float v = static_cast<float>(ring) / static_cast<float>(rings);
			float phi = v * glm::pi<float>();

			for (int seg = 0; seg <= segments; ++seg)
			{
				float u = static_cast<float>(seg) / static_cast<float>(segments);
				float theta = u * glm::two_pi<float>();

				glm::vec3 dir = {
					glm::sin(phi) * glm::cos(theta),
					glm::cos(phi),
					glm::sin(phi) * glm::sin(theta)
				};

				Vertex vert;
				vert.position = dir;
				vert.normal = dir;
				vert.uv = { u, v };
				vert.tangent = glm::normalize(glm::vec3(-glm::sin(theta), 0.f, glm::cos(theta)));
				mesh.vertices.push_back(vert);
			}
		}

		uint32_t stride = static_cast<uint32_t>(segments) + 1;
		for (int ring = 0; ring < rings; ++ring)
		{
			for (int seg = 0; seg < segments; ++seg)
			{
				uint32_t a = static_cast<uint32_t>(ring) * stride + static_cast<uint32_t>(seg);
				uint32_t b = a + 1;
				uint32_t c = a + stride + 1;
				uint32_t d = a + stride;

				mesh.indices.push_back(a);
				mesh.indices.push_back(b);
				mesh.indices.push_back(c);

				mesh.indices.push_back(a);
				mesh.indices.push_back(c);
				mesh.indices.push_back(d);
			}
		}

		return mesh;
	}

	SubMesh PrimitiveMeshes::makeCylinder(int segments)
	{
		SubMesh mesh;
		mesh.name = "Cylinder";

		// Paroi latérale
		for (int seg = 0; seg <= segments; ++seg)
		{
			float u = static_cast<float>(seg) / static_cast<float>(segments);
			float theta = u * glm::two_pi<float>();
			glm::vec3 dir = { glm::cos(theta), 0.f, glm::sin(theta) };

			for (int y = 0; y < 2; ++y)
			{
				Vertex v;
				v.position = { dir.x, y == 0 ? -1.f : 1.f, dir.z };
				v.normal = dir;
				v.uv = { u, static_cast<float>(y) };
				v.tangent = glm::normalize(glm::vec3(-dir.z, 0.f, dir.x));
				mesh.vertices.push_back(v);
			}
		}

		for (int seg = 0; seg < segments; ++seg)
		{
			uint32_t a = static_cast<uint32_t>(seg) * 2;
			uint32_t b = a + 1;
			uint32_t c = a + 3;
			uint32_t d = a + 2;

			mesh.indices.push_back(a);
			mesh.indices.push_back(b);
			mesh.indices.push_back(c);

			mesh.indices.push_back(a);
			mesh.indices.push_back(c);
			mesh.indices.push_back(d);
		}

		// Capuchons haut/bas
		auto addCap = [&](float y, float normalY)
		{
			uint32_t centerIdx = static_cast<uint32_t>(mesh.vertices.size());
			Vertex center;
			center.position = { 0.f, y, 0.f };
			center.normal = { 0.f, normalY, 0.f };
			center.uv = { 0.5f, 0.5f };
			center.tangent = { 1.f, 0.f, 0.f };
			mesh.vertices.push_back(center);

			uint32_t ringStart = static_cast<uint32_t>(mesh.vertices.size());
			for (int seg = 0; seg <= segments; ++seg)
			{
				float u = static_cast<float>(seg) / static_cast<float>(segments);
				float theta = u * glm::two_pi<float>();

				Vertex v;
				v.position = { glm::cos(theta), y, glm::sin(theta) };
				v.normal = { 0.f, normalY, 0.f };
				v.uv = { glm::cos(theta) * 0.5f + 0.5f, glm::sin(theta) * 0.5f + 0.5f };
				v.tangent = { 1.f, 0.f, 0.f };
				mesh.vertices.push_back(v);
			}

			for (int seg = 0; seg < segments; ++seg)
			{
				uint32_t a = ringStart + static_cast<uint32_t>(seg);
				uint32_t b = a + 1;

				if (normalY > 0.f)
				{
					mesh.indices.push_back(centerIdx);
					mesh.indices.push_back(a);
					mesh.indices.push_back(b);
				}
				else
				{
					mesh.indices.push_back(centerIdx);
					mesh.indices.push_back(b);
					mesh.indices.push_back(a);
				}
			}
		};

		addCap(1.f, 1.f);
		addCap(-1.f, -1.f);

		return mesh;
	}
}
