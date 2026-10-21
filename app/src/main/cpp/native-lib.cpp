#include <jni.h>
#include <string>
#include <android/bitmap.h>
#include <android/log.h>
#include <cstdint>


#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "GrayScaleNative", __VA_ARGS__)

extern "C"
JNIEXPORT jboolean JNICALL
Java_com_yys_jnidemo_HandleNative_toGray(JNIEnv *env, jobject thiz, jobject src, jobject dest) {
    //获取图片信息
    AndroidBitmapInfo srcInfo; //创建栈内对象
    AndroidBitmapInfo destInfo;
    if (AndroidBitmap_getInfo(env, src, &srcInfo) < 0 ||
            AndroidBitmap_getInfo(env, dest, &destInfo) < 0){
        LOGE("无法读取 Bitmap 信息");
        return JNI_FALSE;
    }
    // ARGB_8888是图片的默认内存格式
    // HARDWARE格式位图是不能处理灰度的
    if (srcInfo.format != ANDROID_BITMAP_FORMAT_RGBA_8888 ||
        destInfo.format != ANDROID_BITMAP_FORMAT_RGBA_8888){
        LOGE("Bitmap格式不对");
        return JNI_FALSE;
    }

    //对比原图片和目标图片的大小，必须一致
    if (srcInfo.width != destInfo.width || srcInfo.height != destInfo.height){
        LOGE("目标图片和原图片大小不一致！");
        return JNI_FALSE;
    }

    // 锁定像素，拿到图像内存指针
    // lock 之后，stride 表示“一行的字节数”（有时会比 width*4 多一点，属于正常对齐）。
    // void* 属于泛型指针，后面具体的运算时需要强转
    void *srcPixels = nullptr;
    void *destPixels = nullptr;
    if (AndroidBitmap_lockPixels(env, src, &srcPixels) < 0){
        LOGE("源图片锁定像素失败！");
        return JNI_FALSE;
    }
    if (AndroidBitmap_lockPixels(env, dest, &destPixels) < 0){
        LOGE("目标图片锁定像素失败！");
        return JNI_FALSE;
    }

    const int width = srcInfo.width;
    const int height = srcInfo.height;
    const int srcStride = srcInfo.stride;
    const int destStride = destInfo.stride;

    // 把void*转换为按字节寻址的指针，方便一个个字节第读写
    uint8_t *srcBase = static_cast<uint8_t *>(srcPixels);
    uint8_t *destBase = static_cast<uint8_t *>(destPixels);

    //利用双重循环，逐像素处理
    for (int y = 0; y < height; y++) {
        //算出图片第y行在内存中的起始位置
        const uint8_t *srcRow = srcBase + y * srcStride;
        uint8_t *destRow = destBase + y * destStride;

        for (int x = 0; x < width; x++) {
            //每个像素4个字节，R、G、B、A
            // srcRow并不是数组，这里下标取值只是语法糖
            const uint8_t r = srcRow[x * 4 + 0];
            const uint8_t g = srcRow[x * 4 + 1];
            const uint8_t b = srcRow[x * 4 + 2];

            //计算灰度值
            const uint8_t gray = static_cast<uint8_t>(r * 0.299f + g * 0.587f + b * 0.114f);

            //把目的图片内存中原来的色彩值都换成灰度值
            destRow[x * 4 + 0] = gray;
            destRow[x * 4 + 1] = gray;
            destRow[x * 4 + 2] = gray;
            destRow[x * 4 + 3] = srcRow[x * 4 + 3];
        }
    }

    //解锁像素，务必跟AndroidBitmap_lockPixels成对出现
    if (AndroidBitmap_unlockPixels(env, src) < 0 ||
            AndroidBitmap_unlockPixels(env, dest) < 0) {
        LOGE("图片像素解锁失败！");
        return JNI_FALSE;
    }

    return JNI_TRUE;

}