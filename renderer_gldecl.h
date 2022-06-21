#ifndef RENDERER_GLDECL_H
#define RENDERER_GLDECL_H

#if defined(__APPLE__)
#define GL_SILENCE_DEPRECATION
#define APIENTRY
#include <OpenGL/gl.h>
#define GL_RG16F   0x822F
#define GL_RGB16F  0x881B
#define GL_RGBA16F 0x881A
#else
#include <GL/gl.h>
#endif

typedef void      (APIENTRY pglGenBuffers_t)(GLsizei, GLuint *);
typedef void      (APIENTRY pglDeleteBuffers_t)(GLsizei, GLuint *);
typedef void      (APIENTRY pglClearColor_t)(GLfloat, GLfloat, GLfloat, GLfloat);
typedef void      (APIENTRY pglClear_t)(GLbitfield);
typedef void      (APIENTRY pglEnable_t)(GLenum);
typedef void      (APIENTRY pglDisable_t)(GLenum);
typedef void      (APIENTRY pglDepthFunc_t)(GLenum);
typedef void      (APIENTRY pglDepthMask_t)(GLboolean);
typedef const GLubyte
                 *(APIENTRY pglGetString_t)(GLenum);
typedef GLenum    (APIENTRY pglGetError_t)(void);
typedef void      (APIENTRY pglBindBuffer_t)(GLenum, GLuint);
typedef void      (APIENTRY pglBufferData_t)(GLenum, GLsizeiptr, const GLvoid *, GLenum);
typedef void      (APIENTRY pglGenVertexArrays_t)(GLsizei, GLuint *);
typedef void      (APIENTRY pglDeleteVertexArrays_t)(GLsizei, GLuint *);
typedef void      (APIENTRY pglBindVertexArray_t)(GLuint);
typedef void      (APIENTRY pglDrawArrays_t)(GLenum, GLint, GLsizei);
typedef void      (APIENTRY pglVertexAttribPointer_t)(GLuint, GLint, GLenum, GLboolean, GLsizei, GLvoid *);
typedef void      (APIENTRY pglEnableVertexAttribArray_t)(GLuint);
typedef GLuint    (APIENTRY pglCreateShader_t)(GLenum);
typedef void      (APIENTRY pglDeleteShader_t)(GLuint);
typedef void      (APIENTRY pglShaderSource_t)(GLuint, GLsizei, const GLchar **, const GLint *);
typedef void      (APIENTRY pglCompileShader_t)(GLuint);
typedef void      (APIENTRY pglGetShaderiv_t)(GLuint, GLenum, GLint *);
typedef GLuint    (APIENTRY pglCreateProgram_t)(void);
typedef void      (APIENTRY pglDeleteProgram_t)(GLuint);
typedef void      (APIENTRY pglAttachShader_t)(GLuint, GLuint);
typedef void      (APIENTRY pglGetShaderInfoLog_t)(GLuint, GLsizei, GLsizei *, GLchar *);
typedef void      (APIENTRY pglLinkProgram_t)(GLuint);
typedef void      (APIENTRY pglUseProgram_t)(GLuint);
typedef void      (APIENTRY pglGetProgramiv_t)(GLuint, GLenum, GLint *);
typedef void      (APIENTRY pglGetProgramInfoLog_t)(GLuint, GLsizei, GLsizei *, GLchar *);
typedef GLint     (APIENTRY pglGetUniformLocation_t)(GLuint, const GLchar *);
typedef void      (APIENTRY pglUniformMatrix3fv_t)(GLint, GLsizei, GLboolean, const GLfloat *);
typedef void      (APIENTRY pglUniformMatrix4fv_t)(GLint, GLsizei, GLboolean, const GLfloat *);
typedef void      (APIENTRY pglUniform1i_t)(GLint, GLint);
typedef void      (APIENTRY pglUniform1f_t)(GLint, GLfloat);
typedef void      (APIENTRY pglUniform1fv_t)(GLint, GLsizei, const GLfloat *);
typedef void      (APIENTRY pglUniform2fv_t)(GLint, GLsizei, const GLfloat *);
typedef void      (APIENTRY pglUniform3fv_t)(GLint, GLsizei, const GLfloat *);
typedef void      (APIENTRY pglDrawElements_t)(GLenum, GLsizei, GLenum, const GLvoid *);
typedef void      (APIENTRY pglViewport_t)(GLint, GLint, GLsizei, GLsizei);
typedef void      (APIENTRY pglCullFace_t)(GLenum);
typedef void      (APIENTRY pglGenFramebuffers_t)(GLsizei, GLuint *);
typedef void      (APIENTRY pglDeleteFramebuffers_t)(GLsizei, GLuint *);
typedef void      (APIENTRY pglBindFramebuffer_t)(GLenum, GLuint);
typedef GLboolean (APIENTRY pglIsFramebuffer_t)(GLuint);
typedef GLenum    (APIENTRY pglCheckFramebufferStatus_t)(GLenum);
typedef void      (APIENTRY pglDrawBuffer_t)(GLenum);
typedef void      (APIENTRY pglReadBuffer_t)(GLenum);
typedef void      (APIENTRY pglDrawBuffers_t)(GLsizei, const GLenum *);
typedef void      (APIENTRY pglGenTextures_t)(GLsizei, GLuint *);
typedef void      (APIENTRY pglDeleteTextures_t)(GLsizei, const GLuint *);
typedef void      (APIENTRY pglBindTexture_t)(GLenum, GLuint);
typedef void      (APIENTRY pglActiveTexture_t)(GLenum);
typedef void      (APIENTRY pglTexImage2D_t)(GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const GLvoid *);
typedef void      (APIENTRY pglTexParameteri_t)(GLenum, GLenum, GLint);
typedef void      (APIENTRY pglTexParameterfv_t)(GLenum, GLenum, const GLfloat *);
typedef void      (APIENTRY pglFramebufferTexture2D_t)(GLenum, GLenum, GLenum, GLuint, GLint);
typedef void      (APIENTRY pglGenRenderbuffers_t)(GLsizei, GLuint *);
typedef void      (APIENTRY pglDeleteRenderbuffers_t)(GLsizei, GLuint *);
typedef void      (APIENTRY pglBindRenderbuffer_t)(GLenum, GLuint);
typedef void      (APIENTRY pglRenderbufferStorage_t)(GLenum, GLenum, GLsizei, GLsizei);
typedef void      (APIENTRY pglFramebufferRenderbuffer_t)(GLenum, GLenum, GLenum, GLuint);

typedef struct rdGL rdGL;
struct rdGL
{
	pglGenBuffers_t              *GenBuffers;
	pglDeleteBuffers_t           *DeleteBuffers;
	pglClearColor_t              *ClearColor;
	pglClear_t                   *Clear;
	pglEnable_t                  *Enable;
	pglDisable_t                 *Disable;
	pglDepthFunc_t               *DepthFunc;
	pglDepthMask_t               *DepthMask;
	pglGetString_t               *GetString;
	pglGetError_t                *GetError;
	pglBindBuffer_t              *BindBuffer;
	pglBufferData_t              *BufferData;
	pglGenVertexArrays_t         *GenVertexArrays;
	pglDeleteVertexArrays_t      *DeleteVertexArrays;
	pglBindVertexArray_t         *BindVertexArray;
	pglDrawArrays_t              *DrawArrays;
	pglVertexAttribPointer_t     *VertexAttribPointer;
	pglEnableVertexAttribArray_t *EnableVertexAttribArray;
	pglCreateShader_t            *CreateShader;
	pglDeleteShader_t            *DeleteShader;
	pglShaderSource_t            *ShaderSource;
	pglCompileShader_t           *CompileShader;
	pglGetShaderiv_t             *GetShaderiv;
	pglCreateProgram_t           *CreateProgram;
	pglDeleteProgram_t           *DeleteProgram;
	pglAttachShader_t            *AttachShader;
	pglGetShaderInfoLog_t        *GetShaderInfoLog;
	pglLinkProgram_t             *LinkProgram;
	pglUseProgram_t              *UseProgram;
	pglGetProgramiv_t            *GetProgramiv;
	pglGetProgramInfoLog_t       *GetProgramInfoLog;
	pglGetUniformLocation_t      *GetUniformLocation;
	pglUniformMatrix3fv_t        *UniformMatrix3fv;
	pglUniformMatrix4fv_t        *UniformMatrix4fv;
	pglUniform1i_t               *Uniform1i;
	pglUniform1f_t               *Uniform1f;
	pglUniform1fv_t              *Uniform1fv;
	pglUniform2fv_t              *Uniform2fv;
	pglUniform3fv_t              *Uniform3fv;
	pglDrawElements_t            *DrawElements;
	pglViewport_t                *Viewport;
	pglCullFace_t                *CullFace;
	pglGenFramebuffers_t         *GenFramebuffers;
	pglBindFramebuffer_t         *BindFramebuffer;
	pglIsFramebuffer_t           *IsFramebuffer;
	pglCheckFramebufferStatus_t  *CheckFramebufferStatus;
	pglDrawBuffer_t              *DrawBuffer;
	pglReadBuffer_t              *ReadBuffer;
	pglDrawBuffers_t             *DrawBuffers;
	pglGenTextures_t             *GenTextures;
	pglDeleteTextures_t          *DeleteTextures;
	pglBindTexture_t             *BindTexture;
	pglActiveTexture_t           *ActiveTexture;
	pglTexImage2D_t              *TexImage2D;
	pglTexParameteri_t           *TexParameteri;
	pglTexParameterfv_t          *TexParameterfv;
	pglFramebufferTexture2D_t    *FramebufferTexture2D;
	pglGenRenderbuffers_t        *GenRenderbuffers;
	pglDeleteRenderbuffers_t     *DeleteRenderbuffers;
	pglDeleteFramebuffers_t      *DeleteFramebuffers;
	pglBindRenderbuffer_t        *BindRenderbuffer;
	pglRenderbufferStorage_t     *RenderbufferStorage;
	pglFramebufferRenderbuffer_t *FramebufferRenderbuffer;
};

#endif
