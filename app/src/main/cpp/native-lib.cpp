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

jobject _callback;
JNIEnv* _env;

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

class HostFunctionMetadata {
public:
    static void initialize(JSContextRef ctx, JSObjectRef object) {
        logd("initialize called");
    }

    static JSValueRef call(
            JSContextRef ctx,
            JSObjectRef function,
            JSObjectRef thisObject,
            size_t argumentCount,
            const JSValueRef arguments[],
            JSValueRef* exception) {
        logd("function called, argument count %d", argumentCount);

        JSStringRef jsStringRef = JSValueToStringCopy(_ctx, arguments[0], NULL);
        char* params = JSStringToCString(jsStringRef);

        jclass clazz = _env->FindClass("com/example/javascript/engine/sample/JSCallback");
        jmethodID methodId = _env->GetMethodID(clazz, "onResult", "(Ljava/lang/Object;)V");
        _env->CallVoidMethod(_callback, methodId, _env->NewStringUTF(params));

        return JSValueMakeUndefined(ctx);
    }

    static void finalize(JSObjectRef object) {
        logd("finalized called");
    }

    static JSObjectRef callAsConstructor(
            JSContextRef ctx,
            JSObjectRef constructor,
            size_t argumentCount,
            const JSValueRef arguments[],
            JSValueRef* exception
    ) {
        logd("callAsConstructor called");
        return JSValueToObject(_ctx, JSValueMakeUndefined(_ctx), NULL);
    }
};

JSValueRef call(
        JSContextRef ctx,
        JSObjectRef function,
        JSObjectRef thisObject,
        size_t argumentCount,
        const JSValueRef arguments[],
        JSValueRef* exception) {

    jclass clazz = _env->FindClass("com/example/javascript/engine/sample/JSCallback");
    jmethodID methodId = _env->GetMethodID(clazz, "invoke", "(Ljava/lang/Object;)V");


//    JSValueRef a = arguments[0];
//    JSType type = JSValueGetType(_ctx, a);
    logd("type is %d", argumentCount);
//    JSStringRef jsStringRef = JSValueToStringCopy(_ctx, arguments[0], NULL);
//    char* params = JSStringToCString(jsStringRef);
//    logd("argument length: %s", params);

    return JSValueMakeUndefined(ctx);
}

extern "C"
JNIEXPORT void JNICALL
Java_com_example_javascript_engine_sample_JniUtil_callJsFunction(JNIEnv *env, jobject thiz,
        jobject callback) {
    _env = env;
    _callback = callback;

    // 从Context中获取函数对象
    JSStringRef funcName = JSStringCreateWithUTF8CString("welcomeWithCallback");
    JSObjectRef globalObj = JSContextGetGlobalObject(_ctx);
    JSValueRef func = JSObjectGetProperty(_ctx, globalObj, funcName, NULL);
    JSStringRelease(funcName);

    // 转换为函数对象
    JSObjectRef funcObject = JSValueToObject(_ctx, func, NULL);

    // 注入回调函数
    JSClassDefinition functionClass = kJSClassDefinitionEmpty;
    functionClass.version = 0;
    functionClass.attributes = kJSClassAttributeNoAutomaticPrototype;
    functionClass.callAsFunction = HostFunctionMetadata::call;

    JSClassRef hostFunctionClass = JSClassCreate(&functionClass);

    HostFunctionMetadata metadata;
    JSObjectRef funcRef = JSObjectMake(_ctx, hostFunctionClass, &metadata);

    // 调用函数
    JSValueRef args[1];
    args[0] = funcRef;
    JSObjectCallAsFunction(_ctx, funcObject, NULL, 1, args, NULL);
}