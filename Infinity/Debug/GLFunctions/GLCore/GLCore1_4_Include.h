#define GLI_INCLUDE_GLCORE1_4


///////////////////////////////////////////////////////////////////////////////
// Masks
///////////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////////
// Error enum
///////////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////////
// Main enum
///////////////////////////////////////////////////////////////////////////////

// Main enum for all standard enums
//  (may want to break up for speed)
enum Main {

/* OpenGL 1.4 */

  GL_GENERATE_MIPMAP                    =  0x8191,
  GL_GENERATE_MIPMAP_HINT               =  0x8192,
  GL_DEPTH_COMPONENT16                  =  0x81A5,
  GL_DEPTH_COMPONENT24                  =  0x81A6,
  GL_DEPTH_COMPONENT32                  =  0x81A7,
  GL_TEXTURE_DEPTH_SIZE                 =  0x884A,
  GL_DEPTH_TEXTURE_MODE                 =  0x884B,
  GL_TEXTURE_COMPARE_MODE               =  0x884C,
  GL_TEXTURE_COMPARE_FUNC               =  0x884D,
  GL_COMPARE_R_TO_TEXTURE               =  0x884E,
  GL_FOG_COORDINATE_SOURCE              =  0x8450,
  GL_FOG_COORDINATE                     =  0x8451,
  GL_FRAGMENT_DEPTH                     =  0x8452,
  GL_CURRENT_FOG_COORDINATE             =  0x8453,
  GL_FOG_COORDINATE_ARRAY_TYPE          =  0x8454,
  GL_FOG_COORDINATE_ARRAY_STRIDE        =  0x8455,
  GL_FOG_COORDINATE_ARRAY_POINTER       =  0x8456,
  GL_FOG_COORDINATE_ARRAY               =  0x8457,
  GL_POINT_SIZE_MIN                     =  0x8126,
  GL_POINT_SIZE_MAX                     =  0x8127,
  GL_POINT_FADE_THRESHOLD_SIZE          =  0x8128,
  GL_POINT_DISTANCE_ATTENUATION         =  0x8129,
  GL_COLOR_SUM                          =  0x8458,
  GL_CURRENT_SECONDARY_COLOR            =  0x8459,
  GL_SECONDARY_COLOR_ARRAY_SIZE         =  0x845A,
  GL_SECONDARY_COLOR_ARRAY_TYPE         =  0x845B,
  GL_SECONDARY_COLOR_ARRAY_STRIDE       =  0x845C,
  GL_SECONDARY_COLOR_ARRAY_POINTER      =  0x845D,
  GL_SECONDARY_COLOR_ARRAY              =  0x845E,
  GL_BLEND_DST_RGB                      =  0x80C8,
  GL_BLEND_SRC_RGB                      =  0x80C9,
  GL_BLEND_DST_ALPHA                    =  0x80CA,
  GL_BLEND_SRC_ALPHA                    =  0x80CB,
  GL_INCR_WRAP                          =  0x8507,
  GL_DECR_WRAP                          =  0x8508,
  GL_TEXTURE_FILTER_CONTROL             =  0x8500,
  GL_TEXTURE_LOD_BIAS                   =  0x8501,
  GL_MAX_TEXTURE_LOD_BIAS               =  0x84FD,
  GL_MIRRORED_REPEAT                    =  0x8370,

};

///////////////////////////////////////////////////////////////////////////////
// OpenGL 1.4 functions
///////////////////////////////////////////////////////////////////////////////


void glFogCoordf (GLfloat coord);

void glFogCoordfv (const GLfloat *coord);

void glFogCoordd (GLdouble coord);

void glFogCoorddv (const GLdouble *coord);

void glFogCoordPointer (GLenum[Main] type, GLsizei stride, const GLvoid *pointer);

void glMultiDrawArrays (GLenum[Primitives] mode, GLint *first, GLsizei *count, GLsizei primcount);

void glMultiDrawElements (GLenum[Primitives] mode, GLsizei *count, GLenum[Main] type, const GLvoid **indices, GLsizei primcount);

void glPointParameterf (GLenum[Main] pname, GLfloat param);

void glPointParameterfv (GLenum[Main] pname, GLfloat *params);

void glPointParameteri (GLenum[Main] pname, GLint param);

void glPointParameteriv (GLenum[Main] pname, GLint *params);

void glSecondaryColor3b (GLbyte red, GLbyte green, GLbyte blue);

void glSecondaryColor3bv (const GLbyte *v);

void glSecondaryColor3d (GLdouble red, GLdouble green, GLdouble blue);

void glSecondaryColor3dv (const GLdouble *v);

void glSecondaryColor3f (GLfloat red, GLfloat green, GLfloat blue);

void glSecondaryColor3fv (const GLfloat *v);

void glSecondaryColor3i (GLint red, GLint green, GLint blue);

void glSecondaryColor3iv (const GLint *v);

void glSecondaryColor3s (GLshort red, GLshort green, GLshort blue);

void glSecondaryColor3sv (const GLshort *v);

void glSecondaryColor3ub (GLubyte red, GLubyte green, GLubyte blue);

void glSecondaryColor3ubv (const GLubyte *v);

void glSecondaryColor3ui (GLuint red, GLuint green, GLuint blue);

void glSecondaryColor3uiv (const GLuint *v);

void glSecondaryColor3us (GLushort red, GLushort green, GLushort blue);

void glSecondaryColor3usv (const GLushort *v);

void glSecondaryColorPointer (GLint size, GLenum[Main] type, GLsizei stride, GLvoid *pointer);

void glBlendFuncSeparate (GLenum[Main] sfactorRGB, GLenum[Main] dfactorRGB, GLenum[Main] sfactorAlpha, GLenum[Main] dfactorAlpha);

void glWindowPos2d (GLdouble x, GLdouble y);

void glWindowPos2f (GLfloat x, GLfloat y);

void glWindowPos2i (GLint x, GLint y);

void glWindowPos2s (GLshort x, GLshort y);

void glWindowPos2dv (const GLdouble *p);

void glWindowPos2fv (const GLfloat *p);

void glWindowPos2iv (const GLint *p);

void glWindowPos2sv (const GLshort *p);

void glWindowPos3d (GLdouble x, GLdouble y, GLdouble z);

void glWindowPos3f (GLfloat x, GLfloat y, GLfloat z);

void glWindowPos3i (GLint x, GLint y, GLint z);

void glWindowPos3s (GLshort x, GLshort y, GLshort z);

void glWindowPos3dv (const GLdouble *p);

void glWindowPos3fv (const GLfloat *p);

void glWindowPos3iv (const GLint *p);

void glWindowPos3sv (const GLshort *p);



