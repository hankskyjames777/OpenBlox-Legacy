#ifndef OPENBLOX_OPENBLOX_H_
#define OPENBLOX_OPENBLOX_H_

//#define OPENBLOX_JNI //Remove the comments from this to compile for Android

#include <iostream>
#include <string>
#include <sstream>
#include <map>
#include <vector>
#include <algorithm>
#include <stdexcept>

#include <cstdlib>
#include <cstring>

extern "C"{
	#include "../lua/lua.h"
	#include "../lua/lualib.h"
	#include "../lua/lauxlib.h"
}

#ifndef OPENBLOX_JNI
#include <GL/glew.h>

#define GLFW_DLL
#include <GLFW/glfw3.h>

#include <pthread.h>

#define LOGI(...) printf(__VA_ARGS__)
#define LOGW(...) printf(__VA_ARGS__)
#define LOGE(...) fprintf(stderr, __VA_ARGS__)
#else
#include <jni.h>
#include <android/log.h>

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#define LOG_TAG "libOpenBlox"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#endif

//Helpers
#include "static_init.h"
#include "Utility.h"
#include "Factory.h"

#include "BaseGame.h"
#endif
