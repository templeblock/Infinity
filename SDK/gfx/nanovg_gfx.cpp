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

// Create flags

// Flag indicating if geoemtry based anti-aliasing is used (may not be needed when using MSAA).
#define NVG_ANTIALIAS 1 	

// Flag indicating if strokes should be drawn using stencil buffer. The rendering will be a little
// slower, but path overlaps (i.e. self-intersecting or sharp turns) will be drawn just once.
#define NVG_STENCIL_STROKES 2

NVGcontext* nvgCreateGL3(int flags);
void nvgDeleteGL3(NVGcontext* ctx);

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

struct GLNVGshader {
    GLuint prog;
    GLuint frag;
    GLuint vert;
};
typedef struct GLNVGshader GLNVGshader;

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
    int triangleOffset;
    int triangleCount;
    int uniformOffset;
};
typedef struct GLNVGcall GLNVGcall;

struct GLNVGpath {
    int fillOffset;
    int fillCount;
    int strokeOffset;
    int strokeCount;
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
    GLNVGshader shader;
    GLNVGtexture* textures;
    float view[2];
    int ntextures;
    int ctextures;
    int textureId;
    GLuint vertBuf;
    GLuint vertArr;
    int flags;

    // Per frame buffers
    GLNVGcall* calls;
    int ccalls;
    int ncalls;
    GLNVGpath* paths;
    int cpaths;
    int npaths;

    struct NVGvertex* verts;
    int cverts;
    int nverts;

    unsigned char* uniforms;
    int cuniforms;
    int nuniforms;
    GLuint fragBuf;
    int fragSize;
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

    memset(tex, 0, sizeof(*tex));
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
            memset(&gl->textures[i], 0, sizeof(gl->textures[i]));
            return 1;
        }
    }
    return 0;
}

static void glnvg__dumpShaderError(GLuint shader, const char* name, const char* type)
{
    char str[512 + 1];
    int len = 0;
    glGetShaderInfoLog(shader, 512, &len, str);
    if (len > 512) len = 512;
    str[len] = '\0';
    printf("Shader %s/%s error:\n%s\n", name, type, str);
}

static void glnvg__dumpProgramError(GLuint prog, const char* name)
{
    char str[512 + 1];
    int len = 0;
    glGetProgramInfoLog(prog, 512, &len, str);
    if (len > 512) len = 512;
    str[len] = '\0';
    printf("Program %s error:\n%s\n", name, str);
}

static int glnvg__checkError(const char* str)
{
    GLenum err = glGetError();
    if (err != GL_NO_ERROR) {
        printf("Error %08x after %s\n", err, str);
        return 1;
    }
    return 0;
}

static int glnvg__createShader(GLNVGshader* shader, const char* name, const char* header, const char* opts, const char* vshader, const char* fshader)
{
    GLint status;
    GLuint prog, vert, frag;
    const char* str[3];
    str[0] = header;
    str[1] = opts != NULL ? opts : "";

    memset(shader, 0, sizeof(*shader));

    prog = glCreateProgram();
    vert = glCreateShader(GL_VERTEX_SHADER);
    frag = glCreateShader(GL_FRAGMENT_SHADER);
    str[2] = vshader;
    glShaderSource(vert, 3, str, 0);
    str[2] = fshader;
    glShaderSource(frag, 3, str, 0);

    glCompileShader(vert);
    glGetShaderiv(vert, GL_COMPILE_STATUS, &status);
    if (status != GL_TRUE) {
        glnvg__dumpShaderError(vert, name, "vert");
        return 0;
    }

    glCompileShader(frag);
    glGetShaderiv(frag, GL_COMPILE_STATUS, &status);
    if (status != GL_TRUE) {
        glnvg__dumpShaderError(frag, name, "frag");
        return 0;
    }

    glAttachShader(prog, vert);
    glAttachShader(prog, frag);

    glBindAttribLocation(prog, 0, "vertex");
    glBindAttribLocation(prog, 1, "tcoord");

    glLinkProgram(prog);
    glGetProgramiv(prog, GL_LINK_STATUS, &status);
    if (status != GL_TRUE) {
        glnvg__dumpProgramError(prog, name);
        return 0;
    }

    shader->prog = prog;
    shader->vert = vert;
    shader->frag = frag;

    return 1;
}

static void glnvg__deleteShader(GLNVGshader* shader)
{
    if (shader->prog != 0)
        glDeleteProgram(shader->prog);
    if (shader->vert != 0)
        glDeleteShader(shader->vert);
    if (shader->frag != 0)
        glDeleteShader(shader->frag);
}

static int glnvg__renderCreate(void* uptr)
{
    GLNVGcontext* gl = (GLNVGcontext*)uptr;
    int align = 4;

    // TODO: mediump float may not be enough for GLES2 in iOS.
    // see the following discussion: https://github.com/memononen/nanovg/issues/46
    static const char* shaderHeader =
        "#version 430 core\n";

    static const char* fillVertShader =
        "	layout(location=0) uniform vec2 viewSize;\n"
        "	in vec2 vertex;\n"
        "	in vec2 tcoord;\n"
        "	out vec2 ftcoord;\n"
        "	out vec2 fpos;\n"
        "void main(void) {\n"
        "	ftcoord = tcoord;\n"
        "	fpos = vertex;\n"
        "	gl_Position = vec4(2.0*vertex.x/viewSize.x - 1.0, 1.0 - 2.0*vertex.y/viewSize.y, 0, 1);\n"
        "}\n";

    static const char* fillFragShader =
        "	layout(binding=0, std140) uniform frag {\n"
        "		mat3 scissorMat;\n"
        "		mat3 paintMat;\n"
        "		vec4 innerCol;\n"
        "		vec4 outerCol;\n"
        "		vec2 scissorExt;\n"
        "		vec2 scissorScale;\n"
        "		vec2 extent;\n"
        "		float radius;\n"
        "		float feather;\n"
        "		float strokeMult;\n"
        "		float strokeThr;\n"
        "		int texType;\n"
        "		int type;\n"
        "	};\n"
        "	layout(binding=0) uniform sampler2D tex;\n"
        "	in vec2 ftcoord;\n"
        "	in vec2 fpos;\n"
        "	out vec4 outColor;\n"
        "\n"
        "float sdroundrect(vec2 pt, vec2 ext, float rad) {\n"
        "	vec2 ext2 = ext - vec2(rad,rad);\n"
        "	vec2 d = abs(pt) - ext2;\n"
        "	return min(max(d.x,d.y),0.0) + length(max(d,0.0)) - rad;\n"
        "}\n"
        "\n"
        "// Scissoring\n"
        "float scissorMask(vec2 p) {\n"
        "	vec2 sc = (abs((scissorMat * vec3(p,1.0)).xy) - scissorExt);\n"
        "	sc = vec2(0.5,0.5) - sc * scissorScale;\n"
        "	return clamp(sc.x,0.0,1.0) * clamp(sc.y,0.0,1.0);\n"
        "}\n"
        "#ifdef EDGE_AA\n"
        "// Stroke - from [0..1] to clipped pyramid, where the slope is 1px.\n"
        "float strokeMask() {\n"
        "	return min(1.0, (1.0-abs(ftcoord.x*2.0-1.0))*strokeMult) * min(1.0, ftcoord.y);\n"
        "}\n"
        "#endif\n"
        "\n"
        "void main(void) {\n"
        "   vec4 result;\n"
        "	float scissor = scissorMask(fpos);\n"
        "#ifdef EDGE_AA\n"
        "	float strokeAlpha = strokeMask();\n"
        "#else\n"
        "	float strokeAlpha = 1.0;\n"
        "#endif\n"
        "	if (type == 0) {			// Gradient\n"
        "		// Calculate gradient color using box gradient\n"
        "		vec2 pt = (paintMat * vec3(fpos,1.0)).xy;\n"
        "		float d = clamp((sdroundrect(pt, extent, radius) + feather*0.5) / feather, 0.0, 1.0);\n"
        "		vec4 color = mix(innerCol,outerCol,d);\n"
        "		// Combine alpha\n"
        "		color *= strokeAlpha * scissor;\n"
        "		result = color;\n"
        "	} else if (type == 1) {		// Image\n"
        "		// Calculate color fron texture\n"
        "		vec2 pt = (paintMat * vec3(fpos,1.0)).xy / extent;\n"
        "		vec4 color = texture(tex, pt);\n"
        "		if (texType == 1) color = vec4(color.xyz*color.w,color.w);"
        "		if (texType == 2) color = vec4(color.x);"
        "		// Apply color tint and alpha.\n"
        "		color *= innerCol;\n"
        "		// Combine alpha\n"
        "		color *= strokeAlpha * scissor;\n"
        "		result = color;\n"
        "	} else if (type == 2) {		// Stencil fill\n"
        "		result = vec4(1,1,1,1);\n"
        "	} else if (type == 3) {		// Textured tris\n"
        "		vec4 color = texture(tex, ftcoord);\n"
        "		if (texType == 1) color = vec4(color.xyz*color.w,color.w);"
        "		if (texType == 2) color = vec4(color.x);"
        "		color *= scissor;\n"
        "		result = color * innerCol;\n"
        "	}\n"
        "#ifdef EDGE_AA\n"
        "	if (strokeAlpha < strokeThr) discard;\n"
        "#endif\n"
        "	outColor = result;\n"
        "}\n";

    glnvg__checkError("init");

    if (gl->flags & NVG_ANTIALIAS) {
        if (glnvg__createShader(&gl->shader, "shader", shaderHeader, "#define EDGE_AA 1\n", fillVertShader, fillFragShader) == 0)
            return 0;
    } else {
        if (glnvg__createShader(&gl->shader, "shader", shaderHeader, NULL, fillVertShader, fillFragShader) == 0)
            return 0;
    }

    glnvg__checkError("uniform locations");

    // Create dynamic vertex array
    glGenVertexArrays(1, &gl->vertArr);
    glGenBuffers(1, &gl->vertBuf);

    // Create UBOs
    glGenBuffers(1, &gl->fragBuf);
    glGetIntegerv(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, &align);
    gl->fragSize = sizeof(GLNVGfragUniforms) + align - sizeof(GLNVGfragUniforms) % align;

    glnvg__checkError("create done");

    glFinish();

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

    if (glnvg__checkError("create tex"))
        return 0;

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
    GLNVGtexture* tex = NULL;
    float invxform[6];

    memset(frag, 0, sizeof(*frag));

    frag->innerCol = glnvg__premulColor(paint->innerColor);
    frag->outerCol = glnvg__premulColor(paint->outerColor);

    if (scissor->extent[0] < -0.5f || scissor->extent[1] < -0.5f) {
        memset(frag->scissorMat, 0, sizeof(frag->scissorMat));
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

    memcpy(frag->extent, paint->extent, sizeof(frag->extent));
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
    glBindBufferRange(GL_UNIFORM_BUFFER, GFX_NVG_BINDING_FRAG_UBO, gl->fragBuf, uniformOffset, sizeof(GLNVGfragUniforms));

    if (image != 0) {
        GLNVGtexture* tex = glnvg__findTexture(gl, image);
        glBindTexture(GL_TEXTURE_2D, tex != NULL ? tex->tex : 0);
        glnvg__checkError("tex paint tex");
    } else {
        glBindTexture(GL_TEXTURE_2D, 0);
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
    GLNVGpath* paths = &gl->paths[call->pathOffset];
    int i, npaths = call->pathCount;

    // Draw shapes
    glEnable(GL_STENCIL_TEST);
    glStencilMask(0xff);
    glStencilFunc(GL_ALWAYS, 0, 0xff);
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

    // set bindpoint for solid loc
    glnvg__setUniforms(gl, call->uniformOffset, 0);
    glnvg__checkError("fill simple");

    glStencilOpSeparate(GL_FRONT, GL_KEEP, GL_KEEP, GL_INCR_WRAP);
    glStencilOpSeparate(GL_BACK, GL_KEEP, GL_KEEP, GL_DECR_WRAP);
    glDisable(GL_CULL_FACE);
    for (i = 0; i < npaths; i++)
        glDrawArrays(GL_TRIANGLE_FAN, paths[i].fillOffset, paths[i].fillCount);
    glEnable(GL_CULL_FACE);

    // Draw anti-aliased pixels
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

    glnvg__setUniforms(gl, call->uniformOffset + gl->fragSize, call->image);
    glnvg__checkError("fill fill");

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
    GLNVGpath* paths = &gl->paths[call->pathOffset];
    int i, npaths = call->pathCount;

    glnvg__setUniforms(gl, call->uniformOffset, call->image);
    glnvg__checkError("convex fill");

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
    GLNVGpath* paths = &gl->paths[call->pathOffset];
    int npaths = call->pathCount, i;

    if (gl->flags & NVG_STENCIL_STROKES) {

        glEnable(GL_STENCIL_TEST);
        glStencilMask(0xff);

        // Fill the stroke base without overlap
        glStencilFunc(GL_EQUAL, 0x0, 0xff);
        glStencilOp(GL_KEEP, GL_KEEP, GL_INCR);
        glnvg__setUniforms(gl, call->uniformOffset + gl->fragSize, call->image);
        glnvg__checkError("stroke fill 0");
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
        glnvg__checkError("stroke fill 1");
        for (i = 0; i < npaths; i++)
            glDrawArrays(GL_TRIANGLE_STRIP, paths[i].strokeOffset, paths[i].strokeCount);
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

        glDisable(GL_STENCIL_TEST);

        //		glnvg__convertPaint(gl, nvg__fragUniformPtr(gl, call->uniformOffset + gl->fragSize), paint, scissor, strokeWidth, fringe, 1.0f - 0.5f/255.0f);

    } else {
        glnvg__setUniforms(gl, call->uniformOffset, call->image);
        glnvg__checkError("stroke fill");
        // Draw Strokes
        for (i = 0; i < npaths; i++)
            glDrawArrays(GL_TRIANGLE_STRIP, paths[i].strokeOffset, paths[i].strokeCount);
    }
}

static void glnvg__triangles(GLNVGcontext* gl, GLNVGcall* call)
{
    glnvg__setUniforms(gl, call->uniformOffset, call->image);
    glnvg__checkError("triangles fill");

    glDrawArrays(GL_TRIANGLES, call->triangleOffset, call->triangleCount);
}

static void glnvg__renderFlush(void* uptr)
{
    GLNVGcontext* gl = (GLNVGcontext*)uptr;
    int i;

    if (gl->ncalls > 0) {

        // Setup require GL state.
        glUseProgram(gl->shader.prog);

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
        glActiveTexture(GL_TEXTURE0);

        // Upload ubo for frag shaders
        glNamedBufferDataEXT(gl->fragBuf, gl->nuniforms * gl->fragSize, gl->uniforms, GL_STREAM_DRAW);

        // Upload vertex data
        glBindVertexArray(gl->vertArr);
        glBindBuffer(GL_ARRAY_BUFFER, gl->vertBuf);
        glNamedBufferDataEXT(gl->vertBuf, gl->nverts * sizeof(NVGvertex), gl->verts, GL_STREAM_DRAW);
        glEnableVertexAttribArray(0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(NVGvertex), (const GLvoid*)(size_t)0);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(NVGvertex), (const GLvoid*)(0 + 2 * sizeof(float)));

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

        glDisableVertexAttribArray(0);
        glDisableVertexAttribArray(1);
        glBindVertexArray(0);
        glUseProgram(0);
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    // Reset calls
    gl->nverts = 0;
    gl->npaths = 0;
    gl->ncalls = 0;
    gl->nuniforms = 0;
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
    memset(ret, 0, sizeof(GLNVGcall));
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

static int glnvg__allocVerts(GLNVGcontext* gl, int n)
{
    int ret = 0;
    if (gl->nverts + n > gl->cverts) {
        NVGvertex* verts;
        int cverts = core::max(gl->nverts + n, 4096) + gl->cverts / 2; // 1.5x Overallocate
        verts = (NVGvertex*)realloc(gl->verts, sizeof(NVGvertex) * cverts);
        if (verts == NULL) return -1;
        gl->verts = verts;
        gl->cverts = cverts;
    }
    ret = gl->nverts;
    gl->nverts += n;
    return ret;
}

static int glnvg__allocFragUniforms(GLNVGcontext* gl, int n)
{
    int ret = 0, structSize = gl->fragSize;
    if (gl->nuniforms + n > gl->cuniforms) {
        unsigned char* uniforms;
        int cuniforms = core::max(gl->nuniforms + n, 128) + gl->cuniforms / 2; // 1.5x Overallocate
        uniforms = (unsigned char*)realloc(gl->uniforms, structSize * cuniforms);
        if (uniforms == NULL) return -1;
        gl->uniforms = uniforms;
        gl->cuniforms = cuniforms;
    }
    ret = gl->nuniforms * structSize;
    gl->nuniforms += n;
    return ret;
}

static GLNVGfragUniforms* nvg__fragUniformPtr(GLNVGcontext* gl, int i)
{
    return (GLNVGfragUniforms*)&gl->uniforms[i];
}

static void glnvg__vset(NVGvertex* vtx, float x, float y, float u, float v)
{
    vtx->x = x;
    vtx->y = y;
    vtx->u = u;
    vtx->v = v;
}

static void glnvg__renderFill(void* uptr, NVGpaint* paint, NVGscissor* scissor, float fringe,
    const float* bounds, const NVGpath* paths, int npaths)
{
    GLNVGcontext* gl = (GLNVGcontext*)uptr;
    GLNVGcall* call = glnvg__allocCall(gl);
    NVGvertex* quad;
    GLNVGfragUniforms* frag;
    int i, maxverts, offset;

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
    offset = glnvg__allocVerts(gl, maxverts);
    if (offset == -1) goto error;

    for (i = 0; i < npaths; i++) {
        GLNVGpath* copy = &gl->paths[call->pathOffset + i];
        const NVGpath* path = &paths[i];
        memset(copy, 0, sizeof(GLNVGpath));
        if (path->nfill > 0) {
            copy->fillOffset = offset;
            copy->fillCount = path->nfill;
            memcpy(&gl->verts[offset], path->fill, sizeof(NVGvertex) * path->nfill);
            offset += path->nfill;
        }
        if (path->nstroke > 0) {
            copy->strokeOffset = offset;
            copy->strokeCount = path->nstroke;
            memcpy(&gl->verts[offset], path->stroke, sizeof(NVGvertex) * path->nstroke);
            offset += path->nstroke;
        }
    }

    // Quad
    call->triangleOffset = offset;
    call->triangleCount = 6;
    quad = &gl->verts[call->triangleOffset];
    glnvg__vset(&quad[0], bounds[0], bounds[3], 0.5f, 1.0f);
    glnvg__vset(&quad[1], bounds[2], bounds[3], 0.5f, 1.0f);
    glnvg__vset(&quad[2], bounds[2], bounds[1], 0.5f, 1.0f);

    glnvg__vset(&quad[3], bounds[0], bounds[3], 0.5f, 1.0f);
    glnvg__vset(&quad[4], bounds[2], bounds[1], 0.5f, 1.0f);
    glnvg__vset(&quad[5], bounds[0], bounds[1], 0.5f, 1.0f);

    // Setup uniforms for draw calls
    if (call->type == GLNVG_FILL) {
        call->uniformOffset = glnvg__allocFragUniforms(gl, 2);
        if (call->uniformOffset == -1) goto error;
        // Simple shader for stencil
        frag = nvg__fragUniformPtr(gl, call->uniformOffset);
        memset(frag, 0, sizeof(*frag));
        frag->strokeThr = -1.0f;
        frag->type = NSVG_SHADER_SIMPLE;
        // Fill shader
        glnvg__convertPaint(gl, nvg__fragUniformPtr(gl, call->uniformOffset + gl->fragSize), paint, scissor, fringe, fringe, -1.0f);
    } else {
        call->uniformOffset = glnvg__allocFragUniforms(gl, 1);
        if (call->uniformOffset == -1) goto error;
        // Fill shader
        glnvg__convertPaint(gl, nvg__fragUniformPtr(gl, call->uniformOffset), paint, scissor, fringe, fringe, -1.0f);
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
    GLNVGcontext* gl = (GLNVGcontext*)uptr;
    GLNVGcall* call = glnvg__allocCall(gl);
    int i, maxverts, offset;

    if (call == NULL) return;

    call->type = GLNVG_STROKE;
    call->pathOffset = glnvg__allocPaths(gl, npaths);
    if (call->pathOffset == -1) goto error;
    call->pathCount = npaths;
    call->image = paint->image;

    // Allocate vertices for all the paths.
    maxverts = glnvg__maxVertCount(paths, npaths);
    offset = glnvg__allocVerts(gl, maxverts);
    if (offset == -1) goto error;

    for (i = 0; i < npaths; i++) {
        GLNVGpath* copy = &gl->paths[call->pathOffset + i];
        const NVGpath* path = &paths[i];
        memset(copy, 0, sizeof(GLNVGpath));
        if (path->nstroke) {
            copy->strokeOffset = offset;
            copy->strokeCount = path->nstroke;
            memcpy(&gl->verts[offset], path->stroke, sizeof(NVGvertex) * path->nstroke);
            offset += path->nstroke;
        }
    }

    if (gl->flags & NVG_STENCIL_STROKES) {
        // Fill shader
        call->uniformOffset = glnvg__allocFragUniforms(gl, 2);
        if (call->uniformOffset == -1) goto error;

        glnvg__convertPaint(gl, nvg__fragUniformPtr(gl, call->uniformOffset), paint, scissor, strokeWidth, fringe, -1.0f);
        glnvg__convertPaint(gl, nvg__fragUniformPtr(gl, call->uniformOffset + gl->fragSize), paint, scissor, strokeWidth, fringe, 1.0f - 0.5f / 255.0f);

    } else {
        // Fill shader
        call->uniformOffset = glnvg__allocFragUniforms(gl, 1);
        if (call->uniformOffset == -1) goto error;
        glnvg__convertPaint(gl, nvg__fragUniformPtr(gl, call->uniformOffset), paint, scissor, strokeWidth, fringe, -1.0f);
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
    GLNVGcontext* gl = (GLNVGcontext*)uptr;
    GLNVGcall* call = glnvg__allocCall(gl);
    GLNVGfragUniforms* frag;

    if (call == NULL) return;

    call->type = GLNVG_TRIANGLES;
    call->image = paint->image;

    // Allocate vertices for all the paths.
    call->triangleOffset = glnvg__allocVerts(gl, nverts);
    if (call->triangleOffset == -1) goto error;
    call->triangleCount = nverts;

    memcpy(&gl->verts[call->triangleOffset], verts, sizeof(NVGvertex) * nverts);

    // Fill shader
    call->uniformOffset = glnvg__allocFragUniforms(gl, 1);
    if (call->uniformOffset == -1) goto error;
    frag = nvg__fragUniformPtr(gl, call->uniformOffset);
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

    glnvg__deleteShader(&gl->shader);

    if (gl->fragBuf != 0)
        glDeleteBuffers(1, &gl->fragBuf);
    if (gl->vertArr != 0)
        glDeleteVertexArrays(1, &gl->vertArr);
    if (gl->vertBuf != 0)
        glDeleteBuffers(1, &gl->vertBuf);

    for (i = 0; i < gl->ntextures; i++) {
        if (gl->textures[i].tex != 0 && (gl->textures[i].flags & NVGL_TEXTURE_NODELETE) == 0)
            glDeleteTextures(1, &gl->textures[i].tex);
    }
    free(gl->textures);

    free(gl->paths);
    free(gl->verts);
    free(gl->uniforms);
    free(gl->calls);

    free(gl);
}


NVGcontext* nvgCreateGL3(int flags)
{
    NVGparams params;
    NVGcontext* ctx = NULL;
    GLNVGcontext* gl = (GLNVGcontext*)malloc(sizeof(GLNVGcontext));
    if (gl == NULL) goto error;
    memset(gl, 0, sizeof(GLNVGcontext));

    memset(&params, 0, sizeof(params));
    params.renderCreate = glnvg__renderCreate;
    params.renderCreateTexture = glnvg__renderCreateTexture;
    params.renderDeleteTexture = glnvg__renderDeleteTexture;
    params.renderUpdateTexture = glnvg__renderUpdateTexture;
    params.renderGetTextureSize = glnvg__renderGetTextureSize;
    params.renderViewport = glnvg__renderViewport;
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