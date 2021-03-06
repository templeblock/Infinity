//
// Copyright (c) 2009-2013 Mikko Mononen memon@inside.org
//
// This software is provided 'as-is', without any express or implied
// warranty.  In no event will the authors be held liable for any damages
// arising from the use of this software.
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgment in the product documentation would be
//    appreciated but is not required.
// 2. Altered source versions must be plainly marked as such, and must not be
//    misrepresented as being the original software.
// 3. This notice may not be removed or altered from any source distribution.
//

#include <gfx/gfx.h>
#include "gfx_res.h"

enum NVGLtextureflags {
    NVGL_TEXTURE_FLIP_Y = 0x01,
    NVGL_TEXTURE_NODELETE = 0x02,
    NVGL_TEXTURE_PREMULTIPLIED = 0x04
};

enum GLNVGshaderType {
    NSVG_SHADER_FILLGRAD,
    NSVG_SHADER_FILLIMG,
    NSVG_SHADER_SIMPLE,
    NSVG_SHADER_IMG
};

enum gfx_nvg_private
{
    GFX_NVG_LOC_VIEWSIZE = 0,
    GFX_NVG_BINDING_TEX = 0,
    GFX_NVG_BINDING_FRAG_UBO = 0,
};

struct GLNVGtexture {
    int id;
    GLuint tex;
    int width, height;
    int type;
    int flags;
};
typedef struct GLNVGtexture GLNVGtexture;

enum GLNVGcallType {
    GLNVG_NONE = 0,
    GLNVG_FILL,
    GLNVG_CONVEXFILL,
    GLNVG_STROKE,
    GLNVG_TRIANGLES,
};

struct GLNVGcall {
    int type;
    int image;
    int pathOffset;
    int pathCount;
    GLuint triangleOffset;
    GLuint triangleCount;
    GLuint uniformOffset;
};
typedef struct GLNVGcall GLNVGcall;

struct GLNVGpath {
    GLuint fillOffset;
    GLuint fillCount;
    GLuint strokeOffset;
    GLuint strokeCount;
};
typedef struct GLNVGpath GLNVGpath;

struct GLNVGfragUniforms {
    float scissorMat[12]; // matrices are actually 3 vec4s
    float paintMat[12];
    struct NVGcolor innerCol;
    struct NVGcolor outerCol;
    float scissorExt[2];
    float scissorScale[2];
    float extent[2];
    float radius;
    float feather;
    float strokeMult;
    float strokeThr;
    int texType;
    int type;
};
typedef struct GLNVGfragUniforms GLNVGfragUniforms;

struct GLNVGcontext {
    GLNVGtexture* textures;
    float view[2];
    int ntextures;
    int ctextures;
    int textureId;
    int flags;

    // Per frame buffers
    GLNVGcall* calls;
    int ccalls;
    int ncalls;
    GLNVGpath* paths;
    int cpaths;
    int npaths;

    GLuint fragSize;
};
typedef struct GLNVGcontext GLNVGcontext;

static GLNVGtexture* glnvg__allocTexture(GLNVGcontext* gl)
{
    GLNVGtexture* tex = NULL;
    int i;

    for (i = 0; i < gl->ntextures; i++) {
        if (gl->textures[i].id == 0) {
            tex = &gl->textures[i];
            break;
        }
    }
    if (tex == NULL) {
        if (gl->ntextures + 1 > gl->ctextures) {
            GLNVGtexture* textures;
            int ctextures = core::max(gl->ntextures + 1, 4) + gl->ctextures / 2; // 1.5x Overallocate
            textures = (GLNVGtexture*)realloc(gl->textures, sizeof(GLNVGtexture)*ctextures);
            if (textures == NULL) return NULL;
            gl->textures = textures;
            gl->ctextures = ctextures;
        }
        tex = &gl->textures[gl->ntextures++];
    }

    mem_zero(tex);
    tex->id = ++gl->textureId;

    return tex;
}

static GLNVGtexture* glnvg__findTexture(GLNVGcontext* gl, int id)
{
    int i;
    for (i = 0; i < gl->ntextures; i++)
        if (gl->textures[i].id == id)
            return &gl->textures[i];
    return NULL;
}

static int glnvg__deleteTexture(GLNVGcontext* gl, int id)
{
    int i;
    for (i = 0; i < gl->ntextures; i++) {
        if (gl->textures[i].id == id) {
            if (gl->textures[i].tex != 0 && (gl->textures[i].flags & NVGL_TEXTURE_NODELETE) == 0)
                glDeleteTextures(1, &gl->textures[i].tex);
            mem_zero(&gl->textures[i]);
            return 1;
        }
    }
    return 0;
}

static int glnvg__renderCreate(void* uptr)
{
    GLNVGcontext* gl = (GLNVGcontext*)uptr;

    gl->fragSize = bit_align_up(sizeof(GLNVGfragUniforms), gfx::caps.uboAlignment);

    return 1;
}

static int glnvg__renderCreateTexture(void* uptr, int type, int w, int h, int imageFlags, const unsigned char* data)
{
    GLNVGcontext* gl = (GLNVGcontext*)uptr;
    GLNVGtexture* tex = glnvg__allocTexture(gl);

    if (tex == NULL) return 0;

    glGenTextures(1, &tex->tex);
    tex->width = w;
    tex->height = h;
    tex->type = type;

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, tex->width);
    glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
    glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);

    if (type == NVG_TEXTURE_RGBA)
        glTextureImage2DEXT(tex->tex, GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    else
        glTextureImage2DEXT(tex->tex, GL_TEXTURE_2D, 0, GL_RED, w, h, 0, GL_RED, GL_UNSIGNED_BYTE, data);

    if (imageFlags & NVG_IMAGE_GENERATE_MIPMAPS) {
        glTextureParameteriEXT(tex->tex, GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    } else {
        glTextureParameteriEXT(tex->tex, GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    }
    glTextureParameteriEXT(tex->tex, GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
    glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);

    // The new way to build mipmaps on GLES and GL3
    if (imageFlags & NVG_IMAGE_GENERATE_MIPMAPS) {
        glGenerateTextureMipmapEXT(tex->tex, GL_TEXTURE_2D);
    }

    return tex->id;
}


static int glnvg__renderDeleteTexture(void* uptr, int image)
{
    GLNVGcontext* gl = (GLNVGcontext*)uptr;
    return glnvg__deleteTexture(gl, image);
}

static int glnvg__renderUpdateTexture(void* uptr, int image, int x, int y, int w, int h, const unsigned char* data)
{
    GLNVGcontext* gl = (GLNVGcontext*)uptr;
    GLNVGtexture* tex = glnvg__findTexture(gl, image);

    if (tex == NULL) return 0;

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, tex->width);
    glPixelStorei(GL_UNPACK_SKIP_PIXELS, x);
    glPixelStorei(GL_UNPACK_SKIP_ROWS, y);

    if (tex->type == NVG_TEXTURE_RGBA)
        glTextureSubImage2DEXT(tex->tex, GL_TEXTURE_2D, 0, x, y, w, h, GL_RGBA, GL_UNSIGNED_BYTE, data);
    else
        glTextureSubImage2DEXT(tex->tex, GL_TEXTURE_2D, 0, x, y, w, h, GL_RED, GL_UNSIGNED_BYTE, data);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
    glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);

    return 1;
}

static int glnvg__renderGetTextureSize(void* uptr, int image, int* w, int* h)
{
    GLNVGcontext* gl = (GLNVGcontext*)uptr;
    GLNVGtexture* tex = glnvg__findTexture(gl, image);
    if (tex == NULL) return 0;
    *w = tex->width;
    *h = tex->height;
    return 1;
}

static void glnvg__xformToMat3x4(float* m3, float* t)
{
    m3[0] = t[0];
    m3[1] = t[1];
    m3[2] = 0.0f;
    m3[3] = 0.0f;
    m3[4] = t[2];
    m3[5] = t[3];
    m3[6] = 0.0f;
    m3[7] = 0.0f;
    m3[8] = t[4];
    m3[9] = t[5];
    m3[10] = 1.0f;
    m3[11] = 0.0f;
}

static NVGcolor glnvg__premulColor(NVGcolor c)
{
    c.r *= c.a;
    c.g *= c.a;
    c.b *= c.a;
    return c;
}

static int glnvg__convertPaint(GLNVGcontext* gl, GLNVGfragUniforms* frag, NVGpaint* paint,
    NVGscissor* scissor, float width, float fringe, float strokeThr)
{
    PROFILER_CPU_TIMESLICE("NVG backend convertPaint");

    GLNVGtexture* tex = NULL;
    float invxform[6];

    mem_zero(frag);

    frag->innerCol = glnvg__premulColor(paint->innerColor);
    frag->outerCol = glnvg__premulColor(paint->outerColor);

    if (scissor->extent[0] < -0.5f || scissor->extent[1] < -0.5f) {
        mem_zero(frag->scissorMat);
        frag->scissorExt[0] = 1.0f;
        frag->scissorExt[1] = 1.0f;
        frag->scissorScale[0] = 1.0f;
        frag->scissorScale[1] = 1.0f;
    } else {
        nvgTransformInverse(invxform, scissor->xform);
        glnvg__xformToMat3x4(frag->scissorMat, invxform);
        frag->scissorExt[0] = scissor->extent[0];
        frag->scissorExt[1] = scissor->extent[1];
        frag->scissorScale[0] = ml::sqrt(scissor->xform[0] * scissor->xform[0] + scissor->xform[2] * scissor->xform[2]) / fringe;
        frag->scissorScale[1] = ml::sqrt(scissor->xform[1] * scissor->xform[1] + scissor->xform[3] * scissor->xform[3]) / fringe;
    }

    mem_copy(frag->extent, paint->extent, sizeof(frag->extent));
    frag->strokeMult = (width*0.5f + fringe*0.5f) / fringe;
    frag->strokeThr = strokeThr;

    if (paint->image != 0) {
        tex = glnvg__findTexture(gl, paint->image);
        if (tex == NULL) return 0;
        if ((tex->flags & NVGL_TEXTURE_FLIP_Y) != 0) {
            float flipped[6];
            nvgTransformScale(flipped, 1.0f, -1.0f);
            nvgTransformMultiply(flipped, paint->xform);
            nvgTransformInverse(invxform, flipped);
        } else {
            nvgTransformInverse(invxform, paint->xform);
        }
        frag->type = NSVG_SHADER_FILLIMG;

        if (tex->type == NVG_TEXTURE_RGBA)
            frag->texType = (tex->flags & NVGL_TEXTURE_PREMULTIPLIED) ? 0 : 1;
        else
            frag->texType = 2;
        //		printf("frag->texType = %d\n", frag->texType);
    } else {
        frag->type = NSVG_SHADER_FILLGRAD;
        frag->radius = paint->radius;
        frag->feather = paint->feather;
        nvgTransformInverse(invxform, paint->xform);
    }

    glnvg__xformToMat3x4(frag->paintMat, invxform);

    return 1;
}

static GLNVGfragUniforms* nvg__fragUniformPtr(GLNVGcontext* gl, int i);

static void glnvg__setUniforms(GLNVGcontext* gl, int uniformOffset, int image)
{
    glBindBufferRange(GL_UNIFORM_BUFFER, GFX_NVG_BINDING_FRAG_UBO, gfx::dynBuffer, uniformOffset, sizeof(GLNVGfragUniforms));

    if (image != 0) {
        GLNVGtexture* tex = glnvg__findTexture(gl, image);
        glBindMultiTextureEXT(GL_TEXTURE0, GL_TEXTURE_2D, tex != NULL ? tex->tex : 0);
    } else {
        glBindMultiTextureEXT(GL_TEXTURE0, GL_TEXTURE_2D, 0);
    }
}

static void glnvg__renderViewport(void* uptr, int width, int height)
{
    GLNVGcontext* gl = (GLNVGcontext*)uptr;
    gl->view[0] = (float)width;
    gl->view[1] = (float)height;
}

static void glnvg__fill(GLNVGcontext* gl, GLNVGcall* call)
{
    PROFILER_CPU_TIMESLICE("NVG backend fill");

    GLNVGpath* paths = &gl->paths[call->pathOffset];
    int i, npaths = call->pathCount;

    // Draw shapes
    glEnable(GL_STENCIL_TEST);
    glStencilMask(0xff);
    glStencilFunc(GL_ALWAYS, 0, 0xff);
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

    // set bindpoint for solid loc
    glnvg__setUniforms(gl, call->uniformOffset, 0);

    glStencilOpSeparate(GL_FRONT, GL_KEEP, GL_KEEP, GL_INCR_WRAP);
    glStencilOpSeparate(GL_BACK, GL_KEEP, GL_KEEP, GL_DECR_WRAP);
    glDisable(GL_CULL_FACE);
    for (i = 0; i < npaths; i++)
        glDrawArrays(GL_TRIANGLE_FAN, paths[i].fillOffset, paths[i].fillCount);
    glEnable(GL_CULL_FACE);

    // Draw anti-aliased pixels
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

    glnvg__setUniforms(gl, call->uniformOffset + gl->fragSize, call->image);

    if (gl->flags & NVG_ANTIALIAS) {
        glStencilFunc(GL_EQUAL, 0x00, 0xff);
        glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
        // Draw fringes
        for (i = 0; i < npaths; i++)
            glDrawArrays(GL_TRIANGLE_STRIP, paths[i].strokeOffset, paths[i].strokeCount);
    }

    // Draw fill
    glStencilFunc(GL_NOTEQUAL, 0x0, 0xff);
    glStencilOp(GL_ZERO, GL_ZERO, GL_ZERO);
    glDrawArrays(GL_TRIANGLES, call->triangleOffset, call->triangleCount);

    glDisable(GL_STENCIL_TEST);
}

static void glnvg__convexFill(GLNVGcontext* gl, GLNVGcall* call)
{
    PROFILER_CPU_TIMESLICE("NVG backend convexFill");

    GLNVGpath* paths = &gl->paths[call->pathOffset];
    int i, npaths = call->pathCount;

    glnvg__setUniforms(gl, call->uniformOffset, call->image);

    for (i = 0; i < npaths; i++)
        glDrawArrays(GL_TRIANGLE_FAN, paths[i].fillOffset, paths[i].fillCount);
    if (gl->flags & NVG_ANTIALIAS) {
        // Draw fringes
        for (i = 0; i < npaths; i++)
            glDrawArrays(GL_TRIANGLE_STRIP, paths[i].strokeOffset, paths[i].strokeCount);
    }
}

static void glnvg__stroke(GLNVGcontext* gl, GLNVGcall* call)
{
    PROFILER_CPU_TIMESLICE("NVG backend stroke");

    GLNVGpath* paths = &gl->paths[call->pathOffset];
    int npaths = call->pathCount, i;

    if (gl->flags & NVG_STENCIL_STROKES) {

        glEnable(GL_STENCIL_TEST);
        glStencilMask(0xff);

        // Fill the stroke base without overlap
        glStencilFunc(GL_EQUAL, 0x0, 0xff);
        glStencilOp(GL_KEEP, GL_KEEP, GL_INCR);
        glnvg__setUniforms(gl, call->uniformOffset + gl->fragSize, call->image);
        for (i = 0; i < npaths; i++)
            glDrawArrays(GL_TRIANGLE_STRIP, paths[i].strokeOffset, paths[i].strokeCount);

        // Draw anti-aliased pixels.
        glnvg__setUniforms(gl, call->uniformOffset, call->image);
        glStencilFunc(GL_EQUAL, 0x00, 0xff);
        glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
        for (i = 0; i < npaths; i++)
            glDrawArrays(GL_TRIANGLE_STRIP, paths[i].strokeOffset, paths[i].strokeCount);

        // Clear stencil buffer.		
        glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
        glStencilFunc(GL_ALWAYS, 0x0, 0xff);
        glStencilOp(GL_ZERO, GL_ZERO, GL_ZERO);
        for (i = 0; i < npaths; i++)
            glDrawArrays(GL_TRIANGLE_STRIP, paths[i].strokeOffset, paths[i].strokeCount);
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

        glDisable(GL_STENCIL_TEST);

        //		glnvg__convertPaint(gl, nvg__fragUniformPtr(gl, call->uniformOffset + gl->fragSize), paint, scissor, strokeWidth, fringe, 1.0f - 0.5f/255.0f);

    } else {
        // Draw Strokes
        glnvg__setUniforms(gl, call->uniformOffset, call->image);
        for (i = 0; i < npaths; i++)
            glDrawArrays(GL_TRIANGLE_STRIP, paths[i].strokeOffset, paths[i].strokeCount);
    }
}

static void glnvg__triangles(GLNVGcontext* gl, GLNVGcall* call)
{
    PROFILER_CPU_TIMESLICE("NVG backend triangles");

    glnvg__setUniforms(gl, call->uniformOffset, call->image);
    glDrawArrays(GL_TRIANGLES, call->triangleOffset, call->triangleCount);
}

static void glnvg__renderCancel(void* uptr) {
	GLNVGcontext* gl = (GLNVGcontext*)uptr;
	gl->npaths = 0;
	gl->ncalls = 0;
}

static void glnvg__renderFlush(void* uptr)
{
    PROFILER_CPU_TIMESLICE("NVG backend flush");

    GLNVGcontext* gl = (GLNVGcontext*)uptr;
    int i;

    if (gl->ncalls > 0) {

        // Setup require GL state.
        glUseProgram((gl->flags & NVG_ANTIALIAS) ? gfx_res::prgNanoVGAA : gfx_res::prgNanoVG);

        glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        glFrontFace(GL_CCW);
        glEnable(GL_BLEND);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_SCISSOR_TEST);
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        glStencilMask(0xffffffff);
        glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
        glStencilFunc(GL_ALWAYS, 0, 0xffffffff);

        // Upload vertex data
        glBindVertexArray(vf::p2uv2_vertex_t::vao);
        glBindVertexBuffer(0, gfx::dynBuffer, 0, sizeof(NVGvertex));

        // Set view and texture just once per frame.
        glUniform2fv(GFX_NVG_LOC_VIEWSIZE, 1, gl->view);

        for (i = 0; i < gl->ncalls; i++) {
            GLNVGcall* call = &gl->calls[i];
            if (call->type == GLNVG_FILL)
                glnvg__fill(gl, call);
            else if (call->type == GLNVG_CONVEXFILL)
                glnvg__convexFill(gl, call);
            else if (call->type == GLNVG_STROKE)
                glnvg__stroke(gl, call);
            else if (call->type == GLNVG_TRIANGLES)
                glnvg__triangles(gl, call);
        }

        glBindVertexArray(0);
        glUseProgram(0);
    }

    // Reset calls
    gl->npaths = 0;
    gl->ncalls = 0;
}

static int glnvg__maxVertCount(const NVGpath* paths, int npaths)
{
    int i, count = 0;
    for (i = 0; i < npaths; i++) {
        count += paths[i].nfill;
        count += paths[i].nstroke;
    }
    return count;
}

static GLNVGcall* glnvg__allocCall(GLNVGcontext* gl)
{
    GLNVGcall* ret = NULL;
    if (gl->ncalls + 1 > gl->ccalls) {
        GLNVGcall* calls;
        int ccalls = core::max(gl->ncalls + 1, 128) + gl->ccalls / 2; // 1.5x Overallocate
        calls = (GLNVGcall*)realloc(gl->calls, sizeof(GLNVGcall) * ccalls);
        if (calls == NULL) return NULL;
        gl->calls = calls;
        gl->ccalls = ccalls;
    }
    ret = &gl->calls[gl->ncalls++];
    mem_zero(ret);
    return ret;
}

static int glnvg__allocPaths(GLNVGcontext* gl, int n)
{
    int ret = 0;
    if (gl->npaths + n > gl->cpaths) {
        GLNVGpath* paths;
        int cpaths = core::max(gl->npaths + n, 128) + gl->cpaths / 2; // 1.5x Overallocate
        paths = (GLNVGpath*)realloc(gl->paths, sizeof(GLNVGpath) * cpaths);
        if (paths == NULL) return -1;
        gl->paths = paths;
        gl->cpaths = cpaths;
    }
    ret = gl->npaths;
    gl->npaths += n;
    return ret;
}

static void glnvg__renderFill(void* uptr, NVGpaint* paint, NVGscissor* scissor, float fringe,
    const float* bounds, const NVGpath* paths, int npaths)
{
    PROFILER_CPU_TIMESLICE("NVG backend renderFill");

    GLNVGcontext* gl = (GLNVGcontext*)uptr;
    GLNVGcall* call = glnvg__allocCall(gl);
    GLNVGfragUniforms* frag;
    GLuint maxverts, offset;

    if (call == NULL) return;

    call->type = GLNVG_FILL;
    call->pathOffset = glnvg__allocPaths(gl, npaths);
    if (call->pathOffset == -1) goto error;
    call->pathCount = npaths;
    call->image = paint->image;

    if (npaths == 1 && paths[0].convex)
        call->type = GLNVG_CONVEXFILL;

    // Allocate vertices for all the paths.
    maxverts = glnvg__maxVertCount(paths, npaths) + 6;
    NVGvertex* vtx = gfx::frameAllocVertices<NVGvertex>(maxverts, &offset);
    if (!vtx) goto error;

    for (int i = 0; i < npaths; i++) {
        GLNVGpath* copy = &gl->paths[call->pathOffset + i];
        const NVGpath* path = &paths[i];
        mem_zero(copy);
        if (path->nfill > 0) {
            copy->fillOffset = offset;
            copy->fillCount = path->nfill;
            mem_copy(vtx, path->fill, sizeof(NVGvertex) * path->nfill);
            offset += path->nfill;
            vtx    += path->nfill;
        }
        if (path->nstroke > 0) {
            copy->strokeOffset = offset;
            copy->strokeCount = path->nstroke;
            mem_copy(vtx, path->stroke, sizeof(NVGvertex) * path->nstroke);
            offset += path->nstroke;
            vtx    += path->nstroke;
        }
    }

    // Quad
    call->triangleOffset = offset;
    call->triangleCount = 6;
    *vtx++ = {bounds[0], bounds[3], 0.5f, 1.0f};
    *vtx++ = {bounds[2], bounds[3], 0.5f, 1.0f};
    *vtx++ = {bounds[2], bounds[1], 0.5f, 1.0f};

    *vtx++ = {bounds[0], bounds[3], 0.5f, 1.0f};
    *vtx++ = {bounds[2], bounds[1], 0.5f, 1.0f};
    *vtx++ = {bounds[0], bounds[1], 0.5f, 1.0f};

    gfx::dynbufAlignMem(gfx::caps.uboAlignment, &call->uniformOffset);

    // Setup uniforms for draw calls
    if (call->type == GLNVG_FILL) {
        // Simple shader for stencil
        frag = (GLNVGfragUniforms*)gfx::dynbufAlloc(gl->fragSize);
        if (!frag) goto error;
        mem_zero(frag);
        frag->strokeThr = -1.0f;
        frag->type = NSVG_SHADER_SIMPLE;

        // Fill shader
        frag = (GLNVGfragUniforms*)gfx::dynbufAlloc(gl->fragSize);
        if (!frag) goto error;
        glnvg__convertPaint(gl, frag, paint, scissor, fringe, fringe, -1.0f);
    } else {
        // Fill shader
        frag = (GLNVGfragUniforms*)gfx::dynbufAlloc(gl->fragSize);
        if (!frag) goto error;
        glnvg__convertPaint(gl, frag, paint, scissor, fringe, fringe, -1.0f);
    }

    return;

error:
    // We get here if call alloc was ok, but something else is not.
    // Roll back the last call to prevent drawing it.
    if (gl->ncalls > 0) gl->ncalls--;
}

static void glnvg__renderStroke(void* uptr, NVGpaint* paint, NVGscissor* scissor, float fringe,
    float strokeWidth, const NVGpath* paths, int npaths)
{
    PROFILER_CPU_TIMESLICE("NVG backend renderStroke");

    GLNVGcontext* gl = (GLNVGcontext*)uptr;
    GLNVGcall* call = glnvg__allocCall(gl);
    GLuint maxverts, offset;

    if (call == NULL) return;

    call->type = GLNVG_STROKE;
    call->pathOffset = glnvg__allocPaths(gl, npaths);
    if (call->pathOffset == -1) goto error;
    call->pathCount = npaths;
    call->image = paint->image;

    // Allocate vertices for all the paths.
    maxverts = glnvg__maxVertCount(paths, npaths);
    NVGvertex* vtx = gfx::frameAllocVertices<NVGvertex>(maxverts, &offset);
    if (!vtx) goto error;

    for (int i = 0; i < npaths; i++) {
        GLNVGpath* copy = &gl->paths[call->pathOffset + i];
        const NVGpath* path = &paths[i];
        mem_zero(copy);
        if (path->nstroke) {
            copy->strokeOffset = offset;
            copy->strokeCount  = path->nstroke;
            mem_copy(vtx, path->stroke, sizeof(NVGvertex) * path->nstroke);
            offset += path->nstroke;
            vtx    += path->nstroke;
        }
    }

    GLNVGfragUniforms* frag;

    gfx::dynbufAlignMem(gfx::caps.uboAlignment, &call->uniformOffset);

    if (gl->flags & NVG_STENCIL_STROKES) {
        // Fill shader
        frag = (GLNVGfragUniforms*)gfx::dynbufAlloc(gl->fragSize);
        if (!frag) goto error;
        glnvg__convertPaint(gl, frag, paint, scissor, strokeWidth, fringe, -1.0f);

        frag = (GLNVGfragUniforms*)gfx::dynbufAlloc(gl->fragSize);
        if (!frag) goto error;
        glnvg__convertPaint(gl, frag, paint, scissor, strokeWidth, fringe, 1.0f - 0.5f / 255.0f);

    } else {
        // Fill shader
        frag = (GLNVGfragUniforms*)gfx::dynbufAlloc(gl->fragSize);
        if (!frag) goto error;
        glnvg__convertPaint(gl, frag, paint, scissor, strokeWidth, fringe, -1.0f);
    }

    return;

error:
    // We get here if call alloc was ok, but something else is not.
    // Roll back the last call to prevent drawing it.
    if (gl->ncalls > 0) gl->ncalls--;
}

static void glnvg__renderTriangles(void* uptr, NVGpaint* paint, NVGscissor* scissor,
    const NVGvertex* verts, int nverts)
{
    PROFILER_CPU_TIMESLICE("NVG backend renderTriangles");

    GLNVGcontext* gl = (GLNVGcontext*)uptr;
    GLNVGcall* call = glnvg__allocCall(gl);
    GLNVGfragUniforms* frag;

    if (call == NULL) return;

    call->type = GLNVG_TRIANGLES;
    call->image = paint->image;

    // Allocate vertices for all the paths.
    NVGvertex* vtx = gfx::frameAllocVertices<NVGvertex>(nverts, &call->triangleOffset);
    if (!vtx) goto error;

    call->triangleCount = nverts;
    mem_copy(vtx, verts, sizeof(NVGvertex) * nverts);

    gfx::dynbufAlignMem(gfx::caps.uboAlignment, &call->uniformOffset);

    // Fill shader
    frag = (GLNVGfragUniforms*)gfx::dynbufAlloc(gl->fragSize);
    if (!frag) goto error;
    glnvg__convertPaint(gl, frag, paint, scissor, 1.0f, 1.0f, -1.0f);
    frag->type = NSVG_SHADER_IMG;

    return;

error:
    // We get here if call alloc was ok, but something else is not.
    // Roll back the last call to prevent drawing it.
    if (gl->ncalls > 0) gl->ncalls--;
}

static void glnvg__renderDelete(void* uptr)
{
    GLNVGcontext* gl = (GLNVGcontext*)uptr;
    int i;
    if (gl == NULL) return;

    for (i = 0; i < gl->ntextures; i++) {
        if (gl->textures[i].tex != 0 && (gl->textures[i].flags & NVGL_TEXTURE_NODELETE) == 0)
            glDeleteTextures(1, &gl->textures[i].tex);
    }
    free(gl->textures);

    free(gl->paths);
    free(gl->calls);

    free(gl);
}


NVGcontext* nvgCreateGL3(int flags)
{
    NVGparams params;
    NVGcontext* ctx = NULL;
    GLNVGcontext* gl = (GLNVGcontext*)malloc(sizeof(GLNVGcontext));
    if (gl == NULL) goto error;
    mem_zero(gl);

    mem_zero(&params);
    params.renderCreate = glnvg__renderCreate;
    params.renderCreateTexture = glnvg__renderCreateTexture;
    params.renderDeleteTexture = glnvg__renderDeleteTexture;
    params.renderUpdateTexture = glnvg__renderUpdateTexture;
    params.renderGetTextureSize = glnvg__renderGetTextureSize;
    params.renderViewport = glnvg__renderViewport;
	params.renderCancel = glnvg__renderCancel;
    params.renderFlush = glnvg__renderFlush;
    params.renderFill = glnvg__renderFill;
    params.renderStroke = glnvg__renderStroke;
    params.renderTriangles = glnvg__renderTriangles;
    params.renderDelete = glnvg__renderDelete;
    params.userPtr = gl;
    params.edgeAntiAlias = flags & NVG_ANTIALIAS ? 1 : 0;

    gl->flags = flags;

    ctx = nvgCreateInternal(&params);
    if (ctx == NULL) goto error;

    return ctx;

error:
    // 'gl' is freed by nvgDeleteInternal.
    if (ctx != NULL) nvgDeleteInternal(ctx);
    return NULL;
}

void nvgDeleteGL3(NVGcontext* ctx)
{
    nvgDeleteInternal(ctx);
}
