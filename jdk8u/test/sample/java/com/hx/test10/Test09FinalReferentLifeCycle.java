package com.hx.test10;

/**
 * https://jerryhe.blog.csdn.net/article/details/102647444
 * FinalReferentLifeCycle
 *
 * @author Jerry.X.He <970655147@qq.com>
 * @version 1.0
 * @date 2019-10-18 09:48
 */
public class Test09FinalReferentLifeCycle {

    /**
     * lock
     */
    static Object lock = new Object();

    // Test09FinalReferentLifeCycle
    // select s from com.hx.test02.Test09FinalReferentLifeCycle$FinalizedClazz s
    public static void main(String[] args) throws Exception {

        FinalizedClazz obj = new FinalizedClazz("randomString");
        obj = null;

        System.gc();
        Thread.sleep(1000);
        System.gc();

        // `obj` is removed
        System.out.println(" end ... ");

    }

    /**
     * FinalizedClazz
     *
     * @author Jerry.X.He <970655147@qq.com>
     * @version 1.0
     * @date 2019-10-18 15:58
     */
    static class FinalizedClazz {
        // for debug
        private String ident;

        public FinalizedClazz(String ident) {
            this.ident = ident;
        }

        protected void finalize() {
            System.out.println(" do finalize, ident : " + ident);

            // wait on FinalizerThread ?
//            synchronized (lock) {
//                try {
//                    lock.wait();
//                } catch (Exception e) {
//                    e.printStackTrace();
//                }
//            }
        }

    }

}