#ifndef __CDLODTERRAIN_H_INCLUDED__
#	define __CDLODTERRAIN_H_INCLUDED__

#include <framework.h>
#include <vector>
#include <glm/glm.h>
#include <vi.h>
#include "math.h"

struct PatchData;

class CDLODTerrain
{
public:
    static const size_t MAX_LOD_COUNT = 8;
    static const size_t MAX_PATCH_COUNT = 4096;

    size_t gridDimX;
    size_t gridDimY;
    size_t LODCount;
    size_t patchDim;
    float  cellSize;

    float heightScale;
    float startX;
    float startY;
    float morphZoneRatio;
    bool  useOverDrawOptimization;

    bool		useInstancing;

    GLuint		prgInstancedTerrain, prgTerrain;
    GLuint		mHeightmapTex;
    GLuint		mColorRampTex;
    GLuint		vaoInst, vao;

    GLsizei		uniTerrainOffset, uniViewOffset, uniGradientOffset, uniPatchOffset;

    struct ViewData
    {
        ml::mat4x4	uMVP;
        vec4		uLODViewK;
    } viewData;

    GLuint		geomVBO, instVBO, ibo, ubo;

    PatchData*	instData;
    size_t		patchCount;
    size_t		maxPatchCount;
    GLsizei		idxCount;

    glm::vec3	viewDir;
    glm::vec3   viewPoint;

    ml::mat4x4	sseVP;

    struct LODDesc
    {
        float	rangeStart;
        size_t	patchDim;
    };

    LODDesc		LODs[MAX_LOD_COUNT];

    void generateGeometry();
    void generateBBoxData(uint16_t* data);

    void setHeightmap(uint16_t* data, size_t width, size_t height);
    void setSelectMatrix(glm::mat4& mat);
    void setMVPMatrix(glm::mat4& mat);

    void calculateLODParams(vec4* morphParams);

    void addPatchToQueue(size_t level, size_t i, size_t j);

    void selectQuadsForDrawing(size_t level, size_t i, size_t j, bool skipFrustumTest=false);

    void initialize();
    void cleanup();

    void drawTerrain();

    void reset();

    bool drawWireframe;

    CPUTimer	cpuTimer;
    CPUTimer	cpuSelectTimer;
    CPUTimer	cpuDrawTimer;
    GPUTimer	gpuTimer;

    double cpuTime;
    double cpuSelectTime;
    double cpuDrawTime;
    double gpuTime;

    float getCPUTime() {return (float)cpuTime;}
    float getCPUSelectTime() {return (float)cpuSelectTime;}
    float getCPUDrawTime() {return (float)cpuDrawTime;}
    float getGPUTime() {return (float)gpuTime;}
};

#endif