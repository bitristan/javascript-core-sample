package com.example.javascript.engine.sample

import android.os.Build
import android.util.Log

object NativeModule {

  @JvmStatic
  fun getSystemInfo(): String {
    Log.d("JniUtil", "NativeModule getSystemInfo callback")
    return Build.FINGERPRINT
  }

}