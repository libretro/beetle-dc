#include <math.h>
#include <string.h>

#include <libretro.h>

#include "gles.h"

#ifndef GL_RED
#define GL_RED                            0x1903
#endif
#ifndef GL_MAJOR_VERSION
#define GL_MAJOR_VERSION                  0x821B
#endif
#ifndef GL_MINOR_VERSION
#define GL_MINOR_VERSION                  0x821C
#endif

GLCache glcache;
gl_ctx gl;

struct ShaderUniforms_t ShaderUniforms;

u32 gcflip;

float fb_scale_x, fb_scale_y;

//Fragment and vertex shaders code

static const char* VertexShaderSource = R"(%s
#define TARGET_GL %s
#define pp_Gouraud %d

#define GLES2 0
#define GLES3 1
#define GL2 2
#define GL3 3

#if TARGET_GL == GL2
#define highp
#define lowp
#define mediump
#endif
#if TARGET_GL == GLES2 || TARGET_GL == GL2
#define in attribute
#define out varying
#endif


#if TARGET_GL == GL3 || TARGET_GL == GLES3
#if pp_Gouraud == 0
#define INTERPOLATION flat
#else
#define INTERPOLATION smooth
#endif
#else
#define INTERPOLATION
#endif

/* Vertex constants*/ 
uniform highp vec4      depth_scale;
uniform highp mat4 normal_matrix;
uniform highp float sp_FOG_DENSITY;

/* Vertex input */
in highp vec4    in_pos;
in lowp  vec4     in_base;
in lowp vec4     in_offs;
in mediump vec2  in_uv;
/* output */
INTERPOLATION out lowp vec4 vtx_base;
INTERPOLATION out lowp vec4 vtx_offs;
              out mediump vec2 vtx_uv;
#if TARGET_GL == GLES2
              out highp float fog_depth;
#endif 

void main()
{
	vtx_base = in_base;
	vtx_offs = in_offs;
	vtx_uv = in_uv;
	highp vec4 vpos = normal_matrix * in_pos;
	vpos.w = 1.0 / vpos.z;
#if TARGET_GL != GLES2
   vpos.z = vpos.w;
#else
   fog_depth = vpos.z * sp_FOG_DENSITY;
   vpos.z=depth_scale.x+depth_scale.y*vpos.w; 
#endif
	vpos.xy *= vpos.w;
	gl_Position = vpos;
}
)";

const char* PixelPipelineShader =
R"(%s
#define TARGET_GL %s

#define cp_AlphaTest %d
#define pp_ClipInside %d
#define pp_UseAlpha %d
#define pp_Texture %d
#define pp_IgnoreTexA %d
#define pp_ShadInstr %d
#define pp_Offset %d
#define pp_FogCtrl %d
#define pp_Gouraud %d
#define pp_BumpMap %d
#define FogClamping %d
#define pp_TriLinear %d
#define pp_Palette %d

#define PI 3.1415926

#define GLES2 0
#define GLES3 1
#define GL2 2
#define GL3 3

#if TARGET_GL == GL2
#define highp
#define lowp
#define mediump
#endif
#if TARGET_GL == GLES3
out highp vec4 FragColor;
#define gl_FragColor FragColor
#define FOG_CHANNEL a
#elif TARGET_GL == GL3
out highp vec4 FragColor;
#define gl_FragColor FragColor
#define FOG_CHANNEL r
#else
#define in varying
#define texture texture2D
#define FOG_CHANNEL a
#endif

#if TARGET_GL == GL3 || TARGET_GL == GLES3
#if pp_Gouraud == 0
#define INTERPOLATION flat
#else
#define INTERPOLATION smooth
#endif
#else
#define INTERPOLATION
#endif

/* Shader program params*/
/* gles has no alpha test stage, so its emulated on the shader */
uniform lowp float cp_AlphaTestValue;
uniform lowp vec4 pp_ClipTest;
uniform lowp vec3 sp_FOG_COL_RAM,sp_FOG_COL_VERT;
uniform highp float sp_FOG_DENSITY;
uniform sampler2D tex,fog_table;
uniform lowp float trilinear_alpha;
uniform lowp vec4 fog_clamp_min;
uniform lowp vec4 fog_clamp_max;
uniform sampler2D palette;
uniform mediump int palette_index;

/* Vertex input*/
INTERPOLATION in lowp vec4 vtx_base;
INTERPOLATION in lowp vec4 vtx_offs;
in mediump vec2 vtx_uv;
#if TARGET_GL == GLES2
in highp float fog_depth;
#endif

lowp float fog_mode2(highp float w)
{
#if TARGET_GL == GLES2
	highp float z = clamp(fog_depth, 1.0, 255.9999);
#else
	highp float z = clamp(w * sp_FOG_DENSITY, 1.0, 255.9999);
#endif
	mediump float exp = floor(log2(z));
	highp float m = z * 16.0 / pow(2.0, exp) - 16.0;
	mediump float idx = floor(m) + exp * 16.0 + 0.5;
	highp vec4 fog_coef = texture(fog_table, vec2(idx / 128.0, 0.75 - (m - floor(m)) / 2.0));
	return fog_coef.FOG_CHANNEL;
}

highp vec4 fog_clamp(highp vec4 col)
{
#if FogClamping == 1
	return clamp(col, fog_clamp_min, fog_clamp_max);
#else
	return col;
#endif
}

#if pp_Palette == 1

lowp vec4 palettePixel(highp vec2 coords)
{
	highp vec2 c = vec2((texture(tex, coords).FOG_CHANNEL * 255.0 + float(palette_index)) / 1023.0, 0.5);
	return texture(palette, c);
}

#endif

void main()
{
	// Clip inside the box
	#if pp_ClipInside==-1
		if (gl_FragCoord.x >= pp_ClipTest.x && gl_FragCoord.x <= pp_ClipTest.z
				&& gl_FragCoord.y >= pp_ClipTest.y && gl_FragCoord.y <= pp_ClipTest.w)
			discard;
	#endif
	
	lowp vec4 color=vtx_base;
	#if pp_UseAlpha==0
		color.a=1.0;
	#endif
	#if pp_FogCtrl==3
		color=vec4(sp_FOG_COL_RAM.rgb,fog_mode2(gl_FragCoord.w));
	#endif
	#if pp_Texture==1
	{
		#if pp_Palette == 0
			lowp vec4 texcol = texture(tex, vtx_uv);
		#else
			lowp vec4 texcol = palettePixel(vtx_uv);
		#endif
		
		#if pp_BumpMap == 1
			highp float s = PI / 2.0 * (texcol.a * 15.0 * 16.0 + texcol.r * 15.0) / 255.0;
			highp float r = 2.0 * PI * (texcol.g * 15.0 * 16.0 + texcol.b * 15.0) / 255.0;
			texcol.a = clamp(vtx_offs.a + vtx_offs.r * sin(s) + vtx_offs.g * cos(s) * cos(r - 2.0 * PI * vtx_offs.b), 0.0, 1.0);
			texcol.rgb = vec3(1.0, 1.0, 1.0);	
		#else
			#if pp_IgnoreTexA==1
				texcol.a=1.0;	
			#endif
			
			#if cp_AlphaTest == 1
				if (cp_AlphaTestValue > texcol.a)
					discard;
			#endif 
		#endif
		#if pp_ShadInstr==0
		{
			color=texcol;
		}
		#endif
		#if pp_ShadInstr==1
		{
			color.rgb*=texcol.rgb;
			color.a=texcol.a;
		}
		#endif
		#if pp_ShadInstr==2
		{
			color.rgb=mix(color.rgb,texcol.rgb,texcol.a);
		}
		#endif
		#if  pp_ShadInstr==3
		{
			color*=texcol;
		}
		#endif
		
		#if pp_Offset==1 && pp_BumpMap == 0
		{
			color.rgb+=vtx_offs.rgb;
		}
		#endif
	}
	#endif
	
	color = fog_clamp(color);
	
	#if pp_FogCtrl == 0
	{
		color.rgb=mix(color.rgb,sp_FOG_COL_RAM.rgb,fog_mode2(gl_FragCoord.w)); 
	}
	#endif
	#if pp_FogCtrl == 1 && pp_Offset==1 && pp_BumpMap == 0
	{
		color.rgb=mix(color.rgb,sp_FOG_COL_VERT.rgb,vtx_offs.a);
	}
	#endif
	
	#if pp_TriLinear == 1
	color *= trilinear_alpha;
	#endif
	
	#if cp_AlphaTest == 1
		color.a=1.0;
	#endif 
	//color.rgb=vec3(gl_FragCoord.w * sp_FOG_DENSITY / 128.0);
#if TARGET_GL != GLES2
	highp float w = gl_FragCoord.w * 100000.0;
	gl_FragDepth = log2(1.0 + w) / 34.0;
#endif
	gl_FragColor =color;
}
)";

static const char* ModifierVolumeShader = 
R"(%s
#define TARGET_GL %s

#define GLES2 0
#define GLES3 1
#define GL2 2
#define GL3 3

#if TARGET_GL == GL2
#define highp
#define lowp
#define mediump
#endif
#if TARGET_GL != GLES2 && TARGET_GL != GL2
out highp vec4 FragColor;
#define gl_FragColor FragColor
#endif

uniform lowp float sp_ShaderColor;
/* Vertex input*/
void main()
{
#if TARGET_GL != GLES2
   highp float w = gl_FragCoord.w * 100000.0;
   gl_FragDepth = log2(1.0 + w) / 34.0;
#endif
   gl_FragColor=vec4(0.0, 0.0, 0.0, sp_ShaderColor);
}
)";


int screen_width  = 640;
int screen_height = 480;
GLuint fogTextureId;
GLuint paletteTextureId;

glm::mat4 ViewportMatrix;

PipelineShader *GetProgram(bool cp_AlphaTest, bool pp_InsideClipping,
		bool pp_Texture, bool pp_UseAlpha, bool pp_IgnoreTexA, u32 pp_ShadInstr, bool pp_Offset,
		u32 pp_FogCtrl, bool pp_Gouraud, bool pp_BumpMap, bool fog_clamping, bool trilinear,
		bool palette)
{
	u32 rv=0;

	rv |= pp_InsideClipping;
	rv<<=1; rv|=cp_AlphaTest;
	rv<<=1; rv|=pp_Texture;
	rv<<=1; rv|=pp_UseAlpha;
	rv<<=1; rv|=pp_IgnoreTexA;
	rv<<=2; rv|=pp_ShadInstr;
	rv<<=1; rv|=pp_Offset;
	rv<<=2; rv|=pp_FogCtrl;
	rv<<=1; rv|=pp_Gouraud;
	rv<<=1; rv|=pp_BumpMap;
	rv<<=1; rv|=fog_clamping;
	rv<<=1; rv|=trilinear;
	rv<<=1; rv|=palette;

	PipelineShader *shader = &gl.shaders[rv];
	if (shader->program == 0)
	{
		shader->cp_AlphaTest = cp_AlphaTest;
		shader->pp_InsideClipping = pp_InsideClipping;
		shader->pp_Texture = pp_Texture;
		shader->pp_UseAlpha = pp_UseAlpha;
		shader->pp_IgnoreTexA = pp_IgnoreTexA;
		shader->pp_ShadInstr = pp_ShadInstr;
		shader->pp_Offset = pp_Offset;
		shader->pp_FogCtrl = pp_FogCtrl;
		shader->pp_Gouraud = pp_Gouraud;
		shader->pp_BumpMap = pp_BumpMap;
		shader->fog_clamping = fog_clamping;
		shader->trilinear = trilinear;
		shader->palette = palette;
		CompilePipelineShader(shader);
	}

#ifdef RPI4_SET_UNIFORM_ATTRIBUTES_BUG
	// rpi 4 has a bug where it does not save uniform and attribute state with
	// the program, so they have to be reinit each time you reuse the program
	glcache.UseProgram(shader->program);
	//setup texture 0 as the input for the shader
	GLuint gu=glGetUniformLocation(shader->program, "tex");
	if (shader->pp_Texture==1)
		glUniform1i(gu,0);
	// Setup texture 1 as the fog table
	gu = glGetUniformLocation(shader->program, "fog_table");
	if (gu != -1)
		glUniform1i(gu, 1);
	ShaderUniforms.Set(shader);
#endif

	return shader;
}

void findGLVersion()
{
	gl.stencil_present = true;
	gl.index_type = GL_UNSIGNED_INT;

	while (true)
		if (glGetError() == GL_NO_ERROR)
			break;
	glGetIntegerv(GL_MAJOR_VERSION, &gl.gl_major);
	if (glGetError() == GL_INVALID_ENUM)
		gl.gl_major = 2;
	else
		glGetIntegerv(GL_MINOR_VERSION, &gl.gl_minor);

	const char *version = (const char *)glGetString(GL_VERSION);
	NOTICE_LOG(RENDERER, "OpenGL version: %s", version);
	if (!strncmp(version, "OpenGL ES", 9))
	{
		gl.is_gles = true;
		if (gl.gl_major >= 3)
		{
			gl.gl_version = "GLES3";
			gl.glsl_version_header = "#version 300 es";
		}
		else
		{
			gl.gl_version = "GLES2";
			gl.glsl_version_header = "";
			gl.index_type = GL_UNSIGNED_SHORT;
		}
		gl.single_channel_format = GL_ALPHA;

		GLint stencilBits = 0;
		glGetIntegerv(GL_STENCIL_BITS, &stencilBits);
		if (stencilBits == 0)
			gl.stencil_present = false;
	}
	else
	{
		gl.is_gles = false;
		if (gl.gl_major >= 3)
		{
			gl.gl_version = "GL3";
			gl.glsl_version_header = "#version 130";
			gl.single_channel_format = GL_RED;
		}
		else
		{
			gl.gl_version = "GL2";
			gl.glsl_version_header = "#version 120";
			gl.single_channel_format = GL_ALPHA;
		}
	}
	gl.max_anisotropy = 1.f;
#ifndef HAVE_OPENGLES2
	if (gl.gl_major >= 3)
	{
		for (u32 i = 0; ; i++)
		{
			const char* extension = (const char *)glGetStringi(GL_EXTENSIONS, i);
			if (extension == nullptr)
				break;
			if (!strcmp(extension, "GL_EXT_texture_filter_anisotropic"))
			{
				glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &gl.max_anisotropy);
				break;
			}
		}
	}
#endif
}

GLuint gl_CompileShader(const char* shader,GLuint type)
{
	GLint result;
	GLint compile_log_len;
	GLuint rv=glCreateShader(type);
	glShaderSource(rv, 1,&shader, NULL);
	glCompileShader(rv);

	//lets see if it compiled ...
	glGetShaderiv(rv, GL_COMPILE_STATUS, &result);
	glGetShaderiv(rv, GL_INFO_LOG_LENGTH, &compile_log_len);

	if (!result && compile_log_len>0)
	{
		if (compile_log_len==0)
			compile_log_len=1;
		char* compile_log=(char*)malloc(compile_log_len);
		*compile_log=0;

		glGetShaderInfoLog(rv, compile_log_len, &compile_log_len, compile_log);
		WARN_LOG(RENDERER, "Shader: %s \n%s\n", result ? "compiled!" : "failed to compile", compile_log);

		free(compile_log);
	}

	return rv;
}

GLuint gl_CompileAndLink(const char* VertexShader, const char* FragmentShader)
{
	GLint compile_log_len;
	GLint result;
	/* Create vertex/fragment shaders */
	GLuint vs      = gl_CompileShader(VertexShader ,GL_VERTEX_SHADER);
	GLuint ps      = gl_CompileShader(FragmentShader ,GL_FRAGMENT_SHADER);
	GLuint program = glCreateProgram();

	glAttachShader(program, vs);
	glAttachShader(program, ps);

	/* Bind vertex attribute to VBO inputs */
	glBindAttribLocation(program, VERTEX_POS_ARRAY,      "in_pos");
	glBindAttribLocation(program, VERTEX_COL_BASE_ARRAY, "in_base");
	glBindAttribLocation(program, VERTEX_COL_OFFS_ARRAY, "in_offs");
	glBindAttribLocation(program, VERTEX_UV_ARRAY,       "in_uv");
	glBindAttribLocation(program, VERTEX_COL_BASE1_ARRAY, "in_base1");
	glBindAttribLocation(program, VERTEX_COL_OFFS1_ARRAY, "in_offs1");
	glBindAttribLocation(program, VERTEX_UV1_ARRAY,       "in_uv1");

#ifndef HAVE_OPENGLES
	glBindFragDataLocation(program, 0, "FragColor");
#endif

	glLinkProgram(program);
	glGetProgramiv(program, GL_LINK_STATUS, &result);
	glGetProgramiv(program, GL_INFO_LOG_LENGTH, &compile_log_len);

	if (!result && compile_log_len>0)
	{
      char *compile_log = NULL;

		if (compile_log_len==0)
			compile_log_len = 1;
		compile_log_len   += 1024;
		compile_log        = (char*)malloc(compile_log_len);
		*compile_log       = 0;

		glGetProgramInfoLog(program, compile_log_len, &compile_log_len, compile_log);
		WARN_LOG(RENDERER, "Shader linking: %s \n (%d bytes), - %s -\n", result ? "linked" : "failed to link", compile_log_len, compile_log);
		WARN_LOG(RENDERER, "VERTEX:\n%s\nFRAGMENT:\n%s\n", VertexShader, FragmentShader);

		free(compile_log);
		die("shader compile fail\n");
	}

	glDeleteShader(vs);
	glDeleteShader(ps);

	glcache.UseProgram(program);

	verify(glIsProgram(program));

	return program;
}

bool CompilePipelineShader(	PipelineShader* s)
{
	char vshader[8192];

	int rc = sprintf(vshader, VertexShaderSource, gl.glsl_version_header, gl.gl_version, s->pp_Gouraud);
	verify(rc + 1 <= (int)sizeof(vshader));

	char pshader[8192];

	rc = sprintf(pshader,PixelPipelineShader, gl.glsl_version_header, gl.gl_version,
                s->cp_AlphaTest,s->pp_InsideClipping,s->pp_UseAlpha,
                s->pp_Texture,s->pp_IgnoreTexA,s->pp_ShadInstr,s->pp_Offset,s->pp_FogCtrl, s->pp_Gouraud, s->pp_BumpMap,
				s->fog_clamping, s->trilinear, s->palette);
	verify(rc + 1 <= (int)sizeof(pshader));

	s->program=gl_CompileAndLink(vshader, pshader);


	//setup texture 0 as the input for the shader
	GLint gu = glGetUniformLocation(s->program, "tex");
	if (s->pp_Texture==1)
		glUniform1i(gu,0);

	//get the uniform locations
	s->depth_scale      = glGetUniformLocation(s->program, "depth_scale");

	s->pp_ClipTest      = glGetUniformLocation(s->program, "pp_ClipTest");

	s->sp_FOG_DENSITY   = glGetUniformLocation(s->program, "sp_FOG_DENSITY");

	s->cp_AlphaTestValue= glGetUniformLocation(s->program, "cp_AlphaTestValue");

	//FOG_COL_RAM,FOG_COL_VERT,FOG_DENSITY;
	if (s->pp_FogCtrl==1 && s->pp_Texture==1)
		s->sp_FOG_COL_VERT=glGetUniformLocation(s->program, "sp_FOG_COL_VERT");
	else
		s->sp_FOG_COL_VERT=-1;
	if (s->pp_FogCtrl==0 || s->pp_FogCtrl==3)
	{
		s->sp_FOG_COL_RAM=glGetUniformLocation(s->program, "sp_FOG_COL_RAM");
	}
	else
	{
		s->sp_FOG_COL_RAM=-1;
	}
	// Setup texture 1 as the fog table
	gu = glGetUniformLocation(s->program, "fog_table");
	if (gu != -1)
		glUniform1i(gu, 1);
	// And texture 2 as palette
	gu = glGetUniformLocation(s->program, "palette");
	if (gu != -1)
		glUniform1i(gu, 2);
	s->palette_index = glGetUniformLocation(s->program, "palette_index");

	s->trilinear_alpha = glGetUniformLocation(s->program, "trilinear_alpha");
	
	if (s->fog_clamping)
	{
		s->fog_clamp_min = glGetUniformLocation(s->program, "fog_clamp_min");
		s->fog_clamp_max = glGetUniformLocation(s->program, "fog_clamp_max");
	}
	else
	{
		s->fog_clamp_min = -1;
		s->fog_clamp_max = -1;
	}
	s->normal_matrix = glGetUniformLocation(s->program, "normal_matrix");

	ShaderUniforms.Set(s);

	return glIsProgram(s->program)==GL_TRUE;
}

/*
GL|ES 2
Slower, smaller subset of gl2

*Optimisation notes*
Keep stuff in packed ints
Keep data as small as possible
Keep vertex programs as small as possible
The drivers more or less suck. Don't depend on dynamic allocation, or any 'complex' feature
as it is likely to be problematic/slow
Do we really want to enable striping joins?

*Design notes*
Follow same architecture as the d3d renderer for now
Render to texture, keep track of textures in GL memory
Direct flip to screen (no vlbank/fb emulation)
Do we really need a combining shader? it is needlessly expensive for openGL | ES
Render contexts
Free over time? we actually care about ram usage here?
Limit max resource size? for psp 48k verts worked just fine

FB:
Pixel clip, mapping

SPG/VO:
mapping

TA:
Tile clip

*/

static void gl_delete_shaders()
{
	for (const auto& it : gl.shaders)
	{
		if (it.second.program != 0)
			glDeleteProgram(it.second.program);
	}
	gl.shaders.clear();
	glDeleteProgram(gl.modvol_shader.program);
	gl.modvol_shader.program = 0;
}

static void gl_term(void)
{
	glcache.DeleteTextures(ARRAY_SIZE(vmuTextureId), vmuTextureId);
	memset(vmuTextureId, 0, sizeof(vmuTextureId));
	glcache.DeleteTextures(ARRAY_SIZE(lightgunTextureId), lightgunTextureId);
	memset(lightgunTextureId, 0, sizeof(lightgunTextureId));

	glDeleteBuffers(1, &gl.vbo.geometry);
	glDeleteBuffers(1, &gl.vbo.modvols);
	glDeleteBuffers(1, &gl.vbo.idxs);
	glDeleteBuffers(1, &gl.vbo.idxs2);
	glDeleteTextures(1, &fbTextureId);
	fbTextureId = 0;
	glDeleteTextures(1, &fogTextureId);
	fogTextureId = 0;
	glcache.DeleteTextures(1, &paletteTextureId);
	paletteTextureId = 0;

	gl_delete_shaders();
}

static bool gl_create_resources(void)
{
	/* create VBOs */
	glGenBuffers(1, &gl.vbo.geometry);
	glGenBuffers(1, &gl.vbo.modvols);
	glGenBuffers(1, &gl.vbo.idxs);
	glGenBuffers(1, &gl.vbo.idxs2);

   findGLVersion();

   char vshader[8192];
   sprintf(vshader, VertexShaderSource, gl.glsl_version_header, gl.gl_version, 1);
   char fshader[8192];
	sprintf(fshader, ModifierVolumeShader, gl.glsl_version_header, gl.gl_version);

	gl.modvol_shader.program = gl_CompileAndLink(vshader, fshader);
	gl.modvol_shader.normal_matrix  = glGetUniformLocation(gl.modvol_shader.program, "normal_matrix");
	gl.modvol_shader.depth_scale = glGetUniformLocation(gl.modvol_shader.program, "depth_scale");
	gl.modvol_shader.sp_ShaderColor = glGetUniformLocation(gl.modvol_shader.program, "sp_ShaderColor");

	return true;
}

void UpdateFogTexture(u8 *fog_table, GLenum texture_slot, GLint fog_image_format)
{
	glActiveTexture(texture_slot);
	if (fogTextureId == 0)
	{
		fogTextureId = glcache.GenTexture();
		glcache.BindTexture(GL_TEXTURE_2D, fogTextureId);
		glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	}
	else
		glcache.BindTexture(GL_TEXTURE_2D, fogTextureId);

	u8 temp_tex_buffer[256];
	MakeFogTexture(temp_tex_buffer);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexImage2D(GL_TEXTURE_2D, 0, fog_image_format, 128, 2, 0, fog_image_format, GL_UNSIGNED_BYTE, temp_tex_buffer);
	glCheck();

	glActiveTexture(GL_TEXTURE0);
}

void UpdatePaletteTexture(GLenum texture_slot)
{
	glActiveTexture(texture_slot);
	if (paletteTextureId == 0)
	{
		paletteTextureId = glcache.GenTexture();
		glcache.BindTexture(GL_TEXTURE_2D, paletteTextureId);
		glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	}
	else
		glcache.BindTexture(GL_TEXTURE_2D, paletteTextureId);

   glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1024, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, palette32_ram);
	glCheck();

	glActiveTexture(GL_TEXTURE0);
}

void DrawVmuTexture(u8 vmu_screen_number);
void DrawGunCrosshair(u8 port);

void vertex_buffer_unmap(void)
{
   glBindBuffer(GL_ARRAY_BUFFER, 0);
   glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

void DoCleanup() {
}

static void upload_vertex_indices()
{
	if (gl.index_type == GL_UNSIGNED_SHORT)
	{
		static bool overrun;
		static List<u16> short_idx;
		if (short_idx.daty != NULL)
			short_idx.Free();
		short_idx.Init(pvrrc.idx.used(), &overrun, NULL);
		for (u32 *p = pvrrc.idx.head(); p < pvrrc.idx.LastPtr(0); p++)
			*(short_idx.Append()) = *p;
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, short_idx.bytes(), short_idx.head(), GL_STREAM_DRAW);
	}
	else
		glBufferData(GL_ELEMENT_ARRAY_BUFFER,pvrrc.idx.bytes(),pvrrc.idx.head(),GL_STREAM_DRAW);
}

static bool RenderFrame(void)
{
	int vmu_screen_number = 0 ;
	int lightgun_port = 0 ;

	DoCleanup();

	bool is_rtt = pvrrc.isRTT;

	float vtx_min_fZ = 0.f;	//pvrrc.fZ_min;
	float vtx_max_fZ = pvrrc.fZ_max;

	//sanitise the values, now with NaN detection (for omap)
	//0x49800000 is 1024*1024. Using integer math to avoid issues w/ infs and nans
	if ((s32&)vtx_max_fZ < 0 || (u32&)vtx_max_fZ > 0x49800000)
		vtx_max_fZ = 10 * 1024;

	//add some extra range to avoid clipping border cases
	vtx_min_fZ *= 0.98f;
	vtx_max_fZ *= 1.001f;

	TransformMatrix<true> matrices(pvrrc);
	ShaderUniforms.normal_mat = matrices.GetNormalMatrix();
	const glm::mat4& scissor_mat = matrices.GetScissorMatrix();
	ViewportMatrix = matrices.GetViewportMatrix();

	if (!is_rtt)
		gcflip = 0;
	else
		gcflip = 1;

	ShaderUniforms.depth_coefs[0] = 2 / (vtx_max_fZ - vtx_min_fZ);
	ShaderUniforms.depth_coefs[1] = -vtx_min_fZ - 1;
	ShaderUniforms.depth_coefs[2] = 0;
	ShaderUniforms.depth_coefs[3] = 0;

	//VERT and RAM fog color constants
	u8* fog_colvert_bgra = (u8*)&FOG_COL_VERT;
	u8* fog_colram_bgra = (u8*)&FOG_COL_RAM;
	ShaderUniforms.ps_FOG_COL_VERT[0] = fog_colvert_bgra[2] / 255.0f;
	ShaderUniforms.ps_FOG_COL_VERT[1] = fog_colvert_bgra[1] / 255.0f;
	ShaderUniforms.ps_FOG_COL_VERT[2] = fog_colvert_bgra[0] / 255.0f;

	ShaderUniforms.ps_FOG_COL_RAM[0] = fog_colram_bgra[2] / 255.0f;
	ShaderUniforms.ps_FOG_COL_RAM[1] = fog_colram_bgra[1] / 255.0f;
	ShaderUniforms.ps_FOG_COL_RAM[2] = fog_colram_bgra[0] / 255.0f;

	//Fog density constant
	u8* fog_density = (u8*)&FOG_DENSITY;
	float fog_den_mant = fog_density[1] / 128.0f;  //bit 7 -> x. bit, so [6:0] -> fraction -> /128
	s32 fog_den_exp = (s8)fog_density[0];
	ShaderUniforms.fog_den_float = fog_den_mant * powf(2.0f, fog_den_exp);

	ShaderUniforms.fog_clamp_min[0] = ((pvrrc.fog_clamp_min >> 16) & 0xFF) / 255.0f;
	ShaderUniforms.fog_clamp_min[1] = ((pvrrc.fog_clamp_min >> 8) & 0xFF) / 255.0f;
	ShaderUniforms.fog_clamp_min[2] = ((pvrrc.fog_clamp_min >> 0) & 0xFF) / 255.0f;
	ShaderUniforms.fog_clamp_min[3] = ((pvrrc.fog_clamp_min >> 24) & 0xFF) / 255.0f;

	ShaderUniforms.fog_clamp_max[0] = ((pvrrc.fog_clamp_max >> 16) & 0xFF) / 255.0f;
	ShaderUniforms.fog_clamp_max[1] = ((pvrrc.fog_clamp_max >> 8) & 0xFF) / 255.0f;
	ShaderUniforms.fog_clamp_max[2] = ((pvrrc.fog_clamp_max >> 0) & 0xFF) / 255.0f;
	ShaderUniforms.fog_clamp_max[3] = ((pvrrc.fog_clamp_max >> 24) & 0xFF) / 255.0f;


	if (fog_needs_update && settings.rend.Fog)
	{
		fog_needs_update=false;
		UpdateFogTexture((u8 *)FOG_TABLE, GL_TEXTURE1, gl.single_channel_format);
	}
	if (palette_updated)
	{
		UpdatePaletteTexture(GL_TEXTURE2);
		palette_updated = false;
	}

	glcache.UseProgram(gl.modvol_shader.program);

	glUniform4fv(gl.modvol_shader.depth_scale, 1, ShaderUniforms.depth_coefs);
	glUniformMatrix4fv(gl.modvol_shader.normal_matrix, 1, GL_FALSE, &ShaderUniforms.normal_mat[0][0]);

	ShaderUniforms.PT_ALPHA=(PT_ALPHA_REF&0xFF)/255.0f;

	for (const auto& it : gl.shaders)
	{
		glcache.UseProgram(it.second.program);
		ShaderUniforms.Set(&it.second);
	}

	//setup render target first
	if (is_rtt)
	{
		GLuint channels,format;
		switch(FB_W_CTRL.fb_packmode)
		{
			case 0: //0x0   0555 KRGB 16 bit  (default)	Bit 15 is the value of fb_kval[7].
				channels=GL_RGBA;
				format=GL_UNSIGNED_BYTE;
				break;

			case 1: //0x1   565 RGB 16 bit
				channels=GL_RGB;
				format=GL_UNSIGNED_SHORT_5_6_5;
				break;

			case 2: //0x2   4444 ARGB 16 bit
				channels=GL_RGBA;
				format=GL_UNSIGNED_BYTE;
				break;

			case 3://0x3    1555 ARGB 16 bit    The alpha value is determined by comparison with the value of fb_alpha_threshold.
				channels=GL_RGBA;
				format=GL_UNSIGNED_BYTE;
				break;

			case 4: //0x4   888 RGB 24 bit packed
			case 5: //0x5   0888 KRGB 32 bit    K is the value of fk_kval.
			case 6: //0x6   8888 ARGB 32 bit
				WARN_LOG(RENDERER, "Unsupported render to texture format: %d", FB_W_CTRL.fb_packmode);
				return false;
			case 7: //7     invalid
				die("7 is not valid");
				return false;
		}
		DEBUG_LOG(RENDERER, "RTT packmode=%d stride=%d - %d,%d -> %d,%d\n", FB_W_CTRL.fb_packmode, FB_W_LINESTRIDE.stride * 8,
 				FB_X_CLIP.min, FB_Y_CLIP.min, FB_X_CLIP.max, FB_Y_CLIP.max);
		BindRTT(FB_W_SOF1 & VRAM_MASK, FB_X_CLIP.max - FB_X_CLIP.min + 1, FB_Y_CLIP.max - FB_Y_CLIP.min + 1, channels,format);
	}
	else
	{
		if (settings.rend.PowerVR2Filter && !pvrrc.isRenderFramebuffer)
			postProcessor.SelectFramebuffer();
		glViewport(0, 0, screen_width, screen_height);
	}

	bool wide_screen_on = !is_rtt && settings.rend.WideScreen && !matrices.IsClipped();

	//Color is cleared by the background plane

	glcache.Disable(GL_SCISSOR_TEST);

	glcache.DepthMask(GL_TRUE);
	glClearDepth(0.0);
	glStencilMask(0xFF); glCheck();
	glClearStencil(0);
	glClear(GL_STENCIL_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); glCheck();

	//move vertex to gpu

	if (!pvrrc.isRenderFramebuffer)
	{
		//Main VBO
		glBindBuffer(GL_ARRAY_BUFFER, gl.vbo.geometry); glCheck();
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gl.vbo.idxs); glCheck();

		glBufferData(GL_ARRAY_BUFFER,pvrrc.verts.bytes(),pvrrc.verts.head(),GL_STREAM_DRAW); glCheck();

		upload_vertex_indices();

		//Modvol VBO
		if (pvrrc.modtrig.used())
		{
			glBindBuffer(GL_ARRAY_BUFFER, gl.vbo.modvols); glCheck();
			glBufferData(GL_ARRAY_BUFFER,pvrrc.modtrig.bytes(),pvrrc.modtrig.head(),GL_STREAM_DRAW); glCheck();
		}

		if (!wide_screen_on)
		{
			float width;
			float height;
			float min_x;
			float min_y;
			if (!is_rtt)
			{
				glm::vec4 clip_min(pvrrc.fb_X_CLIP.min, pvrrc.fb_Y_CLIP.min, 0, 1);
				glm::vec4 clip_dim(pvrrc.fb_X_CLIP.max - pvrrc.fb_X_CLIP.min + 1,
								   pvrrc.fb_Y_CLIP.max - pvrrc.fb_Y_CLIP.min + 1, 0, 0);
				clip_min = scissor_mat * clip_min;
				clip_dim = scissor_mat * clip_dim;

				min_x = clip_min[0];
				min_y = clip_min[1];
				width = clip_dim[0];
				height = clip_dim[1];
				if (width < 0)
				{
					min_x += width;
					width = -width;
				}
				if (height < 0)
				{
					min_y += height;
					height = -height;
				}

				if (matrices.GetSidebarWidth() > 0)
				{
					float rounded_offs_x = matrices.GetSidebarWidth() + 0.5f;

					glcache.ClearColor(0.f, 0.f, 0.f, 0.f);
					glcache.Enable(GL_SCISSOR_TEST);
					glcache.Scissor(0, 0, rounded_offs_x, screen_height);
					glClear(GL_COLOR_BUFFER_BIT);
					glcache.Scissor(screen_width - rounded_offs_x, 0, rounded_offs_x, screen_height);
					glClear(GL_COLOR_BUFFER_BIT);
				}
			}
			else
			{
				width = pvrrc.fb_X_CLIP.max - pvrrc.fb_X_CLIP.min + 1;
				height = pvrrc.fb_Y_CLIP.max - pvrrc.fb_Y_CLIP.min + 1;
				min_x = pvrrc.fb_X_CLIP.min;
				min_y = pvrrc.fb_Y_CLIP.min;
				if (settings.rend.RenderToTextureUpscale > 1 && !settings.rend.RenderToTextureBuffer)
				{
					min_x *= settings.rend.RenderToTextureUpscale;
					min_y *= settings.rend.RenderToTextureUpscale;
					width *= settings.rend.RenderToTextureUpscale;
					height *= settings.rend.RenderToTextureUpscale;
				}
			}

			ShaderUniforms.base_clipping.enabled = true;
			ShaderUniforms.base_clipping.x = (int)lroundf(min_x);
			ShaderUniforms.base_clipping.y = (int)lroundf(min_y);
			ShaderUniforms.base_clipping.width = (int)lroundf(width);
			ShaderUniforms.base_clipping.height = (int)lroundf(height);
			glcache.Scissor(ShaderUniforms.base_clipping.x, ShaderUniforms.base_clipping.y, ShaderUniforms.base_clipping.width, ShaderUniforms.base_clipping.height);
			glcache.Enable(GL_SCISSOR_TEST);
		}
		else
		{
			ShaderUniforms.base_clipping.enabled = false;
		}

		DrawStrips();
		if (settings.rend.PowerVR2Filter && !is_rtt)
			postProcessor.Render(hw_render.get_current_framebuffer());
	}
	else
	{
		glcache.ClearColor(0.f, 0.f, 0.f, 0.f);
		glClear(GL_COLOR_BUFFER_BIT);
		glBindBuffer(GL_ARRAY_BUFFER, gl.vbo.geometry); glCheck();
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gl.vbo.idxs); glCheck();
		DrawFramebuffer();
	}

	/* VMU/Crosshair code - libretro-specific */
	if (!is_rtt)
	{
		if (settings.System == DC_PLATFORM_DREAMCAST)
		{
			for ( vmu_screen_number = 0 ; vmu_screen_number < 4 ; vmu_screen_number++)
				if ( vmu_screen_params[vmu_screen_number].vmu_screen_display )
					DrawVmuTexture(vmu_screen_number);
		}

		for ( lightgun_port = 0 ; lightgun_port < 4 ; lightgun_port++)
			DrawGunCrosshair(lightgun_port);
	}

	KillTex = false;

	if (is_rtt)
		ReadRTTBuffer();

	return !is_rtt;
}

void rend_set_fb_scale(float x, float y)
{
	fb_scale_x = x;
	fb_scale_y = y;
}

bool ProcessFrame(TA_context* ctx)
{
   ctx->rend_inuse.lock();

   if (KillTex)
   {
      TexCache.Clear();
      INFO_LOG(RENDERER, "Texture cache cleared");
   }

   if (ctx->rend.isRenderFramebuffer)
	{
		RenderFramebuffer();
		ctx->rend_inuse.unlock();
	}
	else
	{
		if (!ta_parse_vdrc(ctx))
			return false;
	}
   TexCache.CollectCleanup();

   return !ctx->rend.Overrun;
}

struct glesrend : Renderer
{
   bool Init() override
   {
      if (!gl_create_resources())
         return false;

      glcache.EnableCache();

#ifdef HAVE_OPENGLES
      glHint(GL_GENERATE_MIPMAP_HINT, GL_FASTEST);
#endif

#ifdef HAVE_TEXUPSCALE
      if (settings.rend.TextureUpscale > 1)
      {
         // Trick to preload the tables used by xBRZ
         u32 src[] { 0x11111111, 0x22222222, 0x33333333, 0x44444444 };
         u32 dst[16];
         UpscalexBRZ(2, src, dst, 2, 2, false);
      }
#endif
      fog_needs_update = true;
      palette_updated = true;
      TexCache.Clear();

      if (settings.rend.PowerVR2Filter)
      	postProcessor.Init();

      return true;
   }
	void Resize(int w, int h) override { screen_width=w; screen_height=h; }
	void Term() override
   {
		postProcessor.Term();
	   TexCache.Clear();

	   gl_term();
   }

	bool Process(TA_context* ctx) override
   {
#if !defined(TARGET_NO_THREADS)
      if (!settings.rend.ThreadedRendering)
#endif
         glsm_ctl(GLSM_CTL_STATE_BIND, NULL);
      return ProcessFrame(ctx);
   }
	bool Render() override
   {
      bool ret = RenderFrame();
#if !defined(TARGET_NO_THREADS)
      if (!settings.rend.ThreadedRendering)
#endif
    	  glsm_ctl(GLSM_CTL_STATE_UNBIND, NULL);
      return ret;
   }

	virtual u64 GetTexture(TSP tsp, TCW tcw) override {
		return gl_GetTexture(tsp, tcw);
	}
};

Renderer* rend_GLES2() { return new glesrend(); }
