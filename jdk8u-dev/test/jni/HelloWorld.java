/**
 * JNI接口又称本地方法，在Java代码中JNI接口通过native关键字标识，接口定义完后使用javah命令生成头文件，然后使用C/C++实现头文件中的方法，
 * 并将其编译成windows上ddl文件或者Linux上的so文件等动态链接库。
 */
public class HelloWorld {

    static
    {
        System.loadLibrary("HelloWorld");
    }

    public native static void say(String content);

    public static void main(String[] args) {
         HelloWorld.say("JNI native Hello World!");
    }
}