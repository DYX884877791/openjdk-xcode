
/**
 * http://javagoo.com/java/java_native_thread_1.html
 */
public class SetCpu {
    static {
        System.loadLibrary("cpu");
    }
    public static native int getCpu(Runnable r);
}