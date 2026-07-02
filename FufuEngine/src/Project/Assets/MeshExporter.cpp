#include "depch.h"
#include "Project/Assets/MeshExporter.h"
#include <fstream>

namespace Fufu
{
	bool MeshExporter::writeObj(const std::filesystem::path& path, const SubMesh& mesh)
	{
		std::ofstream file(path);
		if (!file.is_open())
		{
			FUFU_ERROR("MeshExporter: failed to open '{}' for writing", path.string());
			return false;
		}

		file << "# Généré par FufuStudio\n";
		file << "o " << (mesh.name.empty() ? "Mesh" : mesh.name) << "\n";

		for (const Vertex& v : mesh.vertices)
			file << "v " << v.position.x << ' ' << v.position.y << ' ' << v.position.z << "\n";
		for (const Vertex& v : mesh.vertices)
			file << "vt " << v.uv.x << ' ' << v.uv.y << "\n";
		for (const Vertex& v : mesh.vertices)
			file << "vn " << v.normal.x << ' ' << v.normal.y << ' ' << v.normal.z << "\n";

		for (std::size_t i = 0; i + 2 < mesh.indices.size(); i += 3)
		{
			// OBJ est en index 1-based
			uint32_t a = mesh.indices[i] + 1;
			uint32_t b = mesh.indices[i + 1] + 1;
			uint32_t c = mesh.indices[i + 2] + 1;
			file << "f " << a << '/' << a << '/' << a << ' '
				<< b << '/' << b << '/' << b << ' '
				<< c << '/' << c << '/' << c << "\n";
		}

		FUFU_INFO("MeshExporter: wrote '{}' ({} vertices, {} triangles)",
			path.string(), mesh.vertices.size(), mesh.indices.size() / 3);
		return true;
	}
}
