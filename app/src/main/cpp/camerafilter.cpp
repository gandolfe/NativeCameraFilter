//
// Created by YYANG219 on 2026/10/22.
//

#include <jni.h>
#include <android/log.h>
#include <cstdint>
#include <mutex>
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <android/hardware_buffer.h>

static std::mutex gWindowMutex;
static ANativeWindow *gWindow = nullptr;

// RGBA_8888 一个像素 4 个字节：R、G、B、A
static constexpr int kPixelBytes = 4;

#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "CameraFilterNative", __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "CameraFilterNative", __VA_ARGS__)


/**
 * 灰度处理
 * @param r 
 * @param g 
 * @param b 
 * @param out 
 */
static void ApplyGray(uint8_t r, uint8_t g, uint8_t b, uint8_t *out){
    // 灰度公式：人眼对绿色最敏感
    uint8_t gray =static_cast<uint8_t>(r * 0.299f + g * 0.587f + b * 0.114f);
    out[0] = gray;
    out[1] = gray;
    out[2] = gray;
    out[3] = 255; //不透明度最大
}

/**
 * 反色处理
 * @param r 
 * @param g 
 * @param b 
 * @param out 
 */
 static void ApplyInvert(uint8_t r, uint8_t g, uint8_t b, uint8_t *out){
    out[0] = static_cast<uint8_t>(255 - r);
    out[1] = static_cast<uint8_t>(255 - r);
    out[2] = static_cast<uint8_t>(255 - r);
    out[3] = 255;
}

/**
 * 怀旧滤镜
 * @param r 
 * @param g 
 * @param b 
 * @param out 
 */
static void ApplyOld(uint8_t r, uint8_t g, uint8_t b, uint8_t *out){
    //怀旧的色彩矩阵是固定的
    int tr = static_cast<int>(r * 0.393f + g * 0.769f + b * 0.189f);
    int tg = static_cast<int>(r * 0.349f + g * 0.686f + b * 0.168f);
    int tb = static_cast<int>(r * 0.272f + g * 0.534f + b * 0.131f);
    out[0] = static_cast<uint8_t>(tr > 255 ? 255 : tr);
    out[1] = static_cast<uint8_t>(tg > 255 ? 255 : tg);
    out[2] = static_cast<uint8_t>(tb > 255 ? 255 : tb);
    out[3] = 255;
}



/**
 * 暖色滤镜
 * @param r 
 * @param g 
 * @param b 
 * @param out 
 */
static void ApplyWarm(uint8_t r, uint8_t g, uint8_t b, uint8_t *out){
    // 暖色：红多一点、蓝少一点
    int wr = r + 30;
    int wb = b - 30;
    out[0] = static_cast<uint8_t>(wr > 255 ? 255 : wr);
    out[1] = static_cast<uint8_t>(g);
    out[2] = static_cast<uint8_t >(wb < 0 ? 0 : wb);
    out[3] = 255;
}


extern "C"
JNIEXPORT void JNICALL
Java_com_yys_jnidemo_CameraFilterNative_setSurface(JNIEnv *env, jobject thiz, jobject surface) {
    std::lock_guard<std::mutex> lock(gWindowMutex);
    //如果之前有窗口，需要先释放
    if (gWindow != nullptr) {
        ANativeWindow_release(gWindow);
        gWindow = nullptr;
    }
    
    // 把Java的Surface转换成native的窗口类型
    gWindow = ANativeWindow_fromSurface(env, surface);
    
}

extern "C"
JNIEXPORT void JNICALL
Java_com_yys_jnidemo_CameraFilterNative_releaseSurface(JNIEnv *env, jobject thiz) {
    std::lock_guard<std::mutex> lock(gWindowMutex);
    if (gWindow != nullptr){
        ANativeWindow_release(gWindow);
        gWindow = nullptr;
    }
}
extern "C"
JNIEXPORT void JNICALL
Java_com_yys_jnidemo_CameraFilterNative_processFrame(JNIEnv *env, jobject thiz, jobject buffer,
                                                     jint src_cols, jint src_rows, jint row_stride,
                                                     jint capacity, jint rotation,
                                                     jint filter_type) {
    std::lock_guard<std::mutex> lock(gWindowMutex);
    
    if (gWindow == nullptr) {
        LOGE("do not exist window surface");
        return; //还没有拿到屏幕窗口，这一帧跳过
    }
    
    uint8_t *src = static_cast<uint8_t *>(env->GetDirectBufferAddress(buffer));
    if (src == nullptr) {
        LOGE("相机缓存内存获取失败");
        return;
    }

    // 旋转 90/270 度时，宽高互换
    const bool swapped = (rotation == 90 || rotation == 270);
    const int rotCols = swapped? src_rows: src_cols; //旋转后一行有多少列
    const int rotRows = swapped? src_cols: src_rows; //旋转后总共有多少行
    
    //锁定屏幕窗口
    ANativeWindow_Buffer buf;
    if (ANativeWindow_lock(gWindow, &buf, nullptr) != 0) {
        LOGE("获取窗口buffer失败");
        return; //获取窗口buffer失败，已经被回收
    }
    if (buf.bits == nullptr){
        LOGE("获取窗口bits失败");
        ANativeWindow_unlockAndPost(gWindow);
        return;
    }

    // 窗口像素格式必须是 4 字节的 RGBA/RGBX，否则我们按 RGBA 写会写坏缓冲
    int destBytesPerPixel = 0; //每个像素占的字节数
    if(buf.format == AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM ||
            buf.format == AHARDWAREBUFFER_FORMAT_R8G8B8X8_UNORM){
        destBytesPerPixel = 4;
    } else {
        LOGE("window buffer 格式不支持！");
        ANativeWindow_unlockAndPost(gWindow);
        return;
    }
    
    const int destW = buf.width;
    const int destH = buf.height;
    const int destStride = buf.stride * destBytesPerPixel; //窗口的stride是以像素计，需要换算成字节
    uint8_t *destBase = static_cast<uint8_t *>(buf.bits);  //获取窗口buffer的内存指针

    for (int y = 0; y < destH; y++) {
        //由于输入的图像缓存，和显示的window大小可能不一样，所以需要缩放,用在图像缓存矩阵
        int ry = y * rotRows/destH;
        uint8_t *destRow = destBase + static_cast<size_t>(y) * destStride;  //size_t表示系统位数一直的无符号整数（通常64位），防止符号溢出

        for (int x = 0; x < destW; x++) {
            int rx = x * rotCols / destW;
            
            //由“旋转后的坐标”反推回“源缓冲区里的坐标”
            //是比较经典的“坐标系旋转逆映射”算法
            int sx = rx;
            int sy = ry;
            switch (rotation) {
                case 90:
                    sx = ry;
                    sy = src_rows - 1 - rx;
                    break;
                case 180:
                    sx = src_cols - 1 - rx;
                    sy = src_rows - 1 - ry;
                    break;
                case 270:
                    sx = src_cols - 1 - ry;
                    sy = rx;
                    break;
                default:
                    break;
            }
            
            //兜底检测
            if (sx <0 || sx > src_cols || sy < 0 || sy > src_rows){
                continue;
            }
            
            const uint8_t *p = src + static_cast<size_t>(sy) * row_stride + sx * kPixelBytes;
            
            uint8_t out[4];

            switch (filter_type) {
                case 1:
                    ApplyInvert(p[0], p[1], p[2],out);
                    break;
                case 2:
                    ApplyOld(p[0], p[1], p[2],out);
                    break;
                case 3:
                    ApplyWarm(p[0], p[1], p[2],out);
                    break;
                default:
                    ApplyGray(p[0], p[1], p[2],out);
                    break;
            }
            
            uint8_t *desP = destRow + x * destBytesPerPixel;
            desP[0] = out[0];
            desP[1] = out[1];
            desP[2] = out[2];
            desP[3] = out[3];
            
        }
    }

    //解锁并提交缓冲，让这一帧显示出来
    ANativeWindow_unlockAndPost(gWindow);
    
}



