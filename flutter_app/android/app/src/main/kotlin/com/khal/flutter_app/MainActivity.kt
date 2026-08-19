package com.khal.flutter_app

import io.flutter.embedding.android.FlutterActivity

class MainActivity : FlutterActivity() {
    companion object {
        init {
            System.loadLibrary("kchess_core")
        }
    }
}
