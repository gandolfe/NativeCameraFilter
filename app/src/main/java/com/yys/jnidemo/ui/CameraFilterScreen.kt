package com.yys.jnidemo.ui

import android.Manifest
import android.content.pm.PackageManager
import android.graphics.PixelFormat
import android.util.Log
import android.view.SurfaceHolder
import android.view.SurfaceView
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.camera.core.CameraSelector
import androidx.camera.core.ImageAnalysis
import androidx.camera.core.ImageProxy
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
import androidx.compose.material3.Button
import androidx.compose.material3.FilterChip
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
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

    // 目标分辨率：尽量拿到 640x480 的小画面，CPU 处理才跟得上实时
    val resolutionSelector = remember {
        ResolutionSelector.Builder()
            .setResolutionStrategy(
                ResolutionStrategy(
                    android.util.Size(640, 480),
                    ResolutionStrategy.FALLBACK_RULE_CLOSEST_LOWER_THEN_HIGHER
                )
            )
            .build()
    }

    // 图像分析器：跟随页面的“组合生命周期”，离开页面时自动释放
    val imageAnalysis = remember {
        ImageAnalysis.Builder()
            .setOutputImageFormat(ImageAnalysis.OUTPUT_IMAGE_FORMAT_RGBA_8888)
            .setBackpressureStrategy(ImageAnalysis.STRATEGY_KEEP_ONLY_LATEST)
            .setResolutionSelector(resolutionSelector)
            .build()
            .also { analysis ->
                analysis.setAnalyzer(viewModel.getAnalyzerExecutor()){imageProxy ->
                    try {
                        analyzeAndRender(imageProxy, viewModel)
                    }finally {
                        imageProxy.close()
                    }
                }
            }
    }
            
    //运行时申请相机权限
    val permissionLauncher = rememberLauncherForActivityResult(
        ActivityResultContracts.RequestPermission()
    ) {granted ->
        if (granted){
            Log.i(TAG,"granted camera permission!")
            bindCamera(context, lifecycleOwner, imageAnalysis)
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
            bindCamera(context, lifecycleOwner, imageAnalysis)
            viewModel.setGranted(true)
        } else {
            viewModel.setGranted(false)
            Log.i(TAG,"start to get camera permission ")
            permissionLauncher.launch(Manifest.permission.CAMERA)
        }
    }
    

    Column { 
        if (grantedState.value){
            AndroidView(
                modifier = Modifier.fillMaxWidth().aspectRatio(rationState.value),
                factory = {ctx ->
                    SurfaceView(ctx).apply { 
                        holder.setFormat(PixelFormat.RGBA_8888)
                        holder.addCallback(object : SurfaceHolder.Callback{
                            override fun surfaceCreated(p0: SurfaceHolder) {
                                CameraFilterNative.setSurface(p0.surface)
                            }

                            override fun surfaceDestroyed(p0: SurfaceHolder) {
                                CameraFilterNative.releaseSurface()
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

fun bindCamera(
    context: android.content.Context,
    lifecycleOwner: LifecycleOwner, 
    imageAnalysis: ImageAnalysis){
    
    val future = ProcessCameraProvider.getInstance(context)
    future.addListener({
        val provider = future.get()
        provider.unbindAll()
        provider.bindToLifecycle(lifecycleOwner, CameraSelector.DEFAULT_BACK_CAMERA, imageAnalysis)
    }, ContextCompat.getMainExecutor(context))
    
}

fun analyzeAndRender(image: ImageProxy, viewModel: CameraViewModel){
    val plane = image.planes[0]  //由于获取的Camera图像的格式是RGBA_8888，所以只有一个平面，就包含了所有图像信息
    val buffer = plane.buffer
    val rowStride = plane.rowStride      // 一行占多少字节
    val pixelStride = plane.pixelStride  // 一个像素占几个字节（RGBA = 4）
    val capacity = buffer.capacity()     // 整个缓冲的字节数

    // 用缓冲自身的布局算出真实的行列数,不能用
    val srcCols = if (pixelStride > 0) rowStride / pixelStride else 0
    val srcRows = if (rowStride > 0) capacity / rowStride else 0
    if (srcCols <= 0 || srcRows <= 0) return

    // 相机给的是传感器朝向的画面（通常是横向），需要顺时针转正
    val rotation = ((image.imageInfo.rotationDegrees % 360) + 360) % 360

    // 旋转后的画面宽高比，用来决定预览区域的布局（90/270 度时宽高互换）
    val swapped = (rotation == 90 || rotation == 270)
    val rotCols = if (swapped) srcRows else srcCols
    val rotRows = if (swapped) srcCols else srcRows
    viewModel.setPreviewAspect(rotCols.toFloat() / rotRows.toFloat())

    CameraFilterNative.processFrame(
        buffer = buffer,
        srcCols = srcCols,
        srcRows = srcRows,
        rowStride = rowStride,
        capacity = capacity,
        rotation = rotation,
        filterType = viewModel.filterType.value
    )
}