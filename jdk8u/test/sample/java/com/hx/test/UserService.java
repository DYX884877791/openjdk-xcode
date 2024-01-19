package com.hx.test;

/**
 * UserService
 *
 * @author Jerry.X.He <970655147@qq.com>
 * @version 1.0
 * @date 2021-10-24 15:47
 */
public interface UserService {

    String updateUser(String username, String password);

    String removeUser(String username, String password);

}