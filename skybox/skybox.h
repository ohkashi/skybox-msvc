#pragma once
#include "common.h"
#include "resource.h"


class TextureImage
{
public:
	TextureImage() : width(0), height(0), imageData(nullptr) {}
	~TextureImage() {
		delete[] imageData;
		imageData = nullptr;
	}

	GLubyte* Load(const char *filename, bool vflip = true) {
		delete[] imageData;
		imageData = nullptr;

		// The factory pointer
		CComPtr<IWICImagingFactory> pWicFactory;

		// Create the COM imaging factory
		HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pWicFactory));
		if (SUCCEEDED(hr)) {
			CComPtr<IWICBitmapDecoder> pDecoder;
			hr = pWicFactory->CreateDecoderFromFilename(
				(LPCWSTR)CA2W(filename),		// Image to be decoded
				NULL,							// Do not prefer a particular vendor
				GENERIC_READ,					// Desired read access to the file
				WICDecodeMetadataCacheOnDemand,	// Cache metadata when needed
				&pDecoder						// Pointer to the decoder
			);
			if (SUCCEEDED(hr)) {
				// Retrieve the first frame of the image from the decoder
				CComPtr<IWICBitmapFrameDecode> pFrame;
				if (SUCCEEDED(hr)) {
					hr = pDecoder->GetFrame(0, &pFrame);
					CComPtr<IWICFormatConverter> pConverter;
					hr = pWicFactory->CreateFormatConverter(&pConverter);

					hr = pConverter->Initialize(
						pFrame,							// Frame
						GUID_WICPixelFormat32bppRGBA,	// Pixel format
						WICBitmapDitherTypeNone,		// Irrelevant
						NULL,							// No palette needed, irrelevant
						0.0,							// Alpha transparency % irrelevant
						WICBitmapPaletteTypeCustom		// Irrelevant
					);
					pConverter->GetSize(&height, &width);
					const UINT stride = width * sizeof(DWORD);
					GLuint imageSize = height * stride;
					imageData = new GLubyte[imageSize];
					pConverter->CopyPixels(0, stride, imageSize, imageData);
					if (vflip) {
						GLubyte* buff = new GLubyte[stride];
						int i, hy = height / 2;
						GLubyte* data[2] = { imageData, imageData + imageSize - stride };
						for (i = 0; i < hy; i++) {
							CopyMemory(buff, data[0], stride);
							CopyMemory(data[0], data[1], stride);
							CopyMemory(data[1], buff, stride);
							data[0] += stride;
							data[1] -= stride;
						}
						delete[] buff;
					}
				}
			}
		}
		return imageData;
	}

	GLuint width;
	GLuint height;
	GLubyte* imageData;
};


class SkyBoxWindow
{
public:
	SkyBoxWindow();
	~SkyBoxWindow();

	bool Initialize();
	int RunLoop();
	void ComputeMatricesFromInputs();
	void SetFullScreen(bool fullscreen);
	bool IsLightTheme();
	bool SetTitleBarDarkMode(HWND hWnd, bool bDarkMode);
	operator GLFWwindow*() const { return m_pWindow; }

	constexpr static float SKYBOX_SIZE = 500.f;

	typedef	struct {
		int	x;
		int	y;
		int	width;
		int	height;
	} Rect;

protected:
	bool InitGL();
	void InitShader();
	void InitMatrix();
	void InitLight();
	void InitSkybox();
	void InitMesh();

	static void KeyCallback(GLFWwindow*, int, int, int, int);
	static void ResizeCallback(GLFWwindow*, int, int);

private:
	GLFWwindow*		m_pWindow;
	GLFWmonitor*	m_pMonitor;
	HWND	m_hWnd;
	Rect	m_rect;
	bool	m_bUpdateViewport;
	bool	m_bFullScreen;
	bool	m_bShowFps;
	Mesh	m_cube;

	// ================================================
	// OpenGL resources
	// ================================================
	GLuint vboSkybox, tboSkybox, vaoSkybox;
	GLint uniSkyboxM, uniSkyboxV, uniSkyboxP;
	GLint uniMeshM, uniMeshV, uniMeshP;
	GLint uniLightColor, uniLightPos, uniLightPower;
	GLint uniDiffuse, uniAmbient, uniSpecular;
	mat4 oriSkyboxM, skyboxM, skyboxV, skyboxP;
	mat4 meshM, meshV, meshP;
	GLuint vsSkybox, fsSkybox, vsModel, fsModel;
	GLuint shaderSkybox, shaderMesh;

	// ================================================
	// Lighting and material settings
	// ================================================
	vec3 lightPosition = vec3(3.f, 3.f, 3.f);
	vec3 lightColor = vec3(1.f, 1.f, 1.f);
	float lightPower = 12.f;
	vec3 materialDiffuse = vec3(0.f, 1.f, 0.f);
	vec3 materialAmbient = vec3(0.f, 0.05f, 0.f);
	vec3 materialSpecular = vec3(1.f, 1.f, 1.f);

	// ================================================
	// Camera settings
	// ================================================
	float verticalAngle = -1.775f;
	float horizontalAngle = 0.935f;
	float initialFoV = 45.0f;
	float speed = 5.0f;
	float mouseSpeed = 0.005f;
	float farPlane = 2000.f;
	vec3 eyePoint = vec3(2.f, 1.2f, -0.8f);
	vec3 eyeDirection =
		vec3(sin(verticalAngle) * cos(horizontalAngle), cos(verticalAngle), sin(verticalAngle) * sin(horizontalAngle));
	vec3 up = vec3(0.f, 1.f, 0.f);

	// ================================================
	// 3D models
	// ================================================
	// Skybox (a large cube)

	constexpr static GLfloat vtxsSkybox[] = {
		// right
		1, -1, -1, 1, -1, 1, 1, 1, 1,
		//
		1, 1, 1, 1, 1, -1, 1, -1, -1,

		// left
		-1, -1, 1, -1, -1, -1, -1, 1, -1,
		//
		-1, 1, -1, -1, 1, 1, -1, -1, 1,

		// top
		-1, 1, -1, 1, 1, -1, 1, 1, 1,
		//
		1, 1, 1, -1, 1, 1, -1, 1, -1,

		// bottom
		-1, -1, -1, -1, -1, 1, 1, -1, -1,
		//
		1, -1, -1, -1, -1, 1, 1, -1, 1,

		// front
		-1, -1, 1, -1, 1, 1, 1, 1, 1,
		//
		1, 1, 1, 1, -1, 1, -1, -1, 1,

		// back
		-1, 1, -1, -1, -1, -1, 1, -1, -1,
		//
		1, -1, -1, 1, 1, -1, -1, 1, -1};
};
