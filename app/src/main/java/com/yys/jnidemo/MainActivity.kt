package com.yys.jnidemo

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import com.yys.jnidemo.ui.GrayscaleScreen
import com.yys.jnidemo.ui.theme.JniDemoTheme

class MainActivity : ComponentActivity() {


    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()
        setContent {
            JniDemoTheme{
                GrayscaleScreen()
            }
        }

    }

    /**
     * A native method that is implemented by the 'jnidemo' native library,
     * which is packaged with this application.
     */
    external fun stringFromJNI(): String

    companion object {
        // Used to load the 'jnidemo' library on application startup.
        init {
            System.loadLibrary("jnidemo")
        }
    }
}