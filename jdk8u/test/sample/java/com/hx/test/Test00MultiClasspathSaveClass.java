package com.hx.test;

/**
 * Test00MultiClasspathSaveClass
 * https://jerryhe.blog.csdn.net/article/details/120936752
 *
 * @author Jerry.X.He <970655147@qq.com>
 * @version 1.0
 * @date 2021-10-24 15:46
 */
public class Test00MultiClasspathSaveClass {

    // Test00MultiClasspathSaveClass
    public static void main(String[] args) {
        UserService userService = new UserServiceFactory().newUserService();
        userService.updateUser("xxx", "xx");
        userService.removeUser("xxx", "xx");
    }
}