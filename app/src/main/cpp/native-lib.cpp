#include <jni.h>
#include <string>

#include "JavaScriptCore/JavaScript.h"

std::string JSStringToStdString(JSStringRef jsString) {
    size_t maxBufferSize = JSStringGetMaximumUTF8CStringSize(jsString);
    char* utf8Buffer = new char[maxBufferSize];
    size_t bytesWritten = JSStringGetUTF8CString(jsString, utf8Buffer, maxBufferSize);
    std::string utf_string = std::string(utf8Buffer, bytesWritten - 1);
    delete [] utf8Buffer;
    return utf_string;
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_example_javascript_engine_sample_MainActivity_stringFromJNI(
        JNIEnv* env,
        jobject /* this */) {
    JSContextGroupRef contextGroup = JSContextGroupCreate();
    JSGlobalContextRef globalContext = JSGlobalContextCreateInGroup(contextGroup, nullptr);

    JSStringRef statement = JSStringCreateWithUTF8CString(
            "function degToRad(value) {return (value * Math.PI) / 180; } ('90 deg = ' + degToRad(90))");

    JSValueRef retValue = JSEvaluateScript(globalContext, statement, nullptr, nullptr, 1, nullptr);

    JSStringRef retString = JSValueToStringCopy(globalContext, retValue, nullptr);

    std::string hello = JSStringToStdString(retString);

    JSGlobalContextRelease(globalContext);
    JSContextGroupRelease(contextGroup);
    JSStringRelease(statement);
    JSStringRelease(retString);

    return env->NewStringUTF(hello.c_str());
}
