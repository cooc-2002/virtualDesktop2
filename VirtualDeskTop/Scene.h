#pragma once
#include "Extras/OVR_Math.h"
#include "GL/CAPI_GLE.h"

using namespace OVR;

namespace cv {
	class VideoCapture;
	class Mat;
};

class Model;
class ScreenCopy;
class videoDevice;
struct IMFActivate;
typedef unsigned int UINT32;

class Scene
{
private:
	int		numModels;
	Model	*Models[10];
	GLuint	*texId;
	GLuint	program;
	DWORD	color;
	UINT32	numCam;
	IMFActivate		**ppDevices;
	videoDevice		*vd; 
	ScreenCopy		*screenCopy;

public:
	Scene();
	~Scene();

	void Init();
	void Release();
	void Add(Model * n);
	void Render(Matrix4f stillview, Matrix4f view, Matrix4f proj);
	void InitShader();
	int InitCams();
	int OpenCamera(videoDevice *vd, IMFActivate * pActivate, unsigned int Id);
	GLuint CreateShader(GLenum type, const GLchar* src);
};