package com.hx.test12;

/**
 * Test29MultiLoaderContextInvoker
 *
 * @author Jerry.X.He <970655147@qq.com>
 * @version 1.0
 * @date 2021-12-19 09:31
 */
public class Test29MultiLoaderContextMain {
 
    // hold 1M
    public static byte[] dummyBytes = new byte[1 * 1024 * 1024];
 
    // Test29MultiLoaderContextInvoker
    public static void main(int idx, String[] args) throws Exception {
        System.out.println(String.format(" Test29MultiLoaderContextMain.main be invoked, idx : %s, args : %s ", idx, args));
        new Thread(() -> {
            try {
                System.out.println(String.format(" bizThread - %s is started ", idx));
                Thread.sleep(10_000);
                System.out.println(String.format(" bizThread - %s is end ", idx));
            } catch (Exception e) {
                e.printStackTrace();
            }
        }).start();
    }
 
}