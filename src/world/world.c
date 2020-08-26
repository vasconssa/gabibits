#include "world/world.h"
#include "resource/gltf_importer.h"
#include "device/vk_device.h"
#include "sx/math.h"
#include "sx/string.h"
#include <stdio.h>

World* create_world(const sx_alloc* alloc) {
    World* world = sx_malloc(alloc, sizeof(*world));
    world->alloc = alloc;
    world->entity_manager = create_entity_manager(alloc);
    world->scene_graph = create_scenegraph(alloc, world->entity_manager);
    world->renderer = create_renderer(alloc, world->entity_manager);
    world->terrain_system = create_terrainsystem(alloc, world->entity_manager, world->renderer->context);
    world->renderer->terrain_system = world->terrain_system;
    world->gui = create_gui(alloc, world->renderer->context);
    world->renderer->gui = world->gui;


    return world;
}

typedef struct aer_sim_params {
    sx_vec3 position;
    sx_vec3 v;
    sx_vec3 vdot;
    float Cl;
    float Clt;
    float CD;
    float Cybeta;
    float Cydeltar;
    float Cm0;
    float Cmalpha;
    float Cmdeltae;
    float Cmq;
    float Cmalphadot;
    float Clbeta;
    float Cldeltar;
    float Cldeltaa;
    float Clp;
    float Clr;
    float Cnbeta;
    float Cnbetadot;
    float Cndeltar;
    float Cndeltaa;
    float Cnp;
    float Cnr;

    float s;
    float m;
    float phi;
    float theta;
    float psi;

    float r;
    float p;
    float q;
    float rdot;
    float pdot;
    float qdot;

    float cbarra;
    float b;
    float cg;

    float Ixx;
    float Iyy;
    float Izz;
    float Ixz;

    float e0;
    float e1;
    float e2;
    float e3;

    sx_mat3 dcm;
} aer_sim_params;

void simulate(aer_sim_params* params, sx_vec3 input, float thrust, float dt) {
    float g = 9.8;
    float deltaa = input.x;
    float deltae = input.y;
    float deltar = input.z;
    float u = params->v.z;
    float w = params->v.y;
    float v = params->v.x;
    float udot = params->vdot.z;
    float wdot = params->vdot.y;
    float vdot = params->vdot.x;
    float Vc = sx_vec3_len(params->v);
    float alpha = sx_atan(w/u) + sx_torad(5);
    float alphadot = (u * wdot - w * udot) / sx_sqrt(u * u + w * w);
    float beta = sx_atan(v/(sx_sqrt(u * u + w * w)));
    float betadot = (vdot*sx_sqrt(u * u + w * w) - v * (u * udot + w * wdot)) / 
                    (sx_sqrt(u * u + w * w)*(u * u + v * v + w * w));
    float p = 1.14;
    float qq = 0.5 * p * Vc * Vc * params->s;

    float lift = qq * params->Cl;
    float drag = qq * params->CD;
    float side_force = qq * (params->Cydeltar * deltar + params->Cybeta * beta);

    float Fx = lift * sx_sin(alpha) - drag * sx_cos(alpha) - params->m * g * sx_sin(params->theta) + thrust;
    float Fy = side_force + params->m * g * sx_sin(params->phi) * sx_cos(params->theta);
    float Fz = -lift * sx_cos(alpha) - drag * sx_sin(alpha) + params->m * g * sx_cos(params->theta) * sx_cos(params->phi);
    printf("Fx: %f, Fy: %f, Fz:%f\n", Fx, Fy, Fz);
    udot = Fx/params->m - params->q * w + params->r * v;
    vdot = Fy/params->m - params->r * u + params->p * w;
    wdot = Fz/params->m - params->p * v + params->q * u;
    printf("udot: %f, vdot: %f, wdot:%f\n", udot, vdot, wdot);
    params->vdot.z = udot;
    params->vdot.y = wdot;
    params->vdot.x = vdot;

    u = u + udot * dt;
    v = v + vdot * dt;
    w = w + wdot * dt;
    params->v.z = u;
    params->v.y = w;
    params->v.x = v;

    float Vn = u * params->dcm.m11 + v * params->dcm.m12 + w * params->dcm.m13;
    float Ve = u * params->dcm.m21 + v * params->dcm.m22 + w * params->dcm.m23;
    float Vd = u * params->dcm.m31 + v * params->dcm.m32 + w * params->dcm.m33;
    printf("Vn: %f, Ve: %f, Vd: %f\n", Vn, Ve, Vd);

    params->position.x += Ve * dt;
    params->position.y += Vd * dt;
    params->position.z -= Vn * dt;

    float pstab = params->p * sx_cos(alpha) + params->r * sx_sin(alpha);
    float rstab = params->r * sx_cos(alpha) - params->p * sx_sin(alpha);

    float Mstab = qq * params->cbarra * (params->Cm0 + params->Cmalpha * alpha + params->Cmdeltae * deltae) +
                    0.25 * p * Vc * params->s * params->cbarra * params->cbarra *(params->Cmq * params->q + params->Cmalpha * alphadot);
    float Lstab = qq * params->b * (params->Clbeta * beta + params->Cldeltaa * deltaa + params->Cldeltar * deltar) +
                    0.25 * p * Vc * params->s * params->b * params->b *(params->Clp * pstab + params->Clr * rstab);
    float Rstab = qq * params->b * (params->Cnbeta * beta + params->Cndeltaa * deltaa + params->Cndeltar * deltar) +
                    0.25 * p * Vc * params->s * params->b * params->b *(params->Cnp * pstab + params->Cnr * rstab);

    float M = Mstab + lift * (params->cg - 0.25) * params->cbarra * sx_cos(alpha) 
                + drag * (params->cg - 0.25) * params->cbarra * sx_sin(alpha);
    float L = Lstab * sx_cos(alpha) - Rstab * sx_sin(alpha);
    float R = Rstab * sx_cos(alpha) + Lstab * sx_sin(alpha) - side_force * (params->cg - 0.25) * params->cbarra;

    params->pdot = (L + (params->Iyy - params->Izz) * params->q * params->r 
                    + params->Ixz * (params->rdot * params->p + params->p * params->q)) / params->Ixx;
    params->qdot = (M + (params->Izz - params->Ixx) * params->r * params->p 
                    + params->Ixz * ( params->r * params->r - params->p * params->p)) / params->Iyy;
    params->rdot = (R + (params->Ixx - params->Iyy) * params->p * params->q 
                    + params->Ixz * ( params->pdot - params->q * params->r)) / params->Izz;

    params->p = params->p + params->pdot * dt;
    params->q = params->q + params->qdot * dt;
    params->r = params->r + params->rdot * dt;

    float gama = 1 - (params->e0 * params->e0 + params->e1 * params->e1 + params->e2 * params->e2 + params->e3 * params->e3);
    float e0dot = -0.5 * (params->e1 * params->p + params->e2 * params->q + params->e3 * params->r) + gama * params->e0;
    float e1dot =  0.5 * (params->e0 * params->p + params->e2 * params->r - params->e3 * params->q) + gama * params->e1;
    float e2dot =  0.5 * (params->e0 * params->q + params->e3 * params->p - params->e1 * params->r) + gama * params->e2;
    float e3dot =  0.5 * (params->e0 * params->r + params->e1 * params->q - params->e2 * params->p) + gama * params->e3;

    params->e0 = params->e0 + e0dot * dt;
    params->e1 = params->e1 + e1dot * dt;
    params->e2 = params->e2 + e2dot * dt;
    params->e3 = params->e3 + e3dot * dt;

    params->dcm.m11 = params->e0 * params->e0 + params->e1 * params->e1 - params->e2 * params->e2 - params->e3 * params->e3;
    params->dcm.m12 = 2 * (params->e1 * params->e2 - params->e0 * params->e3);
    params->dcm.m13 = 2 * (params->e0 * params->e2 + params->e1 * params->e3);
    params->dcm.m21 = 2 * (params->e1 * params->e2 + params->e0 * params->e3);
    params->dcm.m22 = params->e0 * params->e0 - params->e1 * params->e1 + params->e2 * params->e2 - params->e3 * params->e3;
    params->dcm.m23 = 2 * (params->e2 * params->e3 - params->e0 * params->e1);
    params->dcm.m31 = 2 * (params->e1 * params->e3 - params->e0 * params->e2);
    params->dcm.m32 = 2 * (params->e2 * params->e3 + params->e0 * params->e1);
    params->dcm.m33 = params->e0 * params->e0 - params->e1 * params->e1 - params->e2 * params->e2 + params->e3 * params->e3;

    params->theta = sx_asin(-params->dcm.m31);
    params->phi = sx_atan(params->dcm.m32 / params->dcm.m33);
    params->psi = sx_atan(params->dcm.m21 / params->dcm.m11);
    printf("theta: %f, phi: %f, psi: %f\n", params->theta, params->phi, params->psi);
}

typedef struct aer_sim_params2 {
    sx_vec3 v;
    sx_vec3 position;
    sx_mat3 cLm_derivatives;
    sx_mat3 yln_derivatives;

    float roll_damping;
    float pitch_damping;
    float yaw_damping;

    float R;
    float P;
    float Q;
    float Rdot;
    float Pdot;
    float Qdot;

    float s;
    float b;
    float c;
    float m;

    float Ixx;
    float Iyy;
    float Izz;
    float Ixz;

    float phi;
    float theta;
    float psi;

    sx_quat e;
} aer_sim_params2;

void six_dof(aer_sim_params2* pr, sx_vec3 input, float thrust, float dt) {
    float g = 9.8;
    float deltaa = input.x;
    float deltae = input.y;
    float deltar = input.z;
    float u = pr->v.z;
    float w = pr->v.y;
    float v = pr->v.x;
    float Vc = sx_vec3_len(pr->v);
    float p = 1.14;
    float qq = 0.5 * p * u * u * pr->s;
    // Calculate Coeficients
    float alpha = sx_atan(w/u) + sx_torad(1);
    sx_vec3 out = sx_mat3_mul_vec3(&pr->cLm_derivatives, sx_vec3f(1, alpha, deltae));
    float CD = out.x;
    float CL = out.y;
    float Cm = out.z;
    float beta = sx_atan(v/(sx_sqrt(u * u + w * w)));
    beta = sx_atan(v/u);
    out = sx_mat3_mul_vec3(&pr->yln_derivatives, sx_vec3f(beta, deltaa, deltar));
    float Cy = out.x;
    float Cl = out.y;
    float Cn = out.z;

    // calculate force and moments
    float Fax = (-CD * sx_cos(alpha) + CL * sx_sin(alpha)) * qq;
    float Fay = Cy * qq;
    float Faz = (-CL * sx_cos(alpha) - CD * sx_sin(alpha)) * qq;
    printf("Fx: %f, Fy: %f, Fz:%f\n", Fax, Fay, Faz);

    float LA = Cl * qq * pr->b;
    float MA = Cm * qq * pr->c;
    float NA = Cn * qq * pr->b;
    
    float LT = 0;
    float MT = 0;
    float NT = 0;

    pr->Pdot = (LA + LT + pr->Ixz * (pr->Rdot + pr->P * pr->Q) 
                    - (pr->Izz - pr->Iyy) * pr->R * pr->Q) / pr->Ixx;
    pr->Qdot = (MA + MT - (pr->Ixx - pr->Izz) * pr->P * pr->R 
                    - pr->Ixz * (pr->P * pr->P - pr->R * pr->R)) / pr->Iyy;
    pr->Rdot = (NA + NT + pr->Ixz * (pr->Pdot - pr->Q * pr->R) 
                    - (pr->Iyy - pr->Ixx) * pr->P * pr->Q) / pr->Izz;
    printf("pdot: %f, qdot: %f, rdot: %f\n", pr->Pdot, pr->Qdot, pr->Rdot);

    pr->Pdot -= pr->roll_damping * pr->P;
    pr->Qdot -= pr->pitch_damping * pr->Q;
    pr->Rdot -= pr->yaw_damping * pr->R;
    /*pr->Rdot = 0;*/

    pr->P += pr->Pdot * dt;
    pr->Q += pr->Qdot * dt;
    pr->R += pr->Rdot * dt;
    /*pr->R = 0;*/
    printf("p: %f, q: %f, r: %f\n", pr->P, pr->Q, pr->R);

    sx_mat3 transform_dot = sx_mat3v(sx_vec3f(1.0, 0.0, 0.0),
                                     sx_vec3f(sx_sin(pr->phi) * sx_tan(pr->theta), 
                                         sx_cos(pr->phi), sx_sin(pr->phi) * 1.0 / sx_cos(pr->theta)),
                                     sx_vec3f(sx_cos(pr->phi) * sx_tan(pr->theta), -sx_sin(pr->phi),
                                         sx_cos(pr->phi) * 1.0 / sx_cos(pr->theta) ) );

    /*sx_vec3 angles_dot = sx_mat3_mul_vec3(&transform_dot, sx_vec3f(pr->P, pr->Q, pr->R));*/

    /*pr->phi += angles_dot.x * dt;*/
    /*pr->theta += angles_dot.y * dt;*/
    /*pr->psi += angles_dot.z * dt;*/

    float udot = (-pr->m * g * sx_sin(pr->theta) + Fax + thrust) / pr->m 
                    + v * pr->R - w * pr->Q;
    float vdot = (pr->m * g * sx_sin(pr->phi) * sx_cos(pr->theta) + Fay) / pr->m 
                    - u * pr->R + w * pr->P;
    float wdot = (pr->m * g * sx_cos(pr->phi) * sx_cos(pr->theta) + Faz) / pr->m 
                    + u * pr->Q - v * pr->P;
    u = u + udot * dt;
    v = v + vdot * dt;
    w = w + wdot * dt;
    pr->v.z = u;
    pr->v.y = w;
    pr->v.x = v;
    

    sx_mat3 m1 = sx_mat3v(sx_vec3f(sx_cos(pr->psi), -sx_sin(pr->psi), 0.0),
                          sx_vec3f(sx_sin(pr->psi), sx_cos(pr->psi), 0.0),
                          sx_vec3f(0.0, 0.0, 1.0));
    sx_mat3 m2 = sx_mat3v(sx_vec3f(sx_cos(pr->theta), 0.0, sx_sin(pr->theta)),
                          sx_vec3f(0.0, 1.0, 0.0),
                          sx_vec3f(sx_sin(pr->theta), 0.0, sx_cos(pr->theta)));
    sx_mat3 m3 = sx_mat3v(sx_vec3f(1.0, 0.0, 0.0),
                          sx_vec3f(0.0, sx_cos(pr->phi), -sx_sin(pr->phi)),
                          sx_vec3f(0.0, sx_sin(pr->phi), sx_cos(pr->phi)));

    sx_mat3 dcmtemp = sx_mat3_mul(&m1, &m2);
    /*sx_mat3 dcm= sx_mat3_mul(&dcmtemp, &m3);*/
    /*sx_mat3 dcm = sx_mat3_inv(&dc);*/
    sx_mat4 dc = sx_mat4_rotateXYZ(pr->phi, pr->theta, pr->psi);
    sx_mat3 dcm = sx_mat3v(sx_vec3f(dc.col1.x, dc.col1.y, dc.col1.z),
                           sx_vec3f(dc.col2.x, dc.col2.y, dc.col2.z),
                           sx_vec3f(dc.col3.x, dc.col3.y, dc.col3.z));
    


    float gama = 1 - (pr->e.w * pr->e.w + pr->e.x * pr->e.x + pr->e.y * pr->e.y + pr->e.z * pr->e.z);
    float e0dot = -0.5 * (pr->e.x * pr->P + pr->e.y * pr->Q + pr->e.z * pr->R);
    float e1dot =  0.5 * (pr->e.w * pr->P + pr->e.y * pr->R - pr->e.z * pr->Q);
    float e2dot =  0.5 * (pr->e.w * pr->Q + pr->e.z * pr->P - pr->e.x * pr->R);
    float e3dot =  0.5 * (pr->e.w * pr->R + pr->e.x * pr->Q - pr->e.y * pr->P);
    pr->e.w += e0dot * dt;
    pr->e.x += e1dot * dt;
    pr->e.y += e2dot * dt;
    pr->e.z += e3dot * dt;
    pr->e = sx_quat_norm(pr->e);

    sx_vec3 vned = sx_mat3_mul_vec3(&dcm, sx_vec3f(u, v, w));
    vned = sx_vec3_mul_quat(sx_vec3f(u, v, w), sx_quat_inv(pr->e));
    printf("e.x: %f, e.y: %f, e.z: %f, e.w: %f\n", pr->e.x, pr->e.y, pr->e.z, pr->e.w);
    pr->position.z += vned.x * dt;
    pr->position.y -= vned.z * dt;
    pr->position.x += vned.y * dt;
    printf("Vn: %f, Ve: %f, Vd: %f\n", vned.x, vned.y, vned.z);
    printf("u: %f, v: %f, w: %f\n", u, v, w);

    sx_vec3 angles = sx_quat_toeuler(pr->e);
    pr->phi = angles.x;
    pr->theta = angles.y;
    pr->psi = angles.z;

    printf("theta: %f, phi: %f, psi: %f\n", pr->theta, pr->phi, pr->psi);



}

aer_sim_params params;
aer_sim_params2 pr;
void game_init(World* world) {
    MeshData* mesh_data;
    MaterialsData material_data;
    int num_meshes = scene_importer("misc/p51.gltf", &mesh_data, &material_data);
    world->entities = sx_malloc(world->alloc, (num_meshes + 1) * sizeof(Entity));
    init_pbr_materials(world->renderer, &material_data, "misc/");
    sx_free(sx_alloc_malloc(), material_data.base_color_texture_names);
    sx_free(sx_alloc_malloc(), material_data.metallic_roughness_texture_names);
    sx_free(sx_alloc_malloc(), material_data.normal_texture_names);
    sx_free(sx_alloc_malloc(), material_data.occlusion_texture_names);
    sx_free(sx_alloc_malloc(), material_data.emissive_texture_names);
    sx_free(sx_alloc_malloc(), material_data.base_color_factors);
    sx_free(sx_alloc_malloc(), material_data.emissive_factors);
    sx_free(sx_alloc_malloc(), material_data.metallic_factors);
    sx_free(sx_alloc_malloc(), material_data.roughness_factors);

    for (int i = 1; i < num_meshes + 1; i++) {
        world->entities[i] = entity_create(world->entity_manager);
        create_mesh_instance(world->renderer, world->entities[i], &mesh_data[i - 1]);
        sx_mat4 t_matrix = sx_mat4_translate(1796.520996, 1202.708618, 1897.478760);
        sx_mat4 r_matrix = sx_mat4_rotateXYZ(sx_torad(-90), sx_torad(180), sx_torad(-45));
        r_matrix = sx_mat4_ident();
        sx_mat4 result = sx_mat4_mul(&t_matrix, &r_matrix);

        create_transform_instance(world->scene_graph, world->entities[i], &result);
        update_transform(world->renderer, world->entities[i], &result);
        sx_free(sx_alloc_malloc(), mesh_data[i-1].index_data);
        sx_free(sx_alloc_malloc(), mesh_data[i-1].vertex_data);
        sx_free(sx_alloc_malloc(), mesh_data[i-1].vertex_offsets);
        sx_free(sx_alloc_malloc(), mesh_data[i-1].index_offsets);
        sx_free(sx_alloc_malloc(), mesh_data[i-1].material_indices);
    }
    sx_free(sx_alloc_malloc(), mesh_data);
    TerrainTileInfo terrain_info;
    terrain_info.displacement_factor = 2300;
    terrain_info.tessellation_factor = 1.5;
    terrain_info.tessellated_edge_size = 1280 * 10;
    sx_strcpy(terrain_info.terrain_heightmap_texture, 200, "misc/terrain/terrain_demolevel_heightmap2.ktx");
    sx_strcpy(terrain_info.terrain_normal_texture, 200, "misc/terrain/terrain_demolevel_normal2.ktx");
    sx_strcpy(terrain_info.terrain_layers_texture, 200, "misc/terrain/terrain_demolevel_colormap2.ktx");
    world->entities[0] = entity_create(world->entity_manager);
    create_terrain_instance(world->terrain_system, world->entities[0], terrain_info);
    
    struct nk_font_atlas* atlas;

    struct nk_color table[NK_COLOR_COUNT];
    table[NK_COLOR_TEXT] = nk_rgba(210, 210, 210, 255);
    table[NK_COLOR_WINDOW] = nk_rgba(57, 67, 71, 215);
    table[NK_COLOR_HEADER] = nk_rgba(51, 51, 56, 220);
    table[NK_COLOR_BORDER] = nk_rgba(46, 46, 46, 255);
    table[NK_COLOR_BUTTON] = nk_rgba(48, 83, 111, 255);
    table[NK_COLOR_BUTTON_HOVER] = nk_rgba(58, 93, 121, 255);
    table[NK_COLOR_BUTTON_ACTIVE] = nk_rgba(63, 98, 126, 255);
    table[NK_COLOR_TOGGLE] = nk_rgba(50, 58, 61, 255);
    table[NK_COLOR_TOGGLE_HOVER] = nk_rgba(45, 53, 56, 255);
    table[NK_COLOR_TOGGLE_CURSOR] = nk_rgba(48, 83, 111, 255);
    table[NK_COLOR_SELECT] = nk_rgba(57, 67, 61, 255);
    table[NK_COLOR_SELECT_ACTIVE] = nk_rgba(48, 83, 111, 255);
    table[NK_COLOR_SLIDER] = nk_rgba(50, 58, 61, 255);
    table[NK_COLOR_SLIDER_CURSOR] = nk_rgba(48, 83, 111, 245);
    table[NK_COLOR_SLIDER_CURSOR_HOVER] = nk_rgba(53, 88, 116, 255);
    table[NK_COLOR_SLIDER_CURSOR_ACTIVE] = nk_rgba(58, 93, 121, 255);
    table[NK_COLOR_PROPERTY] = nk_rgba(50, 58, 61, 255);
    table[NK_COLOR_EDIT] = nk_rgba(50, 58, 61, 225);
    table[NK_COLOR_EDIT_CURSOR] = nk_rgba(210, 210, 210, 255);
    table[NK_COLOR_COMBO] = nk_rgba(50, 58, 61, 255);
    table[NK_COLOR_CHART] = nk_rgba(50, 58, 61, 255);
    table[NK_COLOR_CHART_COLOR] = nk_rgba(48, 83, 111, 255);
    table[NK_COLOR_CHART_COLOR_HIGHLIGHT] = nk_rgba(255, 0, 0, 255);
    table[NK_COLOR_SCROLLBAR] = nk_rgba(50, 58, 61, 255);
    table[NK_COLOR_SCROLLBAR_CURSOR] = nk_rgba(48, 83, 111, 255);
    table[NK_COLOR_SCROLLBAR_CURSOR_HOVER] = nk_rgba(53, 88, 116, 255);
    table[NK_COLOR_SCROLLBAR_CURSOR_ACTIVE] = nk_rgba(58, 93, 121, 255);
    table[NK_COLOR_TAB_HEADER] = nk_rgba(48, 83, 111, 255);
    nk_style_from_table(&world->gui->data.gui_context, table);
    /*nk_style_default(&world->gui->data.gui_context);*/
    nk_font_stash_begin(world->gui, &atlas);
    nk_font_stash_end(world->gui);
    renderer_prepare(world->renderer);
    params.position = sx_vec3f(1796.520996, 5202.708618, 1897.478760);
    params.v = sx_vec3f(0.0, 0.0, 100.0);
    params.vdot = sx_vec3f(0.0, 0.0, 0.0);
    params.Cl = 0.5;
    params.Clt = 0.5;
    params.CD = 0.04;
    params.Cybeta = -0.01;
    params.Cydeltar = -0.0001;
    params.Cm0 = 0.0;
    params.Cmalpha = -0.5;
    params.Cmdeltae = 0.2;
    params.Cmq = 0.0;
    params.Cmalphadot = 0.0;
    params.Clbeta = 0.0;
    params.Cldeltar = 0.0;
    params.Cldeltaa = 0.01;
    params.Clp = 0.0;
    params.Clr = 0.0;
    params.Cnbeta = 0.07;
    params.Cnbetadot = 0.00;
    params.Cndeltar = 0.01;
    params.Cndeltaa = 0.00;
    params.Cnp = 0.0;
    params.Cnr = 0.0;

    params.s = 21.83;
    params.m = 4000;
    params.phi = 0.0;
    params.theta = 0.0;
    params.psi = 0.0;

    params.r = 0.0;
    params.p = 0.0;
    params.q = 0.0;
    params.rdot = 0.0;
    params.pdot = 0.0;
    params.qdot = 0.0;

    params.cbarra = 8.48 / 3.0;
    params.b = 11.28;
    params.cg = 0.0;
    /*params.cg = 0.2 ;*/

    params.Ixx = 2440472.3314;
    params.Iyy = 26980777.442;
    params.Izz = 29963576.958;
    params.Ixz = 1193119.8065;

    /*params.Ixx = 9000;*/
    /*params.Iyy = 15000;*/
    /*params.Izz = 23000;*/
    /*params.Ixz = 800;*/

    params.e0 = 1.0;
    params.e1 = 0.0;
    params.e2 = 0.0;
    params.e3 = 0.0;

    params.dcm = sx_mat3_ident();

    pr.position = sx_vec3f(1796.520996, 5502.708618, 1897.478760);
    pr.v = sx_vec3f(0.0, 0.0, 40.0);
    float CD0 = 0.12;
    float CDalpha = 0.0001;
    float CDdeltae = 0.0001;
    float CL0 = 0.56;
    float CLalpha = 0.01;
    float CLdeltae = 0.0;
    float Cm0 = 0.0;
    float Cmalpha = -0.5;
    float Cmdeltae = 0.2;

    pr.cLm_derivatives = sx_mat3v(sx_vec3f(CD0, CL0, Cm0),
                                  sx_vec3f(CDalpha, CLalpha, Cmalpha),
                                  sx_vec3f(CDdeltae, CLdeltae, Cmdeltae));

    float CYbeta = -0.01;
    float CYdeltaa = 0.0;
    float CYdeltar = -0.0001;
    float Clbeta = -0.01;
    float Cldeltaa = 0.02;
    float Cldeltar = -0.002;
    float Cnbeta = 0.07;
    float Cndeltaa = 0.0;
    float Cndeltar = 0.01; 
    pr.yln_derivatives = sx_mat3v(sx_vec3f(CYbeta, Clbeta, Cnbeta),
                                  sx_vec3f(CYdeltaa, Cldeltaa, Cndeltaa),
                                  sx_vec3f(CYdeltar, Cldeltar, Cndeltar));

    pr.roll_damping = 4.0;
    pr.pitch_damping = 1.0;
    pr.yaw_damping = 1.2;

    pr.R = 0.0;
    pr.P = 0.0;
    pr.Q = 0.0;
    pr.Rdot = 0.0;
    pr.Pdot = 0.0;
    pr.Qdot = 0.0;

    pr.s = 16.2;
    pr.m = 1000;
    pr.c = 1.47;
    pr.b = 11.28;

    pr.Ixx = 2000;
    pr.Iyy = 3000;
    pr.Izz = 3000;
    pr.Ixz = 2000;

    pr.phi = 0.0;
    pr.theta = 0.0;
    pr.psi = 0.0;
    pr.e = sx_quat_ident();

}



sx_vec3 input;
void world_update(World* world, float dt) {
    static bool upkey = false; 
    static bool downkey = false;
    static bool leftkey = false;
    static bool rightkey = false;
    if (keyboard_button_pressed(device()->input_manager, KB_UP)) upkey = true;
    if (keyboard_button_pressed(device()->input_manager, KB_DOWN)) downkey = true;
    if (keyboard_button_pressed(device()->input_manager, KB_LEFT)) leftkey = true;
    if (keyboard_button_pressed(device()->input_manager, KB_RIGHT)) rightkey = true;

    if (keyboard_button_released(device()->input_manager, KB_UP)) upkey = false;
    if (keyboard_button_released(device()->input_manager, KB_DOWN)) downkey = false;
    if (keyboard_button_released(device()->input_manager, KB_LEFT)) leftkey = false;
    if (keyboard_button_released(device()->input_manager, KB_RIGHT)) rightkey= false;



    if (upkey) {
        input.y -= 0.01;
    }
    if (downkey) { 
        input.y += 0.01;
    }
    if (leftkey) { 
        input.x -= 0.001;
    }
    if (rightkey) { 
        input.x += 0.001;
    }
    printf("\n\n input.x: %f, input.y : %f\n\n", input.x, input.y);
    sx_clamp(input.x, 0.0, 1.0);
    sx_clamp(input.y, 0.0, 1.0);
    /*simulate(&params, input, 55000, dt);*/
    six_dof(&pr, input, 2500, dt);
    static int count = 0;
    float vel = -0.05;
    Entity airplane = world->entities[1];
    sx_mat4 transform = get_local_matrix(world->scene_graph, airplane);
    sx_vec4 fwd = transform.col3;
    sx_vec3 forward = sx_vec3f(fwd.x, fwd.y, fwd.z);
    sx_vec3 position = get_local_position(world->scene_graph, airplane);

    position = pr.position;
    sx_vec3 new_pos = sx_vec3_add(position, sx_vec3_mulf(forward, vel));
    new_pos = pr.position;
    printf("p.x: %f, p.y: %f, p.z: %f\n", pr.position.x, pr.position.y, pr.position.z);
    set_local_position(world->scene_graph, airplane, new_pos);
    set_local_rotation(world->scene_graph, airplane, sx_vec3f(-pr.theta, -pr.psi, pr.phi));
    transform = get_local_matrix(world->scene_graph, airplane);
    fwd = transform.col3;
    forward = sx_vec3f(fwd.x, fwd.y, fwd.z);
    update_transform(world->renderer, airplane, &transform);
    sx_vec3 po = sx_vec3_add(new_pos, sx_vec3_mulf(forward, -15));
    sx_vec3 cam_pos = device()->camera.cam.pos;
    cam_pos = sx_vec3_lerp(cam_pos, po, 1.0);
    printf("dt: %f\n", dt);
    /*device()->camera.cam.pos = po;*/
    /*cam_pos = po;*/
    /*cam_pos.y += 5;*/
    /*cam_pos.x += 30;*/
    /*cam_pos.x = device()->camera.cam.pos.x;*/
    /*cam_pos.z = device()->camera.cam.pos.z;*/
    fps_lookat(&device()->camera, cam_pos, new_pos, sx_vec3f(0, 1, 0));
    count++;
    struct nk_context* ctx = &world->gui->data.gui_context;

    nk_input_begin(ctx);
    sx_vec3 m = mouse_axis(device()->input_manager, MA_CURSOR);
    nk_input_motion(ctx, (int)m.x, (int)m.y);
    nk_input_button(ctx, NK_BUTTON_LEFT, (int)m.x, (int)m.y, mouse_button(device()->input_manager, MB_LEFT));
    nk_input_button(ctx, NK_BUTTON_MIDDLE, (int)m.x, (int)m.y, mouse_button(device()->input_manager, MB_MIDDLE));
    nk_input_button(ctx, NK_BUTTON_RIGHT, (int)m.x, (int)m.y, mouse_button(device()->input_manager, MB_RIGHT));
    nk_input_end(ctx);

    if (nk_begin(&world->gui->data.gui_context, "Gabibits Wingsuit", nk_rect(50, 50, 200, 200),
        NK_WINDOW_BORDER|NK_WINDOW_MOVABLE|NK_WINDOW_SCALABLE|
        NK_WINDOW_CLOSABLE|NK_WINDOW_MINIMIZABLE|NK_WINDOW_TITLE)) {
        enum {EASY, HARD};
        static int op = EASY;

        nk_layout_row_static(&world->gui->data.gui_context, 30, 80, 1);
        if (nk_button_label(&world->gui->data.gui_context, "button"))
            fprintf(stdout, "button pressed\n");
        nk_layout_row_dynamic(&world->gui->data.gui_context, 30, 2);
        if (nk_option_label(&world->gui->data.gui_context, "easy", op == EASY)) op = EASY;
        if (nk_option_label(&world->gui->data.gui_context, "hard", op == HARD)) op = HARD;
    }
    nk_end(&world->gui->data.gui_context);
    
    gui_update(world->gui);
    renderer_render(world->renderer);
    nk_clear(&world->gui->data.gui_context);
    /*nk_buffer_clear(&world->gui->data.cmds);*/
}

void world_destroy(World* world) {
    destroy_terrainsystem(world->terrain_system);
    destroy_gui(world->gui);
    destroy_renderer(world->renderer);
    scenegraph_destroy(world->scene_graph);
    entity_manager_destroy(world->entity_manager);
    sx_free(world->alloc, world->entities);
    sx_free(world->alloc, world);
}

