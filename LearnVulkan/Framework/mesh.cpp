#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"
#include "error.h"
#include "mesh.h"

std::optional<Mesh> load_mesh(const char* file_path)
{
	Mesh mesh;

    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, file_path)) {
        log_error("Failed to load obj from path ", file_path, " error: ", err);
        return std::nullopt;
    }

    for (const auto& shape : shapes) {
        for (const auto& index : shape.mesh.indices) {

            //TODO: make this more efficient by not appending duplicate vertices.

            Vertex& vertex = mesh.vertices.emplace_back();
            mesh.indices.push_back(mesh.indices.size());

            vertex.pos = {
                attrib.vertices[3 * index.vertex_index + 0],
                attrib.vertices[3 * index.vertex_index + 1],
                attrib.vertices[3 * index.vertex_index + 2]
            };

            vertex.uv = {
                attrib.texcoords[2 * index.texcoord_index + 0],
                attrib.texcoords[2 * index.texcoord_index + 1]
            };

            vertex.color = { 1.0f, 1.0f, 1.0f };
        }
    }




	return mesh;
}
