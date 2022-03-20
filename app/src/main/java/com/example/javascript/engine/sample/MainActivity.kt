package com.example.javascript.engine.sample

import androidx.appcompat.app.AppCompatActivity
import android.os.Bundle
import android.util.Log
import android.view.View

class MainActivity : AppCompatActivity() {
  private var loaded = false

  override fun onCreate(savedInstanceState: Bundle?) {
    super.onCreate(savedInstanceState)
    setContentView(R.layout.activity_main)
    JniUtil.createEngine()

    findViewById<View>(R.id.loadScript).setOnClickListener {
      if (!loaded) {
        loaded = true
        JniUtil.loadScript(String(assets.open("app.js").readBytes()))
      }
    }
    findViewById<View>(R.id.callJsFunctionSync).setOnClickListener {
      val result = JniUtil.callJsFunctionSync()
      Log.d(TAG, "result: $result")
    }
    findViewById<View>(R.id.callJsFunction).setOnClickListener {
      JniUtil.callJsFunction(object : JSCallback() {
        override fun onResult(`object`: Any?) {
          Log.d(TAG, "js callback $`object`")
        }
      })
    }
    findViewById<View>(R.id.callNativeModule).setOnClickListener {
      val result = JniUtil.callNativeModule()
      Log.d(TAG, "result: $result")
    }
  }

  override fun onDestroy() {
    super.onDestroy()
    JniUtil.destroyEngine()
  }

  companion object {
    private const val TAG = "MainActivity"
    init {
      System.loadLibrary("sample")
    }
  }
}
