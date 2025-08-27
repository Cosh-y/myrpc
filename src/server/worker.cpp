#include <iostream>

#include "worker.h"

// 1. 成员初始化列表的初始化方式
worker::worker() : m_thread(&worker::run, this) {
    static int id = 0;
    m_id = ++id;
    // 2. 构造临时匿名对象再赋值
    // 这种写法是正确的，&worker::run 这种语法【&类名::方法名】取出成员函数指针
    // thread 构造函数能判断第一个参数是成员函数指针，此时第二个参数作为调用成员函数的对象，而非一般的函数参数
    // m_thread = std::thread(&worker::run, this);

    // 3. 
    // 这种写法也是正确的，绕开了成员函数指针的问题
    // m_thread = std::thread([this] () {
    //     this->run();
    // });

    // 4. 
    // 这种写法也是正确的，怎么知道 run 是成员函数的？
    // 捕获 this 的 lambda 比较特殊，捕获了 this 的 lambda 内部就仿佛类的一般的成员函数内部一样
    // 可以直接访问类的（包括私有的）成员和函数
    // m_thread = std::thread([this] () {
    //     run();
    // });
}

void worker::run() {
    std::cout << "start worker " << m_id << "\n";
    while (true) {

    }
}