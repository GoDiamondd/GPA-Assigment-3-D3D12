#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <glm/glm.hpp>

namespace INANOA {
	namespace Assets {
		struct ObjVertex
		{
			glm::vec3 pos;
			glm::vec3 nrm;
			glm::vec2 uv;
		};

		struct ObjMesh
		{
			std::vector<ObjVertex> vertices;
			std::vector<uint32_t> indices;
			std::string diffuseTexturePath; // resolved absolute/relative path
		};

		// Minimal: single mesh, triangulated faces, supports v/vt/vn and mtllib/usemtl map_Kd.
		bool LoadObjWithMtl(const std::string& objPath, ObjMesh& outMesh);
	}
}
