package com.yys.jnidemo.ui

import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.PickVisualMediaRequest
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.Button
import androidx.compose.material3.ElevatedCard
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.unit.dp
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import androidx.lifecycle.viewmodel.compose.viewModel
import coil.compose.AsyncImage
import com.yys.jnidemo.GrayViewModel

@Composable
fun GrayscaleScreen(viewModel: GrayViewModel = viewModel()){

    val context = LocalContext.current

    val selectedBitmap = viewModel.srcBitmap.collectAsStateWithLifecycle()

    val grayBitmap = viewModel.destBitmap.collectAsStateWithLifecycle()

    val pickLauncher =
        rememberLauncherForActivityResult(contract = ActivityResultContracts.PickVisualMedia()) { uri ->
            if (uri != null) {
                viewModel.updateSrcBitmapByUri(context, uri)
            }
        }

    Column(modifier = Modifier.padding(top = 50.dp)) {

        if (selectedBitmap.value == null) {
            ElevatedCard(
                modifier = Modifier
                    .fillMaxWidth()
                    .height(300.dp)
                    .background(Color.LightGray)
                    .padding(20.dp)
            ) {
                Box(
                    modifier = Modifier.fillMaxSize(),
                    contentAlignment = Alignment.Center
                ) {
                    Text(
                        "请选择图片！",
                        color = Color.DarkGray,
                        style = MaterialTheme.typography.bodyLarge
                    )
                }

            }
        } else {
            AsyncImage(
                model = selectedBitmap.value,
                contentDescription = "选中的图片",
                modifier = Modifier
                    .fillMaxWidth()
                    .height(300.dp),
                contentScale = ContentScale.Crop
            )
        }

        Row(
            modifier = Modifier
                .fillMaxWidth()
                .height(150.dp),
            horizontalArrangement = Arrangement.Center,
            verticalAlignment = Alignment.CenterVertically
        ) {
            Button(onClick = {
                pickLauncher.launch(
                    PickVisualMediaRequest(ActivityResultContracts.PickVisualMedia.ImageOnly)
                )
            }) {
                Text("选择图片")
            }

            Spacer(modifier = Modifier.fillMaxWidth(0.2f))

            Button(onClick = { viewModel.grayHandle() }) {
                Text("灰度处理")
            }
        }


        if (selectedBitmap.value != null) {
            AsyncImage(
                model = grayBitmap.value,
                contentDescription = "灰度处理的图片",
                modifier = Modifier
                    .fillMaxWidth()
                    .height(300.dp),
                contentScale = ContentScale.Crop
            )
        }
    }
}