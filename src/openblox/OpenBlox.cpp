#include "OpenBlox.h"

#include "../ob_instance/DataModel.h"

#include <unistd.h>
#include <stdio.h>

#include "WindowUtils.h"

#include "OpenBloxRenderUtil.h"

#ifdef _WIN32
#pragma comment( lib, "ws2_32" )
#include <WinSock2.h>
#endif

OpenBlox::BaseGame* game;

OpenBlox::Thread* renderThread;

lua_State* L = NULL;

double lastTime = glfwGetTime();
int nbFrames = 0;

void render(){
	OpenBlox::renderLock = true;

	double currentTime = glfwGetTime();
	nbFrames++;
	if(currentTime - lastTime >= 1.0){
		std::string newTitle = "OpenBlox - TPS: ";
		newTitle = newTitle + ((std::ostringstream&)(std::ostringstream() << std::dec << 1000.0/double(nbFrames))).str();

		glfwSetWindowTitle(OpenBlox::getWindow(), newTitle.c_str());

		nbFrames = 0;
		lastTime += 1.0;
	}

	//float ratio;
	int width, height;

	OpenBlox::getFramebufferSize(&width, &height);
	//ratio = width / (float)height;

	glViewport(0, 0, width, height);
	glClear(GL_COLOR_BUFFER_BIT);

	ob_instance::DataModel* dm = game->getDataModel();
	if(dm){
		dm->render();
	}

	OpenBlox::renderLock = false;
}

bool wasResized = false;

void taskLoop(){
	GLFWwindow* window = OpenBlox::getWindow();

	while(!glfwWindowShouldClose(window)){
		ob_instance::DataModel* dm = game->getDataModel();
		if(dm){
			if(dm->runService){
				if(dm->runService->RenderStepped){
					std::vector<ob_type::VarWrapper> args = std::vector<ob_type::VarWrapper>();

					dm->runService->RenderStepped->Fire(args);
				}
			}
		}
		OpenBlox::ThreadScheduler::Tick();

		if(dm){
			if(dm->runService){
				if(dm->runService->Heartbeat){
					std::vector<ob_type::VarWrapper> args = std::vector<ob_type::VarWrapper>();
					args.push_back(ob_type::VarWrapper(1/30));

					dm->runService->Heartbeat->Fire(args);
				}
				if(dm->runService->Stepped){
					std::vector<ob_type::VarWrapper> args = std::vector<ob_type::VarWrapper>();
					args.push_back((OpenBlox::currentTimeMillis() - OpenBlox::BaseGame::elapsedTime()) / 1000.0);
					args.push_back(ob_type::VarWrapper(1/30));

					dm->runService->Stepped->Fire(args);
				}
			}
			if(wasResized){
				wasResized = false;
				if(dm->starterGui){
					int w;
					int h;
					OpenBlox::getFramebufferSize(&w, &h);

					dm->starterGui->sizeChanged(w, h);
				}
			}
		}
		usleep(10000);
	}
}

void size_callback(int width, int height){
	wasResized = true;
}

#ifndef OPENBLOX_JNI
void glfw_error_callback(int error, const char* description){
	LOGE("[GLFW] %s", description);
}

void glfw_window_size_callback(GLFWwindow* window, int width, int height){
	size_callback(width, height);
}

void renderLoop(){
	GLFWwindow* window = OpenBlox::getWindow();
	glfwMakeContextCurrent(window);

	{
		const GLubyte* vendor = glGetString(GL_VENDOR);
		const GLubyte* renderer = glGetString(GL_RENDERER);
		const GLubyte* version = glGetString(GL_VERSION);
		const GLubyte* shading_version = glGetString(GL_SHADING_LANGUAGE_VERSION);

		LOGI("[GL] Vendor: %s", vendor);
		LOGI("[GL] Renderer: %s", renderer);
		LOGI("[GL] OpenGL Version: %s", version);
		LOGI("[GL] Shading Language Version: %s", shading_version);
	}

	while(!glfwWindowShouldClose(window)){
		render();

		glfwSwapBuffers(window);
		usleep(500);
	}
}

#include "../easywsclient/easywsclient.hpp"

int main(){
	#ifdef _WIN32
		INT rc;
		WSADATA wsaData;

		rc = WSAStartup(MAKEWORD(2, 2), &wsaData);
		if(rc){
			LOGW("WSAStartup failed.");
		}
	#endif

	glfwSetErrorCallback(glfw_error_callback);
	if(!glfwInit()){
		LOGE("[GLFW] Failed to initialize library.");
		return 1;
	}

	OpenBlox::BaseGame::InstanceFactory = new OpenBlox::Factory();

	game = new OpenBlox::BaseGame();

	static_init::execute();

	#ifdef __APPLE__
	{
		glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
		glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
		glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
		glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	}
	#endif
	glfwWindowHint(GLFW_SAMPLES, OPENBLOX_AA_SAMPLES);

	if(!OpenBlox::createGLContext()){
		return 1;
	}

	GLFWwindow* window = OpenBlox::getWindow();

	glfwMakeContextCurrent(window);
	{
		glewExperimental = GL_TRUE;
		if(glewInit() != GLEW_OK){
			LOGE("[GLFW] Failed to initialize GLEW.");
			glfwTerminate();
			return 1;
		}
	}
	glfwMakeContextCurrent(NULL);

	glfwSetWindowSizeCallback(window, glfw_window_size_callback);

	renderThread = new OpenBlox::Thread(renderLoop);
	OpenBlox::ThreadScheduler::taskThread = new OpenBlox::Thread(taskLoop);

	int val;
	val = renderThread->start();
	if(val){
		LOGE("[CORE] Failed to create render thread.");
		glfwTerminate();
		return 1;
	}

	val = OpenBlox::ThreadScheduler::taskThread->start();
	if(val){
		LOGE("[CORE] Failed to create task thread.");
		glfwTerminate();
		return 1;
	}

	L = OpenBlox::BaseGame::newLuaState();

	OpenBlox::ThreadScheduler::RunOnTaskThread([](va_list args){
		lua_resume(L, 0);

		char* fileName = "init.lua";

		if(!OpenBlox::fileIsReadable(fileName)){
			LOGE("init.lua does not exist or is not readable.");
			glfwSetWindowShouldClose(OpenBlox::getWindow(), true);
			return;
		}

		char* initFile = OpenBlox::fileGetContents(fileName);

		int s = luaL_loadbuffer(L, initFile, strlen(initFile), "@game.Workspace.Script");
		if(s == 0){
			s = lua_pcall(L, 0, LUA_MULTRET, 0);
		}

		if(s != 0){
			game->handle_lua_errors(L);
		}
	}, 100);

	while(!glfwWindowShouldClose(window)){
		glfwWaitEvents();
	}

	renderThread->join();
	OpenBlox::ThreadScheduler::taskThread->join();

	lua_close(L);

	glfwDestroyWindow(window);
	OpenBlox::BaseGame::getInstanceFactory()->releaseTable();
	delete game;
	glfwTerminate();
	#ifdef _WIN32
		WSACleanup();
	#endif
	return 0;
}
#endif
