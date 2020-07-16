#define CGLTF_IMPLEMENTATION
#include "cgltf/cgltf.h"
#include "sx/allocator.h"
#include "sx/string.h"

#include "resource/gltf_importer.h"
#include <stdio.h>

cgltf_size gltf_type_count[] = {0, 1, 2, 3, 4, 4, 9, 16};
cgltf_size gltf_type_size[] = {0, 1, 1, 2, 2, 4, 4};

int scene_importer(const char* path, MeshData** mesh_data, MaterialsData* material_data) {
    cgltf_options options = { 0 };
    cgltf_data* data = NULL;
    cgltf_result result1 = cgltf_parse_file(&options, path, &data);
    cgltf_result result2 = cgltf_load_buffers(&options, data, path);
    int num_meshes = 0;
    if (result1 == cgltf_result_success && result2 == cgltf_result_success) {
        cgltf_size total_prims = 0;
        *mesh_data = sx_malloc(sx_alloc_malloc(), data->meshes_count * sizeof(MeshData));
        MeshData* md = *mesh_data;
        num_meshes = data->meshes_count;
        for (cgltf_size mesh_index = 0; mesh_index < data->meshes_count; mesh_index ++) {
            cgltf_mesh* mesh = &data->meshes[mesh_index];
            cgltf_size vertex_size = 0;
            cgltf_size index_size = 0;
            // Count vertex_size
            md[mesh_index].num_primitives = mesh->primitives_count;
            md[mesh_index].vertex_offsets = sx_malloc(sx_alloc_malloc(), mesh->primitives_count * sizeof(uint32_t));
            md[mesh_index].index_offsets = sx_malloc(sx_alloc_malloc(), mesh->primitives_count * sizeof(uint32_t));
            md[mesh_index].material_indices = sx_malloc(sx_alloc_malloc(), mesh->primitives_count * sizeof(uint32_t));
            for (cgltf_size prim_index = 0; prim_index < mesh->primitives_count; prim_index++) {
                cgltf_primitive* prim = &mesh->primitives[prim_index];
                cgltf_attribute* attr = &prim->attributes[0];
                cgltf_accessor* acc = attr->data;

                md[mesh_index].vertex_offsets[prim_index] = vertex_size;
                md[mesh_index].index_offsets[prim_index] = index_size;

                vertex_size += 8 * acc->count;
                index_size += prim->indices->count;
            }
            md[mesh_index].vertex_data = sx_malloc(sx_alloc_malloc(), vertex_size * sizeof(float));
            md[mesh_index].index_data = sx_malloc(sx_alloc_malloc(), index_size * sizeof(uint32_t));
            md[mesh_index].vertex_size = vertex_size;
            md[mesh_index].index_size = index_size;
            total_prims += mesh->primitives_count;
        }
        material_data->num_materials = total_prims;
        material_data->base_color_texture_names = sx_malloc(sx_alloc_malloc(), total_prims * sizeof(TextureNames));
        material_data->metallic_roughness_texture_names = sx_malloc(sx_alloc_malloc(), total_prims * sizeof(TextureNames));
        material_data->normal_texture_names = sx_malloc(sx_alloc_malloc(), total_prims * sizeof(TextureNames));
        material_data->occlusion_texture_names = sx_malloc(sx_alloc_malloc(), total_prims * sizeof(TextureNames));
        material_data->emissive_texture_names = sx_malloc(sx_alloc_malloc(), total_prims * sizeof(TextureNames));
        material_data->metallic_factors = sx_malloc(sx_alloc_malloc(), total_prims * sizeof(float));
        material_data->roughness_factors = sx_malloc(sx_alloc_malloc(), total_prims * sizeof(float));
        material_data->base_color_factors = sx_malloc(sx_alloc_malloc(), total_prims * sizeof(sx_vec4));
        material_data->emissive_factors = sx_malloc(sx_alloc_malloc(), total_prims * sizeof(sx_vec3));

        total_prims = 0;
        for (cgltf_size mesh_index = 0; mesh_index < data->meshes_count; mesh_index ++) {
            cgltf_mesh* mesh = &data->meshes[mesh_index];
            cgltf_size v_offset = 0;
            cgltf_size attr_offset = 0;
            cgltf_size idx_offset = 0;
            for (cgltf_size prim_index = 0; prim_index < mesh->primitives_count; prim_index++) {
                cgltf_primitive* prim = &mesh->primitives[prim_index];

                for (cgltf_size i = 0; i < prim->indices->count; i++) {
                    md[mesh_index].index_data[i] = cgltf_accessor_read_index(prim->indices, i);
                }

                for (cgltf_size attr_index = 0; attr_index < prim->attributes_count; attr_index++) {
                    cgltf_attribute* attr = &prim->attributes[attr_index];
                    if (attr->type == cgltf_attribute_type_position) {
                        attr_offset = 0;
                    } else if (attr->type == cgltf_attribute_type_normal) {
                        attr_offset = 3;
                    } else if (attr->type == cgltf_attribute_type_texcoord) {
                        attr_offset = 6;
                    }
                    cgltf_accessor* acc = attr->data;
                    v_offset = 0;
                    for (cgltf_size i = 0; i < acc->count; i++) {
                        float* out = md[mesh_index].vertex_data + attr_offset + v_offset;;
                        int num_floats = 3;
                        if (attr->type == cgltf_attribute_type_texcoord) 
                            num_floats = 2;
                        cgltf_accessor_read_float(acc, i, out, num_floats);
                        v_offset += 8;
                    }
                }
                cgltf_material* mat = prim->material;

                sx_memcpy(&material_data->base_color_factors[total_prims], mat->pbr_metallic_roughness.base_color_factor, 4 * sizeof(float));
                if (mat->pbr_metallic_roughness.base_color_texture.texture) {
                    char name[200];
                    char ext[10];
                    sx_split(name, 200, ext, 10, mat->pbr_metallic_roughness.base_color_texture.texture->image->uri, '.');
                    sx_strcat(name, 200, ".ktx");
                    sx_strcpy(material_data->base_color_texture_names[total_prims].names, 200, name);
                } else {
                    sx_strcpy(material_data->base_color_texture_names[total_prims].names, 200, "empty.ktx");
                }
                sx_memcpy(&material_data->roughness_factors[total_prims], &mat->pbr_metallic_roughness.roughness_factor, 1 * sizeof(float));
                sx_memcpy(&material_data->metallic_factors[total_prims], &mat->pbr_metallic_roughness.metallic_factor, 1 * sizeof(float));
                if (mat->pbr_metallic_roughness.metallic_roughness_texture.texture) {
                    char name[200];
                    char ext[10];
                    sx_split(name, 200, ext, 10, mat->pbr_metallic_roughness.metallic_roughness_texture.texture->image->uri, '.');
                    sx_strcat(name, 200, ".ktx");
                    sx_strcpy(material_data->metallic_roughness_texture_names[total_prims].names, 200, name);
                } else {
                    sx_strcpy(material_data->metallic_roughness_texture_names[total_prims].names, 200, "empty.ktx");
                }

                if (mat->normal_texture.texture) {
                    char name[200];
                    char ext[10];
                    sx_split(name, 200, ext, 10, mat->normal_texture.texture->image->uri, '.');
                    sx_strcat(name, 200, ".ktx");
                    sx_strcpy(material_data->normal_texture_names[total_prims].names, 200, name);
                } else {
                    sx_strcpy(material_data->normal_texture_names[total_prims].names, 200, "empty.ktx");
                }

                if (mat->occlusion_texture.texture) {
                    char name[200];
                    char ext[10];
                    sx_split(name, 200, ext, 10, mat->occlusion_texture.texture->image->uri, '.');
                    sx_strcat(name, 200, ".ktx");
                    sx_strcpy(material_data->occlusion_texture_names[total_prims].names, 200, name);
                } else {
                    sx_strcpy(material_data->occlusion_texture_names[total_prims].names, 200, "empty.ktx");
                }

                sx_memcpy(&material_data->emissive_factors[total_prims], &mat->emissive_factor, 3 * sizeof(float));
                if (mat->emissive_texture.texture) {
                    char name[200];
                    char ext[10];
                    sx_split(name, 200, ext, 10, mat->emissive_texture.texture->image->uri, '.');
                    sx_strcat(name, 200, ".ktx");
                    sx_strcpy(material_data->emissive_texture_names[total_prims].names, 200, name);
                } else {
                    sx_strcpy(material_data->emissive_texture_names[total_prims].names, 200, "empty.ktx");
                }

                total_prims++;
            }
        }
    }
    return num_meshes;
}

