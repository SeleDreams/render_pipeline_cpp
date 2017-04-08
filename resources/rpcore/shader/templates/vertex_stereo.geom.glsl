/**
 *
 * RenderPipeline
 *
 * Copyright (c) 2016, Center of human-centered interaction for coexistence.
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

%defines%

#pragma include "render_pipeline_base.inc.glsl"
#pragma include "includes/vertex_output.struct.glsl"

layout(triangles) in;
layout(triangle_strip, max_vertices=6) out;

in VertexOutput vInput[];
out VertexOutput vOutput;

%includes%
%inout%

void main()
{
    for (int layer = 0; layer < 2; ++layer)
    {
        gl_Layer = layer;
        for (int i = 0, i_end=gl_in.length(); i < i_end; ++i)
        {
            gl_Position = MainSceneData.stereo_ViewProjectionMatrix[layer] * gl_in[i].gl_Position;
            vOutput = vInput[i];
            EmitVertex();
        }
        EndPrimitive();
    }
}
