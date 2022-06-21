#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <SDL2/SDL.h>

#include "renderer.h"
#include "renderer_gldecl.h"
#include "game.h"

#define SCREEN_WIDTH 1280
#define SCREEN_HEIGHT 960

static void  load_gl(rdGL *gl);
static void *gl_proc(const char *proc);
static void *alloc_or_abort(size_t size);

int main(int argc, char **argv)
{
	SDL_Window   *window;
	SDL_GLContext glc;
	rdGL          gl;

	srand( (unsigned int) time(NULL));

	if (SDL_Init(SDL_INIT_VIDEO) != 0) {
		fprintf(stderr, "Error: couldn't initialize SDL");
		return EXIT_FAILURE;
	}

	if (SDL_GL_LoadLibrary(NULL) != 0) {
		fprintf(stderr, "Error: couldn't load OpenGL: %s\n", SDL_GetError());
		SDL_Quit();
	}

	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

	SDL_ShowCursor(SDL_FALSE);
	window = SDL_CreateWindow("Project 3D", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
		                       SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_OPENGL);
	if (window == NULL) {
		fprintf(stderr, "Error: couldn't create window: %s\n", SDL_GetError());
		SDL_Quit();
		return EXIT_FAILURE;
	}
	SDL_SetWindowGrab(window, SDL_TRUE);
	glc = SDL_GL_CreateContext(window);
	if (glc == NULL) {
		fprintf(stderr, "Error: couldn't create OpenGL context: %s\n", SDL_GetError());
		SDL_DestroyWindow(window);
		SDL_Quit();
		return EXIT_FAILURE;
	}
	SDL_GL_SetSwapInterval(1);
	SDL_SetRelativeMouseMode(SDL_TRUE);
	load_gl(&gl);
	rd_Init(gl, SCREEN_WIDTH, SCREEN_HEIGHT);
	rd_SetCustomAllocator(alloc_or_abort, free);
	gm_Main(window);

	rd_Shutdown();
	SDL_GL_DeleteContext(glc);
	SDL_DestroyWindow(window);
	SDL_Quit();
	return EXIT_SUCCESS;
}

static void load_gl(rdGL *gl)
{
	gl->GenBuffers              = gl_proc("glGenBuffers");
	gl->DeleteBuffers           = gl_proc("glDeleteBuffers");
	gl->ClearColor              = gl_proc("glClearColor");
	gl->Clear                   = gl_proc("glClear");
	gl->Enable                  = gl_proc("glEnable");
	gl->Disable                 = gl_proc("glDisable");
	gl->DepthFunc               = gl_proc("glDepthFunc");
	gl->DepthMask               = gl_proc("glDepthMask");
	gl->GetString               = gl_proc("glGetString");
	gl->GetError                = gl_proc("glGetError");
	gl->BindBuffer              = gl_proc("glBindBuffer");
	gl->BufferData              = gl_proc("glBufferData");
	gl->GenVertexArrays         = gl_proc("glGenVertexArrays");
	gl->DeleteVertexArrays      = gl_proc("glDeleteVertexArrays");
	gl->BindVertexArray         = gl_proc("glBindVertexArray");
	gl->DrawArrays              = gl_proc("glDrawArrays");
	gl->VertexAttribPointer     = gl_proc("glVertexAttribPointer");
	gl->EnableVertexAttribArray = gl_proc("glEnableVertexAttribArray");
	gl->CreateShader            = gl_proc("glCreateShader");
	gl->DeleteShader            = gl_proc("glDeleteShader");
	gl->ShaderSource            = gl_proc("glShaderSource");
	gl->CompileShader           = gl_proc("glCompileShader");
	gl->GetShaderiv             = gl_proc("glGetShaderiv");
	gl->CreateProgram           = gl_proc("glCreateProgram");
	gl->DeleteProgram           = gl_proc("glDeleteProgram");
	gl->AttachShader            = gl_proc("glAttachShader");
	gl->GetShaderInfoLog        = gl_proc("glGetShaderInfoLog");
	gl->LinkProgram             = gl_proc("glLinkProgram");
	gl->UseProgram              = gl_proc("glUseProgram");
	gl->GetProgramiv            = gl_proc("glGetProgramiv");
	gl->GetProgramInfoLog       = gl_proc("glGetProgramInfoLog");
	gl->GetUniformLocation      = gl_proc("glGetUniformLocation");
	gl->UniformMatrix3fv        = gl_proc("glUniformMatrix3fv");
	gl->UniformMatrix4fv        = gl_proc("glUniformMatrix4fv");
	gl->Uniform1i               = gl_proc("glUniform1i");
	gl->Uniform1f               = gl_proc("glUniform1f");
	gl->Uniform1fv              = gl_proc("glUniform1fv");
	gl->Uniform2fv              = gl_proc("glUniform2fv");
	gl->Uniform3fv              = gl_proc("glUniform3fv");
	gl->DrawElements            = gl_proc("glDrawElements");
	gl->Viewport                = gl_proc("glViewport");
	gl->CullFace                = gl_proc("glCullFace");
	gl->GenFramebuffers         = gl_proc("glGenFramebuffers");
	gl->DeleteFramebuffers      = gl_proc("glDeleteFramebuffers");
	gl->BindFramebuffer         = gl_proc("glBindFramebuffer");
	gl->IsFramebuffer           = gl_proc("glIsFramebuffer");
	gl->CheckFramebufferStatus  = gl_proc("glCheckFramebufferStatus");
	gl->DrawBuffer              = gl_proc("glDrawBuffer");
	gl->ReadBuffer              = gl_proc("glReadBuffer");
	gl->DrawBuffers             = gl_proc("glDrawBuffers");
	gl->GenTextures             = gl_proc("glGenTextures");
	gl->DeleteTextures          = gl_proc("glDeleteTextures");
	gl->BindTexture             = gl_proc("glBindTexture");
	gl->ActiveTexture           = gl_proc("glActiveTexture");
	gl->TexImage2D              = gl_proc("glTexImage2D");
	gl->TexParameteri           = gl_proc("glTexParameteri");
	gl->TexParameterfv          = gl_proc("glTexParameterfv");
	gl->FramebufferTexture2D    = gl_proc("glFramebufferTexture2D");
	gl->GenRenderbuffers        = gl_proc("glGenRenderbuffers");
	gl->DeleteRenderbuffers     = gl_proc("glDeleteRenderbuffers");
	gl->BindRenderbuffer        = gl_proc("glBindRenderbuffer");
	gl->RenderbufferStorage     = gl_proc("glRenderbufferStorage");
	gl->FramebufferRenderbuffer = gl_proc("glFramebufferRenderbuffer");
}

static void *gl_proc(const char *proc)
{
	void *fptr = SDL_GL_GetProcAddress(proc);

	if (fptr == NULL) {
		fprintf(stderr, "Error: couldn't load OpenGL function %s\n", proc);
		abort();
	}
	return fptr;
}

static void *alloc_or_abort(size_t size)
{
	void *ptr = malloc(size);

	if (ptr == NULL)
		abort();
	return ptr;
}
