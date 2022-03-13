#include <jni.h>
#include <string>
#include <android/log.h>

#ifndef LOG_TAG
#define LOG_TAG "JniUtil"
#define logd(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#endif

#include <JavaScriptCore/JavaScript.h>

char* jstringToCString(JNIEnv *env, jstring content) {
    jsize length = env->GetStringUTFLength(content);
    char* cstring = (char*) malloc(sizeof(char) * (length + 1));
    env->GetStringUTFRegion(content, 0, length, cstring);
    return cstring;
}

char* JSStringToCString(JSStringRef jsString) {
    size_t maxBufferSize = JSStringGetMaximumUTF8CStringSize(jsString);
    char* utf8Buffer = new char[maxBufferSize];
    JSStringGetUTF8CString(jsString, utf8Buffer, maxBufferSize);
    return utf8Buffer;
}

JSGlobalContextRef _ctx;

extern "C"
JNIEXPORT void JNICALL
Java_com_example_javascript_engine_sample_JniUtil_createEngine(JNIEnv *env, jobject thiz) {
    _ctx = JSGlobalContextCreate(NULL);
}

extern "C"
JNIEXPORT void JNICALL
Java_com_example_javascript_engine_sample_JniUtil_destroyEngine(JNIEnv *env, jobject thiz) {
    JSGlobalContextRelease(_ctx);
    _ctx = nullptr;
}

extern "C"
JNIEXPORT void JNICALL
Java_com_example_javascript_engine_sample_JniUtil_loadScript(
        JNIEnv *env,
        jobject thiz,
        jstring content
) {
    JSStringRef statement = JSStringCreateWithUTF8CString(jstringToCString(env, content));
    JSValueRef jsRef = JSEvaluateScript(_ctx, statement, nullptr, nullptr, 1, nullptr);
    JSStringRelease(statement);
    logd("success evaluate script.");

    if (JSValueIsNumber(_ctx, jsRef)) {
        double ret = JSValueToNumber(_ctx, jsRef, NULL);
        logd("script evalute result: %lf", ret);
    }
}

extern "C"
JNIEXPORT jobject JNICALL
Java_com_example_javascript_engine_sample_JniUtil_callJsFunctionSync(
        JNIEnv *env,
        jobject thiz
) {
    // 从Context中获取函数对象
    JSStringRef funcName = JSStringCreateWithUTF8CString("welcome");
    JSObjectRef globalObj = JSContextGetGlobalObject(_ctx);
    JSValueRef func = JSObjectGetProperty(_ctx, globalObj, funcName, NULL);
    JSStringRelease(funcName);

    // 转换为函数对象
    JSObjectRef funcObject = JSValueToObject(_ctx, func, NULL);
    // 调用函数
    JSValueRef ret = JSObjectCallAsFunction(_ctx, funcObject, NULL, 0, NULL, NULL);
    //处理返回值
    JSStringRef jsStringRef = JSValueToStringCopy(_ctx, ret, NULL);
    char* cstring = JSStringToCString(jsStringRef);
    JSStringRelease(jsStringRef);
    logd("result: %s", cstring);
    jstring jString = env->NewStringUTF(cstring);
    free(cstring);
    return jString;
}

extern "C"
JNIEXPORT void JNICALL
Java_com_example_javascript_engine_sample_JniUtil_callJsFunction(JNIEnv *env, jobject thiz,
        jobject callback) {
    logd("call js function with callback");
}