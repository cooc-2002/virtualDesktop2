#include "Scene.h"
#include "Model.h"
#include "ScreenCopy.h"

#include <core.hpp>
#include <highgui.hpp>
#include <imgproc.hpp>

#include "videoDevice/RawImage.h"
#include "videoDevice/VideoDevice.h"

#include <Mfidl.h>
#include <mfapi.h>
#pragma comment(lib, "mf")
#pragma comment(lib, "Mfplat")

#ifndef OVR_DEBUG_LOG
#define OVR_DEBUG_LOG(x)
#endif

Scene::Scene() {
	numModels = 0;
	screenCopy = new ScreenCopy;
	Init();
}

void Scene::Release(){
	while (numModels-- > 0)
		delete Models[numModels];
	if (program)
	{
		glDeleteProgram(program);
		program = 0;
	}
}

Scene::~Scene(){
	if(screenCopy != NULL) delete screenCopy;
	if (vd != NULL) delete [] vd;

	Release();
}

void Scene::Add(Model * n){
	Models[numModels++] = n;
}

void Scene::Render(Matrix4f stillview, Matrix4f view, Matrix4f proj){

	screenCopy->ScreenUpdate();
	for (int i = 0; i < numModels; ++i) {
		if (i == 0) {
			Models[i]->Render(view, proj);
			//Models[i]->Render(stillview, proj);
		}
		else {
			Models[i]->Render(view, proj);
		}
	}
}

GLuint Scene::CreateShader(GLenum type, const GLchar* src){
	GLuint shader = glCreateShader(type);

	glShaderSource(shader, 1, &src, NULL);
	glCompileShader(shader);

	GLint r;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &r);
	if (!r)
	{
		GLchar msg[1024];
		glGetShaderInfoLog(shader, sizeof(msg), 0, msg);
		if (msg[0]) {
			OVR_DEBUG_LOG(("Compiling shader failed: %s\n", msg));
		}
		return 0;
	}

	return shader;
}

void Scene::Init()
{
	//Initialize shader
	InitShader();

	//Initialize cameras
	InitCams();

	GLuint screenTexId;
	glGenTextures(1, &screenTexId);

	Model * m;
	m = new Model(Vector3f(0, 0, 80), program);  // See through screen
	m->AddSolidQuad(-192.0f / 2.0f, -120.0f / 2.0f, 0.0f, 192.0f, 120.0f, 1);
	m->setTexture(screenTexId, screenCopy->screenData, screenCopy->width, screenCopy->height);
	m->AllocateBuffers();
	Add(m);

	unsigned char *texData;
	for (int i = 0; i < numCam; i++) {
		texData = (vd[i].getRawImageOut())->getpPixels();
		m = new Model(Vector3f(0, 0, 0.5f), program);
		m->AddSolidQuad(-0.5f + i, 0.5f, 0.0f, 0.64f, 0.48f);
		m->setTexture(texId[i], texData, vd[i].getWidth(), vd[i].getHeight());
		m->AllocateBuffers();
		Add(m);
	}

	//cImage = vd[0].getRawImageOut();
	//texData = cImage->getpPixels();
	//m = new Model(Vector3f(0, 0, 0.5f), program);  // screen 2
	//m->AddSolidQuad(0.3f, 0.5f, 0.0f, 0.64f, 0.48f);
	//m->setTexture(texId[1], texData, vd[1].getWidth(), vd[1].getHeight());
	//m->AllocateBuffers();
	//Add(m);
}

int Scene::InitCams() {
	int i;
	HRESULT hr = S_OK;
	IMFAttributes *pAttributes;
	hr = MFCreateAttributes(&pAttributes, 1);
	if (SUCCEEDED(hr))
		hr = pAttributes->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);
	hr = MFEnumDeviceSources(pAttributes, &ppDevices, &numCam);

	vd = new videoDevice[numCam];
	texId = new GLuint [numCam];
	for (i = 0; i < numCam; i++)
		OpenCamera(vd+i, ppDevices[i], i);
	glGenTextures(numCam, texId);

	return numCam;
}

int Scene::OpenCamera(videoDevice *vd, IMFActivate * pActivate, unsigned int Id) {
	int mediaType = 0;

	if (SUCCEEDED(vd->readInfoOfDevice(pActivate, Id))) {
		pActivate->Release();
		pActivate = NULL;

		//vd->setupDevice(6);
		//vd->setupDevice(320, 240, 15);
		int fps = 60;
		int numFormat = vd->getCountFormats();
		for (int i = 0; i < numFormat; i++) {
			MediaType param = vd->getFormat(i);
			//printf("%d x %d with %d fps\n", param.height, param.width, param.MF_MT_FRAME_RATE);

			if (param.width == 320 && fps > param.MF_MT_FRAME_RATE) {
				fps = param.MF_MT_FRAME_RATE;
				mediaType = i;
			}
		}
		vd->setupDevice(mediaType);
		while (vd->isFrameNew() && vd->isDeviceSetup() == 0)
			Sleep(10);

		return 1;
	}
	else
		return 0;
}

void Scene::InitShader() {
	//for shader and make program and Construct geometry
	static const GLchar* VertexShaderSrc =
		"#version 150\n"
		"uniform mat4 matWVP;\n"
		"in      vec4 Position;\n"
		"in      vec4 Color;\n"
		"in      vec2 TexCoord;\n"
		"out     vec2 oTexCoord;\n"
		"out     vec4 oColor;\n"
		"void main()\n"
		"{\n"
		"   gl_Position = (matWVP * Position);\n"
		"   oTexCoord   = TexCoord;\n"
		"   oColor.rgb  = pow(Color.rgb, vec3(2.2));\n"   // convert from sRGB to linear
		"   oColor.a    = Color.a;\n"
		"}\n";

	static const char* FragmentShaderSrc =
		"#version 150\n"
		"uniform sampler2D Texture0;\n"
		"in      vec4      oColor;\n"
		"in      vec2      oTexCoord;\n"
		"out     vec4      FragColor;\n"
		"void main()\n"
		"{\n"
		"   FragColor = texture2D(Texture0, oTexCoord);\n"
		"}\n";

	GLuint    vshader = CreateShader(GL_VERTEX_SHADER, VertexShaderSrc);
	GLuint    fshader = CreateShader(GL_FRAGMENT_SHADER, FragmentShaderSrc);
	program = glCreateProgram();
	glAttachShader(program, vshader);
	glAttachShader(program, fshader);

	glLinkProgram(program);

	glDetachShader(program, vshader);
	glDetachShader(program, fshader);

	GLint r;
	glGetProgramiv(program, GL_LINK_STATUS, &r);
	if (!r)
	{
		GLchar msg[1024];
		glGetProgramInfoLog(program, sizeof(msg), 0, msg);
		OVR_DEBUG_LOG(("Linking shaders failed: %s\n", msg));
	}
	glDeleteShader(vshader);
	glDeleteShader(fshader);
}
