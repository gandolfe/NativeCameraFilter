package com.yys.jnidemo

import android.graphics.Bitmap

object HandleNative {

    init {
        System.loadLibrary("jnidemo")
    }

    external fun toGray(src: Bitmap, dest: Bitmap): Boolean

}