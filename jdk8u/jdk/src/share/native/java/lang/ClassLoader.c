/*
 * Copyright (c) 1996, 2015, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.  Oracle designates this
 * particular file as subject to the "Classpath" exception as provided
 * by Oracle in the LICENSE file that accompanied this code.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 */

#include <stdlib.h>
#include <assert.h>

#include "jni.h"
#include "jni_util.h"
#include "jlong.h"
#include "jvm.h"
#include "java_lang_ClassLoader.h"
#include "java_lang_ClassLoader_NativeLibrary.h"
#include <string.h>

/* defined in libverify.so/verify.dll (src file common/check_format.c) */
extern jboolean VerifyClassname(char *utf_name, jboolean arrayAllowed);
extern jboolean VerifyFixClassname(char *utf_name);

static JNINativeMethod methods[] = {
    {"retrieveDirectives",  "()Ljava/lang/AssertionStatusDirectives;", (void *)&JVM_AssertionStatusDirectives}
};

JNIEXPORT void JNICALL
Java_java_lang_ClassLoader_registerNatives(JNIEnv *env, jclass cls)
{
    (*env)->RegisterNatives(env, cls, methods,
                            sizeof(methods)/sizeof(JNINativeMethod));
}

/* Convert java string to UTF char*. Use local buffer if possible,
   otherwise malloc new memory. Returns null IFF malloc failed. */
char*
getUTF(JNIEnv *env, jstring str, char* localBuf, int bufSize)
{
    char* utfStr = NULL;

    int len = (*env)->GetStringUTFLength(env, str);
    int unicode_len = (*env)->GetStringLength(env, str);
    if (len >= bufSize) {
        utfStr = malloc(len + 1);
        if (utfStr == NULL) {
            JNU_ThrowOutOfMemoryError(env, NULL);
            return NULL;
        }
    } else {
        utfStr = localBuf;
    }
    (*env)->GetStringUTFRegion(env, str, 0, unicode_len, utfStr);

    return utfStr;
}

// The existence or signature of this method is not guaranteed since it
// supports a private method.  This method will be changed in 1.7.
JNIEXPORT jclass JNICALL
Java_java_lang_ClassLoader_defineClass0(JNIEnv *env,
                                        jobject loader,
                                        jstring name,
                                        jbyteArray data,
                                        jint offset,
                                        jint length,
                                        jobject pd)
{
    return Java_java_lang_ClassLoader_defineClass1(env, loader, name, data, offset,
                                                   length, pd, NULL);
}

/**
 * 主要是调用了JVM_DefineClassWithSource()加载类，跟着源码往下走，会发现最终调用的是 jvm.cpp 中的 jvm_define_class_common()方法。
 */
JNIEXPORT jclass JNICALL
Java_java_lang_ClassLoader_defineClass1(JNIEnv *env,
                                        jobject loader,
                                        jstring name,
                                        jbyteArray data,
                                        jint offset,
                                        jint length,
                                        jobject pd,
                                        jstring source)
{
    jbyte *body;
    char *utfName;
    jclass result = 0;
    char buf[128];
    char* utfSource;
    char sourceBuf[1024];

    if (data == NULL) {
        JNU_ThrowNullPointerException(env, 0);
        return 0;
    }

    /* Work around 4153825. malloc crashes on Solaris when passed a
     * negative size.
     */
    if (length < 0) {
        JNU_ThrowArrayIndexOutOfBoundsException(env, 0);
        return 0;
    }

    body = (jbyte *)malloc(length);

    if (body == 0) {
        JNU_ThrowOutOfMemoryError(env, 0);
        return 0;
    }

    (*env)->GetByteArrayRegion(env, data, offset, length, body);

    if ((*env)->ExceptionOccurred(env))
        goto free_body;

    if (name != NULL) {
        utfName = getUTF(env, name, buf, sizeof(buf));
        if (utfName == NULL) {
            goto free_body;
        }
        VerifyFixClassname(utfName);
    } else {
        utfName = NULL;
    }

    if (source != NULL) {
        utfSource = getUTF(env, source, sourceBuf, sizeof(sourceBuf));
        if (utfSource == NULL) {
            goto free_utfName;
        }
    } else {
        utfSource = NULL;
    }
    result = JVM_DefineClassWithSource(env, utfName, loader, body, length, pd, utfSource);

    if (utfSource && utfSource != sourceBuf)
        free(utfSource);

 free_utfName:
    if (utfName && utfName != buf)
        free(utfName);

 free_body:
    free(body);
    return result;
}

JNIEXPORT jclass JNICALL
Java_java_lang_ClassLoader_defineClass2(JNIEnv *env,
                                        jobject loader,
                                        jstring name,
                                        jobject data,
                                        jint offset,
                                        jint length,
                                        jobject pd,
                                        jstring source)
{
    jbyte *body;
    char *utfName;
    jclass result = 0;
    char buf[128];
    char* utfSource;
    char sourceBuf[1024];

    assert(data != NULL); // caller fails if data is null.
    assert(length >= 0);  // caller passes ByteBuffer.remaining() for length, so never neg.
    // caller passes ByteBuffer.position() for offset, and capacity() >= position() + remaining()
    assert((*env)->GetDirectBufferCapacity(env, data) >= (offset + length));

    body = (*env)->GetDirectBufferAddress(env, data);

    if (body == 0) {
        JNU_ThrowNullPointerException(env, 0);
        return 0;
    }

    body += offset;

    if (name != NULL) {
        utfName = getUTF(env, name, buf, sizeof(buf));
        if (utfName == NULL) {
            JNU_ThrowOutOfMemoryError(env, NULL);
            return result;
        }
        VerifyFixClassname(utfName);
    } else {
        utfName = NULL;
    }

    if (source != NULL) {
        utfSource = getUTF(env, source, sourceBuf, sizeof(sourceBuf));
        if (utfSource == NULL) {
            JNU_ThrowOutOfMemoryError(env, NULL);
            goto free_utfName;
        }
    } else {
        utfSource = NULL;
    }
    result = JVM_DefineClassWithSource(env, utfName, loader, body, length, pd, utfSource);

    if (utfSource && utfSource != sourceBuf)
        free(utfSource);

 free_utfName:
    if (utfName && utfName != buf)
        free(utfName);

    return result;
}

JNIEXPORT void JNICALL
Java_java_lang_ClassLoader_resolveClass0(JNIEnv *env, jobject this,
                                         jclass cls)
{
    if (cls == NULL) {
        JNU_ThrowNullPointerException(env, 0);
        return;
    }

    JVM_ResolveClass(env, cls);
}

/*
 * Returns NULL if class not found.
 */
JNIEXPORT jclass JNICALL
Java_java_lang_ClassLoader_findBootstrapClass(JNIEnv *env, jobject loader,
                                              jstring classname)
{
    char *clname;
    jclass cls = 0;
    char buf[128];

    if (classname == NULL) {
        return 0;
    }

    clname = getUTF(env, classname, buf, sizeof(buf));
    if (clname == NULL) {
        JNU_ThrowOutOfMemoryError(env, NULL);
        return NULL;
    }
    VerifyFixClassname(clname);

    if (!VerifyClassname(clname, JNI_TRUE)) {  /* expects slashed name */
        goto done;
    }

    cls = JVM_FindClassFromBootLoader(env, clname);

 done:
    if (clname != buf) {
        free(clname);
    }

    return cls;
}

JNIEXPORT jclass JNICALL
Java_java_lang_ClassLoader_findLoadedClass0(JNIEnv *env, jobject loader,
                                           jstring name)
{
    if (name == NULL) {
        return 0;
    } else {
        return JVM_FindLoadedClass(env, loader, name);
    }
}

static jfieldID handleID;
static jfieldID jniVersionID;
static jfieldID loadedID;
static void *procHandle;

static jboolean initIDs(JNIEnv *env)
{
    //handleID,jniVersionID,loadedID都是静态全局变量，表示NativeLibrary类的handle，jniVersion，loaded三个属性的属性ID
    //可通过属性ID设置实例的属性
    //当第一次加载库文件的时候会根据NativeLibrary类初始化这些静态属性
    if (handleID == 0) {
        jclass this =
            (*env)->FindClass(env, "java/lang/ClassLoader$NativeLibrary");
        if (this == 0)
            return JNI_FALSE;
        handleID = (*env)->GetFieldID(env, this, "handle", "J");
        if (handleID == 0)
            return JNI_FALSE;
        jniVersionID = (*env)->GetFieldID(env, this, "jniVersion", "I");
        if (jniVersionID == 0)
            return JNI_FALSE;
        loadedID = (*env)->GetFieldID(env, this, "loaded", "Z");
        if (loadedID == 0)
             return JNI_FALSE;
        procHandle = getProcessHandle();
    }
    return JNI_TRUE;
}

typedef jint (JNICALL *JNI_OnLoad_t)(JavaVM *, void *);
typedef void (JNICALL *JNI_OnUnload_t)(JavaVM *, void *);

/*
 * Support for finding JNI_On(Un)Load_<lib_name> if it exists.
 * If cname == NULL then just find normal JNI_On(Un)Load entry point
 */
static void *findJniFunction(JNIEnv *env, void *handle,
                                    const char *cname, jboolean isLoad) {
    //字符串指针数组，实际是{"JNI_OnLoad"}
    const char *onLoadSymbols[] = JNI_ONLOAD_SYMBOLS;
    //实际是{"JNI_OnUnload"}
    const char *onUnloadSymbols[] = JNI_ONUNLOAD_SYMBOLS;
    const char **syms;
    int symsLen;
    void *entryName = NULL;
    char *jniFunctionName;
    int i;
    int len;

    // Check for JNI_On(Un)Load<_libname> function
    // 根据isLoad判断 JNI_On(Un)Load<_libname>
    if (isLoad) {
        syms = onLoadSymbols;
        symsLen = sizeof(onLoadSymbols) / sizeof(char *);
    } else {
        syms = onUnloadSymbols;
        symsLen = sizeof(onUnloadSymbols) / sizeof(char *);
    }
    for (i = 0; i < symsLen; i++) {
        // cname + sym + '_' + '\0'
        //检查拼起来的JNI_On(Un)Load<_libname>的长度是否大于最大值FILENAME_MAX
        if ((len = (cname != NULL ? strlen(cname) : 0) + strlen(syms[i]) + 2) >
            FILENAME_MAX) {
            goto done;
        }
        //给jniFunctionName分配内存
        jniFunctionName = malloc(len);
        if (jniFunctionName == NULL) {
            JNU_ThrowOutOfMemoryError(env, NULL);
            goto done;
        }
        //拼成JNI_On(Un)Load<_libname>，拼成的字符串作为底层ddl查找的参数，libname就是要查找的库文件，JNI_OnLoad就是在库文件中查找的目标函数名
        buildJniFunctionName(syms[i], cname, jniFunctionName);
        //查找该方法是否已加载
        entryName = JVM_FindLibraryEntry(handle, jniFunctionName);
        //释放内存
        free(jniFunctionName);
        //如果不为空则终止循环，返回entryName
        if(entryName) {
            break;
        }
    }

 done:
    return entryName;
}

/*
 * Class:     java_lang_ClassLoader_NativeLibrary
 * Method:    load
 * Signature: (Ljava/lang/String;Z)V
 */
JNIEXPORT void JNICALL
Java_java_lang_ClassLoader_00024NativeLibrary_load
  (JNIEnv *env, jobject this, jstring name, jboolean isBuiltin)
{
    const char *cname;
    jint jniVersion;
    jthrowable cause;
    void * handle;

    //jniVersionID等初始化
    if (!initIDs(env))
        return;

    //将java String对象的字符串复制到cname 字符数组中
    cname = JNU_GetStringPlatformChars(env, name, 0);
    if (cname == 0)
        return;
    //如果未加载则加载该库文件
    handle = isBuiltin ? procHandle : JVM_LoadLibrary(cname);
    //如果加载完成
    if (handle) {
        JNI_OnLoad_t JNI_OnLoad;
        //获取该库文件中的JNI_OnLoad函数
        JNI_OnLoad = (JNI_OnLoad_t)findJniFunction(env, handle,
                                               isBuiltin ? cname : NULL,
                                               JNI_TRUE);
        //如果库文件包含了JNI_OnLoad函数
        if (JNI_OnLoad) {
            JavaVM *jvm;
            //通过JNIEnv获取对应的JavaVM
            (*env)->GetJavaVM(env, &jvm);
            //获取库文件要求的jniVersion
            jniVersion = (*JNI_OnLoad)(jvm, NULL);
        } else {
            jniVersion = 0x00010001;
        }

        //如果出现异常
        cause = (*env)->ExceptionOccurred(env);
        if (cause) {
            (*env)->ExceptionClear(env);
            (*env)->Throw(env, cause);
            if (!isBuiltin) {
                JVM_UnloadLibrary(handle);
            }
            goto done;
        }

        //如果库文件要求的jniVersion不支持则抛出异常
        if (!JVM_IsSupportedJNIVersion(jniVersion) ||
            (isBuiltin && jniVersion < JNI_VERSION_1_8)) {
            char msg[256];
            jio_snprintf(msg, sizeof(msg),
                         "unsupported JNI version 0x%08X required by %s",
                         jniVersion, cname);
            JNU_ThrowByName(env, "java/lang/UnsatisfiedLinkError", msg);
            if (!isBuiltin) {
                JVM_UnloadLibrary(handle);
            }
            goto done;
        }
        //如果库文件要求的jniVersion支持则设置NativeLibrary实例的jniVersion属性
        (*env)->SetIntField(env, this, jniVersionID, jniVersion);
    } else {
        cause = (*env)->ExceptionOccurred(env);
        if (cause) {
            (*env)->ExceptionClear(env);
            (*env)->SetLongField(env, this, handleID, (jlong)0);
            (*env)->Throw(env, cause);
        }
        goto done;
    }
    //如果加载成功，则设置NativeLibrary实例的handle属性和loaded属性
    (*env)->SetLongField(env, this, handleID, ptr_to_jlong(handle));
    (*env)->SetBooleanField(env, this, loadedID, JNI_TRUE);

 done:
    //释放cname对应的内存
    JNU_ReleaseStringPlatformChars(env, name, cname);
}

/*
 * Class:     java_lang_ClassLoader_NativeLibrary
 * Method:    unload
 * Signature: (Z)V
 */
JNIEXPORT void JNICALL
Java_java_lang_ClassLoader_00024NativeLibrary_unload
(JNIEnv *env, jobject this, jstring name, jboolean isBuiltin)
{
    const char *onUnloadSymbols[] = JNI_ONUNLOAD_SYMBOLS;
    void *handle;
    JNI_OnUnload_t JNI_OnUnload;
     const char *cname;

    if (!initIDs(env))
        return;
    cname = JNU_GetStringPlatformChars(env, name, 0);
    if (cname == NULL) {
        return;
    }
    handle = jlong_to_ptr((*env)->GetLongField(env, this, handleID));
    JNI_OnUnload = (JNI_OnUnload_t )findJniFunction(env, handle,
                                                isBuiltin ? cname : NULL,
                                                JNI_FALSE);
    if (JNI_OnUnload) {
        JavaVM *jvm;
        (*env)->GetJavaVM(env, &jvm);
        (*JNI_OnUnload)(jvm, NULL);
    }
    if (!isBuiltin) {
        JVM_UnloadLibrary(handle);
    }
    JNU_ReleaseStringPlatformChars(env, name, cname);
}

/*
 * Class:     java_lang_ClassLoader_NativeLibrary
 * Method:    find
 * Signature: (Ljava/lang/String;)J
 */
JNIEXPORT jlong JNICALL
Java_java_lang_ClassLoader_00024NativeLibrary_find
  (JNIEnv *env, jobject this, jstring name)
{
    jlong handle;
    const char *cname;
    jlong res;

    if (!initIDs(env))
        return jlong_zero;

    handle = (*env)->GetLongField(env, this, handleID);
    cname = (*env)->GetStringUTFChars(env, name, 0);
    if (cname == 0)
        return jlong_zero;
    res = ptr_to_jlong(JVM_FindLibraryEntry(jlong_to_ptr(handle), cname));
    (*env)->ReleaseStringUTFChars(env, name, cname);
    return res;
}
/*
 * Class:     java_lang_ClassLoader
 * Method:    findBuiltinLib
 * Signature: (Ljava/lang/String;)Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL
Java_java_lang_ClassLoader_findBuiltinLib
  (JNIEnv *env, jclass cls, jstring name)
{
    const char *cname;
    char *libName;
    int prefixLen = (int) strlen(JNI_LIB_PREFIX);
    int suffixLen = (int) strlen(JNI_LIB_SUFFIX);
    int len;
    jstring lib;
    void *ret;
    const char *onLoadSymbols[] = JNI_ONLOAD_SYMBOLS;

    //非空校验
    if (name == NULL) {
        JNU_ThrowInternalError(env, "NULL filename for native library");
        return NULL;
    }
    //初始化procHandle，实际是一个dlopen函数的指针
    procHandle = getProcessHandle();
    //将name中的字符串拷贝到cname中
    cname = JNU_GetStringPlatformChars(env, name, 0);
    if (cname == NULL) {
        return NULL;
    }
    // Copy name Skipping PREFIX
    //校验cname的长度是否大于前缀长度加上后缀长度
    len = strlen(cname);
    if (len <= (prefixLen+suffixLen)) {
        JNU_ReleaseStringPlatformChars(env, name, cname);
        return NULL;
    }
    //libName初始化
    libName = malloc(len + 1); //+1 for null if prefix+suffix == 0
    if (libName == NULL) {
        JNU_ReleaseStringPlatformChars(env, name, cname);
        JNU_ThrowOutOfMemoryError(env, NULL);
        return NULL;
    }
    //跳过前缀将cname复制到libName中
    if (len > prefixLen) {
        strcpy(libName, cname+prefixLen);
    }
    //释放cname对应的内存
    JNU_ReleaseStringPlatformChars(env, name, cname);

    // Strip SUFFIX
    //将后缀起始字符置为\0，标记字符串结束，即相当于去掉了后缀
    libName[strlen(libName)-suffixLen] = '\0';

    // Check for JNI_OnLoad_libname function
    //查找该libname是否存在
    ret = findJniFunction(env, procHandle, libName, JNI_TRUE);
    if (ret != NULL) {
        //如果存在返回，用libname构造一个java String并返回
        lib = JNU_NewStringPlatform(env, libName);
        //释放libName的内存
        free(libName);
        return lib;
    }
    //如果不存在，释放libName的内存，返回NULL
    free(libName);
    return NULL;
}
