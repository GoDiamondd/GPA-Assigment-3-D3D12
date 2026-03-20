#include "stdafx.h"
#include "ObjMtlLoader.h"

#include <fstream>
#include <sstream>
#include <algorithm>

namespace INANOA {
	namespace Assets {
		static std::string Trim(const std::string& s)
		{
			size_t b = s.find_first_not_of(" \t\r\n");
			if (b == std::string::npos) return {};
			size_t e = s.find_last_not_of(" \t\r\n");
			return s.substr(b, e - b + 1);
		}

		static std::string DirName(const std::string& path)
		{
			size_t p = path.find_last_of("/\\");
			return (p == std::string::npos) ? std::string() : path.substr(0, p + 1);
		}

		static std::string JoinPath(const std::string& a, const std::string& b)
		{
			if (a.empty()) return b;
			if (b.empty()) return a;
			if (a.back() == '/' || a.back() == '\\') return a + b;
			return a + "/" + b;
		}

		static bool ParseMtlForMapKd(const std::string& mtlPath, std::unordered_map<std::string, std::string>& outMatToMapKd)
		{
			std::ifstream f(mtlPath);
			if (!f.is_open())
				return false;

			std::string line;
			std::string currentMat;
			const std::string baseDir = DirName(mtlPath);

			while (std::getline(f, line))
			{
				line = Trim(line);
				if (line.empty() || line[0] == '#')
					continue;

				std::istringstream iss(line);
				std::string op;
				iss >> op;

				if (op == "newmtl")
				{
					iss >> currentMat;
				}
				else if (op == "map_Kd")
				{
					// Everything after map_Kd is the path, preserve spaces minimally
					std::string tex;
					std::getline(iss, tex);
					tex = Trim(tex);
					if (!currentMat.empty() && !tex.empty())
						outMatToMapKd[currentMat] = JoinPath(baseDir, tex);
				}
			}
			return true;
		}

		struct VertexKey
		{
			int v = 0;
			int vt = 0;
			int vn = 0;
			bool operator==(const VertexKey& o) const { return v == o.v && vt == o.vt && vn == o.vn; }
		};

		struct VertexKeyHash
		{
			size_t operator()(const VertexKey& k) const noexcept
			{
				size_t h = 1469598103934665603ull;
				auto mix = [&](uint64_t x) { h ^= x; h *= 1099511628211ull; };
				mix(static_cast<uint64_t>(k.v));
				mix(static_cast<uint64_t>(k.vt));
				mix(static_cast<uint64_t>(k.vn));
				return h;
			}
		};

		static bool ParseFaceVertex(const std::string& token, VertexKey& out)
		{
			// formats: v, v/vt, v//vn, v/vt/vn
			out = {};

			size_t s1 = token.find('/');
			if (s1 == std::string::npos)
			{
				out.v = std::stoi(token);
				return true;
			}

			size_t s2 = token.find('/', s1 + 1);

			out.v = std::stoi(token.substr(0, s1));

			if (s2 == std::string::npos)
			{
				// v/vt
				out.vt = std::stoi(token.substr(s1 + 1));
				return true;
			}

			// v/.../...
			if (s2 > s1 + 1)
				out.vt = std::stoi(token.substr(s1 + 1, s2 - (s1 + 1)));

			if (s2 + 1 < token.size())
				out.vn = std::stoi(token.substr(s2 + 1));

			return true;
		}

		bool LoadObjWithMtl(const std::string& objPath, ObjMesh& outMesh)
		{
			outMesh = {};

			std::ifstream f(objPath);
			if (!f.is_open())
				return false;

			const std::string baseDir = DirName(objPath);

			std::vector<glm::vec3> positions;
			std::vector<glm::vec3> normals;
			std::vector<glm::vec2> texcoords;

			std::unordered_map<std::string, std::string> matToMapKd;
			std::string activeMtl;
			std::string mtlLibFile;

			std::unordered_map<VertexKey, uint32_t, VertexKeyHash> remap;

			std::string line;
			while (std::getline(f, line))
			{
				line = Trim(line);
				if (line.empty() || line[0] == '#')
					continue;

				std::istringstream iss(line);
				std::string op;
				iss >> op;

				if (op == "mtllib")
				{
					iss >> mtlLibFile;
					if (!mtlLibFile.empty())
						ParseMtlForMapKd(JoinPath(baseDir, mtlLibFile), matToMapKd);
				}
				else if (op == "usemtl")
				{
					iss >> activeMtl;
					auto it = matToMapKd.find(activeMtl);
					if (it != matToMapKd.end())
						outMesh.diffuseTexturePath = it->second;
				}
				else if (op == "v")
				{
					glm::vec3 p{};
					iss >> p.x >> p.y >> p.z;
					positions.push_back(p);
				}
				else if (op == "vt")
				{
					glm::vec2 uv{};
					iss >> uv.x >> uv.y;
					uv.y = 1.0f - uv.y; // OBJ has V-up, D3D typically expects V-down
					texcoords.push_back(uv);
				}
				else if (op == "vn")
				{
					glm::vec3 n{};
					iss >> n.x >> n.y >> n.z;
					normals.push_back(n);
				}
				else if (op == "f")
				{
					// Triangulate fan for n-gons (assumes convex)
					std::vector<VertexKey> face;
					std::string tok;
					while (iss >> tok)
					{
						VertexKey k{};
						if (!ParseFaceVertex(tok, k))
							return false;
						face.push_back(k);
					}
					if (face.size() < 3)
						continue;

					auto emit = [&](const VertexKey& k) -> uint32_t
						{
							auto it = remap.find(k);
							if (it != remap.end())
								return it->second;

							// OBJ indices are 1-based, handle missing streams as 0
							const int vi = (k.v > 0) ? (k.v - 1) : 0;
							const int ti = (k.vt > 0) ? (k.vt - 1) : -1;
							const int ni = (k.vn > 0) ? (k.vn - 1) : -1;

							ObjVertex v{};
							v.pos = (vi >= 0 && vi < (int)positions.size()) ? positions[vi] : glm::vec3(0);
							v.uv = (ti >= 0 && ti < (int)texcoords.size()) ? texcoords[ti] : glm::vec2(0);
							v.nrm = (ni >= 0 && ni < (int)normals.size()) ? normals[ni] : glm::vec3(0, 1, 0);

							uint32_t idx = (uint32_t)outMesh.vertices.size();
							outMesh.vertices.push_back(v);
							remap[k] = idx;
							return idx;
						};

					for (size_t i = 1; i + 1 < face.size(); ++i)
					{
						outMesh.indices.push_back(emit(face[0]));
						outMesh.indices.push_back(emit(face[i]));
						outMesh.indices.push_back(emit(face[i + 1]));
					}
				}
			}

			return !outMesh.vertices.empty() && !outMesh.indices.empty();
		}
	}
}