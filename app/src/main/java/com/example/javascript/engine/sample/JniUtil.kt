package com.example.javascript.engine.sample

object JniUtil {

  external fun createEngine()
  external fun destroyEngine()

  external fun loadScript(content: String)
  external fun callJsFunctionSync(): Any
  external fun callJsFunction(callback: JSCallback)

  external fun callNativeModule(): Any
}