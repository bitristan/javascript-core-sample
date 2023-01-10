# Android平台集成并使用JavaScriptCore

## 概述

JavaScriptCore（简称JSC）是React Native默认的JS引擎。本文我们来看一下如何在安卓平台从0开始集成JSC引擎，并且实现 java 和 js 的互相调用。

## 集成

### 手动编译集成

#### 编译JSC

React 社区提供了一个 jsc-android-buildscripts 项目，专门用于构建编译供Android使用的JavaScriptCore，项目地址：
https://github.com/react-native-community/jsc-android-buildscripts

自己编译的话，clone一下工程代码，按照 README.md 中的说明安装好需要的依赖库，设置好ANDROID_HOME 和 ANDROID_NDK 环境变量，然后使用如下命令直接编译即可
```
npm run clean
npm run download
npm run start
```

需要注意的点是 NDK的版本最好使用 r19，自己编译的话时间比较长，需要耐心等待一会。

编译完成之后，在 dist 目录下有两个文件夹，一个包含jsc的头文件，一个包含打包成的aar文件，aar文件中打包了各个平台的libjsc.so

```
├── include
│   ├── JSBase.h
│   └── ...
└── org
    └── webkit
        ├── android-jsc
        │   ├── maven-metadata.xml
        │   ├── maven-metadata.xml.md5
        │   ├── maven-metadata.xml.sha1
        │   └── r250230
        │       ├── android-jsc-r250230.aar
        ├── android-jsc-cppruntime
        │   └── r250230
        │       ├── android-jsc-cppruntime-r250230.aar
        └── android-jsc-intl
            └── r250230
                ├── android-jsc-intl-r250230.aar
```
于是我们可以开始集成了。

#### 集成JSC到Android工程中

1. 新建Android工程
使用Android Studio新建Android工程，选中Native C++模板工程

2. 集成so
由于aar中的so无法直接被加载，所以我们将aar解压，将so文件放在 app/libs 目录中
```
app/libs/
├── arm64-v8a
│   └── libjsc.so
├── armeabi-v7a
│   └── libjsc.so
├── x86
│   └── libjsc.so
└── x86_64
    └── libjsc.so
```

3. 添加jsc头文件
在 cpp 目录下新建 include/JavaScriptCore 目录，将 jsc 编译生成的dist目录中的 .h 文件都拷贝进去
```
app/src/main/cpp/
├── CMakeLists.txt
├── include
│   └── JavaScriptCore
│       ├── APICallbackFunction.h
│       ├── APICast.h
│       ├── APIUtils.h
│       ├── JavaScriptCore.h
│       ├── JavaScript.h
│       ├── ...........
└── native-lib.cpp
```

4. CMakeLists.txt文件中配置jsc头文件和jsc库的引入
```
// 引入jsc头文件
include_directories(${CMAKE_SOURCE_DIR}/include)

// 导入jsc
add_library(jsc SHARED IMPORTED)
set_target_properties(
        jsc
        PROPERTIES
        IMPORTED_LOCATION
        ${CMAKE_CURRENT_SOURCE_DIR}/../../../libs/${ANDROID_ABI}/libjsc.so)

// 链接jsc到目标库
target_link_libraries(
        sample
        jsc
        ${log-lib})
```

5. 增加cmake编译参数
```
externalNativeBuild {
  cmake {
    cppFlags ''
    arguments "-DANDROID_STL=c++_shared"
  }
}
```

如果不添加的话运行会报以下错误
```
java.lang.UnsatisfiedLinkError: dlopen failed: library "libc++_shared.so" not found
```

6. 现在我们就可以使用JSC来执行js代码了，我们可以先执行一段简单的js代码小试牛刀
```
JSGlobalContextRef context = JSGlobalContextCreate(NULL);
JSStringRef statement = JSStringCreateWithUTF8CString(
  "(function add(a, b) { return a + b } (1, 2))"
);
JSValueRef jsRef = JSEvaluateScript(context, statement, nullptr, nullptr, 1, nullptr);
JSStringRelease(statement);

if (JSValueIsNumber(context, jsRef)) {
  int ret = JSValueToNumber(context, jsRef, NULL);
  logd("script evalute result: %d", ret);
}

JSGlobalContextRelease(context);
```
上述代码执行了一个add函数，从log中应该能看到输出结果 3。


### 使用已经编译好的npm库集成
事实上 npm 仓库已经有编译好的 jsc-android 库，直接集成也是可以的。直接使用yarn命令即可下载安装。
```
yarn add jsc-android

# Or if you would like to try latest version
# yarn add 'jsc-android@next'
```
下载下来之后看路径，跟我们编译出来的dist目录是一模一样的

```
$$ tree -L 1 node_modules/jsc-android/dist/

node_modules/jsc-android/dist/
├── include
└── org

2 directories, 0 files
```

接着还是可以使用上述手动集成的方法将jsc集成到我们的Android工程中。

但是大家想必也发现了这种集成方式非常落后，还需要自己手动拷贝头文件，解压aar等。显然我们可以自定义task来帮我们完成这些工作，实际上RN就是这么来做的。
我们可以添加一个自定义的gradle task，来帮我们完成解压拷贝等工作，这里贴一下RN源码中的task代码

```
task prepareJSC {
    doLast {
        def jscPackagePath = findNodeModulePath(projectDir, "jsc-android")
        if (!jscPackagePath) {
            throw new GradleScriptException("Could not find the jsc-android npm package", null)
        }

        def jscDist = file("$jscPackagePath/dist")
        if (!jscDist.exists()) {
            throw new GradleScriptException("The jsc-android npm package is missing its \"dist\" directory", null)
        }

        def jscAAR = fileTree(jscDist).matching({ it.include "**/android-jsc/**/*.aar" }).singleFile
        def soFiles = zipTree(jscAAR).matching({ it.include "**/*.so" })

        def headerFiles = fileTree(jscDist).matching({ it.include "**/include/*.h" })

        copy {
            from(soFiles)
            from(headerFiles)
            from("src/main/jni/third-party/jsc/Android.mk")

            filesMatching("**/*.h", { it.path = "JavaScriptCore/${it.name}" })

            includeEmptyDirs(false)
            into("$thirdPartyNdkDir/jsc")
        }
    }   
}
```
原理跟我们手动集没什么两样。

## 使用

### JSC的一些基本API介绍

<img src="/images/api.png" width="50%" height="50%">

#### 主要数据结构

- JSGlobalContextRef
JavaScript全局上下文。也就是JavaScript的执行环境
- JSValueRef
JavaScript的一个值，可以是变量、object、函数
- JSObjectRef
JavaScript的一个object或函数
- JSStringRef
JavaScript的一个字符串
- JSClassRef
JavaScript的类
- JSClassDefinition
JavaScript的类定义，使用这个结构，C、C++可以定义和注入JavaScript的类

#### 主要函数

- JSGlobalContextCreate / JSGlobalContextRelease
创建和销毁JavaScript全局上下文。
- JSContextGetGlobalObject
获取JavaScript的Global对象。
- JSObjectSetProperty / JSObjectGetProperty
JavaScript对象的属性操作。
- JSEvaluateScript
执行一段JS脚本。
- JSClassCreate
创建一个JavaScript类。
- JSObjectMake
创建一个JavaScript对象。
- JSObjectCallAsFunction
调用一个JavaScript函数。
- JSStringCreateWithUTF8Cstring / JSStringRelease
创建、销毁一个JavaScript字符串
- JSValueToBoolean / JSValueToNumber / JSValueToStringCopy
JSValueRef转为C++类型
- JSValueMakeBoolean / JSValueMakeNumber / JSValueMakeString
C++类型转为JSValueRef

### 使用JSC API注入一个类

我们可以将一个C++的类映射到JS中，例如定义如下一个C++的类

```
class Person {
public:
    std::string name = "James";

    int getAge() {
        return 100;
    }
};
```

我们在JS中也映射一个类Person
```
// 映射name属性
JSValueRef getName(JSContextRef ctx, JSObjectRef thisObject, JSStringRef, JSValueRef*) {
    Person* p =static_cast<Person*>(JSObjectGetPrivate(thisObject));
    return JSValueMakeString(ctx, JSStringCreateWithUTF8CString(p->name.c_str()));
}

// 映射getAge方法
JSValueRef getAge(JSContextRef ctx, JSObjectRef ,JSObjectRef thisObject, size_t argumentCount,
                  const JSValueRef arguments[],JSValueRef*) {
    Person* p =static_cast<Person*>(JSObjectGetPrivate(thisObject));
    int age = p->getAge();
    return JSValueMakeNumber(ctx, age);
}
```

```
// 类成员变量
static JSStaticValue values[] = {
        {"name", getName, 0, kJSPropertyAttributeNone },
        {0, 0, 0, 0}
};
// 类方法
static JSStaticFunction functions[] = {
        {"getAge", getAge, kJSPropertyAttributeNone},
        {0, 0, 0}
};

JSClassDefinition classDefinition = kJSClassDefinitionEmpty;
classDefinition.version = 0;
classDefinition.staticValues = values;
classDefinition.staticFunctions = functions;

static JSClassRef classRef = JSClassCreate(&classDefinition);
Person p;
JSObjectRef classObj = JSObjectMake(_ctx, classRef, &p);

// 注入Class对象p到js中
JSStringRef objName = JSStringCreateWithUTF8CString("Person");
JSObjectSetProperty(_ctx, global, objName, classObj, kJSPropertyAttributeNone, NULL)
```

这样我们在JS中就能够使用p对象了

```
Person.name;
Person.getAge();
```

需要注意的是这里的Person是一个js中的类对象，name和getAge相当于是Person类的静态变量和静态方法。现在我们想创建一个Person类的实例对象是不行的

```
const p = new Person();  // NULL
```

为了能创建类的实例对象，我们还需要给JSClassDefinition添加其他属性，我们来看看它的定义

```
typedef struct {
    int                                 version;
    JSClassAttributes                   attributes;

    const char*                         className;
    JSClassRef                          parentClass;
        
    const JSStaticValue*                staticValues;
    const JSStaticFunction*             staticFunctions;
    
    JSObjectInitializeCallback          initialize;
    JSObjectFinalizeCallback            finalize;
    JSObjectHasPropertyCallback         hasProperty;
    JSObjectGetPropertyCallback         getProperty;
    JSObjectSetPropertyCallback         setProperty;
    JSObjectDeletePropertyCallback      deleteProperty;
    JSObjectGetPropertyNamesCallback    getPropertyNames;
    JSObjectCallAsFunctionCallback      callAsFunction;
    JSObjectCallAsConstructorCallback   callAsConstructor;
    JSObjectHasInstanceCallback         hasInstance;
    JSObjectConvertToTypeCallback       convertToType;
} JSClassDefinition;
```

基本上看字段的类型就大概能知道是什么作用了，staticValues和staticFunctions就是我们上面用过的，定义类的静态属性和静态方法，为了能创建类的实例，我们需要添加一个JSObjectCallAsConstructorCallback回调，当使用 new 创建类对象的时候就会执行这个方法

```
static JSObjectRef callAsConstructor(
    JSContextRef ctx,
    JSObjectRef constructor,
    size_t argumentCount,
    const JSValueRef arguments[],
    JSValueRef* exception
) {
    Person* object = new Person();
    JSObjectSetPrivate(constructor, static_cast<void*>(object));
    return constructor;
}
```

在这个方法中，我们通过JSObjectSetPrivate给constructor关联一个C++对象，将来我们就能通过这个C++对象来执行实际的属性获取或者方法调用操作了。

例如我们这么来调用

```
const p = new Person();
p.name;
```

这样会走JSClassDefinition的JSObjectGetPropertyCallback回调，在这个回调中我们可以这么来做，通过JSObjectGetPrivate拿到C++对象，获取

```
static JSValueRef getProperty(
        JSContextRef ctx,
        JSObjectRef object,
        JSStringRef propertyName,
        JSValueRef* exception) {
    logd("getProperty %s", JSStringToCString(propertyName));
    void* host = JSObjectGetPrivate(object);
    Person* metadata = static_cast<Person*>(host);
    return JSValueMakeString(ctx, JSStringCreateWithUTF8CString(person->name.c_str()))
}
```

### Java调用JS

#### 加载JS脚本

我们在Java端定义一个加载js脚本的方法

```
object JniUtil {
  external fun loadScript(content: String)
}
```

JNI实现

```
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
}
```

定义一个 app.js 放在 assets 目录中
```
function welcome() {
  return "Hello world!\n";
}

function welcomeWithCallback(callback) {
    callback("hello, world!");
}

function add(a, b) {
  return a + b;
}

add(2, 3);
```

当调用 loadScript 加载这段js代码的时候就将3个函数加载到了一个JSContext中，另外这这个loadScript其实也是有返回值的，只是我们不使用而已。

### 普通函数调用

加载上述 js 代码后，我们尝试来调用 welcome 方法

java端声明

```
object JniUtil {
  external fun callJsFunctionSync(): Any
}
```

JNI实现

```
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
```

其中使用的 _ctx 是创建的一个全局 JSGlobalContextRef 对象

### 带回调的函数调用

和上述无参函数对比，这里需要增加两个步骤：
1. 在JSContext中创建一个回调函数，并作为参数传递给调用对象
2. 在JS的回调函数中通过JNI执行传入的jobject的回调方法

Java端定义
```
abstract class JSCallback {
  abstract fun onResult(`object`: Any?)
}

object JniUtil {
  external fun callJsFunction(callback: JSCallback)
}
```

JNI实现
```
// 注入回调函数
JSClassDefinition functionClass = kJSClassDefinitionEmpty;
functionClass.version = 0;
functionClass.attributes = kJSClassAttributeNoAutomaticPrototype;
functionClass.callAsFunction = callbackFunc;

JSClassRef hostFunctionClass = JSClassCreate(&functionClass);

HostFunctionMetadata metadata;
JSObjectRef funcRef = JSObjectMake(_ctx, hostFunctionClass, &metadata);

// 调用函数
JSValueRef args[1];
args[0] = funcRef;
JSObjectCallAsFunction(_ctx, funcObject, NULL, 1, args, NULL);
```

```
JSValueRef callbackFunc(
    JSContextRef ctx,
    JSObjectRef function,
    JSObjectRef thisObject,
    size_t argumentCount,
    const JSValueRef arguments[],
    JSValueRef* exception) {
 
  JSStringRef jsStringRef = JSValueToStringCopy(_ctx, arguments[0], NULL);
  char* params = JSStringToCString(jsStringRef);

  jclass clazz = _env->FindClass("com/example/javascript/engine/sample/JSCallback");
  jmethodID methodId = _env->GetMethodID(clazz, "onResult", "(Ljava/lang/Object;)V");
  _env->CallVoidMethod(_callback, methodId, _env->NewStringUTF(params));

  return JSValueMakeUndefined(ctx);
}
```

Java端调用

```
JniUtil.callJsFunction(object : JSCallback() {
  override fun onResult(param: Any?) {
    Log.d(TAG, "js callback: $param")
  }
})

// 输出
// D/MainActivity: js callback: hello, world!
```

### JS调用JAVA

只要将Java方法注入到JS中，那么在js中就能直接调用Java方法了。

例如在Java中我们现在有如下方法

```
object NativeModule {
  fun getSystemInfo(): String {
    return Build.FINGERPRINT
  }
}
```

我们先在JS中注入一个全局的 getSystemInfo 函数

```
void registerNativeModule() {
    JSStringRef funcName = JSStringCreateWithUTF8CString("getSystemInfo");
    JSObjectRef funcRef = JSObjectMakeFunctionWithCallback(_ctx, funcName, nativeMethodCallback);
    JSObjectRef global = JSContextGetGlobalObject(_ctx);
    JSObjectSetProperty(_ctx, global, funcName, funcRef, kJSPropertyAttributeNone, NULL);
    JSStringRelease(funcName);
}
```

函数调用的时候会执行nativeMethodCallback方法，其中会通过Jni调用Java方法

```
JSValueRef nativeMethodCallback(
        JSContextRef ctx,
        JSObjectRef function,
        JSObjectRef thisObject,
        size_t argumentCount,
        const JSValueRef arguments[],
        JSValueRef* exception
) {
    jclass clazz = _env->FindClass("com/example/javascript/engine/sample/NativeModule");
    jmethodID methodId = _env->GetStaticMethodID(clazz, "getSystemInfo", "()Ljava/lang/String;");
    jstring ret = (jstring) _env->CallStaticObjectMethod(clazz, methodId);
    char* info = jstringToCString(_env, ret);
    JSStringRef jsString = JSStringCreateWithUTF8CString(info);
    logd("info: %s", info);
    return JSValueMakeString(ctx, jsString);
}
```

然后我们就可以在JS中调用 getSystemInfo 函数了

```
function callNativeModule() {
    return getSystemInfo();
}
```

执行callNativeMethod函数，我们会发现正确返回了结果

```
com.example.javascript.engine.sample D/JniUtil: info: HUAWEI/MHA-AL00/HWMHA:9/HUAWEIMHA-AL00/9.1.0.228C00:user/release-keys
```

### 关于setTimeout

如果我们执行如下一段代码，会发现无法执行成功，为什么呢？

```
function welcomeWithCallback(callback) {
    setTimeout(() => {
        callback("hello, world!");
    }, 0);
}
```

因为setTimeout并不是JSC原生支持的API，所以直接使用肯定是找不到这个方法的，实际上RN在安卓和IOS平台上分别实现了Native的setTimeout，setInterval等方法，然后注入到了JS的全局global对象中，类似的还有 XMLHttpRequest等。

我们可以查看一下global中的属性

```
# console.log(Object.keys(global));

process,__METRO_GLOBAL_PREFIX__,__DEV__,__BUNDLE_START_TIME__,__jsiExecutorDescription,nativeModuleProxy,nativeFlushQueueImmediate,nativeCallSyncHook,nativeLoggingHook,nativePerformanceNow,_WORKLET_RUNTIME,__reanimatedModuleProxy,__r,__d,__c,__registerSegment,$RefreshReg$,$RefreshSig$,__accept,ErrorUtils,GLOBAL,window,self,performance,setInterval,clearInterval,requestAnimationFrame,cancelAnimationFrame,requestIdleCallback,cancelIdleCallback,FormData,File,URL,AbortController,AbortSignal,alert,navigator,__fetchSegment,__getSegment,__fbGenNativeModule,regeneratorRuntime,setTimeout,clearTimeout,setImmediate,clearImmediate,Blob,WebSocket,fetch,Headers,Request,Response,URLSearchParams,XMLHttpRequest,__blobCollectorProvider,FileReader
```
