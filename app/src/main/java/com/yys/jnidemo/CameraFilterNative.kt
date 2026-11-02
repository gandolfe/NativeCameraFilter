package com.yys.jnidemo

import android.graphics.SurfaceTexture
import android.view.Surface
import java.nio.ByteBuffer

object CameraFilterNative {
    
    init {
        System.loadLibrary("jnidemo")
    }
    
    external fun setSurface(surface: Surface)
    
    external fun releaseSurface()

    /**
     * @param buffer     一帧的 RGBA 像素（DirectByteBuffer，零拷贝）
     * @param srcCols    缓冲里一行的像素数 = rowStride / pixelStride
     * @param srcRows    缓冲里完整可用的行数 = capacity / rowStride
     * @param rowStride  一行占多少字节
     * @param capacity   整个缓冲的字节数（用于越界校验）
     * @param rotation   顺时针旋转角度（0/90/180/270），把传感器画面转正
     * @param filterType 0 灰度 / 1 反色 / 2 怀旧 / 3 暖色
     */
    external fun processFrame(
        buffer: ByteBuffer,
        srcCols: Int,
        srcRows: Int,
        rowStride: Int,
        capacity: Int,
        rotation: Int,
        filterType: Int
    )

    /**
     * 根据显示组件的Surface创建EGL/OpenGL上下文，返回一个创建SurfaceTexture所需要的纹理id
     */
    external fun createGLContext(displaySurface: Surface) : Int

    /**
     * 把surfacetexture设置到native层，作为OES纹理的输入buffer存在
     */
    external fun setInputSurface(st: SurfaceTexture)

    /**
     * 触发渲染流程
     */
    external fun renderFrame(matrix: FloatArray)

    /**
     * 释放EGL/GL资源
     */
    external fun destroyGLContext()

    /**
     * 设置滤镜种类
     */
    external fun setFilterType(filterType: Int)
}