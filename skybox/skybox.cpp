// skybox.cpp : Defines the entry point for the application.
//

#include "pch.h"
#include "framework.h"
#include "skybox.h"


int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

	CoInitialize(NULL);

	// Initializations
	SkyBoxWindow skyboxWnd;
	if (!skyboxWnd.Initialize())
		return 1;

	return skyboxWnd.RunLoop();
}

SkyBoxWindow::SkyBoxWindow() : m_pWindow(NULL), m_pMonitor(NULL), m_hWnd(NULL), m_rect{0},
	m_bUpdateViewport(false), m_bFullScreen(false), m_bShowFps(true)
{
}

SkyBoxWindow::~SkyBoxWindow()
{
}

bool SkyBoxWindow::Initialize()
{
	// OpenGL contexts
	if (!InitGL())
		return false;

	// Shaders
	InitShader();

	// Transformation matrices
	InitMatrix();
	InitLight();

	// Skybox
	InitSkybox();

	// 3D model
	InitMesh();

	return true;
}

int SkyBoxWindow::RunLoop()
{
	// A rough way to solve cursor position initialization problem
	// Must call glfwPollEvents once to activate glfwSetCursorPos
	// This is a glfw mechanism problem
	glfwPollEvents();
	glfwSetCursorPos(m_pWindow, m_rect.width / 2.0, m_rect.height / 2.0);

	// Show main mainWindow
	while (!glfwWindowShouldClose(m_pWindow)) {
		if (m_bUpdateViewport) {
			int width = 0, height = 0;
			glfwGetFramebufferSize(m_pWindow, &width, &height);
			glViewport(0, 0, width, height);
			m_bUpdateViewport = false;
		}

		// Clear frame
		glClearColor(97 / 256.f, 175 / 256.f, 239 / 256.f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		// View control
		ComputeMatricesFromInputs();

		// Draw skybox
		glUseProgram(shaderSkybox);
		glBindVertexArray(vaoSkybox);
		glDrawArrays(GL_TRIANGLES, 0, 36);

		// Draw 3d models
		glUseProgram(shaderMesh);
		glBindVertexArray(m_cube.vao);
		glDrawArrays(GL_TRIANGLES, 0, m_cube.faces.size() * 3);

		// Update frame
		glfwSwapBuffers(m_pWindow);

		// Handle events
		glfwPollEvents();

		Sleep(10);
	}

	// Release resources
	glfwTerminate();
	return EXIT_SUCCESS;
}

void SkyBoxWindow::SetFullScreen(bool fullscreen)
{
	if (m_bFullScreen == fullscreen)
		return;

	if (fullscreen) {
		// backup window position and window size
		glfwGetWindowPos(m_pWindow, &m_rect.x, &m_rect.y);
		glfwGetWindowSize(m_pWindow, &m_rect.width, &m_rect.height);

		// get resolution of monitor
		const GLFWvidmode* mode = glfwGetVideoMode(m_pMonitor);

		// switch to full screen
		glfwSetWindowMonitor(m_pWindow, m_pMonitor, 0, 0, mode->width, mode->height, 0);
	} else {
		// restore last window size and position
		glfwSetWindowMonitor(m_pWindow, nullptr, m_rect.x, m_rect.y, m_rect.width, m_rect.height, 0);
	}
	m_bFullScreen = fullscreen;
	m_bUpdateViewport = true;
}

bool SkyBoxWindow::IsLightTheme()
{
	DWORD dwData = 0;
	DWORD cbData = sizeof(dwData);
	auto res = RegGetValueW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
		L"AppsUseLightTheme", RRF_RT_REG_DWORD, nullptr, &dwData, &cbData);
	if (res != ERROR_SUCCESS)
		return true;
	return (dwData != 0);
}

bool SkyBoxWindow::SetTitleBarDarkMode(HWND hWnd, bool bDarkMode)
{
	BOOL value = bDarkMode;
	HRESULT hr = ::DwmSetWindowAttribute(hWnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &value, sizeof(value));
	if (SUCCEEDED(hr)) {
		LONG lStyle = GetWindowLong(hWnd, GWL_STYLE);
		if (lStyle & WS_CAPTION) {
			int value = (int)DWM_SYSTEMBACKDROP_TYPE::DWMSBT_TRANSIENTWINDOW;
			DwmSetWindowAttribute(hWnd, DWMWA_SYSTEMBACKDROP_TYPE, &value, sizeof(value));
		}
		return true;
	}
	return false;
}

// =======================================================
// Recompute transformation matrices from user inputs
// =======================================================
void SkyBoxWindow::ComputeMatricesFromInputs()
{
	// glfwGetTime is called only once, the first time this function is called
	static float lastTime = (float)glfwGetTime();

	// Compute time difference between current and last frame
	float currentTime = (float)glfwGetTime();
	float deltaTime = float(currentTime - lastTime);

	// Get mouse position
	double xpos, ypos;
	glfwGetCursorPos(m_pWindow, &xpos, &ypos);

	// Reset mouse position for next frame
	glfwSetCursorPos(m_pWindow, m_rect.width / 2.0, m_rect.height / 2.0);

	// Compute new orientation
	// The cursor is set to the center of the screen last frame,
	// so (currentCursorPos - center) is the offset of this frame
	horizontalAngle += mouseSpeed * float(xpos - m_rect.width / 2.f);
	verticalAngle += mouseSpeed * float(-ypos + m_rect.height / 2.f);

	// Direction : Spherical coordinates to Cartesian coordinates conversion
	vec3 direction =
		vec3(sin(verticalAngle) * cos(horizontalAngle), cos(verticalAngle), sin(verticalAngle) * sin(horizontalAngle));

	// Right vector
	vec3 right = vec3(cos(horizontalAngle - 3.14 / 2.f), 0.f, sin(horizontalAngle - 3.14 / 2.f));

	// New up vector
	vec3 newUp = cross(right, direction);

	// Move forward
	if (glfwGetKey(m_pWindow, GLFW_KEY_W) == GLFW_PRESS)
	{
		eyePoint += direction * deltaTime * speed;
	}
	// Move backward
	if (glfwGetKey(m_pWindow, GLFW_KEY_S) == GLFW_PRESS)
	{
		eyePoint -= direction * deltaTime * speed;
	}
	// Strafe right
	if (glfwGetKey(m_pWindow, GLFW_KEY_D) == GLFW_PRESS)
	{
		eyePoint += right * deltaTime * speed;
	}
	// Strafe left
	if (glfwGetKey(m_pWindow, GLFW_KEY_A) == GLFW_PRESS)
	{
		eyePoint -= right * deltaTime * speed;
	}

	// Recompute camera matrix
	mat4 newP = perspective(initialFoV, 1.f * m_rect.width / m_rect.height, 0.1f, farPlane);
	mat4 newV = lookAt(eyePoint, eyePoint + direction, newUp);

	// Update skybox transformation matrices
	glUseProgram(shaderSkybox);
	skyboxV = newV;
	skyboxP = newP;
	glUniformMatrix4fv(uniSkyboxV, 1, GL_FALSE, value_ptr(skyboxV));
	glUniformMatrix4fv(uniSkyboxP, 1, GL_FALSE, value_ptr(skyboxP));

	// Make sure that the center of skybox is always at eyePoint
	// The GLM matrix is column major
	skyboxM[3][0] = oriSkyboxM[0][3] + eyePoint.x;
	skyboxM[3][1] = oriSkyboxM[1][3] + eyePoint.y;
	skyboxM[3][2] = oriSkyboxM[2][3] + eyePoint.z;
	glUniformMatrix4fv(uniSkyboxM, 1, GL_FALSE, value_ptr(skyboxM));

	// Update mesh transformation matrices
	glUseProgram(shaderMesh);
	meshV = newV;
	meshP = newP;
	glUniformMatrix4fv(uniMeshV, 1, GL_FALSE, value_ptr(meshV));
	glUniformMatrix4fv(uniMeshP, 1, GL_FALSE, value_ptr(meshP));

	// For the next frame, the "last time" will be "now"
	lastTime = currentTime;
}

// ===================================================================
// Keyboard callback function
// - GLFW keyboard callback reference:
//   https://www.glfw.org/docs/3.3/input_guide.html#input_keyboard
// ===================================================================
void SkyBoxWindow::KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	auto pThis = reinterpret_cast<SkyBoxWindow*>(glfwGetWindowUserPointer(window));

	// Key press event
	if (action == GLFW_PRESS) {
		switch (key) {
		// Esc: close mainWindow
		case GLFW_KEY_ESCAPE:
			glfwSetWindowShouldClose(window, GLFW_TRUE);
			break;
		case GLFW_KEY_ENTER:
			pThis->SetFullScreen(!pThis->m_bFullScreen);
			break;
		// F: polygon fill mode
		case GLFW_KEY_F:
			glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
			break;
		// L: polygon line mode
		case GLFW_KEY_L:
			glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
			break;
		// I: eye point information
		case GLFW_KEY_I:
			std::cout << "eyePoint: " << to_string(pThis->eyePoint) << '\n';
			std::cout << "verticleAngle: " << fmod(pThis->verticalAngle, 6.28f) << ", "
				<< "horizontalAngle: " << fmod(pThis->horizontalAngle, 6.28f) << endl;
			break;
		default:
			break;
		}
	}
}

void SkyBoxWindow::ResizeCallback(GLFWwindow* window, int cx, int cy)
{
	auto pThis = reinterpret_cast<SkyBoxWindow*>(glfwGetWindowUserPointer(window));
	pThis->m_bUpdateViewport = true;
}

// ===================================================================
// Initialize OpenGL context
// ===================================================================
bool SkyBoxWindow::InitGL()
{
	// Initialise GLFW
	if (!glfwInit()) {
		fprintf(stderr, "Failed to initialize GLFW\n");
		getchar();
		exit(EXIT_FAILURE);
		return false;
	}

	// Without setting GLFW_CONTEXT_VERSION_MAJOR and _MINOR£¬
	// OpenGL 1.x will be used
	glfwWindowHint(GLFW_SAMPLES, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);

	// Must apply the following settings if OpenGL version >= 3.0 is used
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	int cxScrn = GetSystemMetrics(SM_CXFULLSCREEN), cyScrn = GetSystemMetrics(SM_CYFULLSCREEN);
	Rect rect{ 0, 0, int(cxScrn / 1.5f), int(cyScrn / 1.5f) };
	rect.x = (cxScrn - rect.width) / 2;
	rect.y = (cyScrn - rect.height) / 2;

	// Create window and its OpenGL context
	const char* title = "GLFW mainWindow with AntTweakBar";
	m_pWindow = glfwCreateWindow(rect.width, rect.height, title, NULL, NULL);
	if (!m_pWindow) {
		std::cout << "Failed to open GLFW mainWindow." << std::endl;
		glfwTerminate();
		exit(EXIT_FAILURE);
		return false;
	}
	glfwSetWindowUserPointer(m_pWindow, reinterpret_cast<void*>(this));
	glfwSetWindowPos(m_pWindow, rect.x, rect.y);

	m_pMonitor = glfwGetPrimaryMonitor();
	glfwGetWindowSize(m_pWindow, &m_rect.width, &m_rect.height);
	glfwGetWindowPos(m_pWindow, &m_rect.x, &m_rect.y);
	m_bUpdateViewport = true;

	m_hWnd = FindWindowA("GLFW30", title);
	if (m_hWnd) {
		HANDLE hIcon = LoadImage(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_SKYBOX), IMAGE_ICON, 32, 32, LR_COLOR);
		SendMessage(m_hWnd, (UINT)WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
		if (!IsLightTheme())
			SetTitleBarDarkMode(m_hWnd, TRUE);
	}
	glfwMakeContextCurrent(m_pWindow);

	// Input settings
	glfwSetInputMode(m_pWindow, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
	glfwSetKeyCallback(m_pWindow, &SkyBoxWindow::KeyCallback);
	glfwSetWindowSizeCallback(m_pWindow, &SkyBoxWindow::ResizeCallback);

	// Without this, glGenVertexArrays will report ERROR!
	glewExperimental = GL_TRUE;

	// nitialize GLEW
	if (glewInit() != GLEW_OK) {
		fprintf(stderr, "Failed to initialize GLEW\n");
		getchar();
		glfwTerminate();
		exit(EXIT_FAILURE);
		return false;
	}

	// Face culling and depth test
	glEnable(GL_CULL_FACE);
	glEnable(GL_DEPTH_TEST);
	return true;
}

// ================================================
// Initialize shaders
// ================================================
void SkyBoxWindow::InitShader()
{
	shaderSkybox = buildShader("../shader/vsSkybox.glsl", "../shader/fsSkybox.glsl");
	shaderMesh = buildShader("../shader/vsModel.glsl", "../shader/fsModel.glsl");
}

// ================================================
// Initialize transformation matrices
// ================================================
void SkyBoxWindow::InitMatrix()
{
	// Model, view and projection matrices
	mat4 M, V, P;
	M = translate(mat4(1.f), vec3(0.f, 0.f, -4.f));
	V = lookAt(eyePoint, eyePoint + eyeDirection, up);
	P = perspective(initialFoV, 1.f * m_rect.width / m_rect.height, 0.01f, farPlane);

	// ----------------------------------------
	// Transformation matrices for mesh
	// ----------------------------------------
	glUseProgram(shaderMesh);

	meshM = M;
	meshV = V;
	meshP = P;

	uniMeshM = myGetUniformLocation(shaderMesh, "model");
	uniMeshV = myGetUniformLocation(shaderMesh, "view");
	uniMeshP = myGetUniformLocation(shaderMesh, "projection");

	glUniformMatrix4fv(uniMeshM, 1, GL_FALSE, value_ptr(meshM));
	glUniformMatrix4fv(uniMeshV, 1, GL_FALSE, value_ptr(meshV));
	glUniformMatrix4fv(uniMeshP, 1, GL_FALSE, value_ptr(meshP));

	// ----------------------------------------
	// Transformation matrices for skybox
	// ----------------------------------------
	glUseProgram(shaderSkybox);

	skyboxM = M;
	oriSkyboxM = skyboxM;
	skyboxV = V;
	skyboxP = P;

	uniSkyboxM = myGetUniformLocation(shaderSkybox, "model");
	uniSkyboxV = myGetUniformLocation(shaderSkybox, "view");
	uniSkyboxP = myGetUniformLocation(shaderSkybox, "projection");

	glUniformMatrix4fv(uniSkyboxM, 1, GL_FALSE, value_ptr(skyboxM));
	glUniformMatrix4fv(uniSkyboxV, 1, GL_FALSE, value_ptr(skyboxV));
	glUniformMatrix4fv(uniSkyboxP, 1, GL_FALSE, value_ptr(skyboxP));
}

// ================================================
// Initialize light source
// ================================================
void SkyBoxWindow::InitLight()
{
	// ---------------------------------
	// Light source for mesh
	// ---------------------------------
	glUseProgram(shaderMesh);

	uniLightColor = myGetUniformLocation(shaderMesh, "lightColor");
	glUniform3fv(uniLightColor, 1, value_ptr(lightColor));

	uniLightPos = myGetUniformLocation(shaderMesh, "lightPosition");
	glUniform3fv(uniLightPos, 1, value_ptr(lightPosition));

	uniLightPower = myGetUniformLocation(shaderMesh, "lightPower");
	glUniform1f(uniLightPower, lightPower);

	uniDiffuse = myGetUniformLocation(shaderMesh, "diffuseColor");
	glUniform3fv(uniDiffuse, 1, value_ptr(materialDiffuse));

	uniAmbient = myGetUniformLocation(shaderMesh, "ambientColor");
	glUniform3fv(uniAmbient, 1, value_ptr(materialAmbient));

	uniSpecular = myGetUniformLocation(shaderMesh, "specularColor");
	glUniform3fv(uniSpecular, 1, value_ptr(materialSpecular));
}

// ================================================
// Initialize skybox
// ================================================
void SkyBoxWindow::InitSkybox()
{
	// Create texture objects
	glActiveTexture(GL_TEXTURE0);
	glGenTextures(1, &tboSkybox);
	glBindTexture(GL_TEXTURE_CUBE_MAP, tboSkybox);

	// Necessary parameter settings for cubemap
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

	// Read images into cubemap
	vector<string> texImages;
	texImages.push_back("../res/left.png");
	texImages.push_back("../res/right.png");
	texImages.push_back("../res/bottom.png");
	texImages.push_back("../res/top.png");
	texImages.push_back("../res/front.png");
	texImages.push_back("../res/back.png");

	TextureImage image;
	for (GLuint i = 0; i < texImages.size(); i++) {
		/*int width, height;
		FIBITMAP *image;
		image = FreeImage_ConvertTo32Bits(FreeImage_Load(FIF_PNG, texImages[i].c_str()));
		width = FreeImage_GetWidth(image);
		height = FreeImage_GetHeight(image);
		glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGBA, width, height, 0, GL_BGRA, GL_UNSIGNED_BYTE,
			(void *)FreeImage_GetBits(image));
		FreeImage_Unload(image);*/
		void* imageData = image.Load(texImages[i].c_str());
		glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGBA, image.width, image.height,
			0, GL_RGBA, GL_UNSIGNED_BYTE, imageData);
	}

	// Set image data to texture objects
	// - If put these code before setting texture,
	//   no skybox will be rendered
	glGenVertexArrays(1, &vaoSkybox);
	glBindVertexArray(vaoSkybox);
	glGenBuffers(1, &vboSkybox);
	glBindBuffer(GL_ARRAY_BUFFER, vboSkybox);
	GLfloat _vtxsSkybox[_countof(vtxsSkybox)] = { 0 };
	for (int i = 0; i < _countof(vtxsSkybox); i++)
		_vtxsSkybox[i] = vtxsSkybox[i] * SKYBOX_SIZE;
	glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * 6 * 6 * 3, _vtxsSkybox, GL_STATIC_DRAW);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
	glEnableVertexAttribArray(0);
}

// ================================================
// Initialize mesh
// - Load 3D model from file
// - Initialize its OpenGL context
// ================================================
void SkyBoxWindow::InitMesh()
{
	m_cube = loadMeshModel("../model/cube.obj");
	initMeshObject(m_cube);
}
