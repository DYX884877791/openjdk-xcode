/**
 * https://jerryhe.blog.csdn.net/article/details/102635876
 */

package com.hx.test02;

import java.lang.ref.PhantomReference;
import java.lang.ref.Reference;
import java.lang.ref.ReferenceQueue;
import java.lang.reflect.Field;

/**
 * Test02WeakReferenceAfterGc
 *
 * @author Jerry.X.He <970655147@qq.com>
 * @version 1.0
 * @date 2019-10-14 11:23
 */
public class Test02WeakReferenceAfterGc {

  // Test02WeakReferenceAfterGc
  // refer : https://hllvm-group.iteye.com/group/topic/34934
  public static void main(String[] args) throws Exception {
    ReferenceQueue<Obj> referenceQueue = new ReferenceQueue<>();
    for (int i = 0; i < 1; i++) {
      Obj o = new Obj("object_" + i);
//      SoftReference<Obj> ref = new SoftReference<>(o, referenceQueue);
//      WeakReference<Obj> ref = new WeakReference<>(o, referenceQueue);
      PhantomReference<Obj> ref = new PhantomReference<>(o, referenceQueue);
      o = null;
      System.gc();
      Field field = Reference.class.getDeclaredField("referent");
      field.setAccessible(true);
      System.out.println(field.get(ref));
    }

    // 这个现象, 有 finalize 和 没有 finalize 是两个不同的情况, 按照理论上来说[常规的思考], 有 finalize 的情况下, 应该会有两个 Reference 分别进入 两个队列, 但是没有
    Thread.sleep(3000);
    System.gc();

    // consumer
    new Thread() {
      public void run() {
        while (true) {
          Object o = referenceQueue.poll();
          if (o != null) {
            try {
              Field rereferent = Reference.class.getDeclaredField("referent");
              rereferent.setAccessible(true);
              Object result = rereferent.get(o);
              System.out.println("gc will collect : " + o.getClass() + "@" + o.hashCode() + ", referent : " + result);
            } catch (Exception e) {
              e.printStackTrace();
            }
          }
        }
      }
    }.start();

  }

  /**
   * Obj
   *
   * @author Jerry.X.He <970655147@qq.com>
   * @version 1.0
   * @date 2019-10-14 11:24
   */
  static class Obj {
    private final String name;

    Obj(String name) {
      this.name=name;
    }

    // test for FinalReference
//    @Override
//    protected void finalize() throws Throwable {
//      System.out.println("执行finalize方法：" + name);
//      super.finalize();
//    }

    @Override
    public String toString() {
      return name;
    }
  }


}