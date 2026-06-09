#if GL_FRAGMENT_PRECISION_HIGH
precision highp float;
#else
precision mediump float;
#endif

layout(location = 0) in vec3 vertex_position;
layout(location = 1) in vec2 uv;
layout(location = 2) in vec3 position;
layout(location = 3) in vec3 rotation;
layout(location = 4) in vec3 scale;
layout(location = 5) in vec2 min_uv;
layout(location = 6) in vec2 max_uv;
layout(location = 7) in float alpha;

uniform mat4 view;
uniform mat4 proj;

out vec2 uv_frag;
out vec3 frag_unprojected_pos;
out float frag_alpha;

mat3 calculate_rotation_matrix(vec3 axis, float angle) {
    float s = sin(angle);
    float c = cos(angle);
    float oc = 1.0 - c;
    
    return mat3(oc * axis.x * axis.x + c,           oc * axis.x * axis.y - axis.z * s,  oc * axis.z * axis.x + axis.y * s,
                oc * axis.x * axis.y + axis.z * s,  oc * axis.y * axis.y + c,           oc * axis.y * axis.z - axis.x * s,
                oc * axis.z * axis.x - axis.y * s,  oc * axis.y * axis.z + axis.x * s,  oc * axis.z * axis.z + c        );
}

void main()
{
    uv_frag = uv;
    if (uv_frag.x > 0.0f) uv_frag.x = max_uv.x;
    else                  uv_frag.x = min_uv.x;
    
    if (uv_frag.y > 0.0f) uv_frag.y = max_uv.y;
    else                  uv_frag.y = min_uv.y;
    
    mat3 rot_x = calculate_rotation_matrix(vec3(1.0f, 0.0f, 0.0f), rotation.x);
    mat3 rot_y = calculate_rotation_matrix(vec3(0.0f, 1.0f, 0.0f), rotation.y);
    mat3 rot_z = calculate_rotation_matrix(vec3(0.0f, 0.0f, 1.0f), rotation.z);
    
    frag_unprojected_pos = (vertex_position  * scale * rot_x * rot_y * rot_z) + position;
    frag_alpha = alpha;

    gl_Position = proj * view * vec4(frag_unprojected_pos, 1.0f);
}
