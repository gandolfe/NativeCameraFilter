package com.yys.jnidemo

import android.content.Context
import android.graphics.Bitmap
import android.graphics.BitmapFactory
import android.graphics.ImageDecoder
import android.net.Uri
import android.os.Build
import androidx.annotation.RequiresApi
import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext

class MainViewModel: ViewModel() {

    private var _srcBitmap = MutableStateFlow<Bitmap?>(null)

    var srcBitmap = _srcBitmap.asStateFlow()

    private var _destBitmap = MutableStateFlow<Bitmap?>(null)

    var destBitmap = _destBitmap.asStateFlow()

    fun updateSrcBitmapByUri(context: Context, uri: Uri){
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P){
            val source = ImageDecoder.createSource(context.contentResolver, uri)
            _srcBitmap.value = ImageDecoder.decodeBitmap(source)
        } else {
            context.contentResolver.openInputStream(uri)?.use { inputStream ->
                _srcBitmap.value = BitmapFactory.decodeStream(inputStream)
            }
        }
    }

    @RequiresApi(Build.VERSION_CODES.O)
    fun grayHandle(){
        if (_srcBitmap.value == null) {
            return
        }
        var src = _srcBitmap.value
        // 这里必须做处理，HARDWARE是硬件位图，存放在GPU中，CPU无法锁定像素
        if (src!!.config == Bitmap.Config.HARDWARE){
            src = src.copy(Bitmap.Config.ARGB_8888, true)
        }
        val dest = Bitmap.createBitmap(src!!.width, src.height, Bitmap.Config.ARGB_8888)
        viewModelScope.launch {
            withContext(Dispatchers.Default){
                HandleNative.toGray(src, dest)
            }
            _destBitmap.value = dest
        }
    }

}