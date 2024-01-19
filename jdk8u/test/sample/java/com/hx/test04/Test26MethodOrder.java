package com.hx.test04;

public class Test26MethodOrder {
    // funcN
    private static int counter = 0;
    private static void func008() {
      System.out.println(counter++);
      if((counter == 16) || (counter == 17)) {
        Test26SynchronizeObject.doClone(new Test26SynchronizeObject());
      }
    }

}