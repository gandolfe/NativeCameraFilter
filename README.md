本demo通过使用NDK，利用GPU对相机的预览画面做滤镜效果
## 1.实现思路

首先在Kotlin层创建一个`SurfaceView`，用作最终预览效果的显示容器。

把SurfaceView对应的`Surface`传递给Native层。

在Native层，首先创建一个用作渲染任务子线程。在子线程中初始化```EGL```环境、编译顶点着色器和片元着色器，创建着色器程序、创建```OES```纹理。

这一步创建的EGL环境，可以让OpenGL ES直接绘制在显示的Surface上，提高了输出的效率。

把OES纹理id传回Kotlin层，并根据此id创建```SurfaceTexture```。

它是链接生产者（Camera）和消费者（OES纹理）的桥梁，可以把底层硬件（相机预览、视频解码器）产生的图像流直接转换成OpenGL ES可以使用的纹理。

SurfaceTexture会提供一个Surface，并把这个Surface提供给相机预览```Preview```。

Preview会直接往这个缓存中写入相机原始图像数据，并把此内存数据直接映射成OES纹理，交给GPU线程处理。

总体来看，就是相机预览往SurfaceTexture对应的buffer写数据，并通过updateTexIamge直接转换成纹理，丢给OpenGL ES的着色器做滤镜画面的绘制，完成后直接提交到SF做合成。


## 2.数据流程
相机预览数据写入SurfaceTexture ->获取本帧的旋转变换矩阵 ->通知GL线程渲染 

-> GL线程updateTexImage读取OES纹理 ->调用着色器做滤镜绘制、旋转纠纠偏

->eglSwapBuffers完成图像提交

## 3.QA：
- **使用GPU方式做滤镜的优势：**

相机预览数据直接写入SurfaceTexture缓存，并且可以可以在一瞬间完成纹理的转换，没有内存的拷贝。

同时EGL打通了OpenGL ES和Android系统窗口（ANativeWindow），在完成渲染后，可以直接输出到屏幕。

全程不会有CPU的内存读取，不会有CPU的像素处理，对内存带宽和CPU压力都是非常好的减压。

- **使用OpenGL ES处理滤镜，相机UseCase为什么不使用ImageAnalysis？**

因为ImageAnalysis的设计初衷是为了面向CPU处理的场景（比如人脸识别、二维码扫描，或者低帧率的遍历像素点），它的图像缓存是在系统内存（CPU访问）。

如果这里使用ImageAnalysis，就会存在内存的拷贝，性能极差。

而Preview的设计理念不是“把数据拿给CPU计算”，而是“把相机画面渲染/填充到一个Surface上”，这恰好契合了OES纹理+SurfaceTexture内存零拷贝的需求。
