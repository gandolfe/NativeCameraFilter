package com.yys.jnidemo.ui

import android.Manifest
import android.content.Context
import android.content.pm.PackageManager
import android.graphics.PixelFormat
import android.graphics.SurfaceTexture
import android.util.Log
import android.view.Surface
import android.view.SurfaceHolder
import android.view.SurfaceView
import android.view.WindowManager
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.camera.core.CameraSelector
import androidx.camera.core.ImageAnalysis
import androidx.camera.core.ImageProxy
import androidx.camera.core.Preview
import androidx.camera.core.resolutionselector.ResolutionSelector
import androidx.camera.core.resolutionselector.ResolutionStrategy
import androidx.camera.lifecycle.ProcessCameraProvider
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.aspectRatio
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Button
import androidx.compose.material3.FilterChip
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.unit.dp
import androidx.compose.ui.viewinterop.AndroidView
import androidx.core.content.ContextCompat
import androidx.lifecycle.LifecycleOwner
import androidx.lifecycle.compose.LocalLifecycleOwner
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import androidx.lifecycle.viewmodel.compose.viewModel
import coil.size.Size
import com.yys.jnidemo.CameraFilterNative
import com.yys.jnidemo.CameraViewModel


private val FILTERS = listOf(
    "置灰" to 0,
    "反色" to 1,
    "怀旧" to 2,
    "暖色" to 3
)

private val TAG = "CameraFilterScreen"

@Composable
fun CameraFilterScreen(viewModel: CameraViewModel = viewModel()){
    
    val context = LocalContext.current

    val lifecycleOwner = LocalLifecycleOwner.current
    
    val grantedState = viewModel.granted.collectAsStateWithLifecycle()
    
    val filterState = viewModel.filterType.collectAsStateWithLifecycle()
    
    val rationState = viewModel.previewAspect.collectAsStateWithLifecycle()

    // 复用同一个数组接收 SurfaceTexture 的变换矩阵，避免每帧分配
    val transformMatrix = remember { FloatArray(16) }

            
    //运行时申请相机权限
    val permissionLauncher = rememberLauncherForActivityResult(
        ActivityResultContracts.RequestPermission()
    ) {granted ->
        if (granted){
            Log.i(TAG,"granted camera permission!")
            viewModel.setGranted(true)
        } else{
            Log.w(TAG, "camera permission granted fail!")
        }
        
    }
    
    // 第一次进入页面的检查
    LaunchedEffect(Unit) {
        val granted = ContextCompat.checkSelfPermission(
            context, Manifest.permission.CAMERA
        ) == PackageManager.PERMISSION_GRANTED
        
        if (granted){
            Log.i(TAG,"has camera permission")
            viewModel.setGranted(true)
        } else {
            viewModel.setGranted(false)
            Log.i(TAG,"start to get camera permission ")
            permissionLauncher.launch(Manifest.permission.CAMERA)
        }
    }

    Column { 
        
        Spacer(modifier = Modifier.height(30.dp))
        
        if (grantedState.value){
            
            Text("实时滤镜（GL ES）",
                style = MaterialTheme.typography.titleLarge,
                modifier = Modifier.align(Alignment.CenterHorizontally))
            
            Spacer(modifier = Modifier.height(12.dp))
            
            AndroidView(
                modifier = Modifier
                    .fillMaxWidth()
                    // 后置摄像头在竖屏下画面为 3:4，按真实比例显示避免拉伸
                    .aspectRatio(3f / 4f)
                    .clip(RoundedCornerShape(12.dp)),
                factory = {ctx ->
                    var sf: SurfaceTexture? = null
                    SurfaceView(ctx).apply { 
                        holder.setFormat(PixelFormat.RGBA_8888)
                        holder.addCallback(object : SurfaceHolder.Callback{
                            override fun surfaceCreated(p0: SurfaceHolder) {
                                //根据显示的surface创建OES纹理，并反馈对应id
                                val texId = CameraFilterNative.createGLContext(p0.surface)
                                if (texId > 0){

                                    //根据OES纹理id创建SurfaceTexture，它是链接生产者（Camera）和消费者(OES纹理)的桥梁
                                    val surfaceTexture = SurfaceTexture(texId)
                                    
                                    //把surfaceTexture设置到native,并在底层保持它的引用
                                    CameraFilterNative.setInputSurface(surfaceTexture)

                                    surfaceTexture.setOnFrameAvailableListener {
                                        surfaceTexture.getTransformMatrix(transformMatrix)
                                        CameraFilterNative.renderFrame(transformMatrix)
                                    }
                                    
                                    val inputSurface = Surface(surfaceTexture)
                                    sf = surfaceTexture
                                    
                                    bindCameraUseCase(context, lifecycleOwner, inputSurface, surfaceTexture)
                                    
                                } else {
                                    Log.w(TAG, "EGL 初始化失败！")
                                }
                                
                            }

                            override fun surfaceDestroyed(p0: SurfaceHolder) {
                                sf?.setOnFrameAvailableListener (null)
                                sf = null
                                CameraFilterNative.destroyGLContext()
                            }
                            
                            override fun surfaceChanged(
                                p0: SurfaceHolder,
                                p1: Int,
                                p2: Int,
                                p3: Int
                            ) {
                                
                            }
                        })
                    }
                    
                }
            )
        } else {
            Column(
                modifier = Modifier.fillMaxWidth(),
                horizontalAlignment = Alignment.CenterHorizontally
            ) {
                Text("需要相机授权")

                Spacer(modifier = Modifier.height(12.dp))

                Button(onClick = { permissionLauncher.launch(Manifest.permission.CAMERA) }) {
                    "申请授权"
                }
            }
        }
        
        Spacer(modifier = Modifier.height(20.dp))

        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.spacedBy(8.dp)
        ) { 
            FILTERS.forEach {(label, type) ->
                FilterChip(
                    selected = filterState.value == type,
                    onClick = {viewModel.setFilterType(type)},
                    label = { Text(text = label)},
                )
            }
        }
    }
    
    
}



fun bindCameraUseCase(context: android.content.Context,
                      lifecycleOwner: LifecycleOwner,
                      inputSurface: Surface,
                      inputSurfaceTexture: SurfaceTexture) {
    val future = ProcessCameraProvider.getInstance(context)
    future.addListener({
        val provider = future.get()
        
        val selector = ResolutionSelector.Builder()
            .setResolutionStrategy(
                ResolutionStrategy(
                    android.util.Size(1440, 1920),
                    ResolutionStrategy.FALLBACK_RULE_CLOSEST_LOWER_THEN_HIGHER
                )
            ).build()
        
        val preview = Preview.Builder()
            .setResolutionSelector(selector)
            .build()
            .apply {
                // 让相机输出方向与屏幕方向一致（否则竖屏下画面会躺倒）
                val rotation = (context.getSystemService(Context.WINDOW_SERVICE) as WindowManager)
                    .defaultDisplay.rotation
                targetRotation = rotation
            }
        
        //把SurfaceTexture包装过的Surface提供给Preview，用作相机画面的输出buffer
        //通过这一步，可以让相机的画面直接写入OES纹理，零拷贝
        preview.setSurfaceProvider{request ->
            inputSurfaceTexture.setDefaultBufferSize(
                request.resolution.width,
                request.resolution.height
            )
            request.provideSurface(inputSurface, ContextCompat.getMainExecutor(context)){
                inputSurfaceTexture.release()
                inputSurface.release()
            }
        }
        
        provider.unbindAll()
        provider.bindToLifecycle(
            lifecycleOwner, 
            CameraSelector.DEFAULT_BACK_CAMERA, 
            preview)

    }, ContextCompat.getMainExecutor(context))
    
}