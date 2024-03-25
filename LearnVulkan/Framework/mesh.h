#pragma once
#include <glm/glm.hpp>
#include <optional>

struct Vertex {
	glm::vec3 pos;
	glm::vec3 color;
	glm::vec2 uv;
};

struct Mesh {
	std::vector<Vertex> vertices;
	std::vector<uint32_t> indices;
};

std::optional<Mesh> load_mesh(const char* file_path);