package com.yys.jnidemo

import androidx.lifecycle.ViewModel
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import java.util.concurrent.ExecutorService
import java.util.concurrent.Executors

class CameraViewModel : ViewModel() {
    
    private var _granted = MutableStateFlow(false)

    val granted = _granted.asStateFlow()

    // 预览区的宽高比（旋转后的画面宽 / 高），由分析线程算好后回传
    private val _previewAspect = MutableStateFlow(4f / 3f)
    
    val previewAspect: StateFlow<Float> = _previewAspect.asStateFlow()
    

    private var _filterType = MutableStateFlow(0)
        set(value) {
            field = value
        }

    val filterType = _filterType.asStateFlow()
    
    private val analyzerExecutor: ExecutorService = Executors.newSingleThreadExecutor()
    
    fun getAnalyzerExecutor(): ExecutorService {
        return analyzerExecutor
    }
    
    
    fun setGranted(granted : Boolean) {
        _granted.value = granted
    }
    
    fun setFilterType(filterType: Int){
        _filterType.value = filterType
    }

    fun setPreviewAspect(aspect: Float) {
        if (_previewAspect.value != aspect) {
            _previewAspect.value = aspect
        }
    }
    

    override fun onCleared() {
        super.onCleared()
        analyzerExecutor.shutdown()
    }
    
}