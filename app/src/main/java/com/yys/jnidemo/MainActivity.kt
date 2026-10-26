package com.yys.jnidemo

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.compose.material3.NavigationBar
import androidx.compose.material3.NavigationBarItem
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import com.yys.jnidemo.ui.CameraFilterScreen
import com.yys.jnidemo.ui.GrayscaleScreen
import com.yys.jnidemo.ui.theme.JniDemoTheme

class MainActivity : ComponentActivity() {


    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()
        setContent {
            JniDemoTheme{
                MainScreen()
            }
        }

    }

    @Composable
    fun MainScreen(){

        var selectedTab by remember { mutableStateOf(0) }

        Scaffold(
            bottomBar = {
                NavigationBar {
                    NavigationBarItem(
                        label = { Text("灰度") },
                        selected = selectedTab == 0,
                        onClick = { selectedTab = 0} ,
                        icon = { }
                    )

                    NavigationBarItem(
                        label = { Text("滤镜") },
                        selected = selectedTab == 1,
                        onClick = { selectedTab = 1} ,
                        icon = { }
                    )

                }
            }
        ) {innerPadding ->
            when(selectedTab){
                0 -> GrayscaleScreen()
                else -> CameraFilterScreen()
            }

        }
    }
}