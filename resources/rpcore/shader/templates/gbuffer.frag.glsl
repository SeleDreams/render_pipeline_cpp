/**
 *
 * RenderPipeline
 *
 * Copyright (c) 2014-2016 tobspr <tobias.springer1@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

#version 430

// Default GBuffer fragment shader. Supports normal mapping, parallax mapping,
// and sampling default textures.

// Set DONT_FETCH_DEFAULT_TEXTURES to prevent any material textures to get sampled
// Set DONT_SET_MATERIAL_PROPERTIES to prevent any material properties to be set.

%defines%

#pragma include "render_pipeline_base.inc.glsl"
#pragma include "includes/vertex_output.struct.glsl"
#pragma include "includes/material.inc.glsl"

%includes%

layout(location = 0) in VertexOutput vOutput;

uniform Panda3DMaterial p3d_Material;

// Late include of the gbuffer packing since it needs the vOutput
#pragma include "includes/normal_mapping.inc.glsl"
#pragma include "includes/gbuffer.inc.glsl"


#if DONT_FETCH_DEFAULT_TEXTURES
    // Don't bind any samplers in this case, so the user can do it on his own
#else
    uniform sampler2D p3d_Texture0;
    uniform sampler2D p3d_Texture1;
    uniform sampler2D p3d_Texture2;
    uniform sampler2D p3d_Texture3;

    // Only use the displacement texture if we actually need it.
    #if OPT_PARALLAX_MAPPING
        uniform sampler2D p3d_Texture4;
    #endif

#endif

%inout%

void main() {

    %main_begin%

    MaterialBaseInput mInput = get_input_from_p3d(p3d_Material);

    vec2 texcoord = vOutput.texcoord;

    // Get texture coordinate
    #if OPT_PARALLAX_MAPPING
        texcoord = get_parallax_texcoord(p3d_Texture4, mInput.normalfactor);
    #endif

    %texcoord%

    // Fetch texture data
    #if DONT_FETCH_DEFAULT_TEXTURES
        float sampled_ior = 0.0;
        float sampled_roughness = 0.0;
    #else
        float sampled_ior = texture(p3d_Texture2, texcoord).x;
        float sampled_roughness = texture(p3d_Texture3, texcoord).x;
    #endif

    #if OPT_ALPHA_TESTING
        #if DONT_FETCH_DEFAULT_TEXTURES
            // No alpha testing when not using default textures
        #else
            // Do binary alpha testing, but weight it based on the distance to the
            // camera. This prevents alpha tested objects getting too thin when
            // viewed from a high distance.
            // TODO: Might want to make the alpha testing distance configurable
            vec4 sampled_diffuse = texture(p3d_Texture0, texcoord);
            #if STEREO_MODE
                float dist_to_camera = distance(MainSceneData.stereo_camera_pos[gl_Layer], vOutput.position);
            #else
                float dist_to_camera = distance(MainSceneData.camera_pos, vOutput.position);
            #endif
            float alpha_factor = mix(0.99, 0.1, saturate(dist_to_camera / 15.0));
            if (sampled_diffuse.w < alpha_factor) discard;
        #endif
    #else
        // In case we don't do alpha testing, we don't need the w-component, so
        // don't fetch it. In practice, most GPU's will still load the w component
        // and discard it, but it surely can't hurt.
        #if DONT_FETCH_DEFAULT_TEXTURES
            vec3 sampled_diffuse = vec3(0);
        #else
            vec3 sampled_diffuse = texture(p3d_Texture0, texcoord).xyz;
        #endif
    #endif

    vec3 material_nrm = vOutput.normal;

    #if OPT_NORMAL_MAPPING
        #if DONT_FETCH_DEFAULT_TEXTURES
            // No normal mapping when not using default textures
        #else
            {
            // Perform normal mapping if enabled
            vec3 sampled_normal = texture(p3d_Texture1, texcoord).xyz;
            vec3 detail_normal = unpack_texture_normal(sampled_normal);
            material_nrm = apply_normal_map(
                vOutput.normal, detail_normal, mInput.normalfactor);
            }
        #endif
    #endif

    // Generate the material output
    MaterialShaderOutput m;

    #if DONT_SET_MATERIAL_PROPERTIES
        // Leave material properties unitialized, and hope the user knows
        // what he's doing.
    #else
        m.shading_model = mInput.shading_model;

        #if DONT_FETCH_DEFAULT_TEXTURES
            m.basecolor = mInput.color;
        #else
            m.basecolor = mInput.color * sampled_diffuse.xyz;
        #endif
        m.normal = material_nrm;
        m.metallic = mInput.metallic;
        m.specular_ior = blend_ior(mInput.specular_ior, sampled_ior);
        m.roughness = mInput.roughness * sampled_roughness;
        m.shading_model_param0 = mInput.arbitrary0;
    #endif

    %material%

    render_material(m);

    %main_end%
}
