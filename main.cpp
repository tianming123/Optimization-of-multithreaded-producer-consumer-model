#include <iostream>
#include <thread>
#include "numUtils.h"
#include <mutex>
#include <condition_variable>
#include<ctime>
#include <unistd.h>

#define MAX_SIZE 100
#define MAX_TIME 1000

using namespace std;

struct DataA{
    int a;
};
struct DataB{
    double data[5];
};
struct data_resource{

    DataA data_a_buffer[MAX_SIZE];
    size_t read_a_position;
    size_t write_a_position;
    std::mutex mtx_a; // 互斥量,保护产品缓冲区
    std::condition_variable not_full_a; // 条件变量, 指示产品缓冲区不为满.
    std::condition_variable not_empty_a; // 条件变量, 指示产品缓冲区不为空.

    DataB data_b_buffer[MAX_SIZE];
    size_t read_b_position;
    size_t write_b_position;
    std::mutex mtx_b; // 互斥量,保护产品缓冲区
    std::condition_variable not_full_b; // 条件变量, 指示产品缓冲区不为满.
    std::condition_variable not_empty_b; // 条件变量, 指示产品缓冲区不为空.

    double sum_b_all;

}instance; // 产品库全局变量

typedef struct data_resource data_resource;
numUtils nu;// 随机数产生工具


// A 生产者
void ProducerA(data_resource *dr){
    DataA da = {nu.aNumberRandom()};
    std::unique_lock<std::mutex> lock(dr->mtx_a);
    while (((dr->write_a_position + 1) % MAX_SIZE)
           == dr->read_a_position){ // 生产者等待"产品库缓冲区不为满"
        (dr->not_full_a).wait(lock);
    }
    (dr->data_a_buffer)[dr->write_a_position] = da;
    (dr->write_a_position)++;
    if (dr->write_a_position == MAX_SIZE) // 写入位置若是在队列最后则重新设置为初始位置.
        dr->write_a_position = 0;
    (dr->not_empty_a).notify_all(); // 通知消费者产品库不为空.
    //cout<< da.a <<endl;
    //cout<< "***da***" <<endl;
}
// A 生产者任务
void ProducerATask(){
    clock_t start = clock();
    clock_t end = clock()+1;
    while((end-start)/100 <MAX_TIME){
        //sleep(3);
        ProducerA(&instance);
        end = clock();
    }
}

// B 生产者
void ProducerB(data_resource *dr){
    DataB db{};
    nu.bNumberRandom(db.data);

    std::unique_lock<std::mutex> lock(dr->mtx_b);
    while (((dr->write_b_position + 1) % MAX_SIZE)
           == dr->read_b_position){ // 生产者等待"产品库缓冲区不为满"
        (dr->not_full_b).wait(lock);
    }
    (dr->data_b_buffer)[dr->write_b_position] = db;
    (dr->write_b_position)++;
    if (dr->write_b_position == MAX_SIZE) // 写入位置若是在队列最后则重新设置为初始位置.
        dr->write_b_position = 0;
    (dr->not_empty_b).notify_all(); // 通知消费者产品库不为空.
//    for (const auto &item : db.data)
//        cout<< item <<endl;
    //cout<< "***db***" <<endl;
}
// B 生产者任务
void ProducerBTask(){
    clock_t start = clock();
    clock_t end = clock()+1;
    while((end-start)/100 <MAX_TIME){
        //sleep(3);
        ProducerB(&instance);
        end = clock();
    }
}
// C 消费者
void ConsumerC(data_resource *dr){
    DataA da{};
    DataB db{};
    double tmp=0;
    std::unique_lock<std::mutex> lock1(dr->mtx_b);
    while (dr->write_b_position == dr->read_b_position) {
        (dr->not_empty_b).wait(lock1);// 消费者等待"产品库缓冲区不为空"这一条件发生.
    }
    db = (dr->data_b_buffer)[dr->read_b_position];
    std::unique_lock<std::mutex> lock2(dr->mtx_a);
    //需要特别考虑的即为初始状态下，DataB已经连续被产生两次，而ProduceA实际没有产生任何DataA，此时需等待
    // 其余情况，若最新的DataA还未产生则可以使用上一个da进行判断
    while((dr->data_a_buffer)[dr->read_a_position].a==0){
        (dr->not_empty_a).wait(lock2);// 消费者等待"产品库缓冲区不为空"这一条件发生.
    }

    if(dr->write_a_position != dr->read_a_position){ // 若有新的DataA产生，则使用新产生的DataA，否则使用最近的一个
        da = (dr->data_a_buffer)[dr->read_a_position];
        (dr->read_a_position)++;
        if(dr->read_a_position>=MAX_SIZE)
            dr->read_a_position = 0;
    }
    else{
        dr->read_a_position == 0 ? da = (dr->data_a_buffer)[dr->read_a_position-1] : da = (dr->data_a_buffer)[MAX_SIZE-1];
    }
    (dr->read_b_position)++;
    if(dr->read_b_position>=MAX_SIZE)
        dr->read_b_position = 0;
    (dr->not_full_a).notify_all();
    (dr->not_full_b).notify_all();
    for (double d:db.data) {
        tmp+=d;
    }
    (da.a%2==0)? dr->sum_b_all += -1*tmp : dr->sum_b_all += tmp;

}
// C 消费者任务
void ConsumerCTask(){
    clock_t start = clock();
    clock_t end = clock()+1;
    while((end-start)/100 <MAX_TIME){
        //sleep(1);
        ConsumerC(&instance);
        end = clock();
    }
}

// D 打印者
void LoggerD(data_resource *dr){
    cout<<dr->sum_b_all<<endl;
    cout<<"***result***"<<endl;
}
// D 打印任务
void LoggerDTask(){
    clock_t start = clock();
    clock_t end = clock()+1;
    while((end-start)/100 <MAX_TIME){
        sleep(1);
        LoggerD(&instance);
        end = clock();
    }
}

void InitResource(data_resource *dr){
    // 初始化读写位置
    dr->read_a_position = 0;
    dr->write_a_position = 0;
    dr->read_b_position = 0;
    dr->write_b_position = 0;
    // 初始化结果
    dr->sum_b_all = 0;
}

int main() {
    InitResource(&instance);
    std::thread ProducerA(ProducerATask);
    std::thread ProducerB(ProducerBTask);
    std::thread ConsumerC(ConsumerCTask);
    std::thread LoggerD(LoggerDTask);

    ProducerA.join();
    ProducerB.join();
    ConsumerC.join();
    LoggerD.join();
}
