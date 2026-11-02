//
// Created by YYANG219 on 2026/10/28.
//

#include <jni.h>
#include <android/log.h>
#include <cstdint>
#include <mutex>
#include <thread>
#include <android/native_window.h>
#include <android/native_window_jni.h>

#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "GLesFilter", __VA_ARGS__)

// ---------- 着色器 ----------
// 顶点着色器：画一个全屏四边形，并把 UV 用 SurfaceTexture 的变换矩阵校正方向。
static const char *VERT_SRC =
        "attribute vec2 aPos;\n"
        "attribute vec2 aUV;\n"
        "uniform mat4 uTransform;\n"
        "varying vec2 vUV;\n"
        "void main() {\n"
        "  vUV = (uTransform * vec4(aUV, 0.0, 1.0)).xy;\n"
        "  gl_Position = vec4(aPos, 0.0, 1.0);\n"
        "}\n";

// 片元着色器：按 uFilter 决定滤镜（0 灰度 / 1 反色 / 2 怀旧 / 3 暖色）。
// 这里就是“原来 CPU 的 for 循环”搬到 GPU 后的等价逻辑，只是它对每个像素并行执行。
static const char *FRAG_SRC =
        "#extension GL_OES_EGL_image_external : require\n"
        "precision mediump float;\n"
        "uniform samplerExternalOES uTex;\n"
        "uniform int uFilter;\n"
        "varying vec2 vUV;\n"
        "void main() {\n"
        "  vec3 c = texture2D(uTex, vUV).rgb;\n"
        "  vec3 o;\n"
        "  if (uFilter == 0) {\n"
        "    float g = c.r * 0.299 + c.g * 0.587 + c.b * 0.114;\n"
        "    o = vec3(g);\n"
        "  } else if (uFilter == 1) {\n"
        "    o = vec3(1.0 - c.r, 1.0 - c.g, 1.0 - c.b);\n"
        "  } else if (uFilter == 2) {\n"
        "    float r = c.r * 0.393 + c.g * 0.769 + c.b * 0.189;\n"
        "    float g = c.r * 0.349 + c.g * 0.686 + c.b * 0.168;\n"
        "    float b = c.r * 0.272 + c.g * 0.534 + c.b * 0.131;\n"
        "    o = min(vec3(r, g, b), vec3(1.0));\n"
        "  } else {\n"
        "    o = vec3(min(c.r + 0.12, 1.0), c.g, max(c.b - 0.12, 0.0));\n"
        "  }\n"
        "  gl_FragColor = vec4(o, 1.0);\n"
        "}\n";



// 引擎状态
struct Engine{
    
    EGLDisplay eglDisplay = EGL_NO_DISPLAY;
    EGLSurface eglSurface = EGL_NO_SURFACE;
    EGLContext eglContext = EGL_NO_CONTEXT;

    GLuint program = 0;  //着色器程序在GPU中的编号
    GLuint tex = 0;                      // 由 SurfaceTexture 喂数据的 OES 纹理
    GLuint vbo = 0;                      // 全屏四边形的顶点缓冲
    GLint aPos = -1, aUV = -1, uTex = -1, uFilter = -1, uTransform = -1;
    
    int texId;
    JavaVM *vm = nullptr;
    jobject displaySurface = nullptr; //Kotlin层Surface
    ANativeWindow *window = nullptr; //Kotlin层Surface对应的窗口
    
    std::mutex mutex;
    std::condition_variable cv;
    std::thread glThread;
    bool ready = false;
    bool exitThread = false;
    bool renderRequested = false;
    std::atomic<int> filter{0};
    
    jobject inputSurfaceTexture = nullptr; //相机输入使用的SurfaceTexture全局引用
    jmethodID updateTexImage = nullptr; // 图片帧变纹理的方法

    float transform[16]{1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
    bool hasTransform = false;

    bool loggedSwapFail = false;
    
};

static Engine *gEngine = nullptr;

/**
 * 编译着色器,类型只有两种：GL_VERTEX_SHADER顶点着色器 / GL_FRAGMENT_SHADER片元着色器
 * @return 
 */
static GLint compileShader(GLenum type, const char *src){
    // 造一个空的着色器对象
    GLuint sh = glCreateShader(type);

    //把源码字符串填进去
    glShaderSource(sh, 1, &src, nullptr);

    //正式编译
    glCompileShader(sh);
    
    //查看编译的结果
    GLint ok = 0;
    glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
    if (!ok){
        LOGE("着色器编译失败");
        glDeleteShader(sh);
        return 0;
    }
    return sh;
}

/**
 * 创建着色器程序，其内部会编译着色器并关联
 */
static GLint linkProgram(const char *vs, const char *fs){
    GLuint v = compileShader(GL_VERTEX_SHADER, vs);
    GLuint f = compileShader(GL_FRAGMENT_SHADER, fs);
    if (!v || !f) return 0;

    //创建着色器程序
    GLuint p = glCreateProgram();

    //程序关联着色器
    glAttachShader(p, v);
    glAttachShader(p, f);
    
    //把程序和着色器做链接
    glLinkProgram(p);
    
    //查看创建的程序连接状态
    GLint ok = 0;
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        LOGE("program 链接失败");
        glDeleteProgram(p);
        return 0;
    }
    glDeleteShader(v);
    glDeleteShader(f);
    return p;
    
}

/**
 * 初始化OpenGL，主要是构建EGL显示桥梁、编译着色器并建立渲染管线
 * @return 
 */
static bool initGL(Engine *e, JNIEnv *env){
    e->window = ANativeWindow_fromSurface(env, e->displaySurface);
    
    //EGL显示与Android图像显示的桥梁
    e->eglDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (e->eglDisplay == EGL_NO_DISPLAY) return false;
    if (!eglInitialize(e->eglDisplay, nullptr, nullptr)) return false;

    //定义硬件需求列表（以 EGL_NONE 结尾）
    EGLint attribs[] = {
            EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
            EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
            EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8,
            EGL_NONE
    };

    EGLConfig config = nullptr;
    EGLint num = 0;
    // EGL根据硬件需求，选择硬件驱动
    if (!eglChooseConfig(e->eglDisplay, attribs, &config, 1, &num) || num < 1) {
        LOGE("eglChooseConfig 失败");
        return false;
    }
    
    // 创建egl的图形缓冲，会直接绑定Android的surface buffer
    e->eglSurface = eglCreateWindowSurface(e->eglDisplay, config, e->window, nullptr);
    if (e->eglSurface == EGL_NO_SURFACE) return false;


    EGLint ctxAttr[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};
    // 创建egl上下文
    e->eglContext = eglCreateContext(e->eglDisplay, config, EGL_NO_CONTEXT, ctxAttr);
    if (e->eglContext == EGL_NO_CONTEXT) return false;
    
    //激活egl的工作环境，把egldisplay、eglsurface、eglcontext绑定在一起
    if (!eglMakeCurrent(e->eglDisplay, e->eglSurface, e->eglSurface, e->eglContext)){
        LOGE("eglMakeCurrent 失败");
        return false;
    }
    
    //编译着色器，并创建着色器程序（渲染管线）
    e->program = linkProgram(VERT_SRC, FRAG_SRC);
    if (!e->program) return false;

    e->aPos = glGetAttribLocation(e->program, "aPos");
    e->aUV = glGetAttribLocation(e->program, "aUV");
    e->uTex = glGetUniformLocation(e->program, "uTex");
    e->uFilter = glGetUniformLocation(e->program, "uFilter");
    e->uTransform = glGetUniformLocation(e->program, "uTransform");
    
    //创建OES纹理,并绑定，这里就会获取到纹理的id
    glGenTextures(1, &e->tex);
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, e->tex);

    glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    //申请缓冲buffer
    static const GLfloat quad[] = {
            -1, -1, 0, 0,  
            1, -1, 1, 0, 
            -1,  1, 0, 1,  
            1,  1, 1, 1,  
    };
    glGenBuffers(1, &e->vbo);                                  
    glBindBuffer(GL_ARRAY_BUFFER, e->vbo);                       
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW); 
    
    return true;
}

/**
 * 真正执行一帧的绘制
 */
static void drawFrame(Engine *e, JNIEnv *env){
    // ----步骤 1： 把相机最新一帧"接"到 OES 纹理上 ；updateTexImage() 取走最新一帧
    if (e->inputSurfaceTexture && e->updateTexImage) {
        env->CallVoidMethod(e->inputSurfaceTexture, e->updateTexImage);
    }

    // ---- 步骤 2：把视口对齐到当前显示表面
    EGLint w = 0, h = 0;
    eglQuerySurface(e->eglDisplay, e->eglSurface, EGL_WIDTH, &w);
    eglQuerySurface(e->eglDisplay, e->eglSurface, EGL_HEIGHT, &h);
    glViewport(0, 0, w, h);

    // 清屏：把上一步留下的颜色刷掉。GL_COLOR_BUFFER_BIT 表示"清颜色缓冲"。
    glClearColor(0, 0, 0, 1);       // 清成不透明黑：R=0 G=0 B=0 A=1
    glClear(GL_COLOR_BUFFER_BIT);   // 清掉上一帧残影

    // ---- 步骤 3：启用管线与资源
    glUseProgram(e->program);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, e->tex);   // 相机帧所在的外部纹理
    glUniform1i(e->uTex, 0);                          // 告诉着色器用纹理单元 0
    glUniform1i(e->uFilter, e->filter.load());        // 滤镜编号，切换时由 setFilter 改
    {
        // 更新矩阵方向
        std::lock_guard<std::mutex> lk(e->mutex);
        if (e->hasTransform) {
            glUniformMatrix4fv(e->uTransform, 1, GL_FALSE, e->transform);
        }
    }

    // ---- 步骤 4：画一个覆盖全屏的四边形 ----
    glBindBuffer(GL_ARRAY_BUFFER, e->vbo);
    glEnableVertexAttribArray(e->aPos);
    glVertexAttribPointer(e->aPos, 2, GL_FLOAT, false, 16, (void *) 0);
    glEnableVertexAttribArray(e->aUV);
    glVertexAttribPointer(e->aUV, 2, GL_FLOAT, false, 16, (void *) 8);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glDisableVertexAttribArray(e->aPos);
    glDisableVertexAttribArray(e->aUV);


    // ---- 步骤 5：提交到屏幕（双缓冲交换）----
    if (!eglSwapBuffers(e->eglDisplay, e->eglSurface) && !e->loggedSwapFail ){
        LOGE("gl buffer提交失败");
        e->loggedSwapFail = true;
    }
    
    
}

/**
 * 关闭EGL，销毁顺序 = 初始化顺序的【逆序】：后创建的先销毁。
 */
static void teardownGL(Engine *e) {
    if (e->program) glDeleteProgram(e->program);
    if (e->tex) glDeleteTextures(1, &e->tex);
    if (e->vbo) glDeleteBuffers(1, &e->vbo);

    if (e->eglDisplay != EGL_NO_DISPLAY) {
        eglMakeCurrent(e->eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (e->eglSurface) eglDestroySurface(e->eglDisplay, e->eglSurface);
        if (e->eglContext) eglDestroyContext(e->eglDisplay, e->eglContext);
        eglTerminate(e->eglDisplay);
    }

    if (e->window) {
        ANativeWindow_release(e->window);
        e->window = nullptr;
    }
}


/**
 * Native的GL循环主线程
 * 在此线程中创建EGL上下文
 * @param g 
 */
static void glThreadFunc(Engine *g){
    //JNIEnv是线程私有的，新线程必须先Attach才能调用Java方法（updateTexImage）
    JNIEnv *env = nullptr;
    if (g->vm->AttachCurrentThread(&env, nullptr) != JNI_OK){
        return;
    }
    
    //初始化EGL和渲染管线
    bool ok = initGL(g, env);
    g->texId = ok? (int)g->tex : -1;
    g->ready = true;
    g->cv.notify_all();         // 通知 createGLContext：纹理 id 已就绪（或初始化失败）

    if (ok){
        while (true){
            {
                std::unique_lock<std::mutex> lk(g->mutex);
                //没有渲染请求的时候，就休眠等待,不空转CPU
                g->cv.wait(lk, [&]{return g->renderRequested || g->exitThread;});
                if (g->exitThread) break;
                g->renderRequested = false;
            }

            drawFrame(g, env);
            
        }
    }

    // 退出前在同一线程销毁 GL 资源（此时上下文仍然 current）
    teardownGL(g);
    g->vm->DetachCurrentThread();
    
}

extern "C"
JNIEXPORT jint JNICALL
Java_com_yys_jnidemo_CameraFilterNative_createGLContext(JNIEnv *env, jobject thiz,
                                                        jobject display_surface) {
   
    //如果之前存在Engine示例，需要先清除
    if (gEngine){
        delete gEngine;
        gEngine = nullptr;
    }
    
    Engine *engine = new Engine();
    env->GetJavaVM(&engine->vm);
    engine->displaySurface = env->NewGlobalRef(display_surface);
    engine->glThread = std::thread(glThreadFunc, engine);

    //休眠等待GL初始化完成
    {
        std::unique_lock<std::mutex> lk(engine->mutex);
        engine->cv.wait(lk, [&]{return engine->ready || engine->exitThread;}); //内部lambda表达式为true时，才会停止等待
    }
    
    gEngine = engine;
    return gEngine->texId;
}


extern "C"
JNIEXPORT void JNICALL
Java_com_yys_jnidemo_CameraFilterNative_setInputSurface(JNIEnv *env, jobject thiz, jobject st) {
    if (!gEngine) return;
    if (gEngine->inputSurfaceTexture) {
        env->DeleteGlobalRef(gEngine->inputSurfaceTexture);
    }
    gEngine->inputSurfaceTexture = env->NewGlobalRef(st);
    jclass cls = env->GetObjectClass(st);
    gEngine->updateTexImage = env->GetMethodID(cls, "updateTexImage","()V");
}

/**
 * 把「方向校正矩阵」和「请渲染」合并成一次 JNI 调用
 * 这里只是触发，真正的updateTexImage/滤镜绘制/swap发生在glThreadFunc 那条线程上。
 */
extern "C"
JNIEXPORT void JNICALL
Java_com_yys_jnidemo_CameraFilterNative_renderFrame(JNIEnv *env, jobject thiz, jfloatArray matrix) {

    if (!gEngine) return;

    
    {
        std::lock_guard<std::mutex> lk(gEngine->mutex);
        
        //更新方向矩阵
        if (matrix != nullptr && env->GetArrayLength(matrix) >= 16) {
            jfloat *p = env->GetFloatArrayElements(matrix, nullptr);
            if (p != nullptr) {
                memcpy(gEngine->transform, p, 16 * sizeof(float));
                gEngine->hasTransform = true;
                env->ReleaseFloatArrayElements(matrix, p, JNI_ABORT);
            }
        }

        // 渲染请求标志。与矩阵更新同处一把锁，保证两者原子生效。
        gEngine->renderRequested = true;
        
    }
    
    //唤醒等待的gl线程
    gEngine->cv.notify_all();
    
}
extern "C"
JNIEXPORT void JNICALL
Java_com_yys_jnidemo_CameraFilterNative_destroyGLContext(JNIEnv *env, jobject thiz) {
    if (!gEngine) return;
    {
        std::unique_lock<std::mutex> lk(gEngine->mutex);
        gEngine->exitThread = true;
        gEngine->renderRequested = true;
    }
    gEngine->cv.notify_all();  //这里让渲染线程退出循环

    if (gEngine->glThread.joinable()) gEngine->glThread.join();  //这里类似于协程的概念，主线程会等待glthread线程退出

    if (gEngine->inputSurfaceTexture) {
        env->DeleteGlobalRef(gEngine->inputSurfaceTexture);
    }

    if (gEngine->displaySurface){
        env->DeleteGlobalRef(gEngine->displaySurface);
    }
    
    delete gEngine;
    gEngine = nullptr;
    
}
extern "C"
JNIEXPORT void JNICALL
Java_com_yys_jnidemo_CameraFilterNative_setFilterType(JNIEnv *env, jobject thiz, jint filter_type) {
    if (!gEngine) return;
    gEngine->filter.store(filter_type);
}