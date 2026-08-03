#pragma once

namespace Fufu 
{
	// UUID 64-bit déterministe basé sur un hash FNV-1a du chemin
	class UUID
	{
	public:
		UUID() = default;
		explicit UUID(uint64_t id) : m_ID(id) {}

		// Génère un UUID depuis un chemin de fichier
		static UUID fromPath(const std::filesystem::path& path)
		{
			return UUID(hash(path.string()));
		}

		uint64_t value() const { return m_ID; }

		bool operator==(const UUID& other) const { return m_ID == other.m_ID; }
		bool operator!=(const UUID& other) const { return m_ID != other.m_ID; }
		explicit operator uint64_t()        const { return m_ID; }
		explicit operator bool()            const { return m_ID != 0; }

	private:
		// FNV-1a 64-bit — déterministe, rapide, bonne distribution
		static uint64_t hash(const std::string& str)
		{
			constexpr uint64_t FNV_OFFSET = 14695981039346656037ULL;
			constexpr uint64_t FNV_PRIME = 1099511628211ULL;

			uint64_t h = FNV_OFFSET;
			for (unsigned char c : str)
			{
				h ^= static_cast<uint64_t>(c);
				h *= FNV_PRIME;
			}
			return h;
		}

		uint64_t m_ID = 0;
	};

}

// Spécialisation std::hash pour utiliser UUID comme clé de map
namespace std 
{
	template<>
	struct hash<Fufu::UUID>
	{
		size_t operator()(const Fufu::UUID& uuid) const noexcept
		{
			return static_cast<size_t>(uuid.value());
		}
	};
}