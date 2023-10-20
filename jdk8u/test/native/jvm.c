/**
 * https://quantum6.blog.csdn.net/article/details/123683545
 * JNI用C加载JDK产生JVM虚拟机，并运行JAVA类main函数(MACOS/LINUX)
 */

#include <stdio.h>
#include <stdlib.h>
 
#include <dlfcn.h>
#include <pthread.h>

#include <jni.h>
#include <string.h>
#include <dlfcn.h>
#include <unistd.h>
 
#ifdef __APPLE__
    #define _DARWIN_BETTER_REALPATH
    #include <mach-o/dyld.h>
    #include <CoreServices/CoreServices.h>
    #include <sys/syscall.h>
    static void dummyCallback(void * info) {};
#else
    #include <syscall.h>
#endif
 
#ifdef  _WINDOWS
#define  LIB_SUFFIX "dll"
#elif __APPLE__
#define  LIB_SUFFIX "dylib"
// JDK_PATH是编译时指定的宏
#define  LIB_JVM_PATH        JDK_PATH       "/lib/server/libjvm." LIB_SUFFIX
#else
#define  LIB_SUFFIX "so"
// JDK_PATH是编译时指定的宏
#define  LIB_JVM_PATH        JDK_PATH       "/lib/amd64/server/libjvm." LIB_SUFFIX
#endif
 
#ifdef  _WINDOWS
#define  LIB_OPEN    LoadLibrary
#define  LIB_CLOSE   FreeLibrary
#define  LIB_METHOD  GetProcAddress
#else
#define  LIB_OPEN    dlopen
#define  LIB_CLOSE   dlclose
#define  LIB_METHOD  dlsym
#endif
 
#define  BUFFER_SIZE         256



 
#define  JNI_CREATE_JNI      "JNI_CreateJavaVM"
 
/**
 从libjvm中找到的函数，产生虚拟机。
 */
typedef int (*CreateJavaVM_t)(JavaVM **ppJvm, void **ppEnv, void *pArgs);
 
static void*     g_pLibHandler = NULL;
static JavaVM*   g_pJvm        = NULL;
static JNIEnv*   g_pJniEnv     = NULL;
static jclass    g_jMainClass  = NULL;
static jmethodID g_jMainMethod = NULL;
 
/**
 从后向前
 */
static void release_for_exit()
{

    printf("release_for_exit\n");

    g_jMainMethod = NULL;
    g_jMainClass  = NULL;
 
    if (g_pJniEnv != NULL)
    {
        g_pJniEnv->ExceptionDescribe();
        g_pJniEnv->ExceptionClear();
        g_pJniEnv = NULL;
    }
 
    printf("g_pJvm is %p\n", g_pJvm);

    if (g_pJvm != NULL)
    {
        printf("will destroy JavaVM\n");
        g_pJvm->DestroyJavaVM();
        g_pJvm = NULL;
    }
    
    if (g_pLibHandler != NULL)
    {
        LIB_CLOSE(g_pLibHandler);
        g_pLibHandler = NULL;
    }
}
 
 
static void load_jvm(JNIEnv **ppEnv, JavaVM **ppJvm)
{
    char pJvmPath[BUFFER_SIZE] = {0};
 
    strcpy(pJvmPath, LIB_JVM_PATH);
    printf("JvmPath is %s\n", pJvmPath);
    g_pLibHandler = LIB_OPEN(pJvmPath, RTLD_NOW | RTLD_GLOBAL);
    if (g_pLibHandler == NULL)
    {
        return;
    }
    
    JavaVMOption options[10];
    int counter = 0;
    options[counter++].optionString = (char*)"-XX:+UseG1GC";
    options[counter++].optionString = (char*)"-XX:-UseAdaptiveSizePolicy";
    options[counter++].optionString = (char*)"-XX:-OmitStackTraceInFastThrow";
    options[counter++].optionString = (char*)"-Xmn512m";
    options[counter++].optionString = (char*)"-Xmx2048m";
    options[counter++].optionString = (char*)"-Djava.library.path=natives";
    
    memset(pJvmPath, 0, BUFFER_SIZE);
    // CLASS_PATH是编译时指定的宏
    sprintf(pJvmPath, "-Djava.class.path=%s", CLASS_PATH);
    options[counter++].optionString = (char*)strdup(pJvmPath);
 
    JavaVMInitArgs vm_args;
    memset(&vm_args, 0, sizeof(vm_args));
    vm_args.version            = JNI_VERSION_1_8;
    vm_args.nOptions           = counter++;
    vm_args.options            = options;
    vm_args.ignoreUnrecognized = JNI_TRUE;
    
    CreateJavaVM_t pCreateJvmFunction = (CreateJavaVM_t)LIB_METHOD(g_pLibHandler, JNI_CREATE_JNI);
    if (pCreateJvmFunction == NULL)
    {
    		return;
    }
    
    int retCode = pCreateJvmFunction(ppJvm, (void**)ppEnv, &vm_args);
    printf("retCode is %d\n", retCode);
    printf("*ppJvm is %p\n", *ppJvm);
    printf("*ppEnv is %p\n", *ppEnv);

    if (retCode != 0 || *ppJvm == NULL || *ppEnv == NULL)
    {
        printf("*ppJvm is NULL\n");
        printf("*ppEnv is NULL\n");

        *ppJvm = NULL;
        *ppEnv = NULL;
    }
    printf("load jvm success\n");
}
 
static void run_java_class()
{
    if (g_pJniEnv == NULL)
    {
        return;
    }
    
    // JAVA_MAIN_CLASS是编译时指定的宏
    g_jMainClass  = g_pJniEnv->FindClass(JAVA_MAIN_CLASS);
    if (g_pJniEnv->ExceptionCheck() == JNI_TRUE || g_jMainClass == NULL )
    {
        return;
    }
    
    g_jMainMethod = g_pJniEnv->GetStaticMethodID(g_jMainClass, "main", "([Ljava/lang/String;)V");
    if (g_pJniEnv->ExceptionCheck() == JNI_TRUE || g_jMainMethod == NULL)
    {
        return;
    }

    g_pJniEnv->CallStaticVoidMethod(g_jMainClass, g_jMainMethod, NULL);
}
 
void thread_function()
{
 
    printf("jvm will load\n");

    load_jvm(&g_pJniEnv, &g_pJvm);


    run_java_class();
 
    return;
}


int main(const int argc, const char** argv) {
#ifdef __APPLE__
    printf("gettid is %lu\n",(size_t)pthread_mach_thread_np(pthread_self()));
#else
    printf("gettid is %lu\n",(size_t)syscall(__NR_gettid));
#endif
    printf("pthread_self is %lu\n",(size_t)pthread_self());
    printf("pid is %lu\n",(size_t)getpid());
    thread_function();
    release_for_exit();
    sleep(600);
    return 0;
}