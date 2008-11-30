#define GLI_INCLUDE_ATI_ENVMAP_BUMP_MAP

enum Main {

  GL_BUMP_ROT_MATRIX_ATI                =   0x8775,
  GL_BUMP_ROT_MATRIX_SIZE_ATI           =   0x8776,
  GL_BUMP_NUM_TEX_UNITS_ATI             =   0x8777,
  GL_BUMP_TEX_UNITS_ATI                 =   0x8778,
  GL_DUDV_ATI                           =   0x8779,
  GL_DU8DV8_ATI                         =   0x877A,
  GL_BUMP_ENVMAP_ATI                    =   0x877B,
  GL_BUMP_TARGET_ATI                    =   0x877C,

};

void glTexBumpParameterivATI (GLenum[Main] pname, const GLint *param);
void glTexBumpParameterfvATI (GLenum[Main] pname, const GLfloat *param);
void glGetTexBumpParameterivATI (GLenum[Main] pname, GLint *param);
void glGetTexBumpParameterfvATI (GLenum[Main] pname, GLfloat *param);
