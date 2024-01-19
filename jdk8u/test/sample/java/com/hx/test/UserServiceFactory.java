package com.hx.test;

public class UserServiceFactory {
    public UserServiceFactory() {
    }

    public UserService newUserService() {
        return new UserServiceImpl();
    }
}