
本demo通过使用NDK，对相机的预览画面做图像滤镜效果，用```C++```实现。

### 1.实现思路：
在Kotlin层创建一个```SurfaceView```，充当画布，用来做滤镜处理后的画面显示。

在Kotlin层使用```CameraX```的API，创建一个```ImageAnalysis```的```UseCase```，并和本界面的```CameraProvider```绑定，持续不断提供原始图片帧。

Kotlin层需要把创建的SurfaceView对应的Surface传递给Native层，Native层会据此创建一个```ANativeWindow```。

ImageAnalysis开始回调图片帧之后，需要把图片缓存帧对应的内存地址、图像宽高、每行占用字节数、每像素占用字节数等信息，传递给Native层。

Native层会直接读取图片缓存内存，并根据ANativeWindow的宽高，还原映射到对应的图片像素点，对像素点做```R、G、B```的数值处理后写入到ANativeWindow对应的buffer中。

在对显示buffer完成遍历和赋值后，最后直接在Native层调提交缓存并把这一帧显示出来。

### 2.数据流程：
Koltin层创建SurfaceView -> JNI传递Surface ->Native层创建ANativeWindow 

-> Koltin创建ImageAnalysis获取图片帧 ->JNI传递内存地址 ->Native层读取图片内存 

->Native层遍历ANativeWindow buffer -> 缩放和旋转逆映射 ->获取到图片缓存对应像素

->操作像素RGB值完成滤镜修改 -> 把像素值写入ANativeWindow buffer -> 完成buffer遍历，提交buffer显示。

### 3.QA：
#### 1.可以直接在Native层对Camera操作获取画面，而不经过Kotlin层吗？

NDK有支持直接操作Camera的API（Camera2 NDK），理论上开销更小；

但是没有```Jetpack CameraX```的支持，需要面对繁琐的API，以及不同厂商兼容性问题。

同时还需要JNI传递生命周期变化，在Native处理。

#### 2.可以直接在Native创建ANativeWindow吗？

不可以，NDK没有提供直接创建```ANativeWindow```的接口。还是需要在Kotlin层创建好```Surface```之后，传递给Native层。

#### 3.使用双重循环读取相机缓冲图像的每个像素，然后再填充到window的buffer中，这样效率高吗？

当前的方案，是使用```CPU```的计算资源去处理的图像，效率不高。如果是处理高分辨率的画面，```CPU```功耗很高，手机会发烫明显。

更科学的方案，是通过```OpenGL ES```去调用GPU资源，并行处理图像。







