#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdexcept>
#include <memory>
#include <vector>
#include <array>
#include <fstream>
#include <string>
#include <string_view>
#include <chrono>

using namespace std::literals::string_literals;

#define GLEW_STATIC
#include <GL/glew.h>
#include <GL/wglew.h>

#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/ext.hpp>

namespace {

	struct Vertex
	{
		glm::vec4 position;
	};

	struct Transform 
	{
		glm::mat4 MVP;
	};

	namespace buffer
	{
		enum type
		{
			VERTEX,
			ELEMENT,
			TRANSFORM,
			MAX
		};
	}

	const std::string title{ "Optimizing Triangle Strips for Fast Rendering" };
	HWND hwnd = nullptr;
	HDC hdc = nullptr;
	HGLRC hglrc = nullptr;
	int windowWidth{1280};
	int windowHeight{720};
	glm::vec2 rotation(0.0f, 0.0f);
	GLuint render_program{};
	GLuint pipeline{};
	GLuint vao{};
	std::array<GLuint, buffer::MAX> buffers;
	
	static const std::vector<Vertex> cubeVertices = {
		Vertex{glm::vec4( 1.0f,	1.0f, 1.0f, 1.0f)},
		Vertex{glm::vec4(-1.0f,	1.0f, 1.0f, 1.0f)},
		Vertex{glm::vec4( 1.0f,	1.0f,-1.0f, 1.0f)},
		Vertex{glm::vec4(-1.0f,	1.0f,-1.0f, 1.0f)},
		Vertex{glm::vec4( 1.0f,-1.0f, 1.0f, 1.0f)},
		Vertex{glm::vec4(-1.0f,-1.0f, 1.0f, 1.0f)},
		Vertex{glm::vec4(-1.0f,-1.0f,-1.0f, 1.0f)},
		Vertex{glm::vec4( 1.0f,-1.0f,-1.0f, 1.0f)}
	};

	static const std::vector<GLuint> cubeIndices = {
		3, 2, 6, 7, 4, 2, 0,
		3, 1, 6, 5, 4, 1, 0
	};
}

bool Init();
void InitGL();
void InitProgram();
void InitBuffer();
void InitVertexArray();
void RenderFrame();
void Shutdown();
void CheckShader(GLuint shader);
void CheckProgram(GLuint program);
GLuint CreateShader(std::string_view filename, GLenum type);
GLuint CreateProgram(const std::vector<GLuint>& shaders);
glm::mat4 Camera(float Translate, const glm::vec2& Rotate);
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);


int WINAPI WinMain( HINSTANCE hInstance,
		HINSTANCE /* hPrevInstance */,
		LPSTR /* lpCmdLine */,
		int /* nShowCmd */ )
{
	const std::string class_name{ "GLWindowClass" };

	WNDCLASSEX wcl = {};                        
	wcl.cbSize = sizeof(WNDCLASSEX);
	wcl.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
	wcl.lpfnWndProc = WndProc;
	wcl.cbClsExtra = 0;
	wcl.cbWndExtra = sizeof(LONG_PTR);
	wcl.hInstance = hInstance;
	wcl.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
	wcl.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wcl.hbrBackground = nullptr;
	wcl.lpszMenuName = nullptr;
	wcl.lpszClassName = class_name.c_str();
	wcl.hIconSm = LoadIcon(nullptr, IDI_APPLICATION);
	
	if (!RegisterClassEx(&wcl))
		return 0;

	DWORD wndExStyle = WS_EX_OVERLAPPEDWINDOW;
	DWORD wndStyle = WS_OVERLAPPEDWINDOW | WS_VISIBLE;

	RECT rc = {};
	SetRect(&rc, 0, 0, windowWidth, windowHeight);
	AdjustWindowRectEx(&rc, wndStyle, FALSE, wndExStyle);
	
	hwnd = CreateWindowEx(wndExStyle, class_name.c_str(), title.c_str(), wndStyle,
		0, 0, rc.right - rc.left, rc.bottom - rc.top, nullptr, nullptr, hInstance, nullptr);
	
	if (!hwnd)
		return 0;
	
	ShowWindow(hwnd, SW_SHOW);
	UpdateWindow(hwnd);
	
	if (!Init())
		return 0;

	std::uint64_t frameCounter{};
	double frameTimer{};
	double fpsTimer{};
	double lastFPS{};
	
	MSG msg = {};
	while (msg.message != WM_QUIT)
	{
		if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else
		{
			// get current time
			auto tStart = std::chrono::high_resolution_clock::now();

			RenderFrame();
			SwapBuffers(hdc);

			// calculate time of rendered
			frameCounter++;
			auto tEnd = std::chrono::high_resolution_clock::now();
			auto tDiff = std::chrono::duration<double, std::milli>(tEnd - tStart).count();
			frameTimer = tDiff / 1000.0;

			// calculate FPS
			fpsTimer += tDiff;
			if (fpsTimer > 1000.0)
			{
				std::string windowTitle = title + " - FPS: " + std::to_string(frameCounter);
				SetWindowText(hwnd, windowTitle.c_str());

				lastFPS = round(1.0 / frameTimer);
				fpsTimer = 0.0;
				frameCounter = 0;
			}
		}
	}
	
	Shutdown();
	UnregisterClass(class_name.c_str(), hInstance);
	return static_cast<int>(msg.wParam);
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	static POINT lastMousePos = {};
	static POINT currentMousePos = {};
	static bool isLeftMouseBtnOn = false;
	
	switch (message)
	{
	case WM_DESTROY:
	case WM_CLOSE:
		PostQuitMessage(0);
		return 0;
	
	case WM_SIZE:
		windowWidth = LOWORD(lParam);
		windowHeight = HIWORD(lParam);
		break;
	
	case WM_KEYDOWN:
		switch (wParam)
		{
		case VK_ESCAPE:
			PostQuitMessage(0);
			break;

		default:
			break;
		}
		break;
	
	case WM_LBUTTONDOWN:
		lastMousePos.x = currentMousePos.x = LOWORD(lParam);
		lastMousePos.y = currentMousePos.y = HIWORD(lParam);
		isLeftMouseBtnOn = true;
		break;
	
	case WM_LBUTTONUP:
		isLeftMouseBtnOn = false;
		break;
	
	case WM_MOUSEMOVE:
		currentMousePos.x = LOWORD(lParam);
		currentMousePos.y = HIWORD(lParam);
		
		if (isLeftMouseBtnOn)
		{
			rotation.x -= (currentMousePos.x - lastMousePos.x);
			rotation.y -= (currentMousePos.y - lastMousePos.y);
		}
		
		lastMousePos.x = currentMousePos.x;
		lastMousePos.y = currentMousePos.y;
		break;
	
	default:
		break;
	}
	
	return DefWindowProc(hWnd, message, wParam, lParam);
}

bool Init()
{
	try
	{
		InitGL();
		InitProgram();
		InitBuffer();
		InitVertexArray();
		return true;
	}
	catch (const std::exception& e)
	{
		MessageBox(nullptr, e.what(), "Exception", MB_OK | MB_ICONERROR);
		return false;
	}
}

void InitGL()
{
	hdc = GetDC(hwnd);
	if (!hdc)
		throw std::runtime_error("GetDC() failed"s);

	PIXELFORMATDESCRIPTOR pfd = {};
	pfd.nSize = sizeof(PIXELFORMATDESCRIPTOR);
	pfd.nVersion = 1;
	pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
	pfd.iPixelType = PFD_TYPE_RGBA;
	pfd.cColorBits = 32;
	pfd.cDepthBits = 24;
	pfd.iLayerType = PFD_MAIN_PLANE;
	
	auto pixelFormat = ChoosePixelFormat(hdc, &pfd);
	if (pixelFormat == 0)
		throw std::runtime_error("ChoosePixelFormat() failed"s);

	if (!SetPixelFormat(hdc, pixelFormat, &pfd))
		throw std::runtime_error("SetPixelFormat() failed"s);

	auto tempCtx = wglCreateContext(hdc);
	if (!tempCtx || !wglMakeCurrent(hdc, tempCtx))
		throw std::runtime_error("Creating temp render context failed"s);

	if (auto error = glewInit(); error != GLEW_OK)
		throw std::runtime_error("GLEW Error: "s + std::to_string(error));

	wglMakeCurrent(nullptr, nullptr);
	wglDeleteContext(tempCtx);

	std::array attribList {
		WGL_CONTEXT_MAJOR_VERSION_ARB, 4,
		WGL_CONTEXT_MINOR_VERSION_ARB, 6,
		WGL_CONTEXT_FLAGS_ARB, WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB,
		WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
		0
	};
	
	hglrc = wglCreateContextAttribsARB(hdc, 0, attribList.data());
	if (!hglrc || !wglMakeCurrent(hdc, hglrc))
		throw std::runtime_error("Creating render context failed"s);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);
}

void InitProgram()
{
	auto vs = CreateShader("shaders/cube.vert", GL_VERTEX_SHADER);
	auto fs = CreateShader("shaders/cube.frag", GL_FRAGMENT_SHADER);
	render_program = CreateProgram({ vs, fs });

	glCreateProgramPipelines(1, &pipeline);
	glUseProgramStages(pipeline, GL_VERTEX_SHADER_BIT | GL_FRAGMENT_SHADER_BIT, render_program);
}

void InitBuffer()
{
	GLint alignment = GL_NONE;
	glGetIntegerv(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, &alignment);
	GLint blockSize = glm::max(GLint(sizeof(Transform)), alignment);
	
	glCreateBuffers(buffer::MAX, &buffers[0]);
	glNamedBufferStorage(buffers[buffer::VERTEX], cubeVertices.size() * sizeof(Vertex), cubeVertices.data(), 0);
	glNamedBufferStorage(buffers[buffer::ELEMENT], cubeIndices.size() * sizeof(GLuint), cubeIndices.data(), 0);
	glNamedBufferStorage(buffers[buffer::TRANSFORM], blockSize, nullptr, GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT);
}

void InitVertexArray()
{
	glCreateVertexArrays(1, &vao);
	
	glVertexArrayAttribBinding(vao, 0, 0);
	glVertexArrayAttribFormat(vao, 0, 4, GL_FLOAT, GL_FALSE, 0);
	glEnableVertexArrayAttrib(vao, 0);

	glVertexArrayVertexBuffer(vao, 0, buffers[buffer::VERTEX], 0, sizeof(Vertex));
	glVertexArrayElementBuffer(vao, buffers[buffer::ELEMENT]);
}

void RenderFrame()
{
	{
		auto transform = static_cast<Transform*>(glMapNamedBufferRange(buffers[buffer::TRANSFORM],
			0, sizeof(Transform), GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT));

		transform->MVP = Camera(5.0f, rotation);

		glUnmapNamedBuffer(buffers[buffer::TRANSFORM]);
	}

	glViewportIndexedf(0, 0.0f, 0.0f, static_cast<GLfloat>(windowWidth), static_cast<GLfloat>(windowHeight));
	glClearBufferfv(GL_COLOR, 0, &glm::vec4(0.2f, 0.2f, 0.3f, 1.0f)[0]);
	glClearBufferfv(GL_DEPTH, 0, &glm::vec4(1.0f)[0]);
		
	glBindProgramPipeline(pipeline);
	glBindVertexArray(vao);
	glBindBufferBase(GL_UNIFORM_BUFFER, 1, buffers[buffer::TRANSFORM]);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, buffers[buffer::VERTEX]);

	// Call to OpenGL draw function. (14 is the elements array size)
	glDrawElementsInstancedBaseVertex(GL_TRIANGLE_STRIP, static_cast<GLsizei>(cubeIndices.size()), GL_UNSIGNED_INT, nullptr, 1, 0);
}

GLuint CreateShader(std::string_view filename, GLenum type)
{
	const auto source = [filename](){
		std::string result;
		std::ifstream stream(filename.data());
		
		if (!stream.is_open()) {
			std::string str{filename};
			throw std::runtime_error("Could not open file: "s + str);
			return result;
		}
		
		stream.seekg(0, std::ios::end);
		result.reserve(static_cast<size_t>(stream.tellg()));
		stream.seekg(0, std::ios::beg);
		
		result.assign(std::istreambuf_iterator<char>{stream},
			std::istreambuf_iterator<char>{});
			
		return result;
	}();
	auto pSource = source.c_str();

	GLuint shader = glCreateShader(type);
	glShaderSource(shader, 1, &pSource, nullptr);
	glCompileShader(shader);
	CheckShader(shader);

	return shader;
}

GLuint CreateProgram(const std::vector<GLuint>& shaders)
{
	GLuint program = glCreateProgram();
	glProgramParameteri(program, GL_PROGRAM_SEPARABLE, GL_TRUE);

	for (const auto& shader : shaders) {
		glAttachShader(program, shader);
	}

	glLinkProgram(program);
	CheckProgram(program);

	for (const auto& shader : shaders) {
		glDetachShader(program, shader);
		glDeleteShader(shader);
	}

	return program;
}

void CheckShader(GLuint shader)
{
	GLint isCompiled{};
	glGetShaderiv(shader, GL_COMPILE_STATUS, &isCompiled);
	if (isCompiled == GL_FALSE)
	{
		GLint maxLength{};
		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &maxLength);
		if (maxLength > 0)
		{
			auto infoLog = std::make_unique<GLchar[]>(maxLength);
			glGetShaderInfoLog(shader, maxLength, &maxLength, infoLog.get());
			glDeleteShader(shader);
			throw std::runtime_error("Error compiled:\n"s + infoLog.get());
		}
	}
}

void CheckProgram(GLuint program)
{
	GLint isLinked{};
	glGetProgramiv(program, GL_LINK_STATUS, &isLinked);
	if (isLinked == GL_FALSE) {
		GLint maxLength{};
		glGetProgramiv(program, GL_INFO_LOG_LENGTH, &maxLength);
		if (maxLength > 0) {
			auto infoLog = std::make_unique<GLchar[]>(maxLength);
			glGetProgramInfoLog(program, maxLength, &maxLength, infoLog.get());
			glDeleteProgram(program);
			throw std::runtime_error("Error linking:\n"s + infoLog.get());
		}
	}
}

glm::mat4 Camera(float Translate, const glm::vec2& Rotate)
{
	auto aspectRatio = static_cast<float>(windowWidth) / static_cast<float>(windowHeight);
	glm::mat4 projection = glm::perspective(glm::pi<float>() * 0.25f, aspectRatio, 0.1f, 100.0f);
	glm::mat4 view = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -Translate));
	view = glm::rotate(view, glm::radians(-Rotate.y), glm::vec3(1.0f, 0.0f, 0.0f));
	view = glm::rotate(view, glm::radians(-Rotate.x), glm::vec3(0.0f, 1.0f, 0.0f));
	glm::mat4 model = glm::scale(glm::mat4(1.0f), glm::vec3(0.5f));
	return projection * view * model;
}

void Shutdown()
{
	glDeleteProgramPipelines(1, &pipeline);
	glDeleteProgram(render_program);
	glDeleteBuffers(buffer::MAX, &buffers[0]);
	glDeleteVertexArrays(1, &vao);

	if (hwnd) {
		if (hdc) {
			if (hglrc) {
				wglMakeCurrent(hdc, nullptr);
				wglDeleteContext(hglrc);
				hglrc = nullptr;
			}

			ReleaseDC(hwnd, hdc);
			hdc = nullptr;
		}

		DestroyWindow(hwnd);
		hwnd = nullptr;
	}
}
