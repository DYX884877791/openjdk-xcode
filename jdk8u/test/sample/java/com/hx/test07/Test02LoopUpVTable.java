package com.hx.test07;

import java.awt.event.ActionListener;
import java.util.AbstractCollection;

/**
 * LookUpVTable
 * https://jerryhe.blog.csdn.net/article/details/106972044
 * https://jerryhe.blog.csdn.net/article/details/106974683
 * https://jerryhe.blog.csdn.net/article/details/106982361
 *
 * @author Jerry.X.He <970655147@qq.com>
 * @version 1.0
 * @date 2020-06-26 11:09
 */
public abstract class Test02LoopUpVTable extends AbstractCollection<String> implements ActionListener {

  // identStr
  private String identStr = "identStr";
  int f01;
  int f02;
  int f03;
  int f04;
  int f05;

  // Test02LoopUpVTable
  public static void main(String[] args) {

//    Test02LoopUpVTable instance = new Test02LoopUpVTable();
//
//    int sz = instance.size();
    int sz = 222;
    System.out.println(" szie : " + sz);

  }

//  @Override
//  public String get(int index) {
//    return null;
//  }


  @Override
  public int size() {
    return 222;
  }

}