package com.hx.test;
 
public class UserServiceImpl implements UserService {
    public UserServiceImpl() {
    }
 
    public String addUser(String username, String password) {
        System.out.println("UserServiceImpl[addUser/updateUser] -> addUser");
        return username;
    }
 
    public String updateUser(String username, String password) {
        System.out.println("UserServiceImpl[addUser/updateUser] -> updateUser");
        return username;
    }

    @Override
    public String removeUser(String username, String password) {
        return null;
    }
}