#C++项目报告
##文件说明
####&#8195;&#8195;numUtils.h &#8195; 生成DataA以及DataB随机数的工具类
####&#8195;&#8195;main.cpp  &#8195; 主运行函数 （MAX_TIME：线程任务的执行周期）
##一、基本实现细节
####&#8195;&#8195;对于生产的DataA以及DataB数据分别构建一个queue来存储生产的数据，queue不满时可以生产，不空时可以消费。  
####&#8195;&#8195;对于这个队列，采用阻塞队列的实现思路。  
####&#8195;&#8195;&#8195;&#8195;首先实现构造函数，初始化unique_lock供condition_variable使用。  
####&#8195;&#8195;&#8195;&#8195;然后在函数体中unlock。申请两个条件变量，分别用来控制consumer和producer。  
####&#8195;&#8195;队列出入的过程为：  
&#8195;&#8195;&#8195;&#8195;1.首先加锁。  
&#8195;&#8195;&#8195;&#8195;2.环判断一下目前的队列情况，对于各自的特殊情况（队满和队空）进行处理。  
&#8195;&#8195;&#8195;&#8195;3.唤醒一个线程来处理特殊情况。  
&#8195;&#8195;&#8195;&#8195;4.等待处理完毕。  
&#8195;&#8195;&#8195;&#8195;5.处理入和出队列操作。  
&#8195;&#8195;&#8195;&#8195;6.最后释放锁。
##二、特殊处理
####&#8195;&#8195;考虑到消费者C进程同时对生产者A和生产者B生产的数据进行消费，对于生产DataB的队列无需特殊处理，按普通的一对一生产者消费者模型处理即可  
####&#8195;&#8195;对于生产DataA的队列，通常来说当如果连续来的数据都为 DataB 时，即此时最新的DataA还未生产，那么消费者C进程取当前队列中最近的一个DataA作为结果加和的判断依据，并且不移动DataA队列读取指针位置。但是会出现初始状态下，生产者B连续产生了两个DataB，生产者A还未生产出任何DataA的情况，此时消费者C需要等待生产者A至少生产出一个DataA才能继续进行后续的任务。
##三、优化思路  
###（1）重复消费问题
####&#8195;&#8195;首先，为了避免重复消费的问题，需要对缓冲区进行加锁，由于DataA处理的特殊性，在上一部分中已经说明其处理思路
###（2）通信延迟问题
####&#8195;&#8195;使用队列在多线程传数据时使用锁虽然是安全的，但是不停的加锁，导致频繁使用一个队列进行io时，速度会非常慢，因为中间有大量等待锁释放和申请锁的时间。
####&#8195;&#8195;因此，可以参考数据库优化中的表锁与行锁的概念，考虑采用扩大加锁范围的方式来避免频繁地加锁解锁操作
&#8195;&#8195;&#8195;&#8195;1.假设缓冲队列是一个二维数组 da[m][n]以及db[m][n];  
&#8195;&#8195;&#8195;&#8195;2.把加锁的纬度从对单个数据加锁变为对整个数据行进行加锁，对于da[i]和db[i]同一时间只允许读或者只允许写；  
&#8195;&#8195;&#8195;&#8195;3.此时遍历的元素就变成了整个数据组；  
&#8195;&#8195;&#8195;&#8195;4.生产者向da[i] db[i]中写数据，即写da[i][j]和db[i][j]，直到将数组写满，然后将数组返回，拿到da[i+1]和db[i+1]继续做写入任务  
&#8195;&#8195;&#8195;&#8195;5.消费者从da[i] db[i]中取数据，即拿da[i][j]和db[i][j]，直到将数组拿空，然后将数组返回，拿到da[i+1]和db[i+1]继续做计算任务  
&#8195;&#8195;&#8195;&#8195;6.当把单个数据锁上升到行锁时，甚至可以不用对DataA做特殊处理，因为DataA和DataB都是以一纬数组的形式返回给消费者的，在位置上是相互对应的。
```asm
#define MAX_M_SIZE 5
#define MAX_N_SIZE 100
struct data_resource{

    DataA data_a_buffer[MAX_M_SIZE][MAX_N_SIZE];
    size_t read_a_position;
    size_t write_a_position;
    std::mutex mtx_a; // 互斥量,保护产品缓冲区
    std::condition_variable not_full_a; // 条件变量, 指示产品缓冲区不为满.
    std::condition_variable not_empty_a; // 条件变量, 指示产品缓冲区不为空.

    DataB data_b_buffer[MAX_M_SIZE][MAX_N_SIZE];
    size_t read_b_position;
    size_t write_b_position;
    std::mutex mtx_b; // 互斥量,保护产品缓冲区
    std::condition_variable not_full_b; // 条件变量, 指示产品缓冲区不为满.
    std::condition_variable not_empty_b; // 条件变量, 指示产品缓冲区不为空.

    double sum_b_all;

}instance; // 产品库全局变量
// A 生产者
void ProducerA(data_resource *dr){
    std::unique_lock<std::mutex> lock(dr->mtx_a);
    while (((dr->write_a_position + 1) % MAX_M_SIZE)
           == dr->read_a_position){ // 生产者等待"产品库缓冲区不为满"
        (dr->not_full_a).wait(lock);
    }
    for(int i=0;i<MAX_N_SIZE;i++){
        DataA da = {nu.aNumberRandom()};
        (dr->data_a_buffer)[dr->write_a_position][i] = da;
    }
    (dr->write_a_position)++;
    if (dr->write_a_position == MAX_M_SIZE) // 写入位置若是在队列最后则重新设置为初始位置.
        dr->write_a_position = 0;
    (dr->not_empty_a).notify_all(); // 通知消费者产品库不为空.
}
// B 生产者
void ProducerB(data_resource *dr){
    std::unique_lock<std::mutex> lock(dr->mtx_b);
    while (((dr->write_b_position + 1) % MAX_M_SIZE)
           == dr->read_b_position){ // 生产者等待"产品库缓冲区不为满"
        (dr->not_full_b).wait(lock);
    }
    for(int i=0;i<MAX_N_SIZE;i++){
        DataB db{};
        nu.bNumberRandom(db.data);
        (dr->data_b_buffer)[dr->write_b_position][i] = db;
    }
    (dr->write_b_position)++;
    if (dr->write_b_position == MAX_M_SIZE) // 写入位置若是在队列最后则重新设置为初始位置.
        dr->write_b_position = 0;
    (dr->not_empty_b).notify_all(); // 通知消费者产品库不为空.
}
// C 消费者
void ConsumerC(data_resource *dr){
    double tmp=0;
    std::unique_lock<std::mutex> lock1(dr->mtx_b);
    while (dr->write_b_position == dr->read_b_position) {
        (dr->not_empty_b).wait(lock1);// 消费者等待"产品库缓冲区不为空"这一条件发生.
    }
    std::unique_lock<std::mutex> lock2(dr->mtx_a);
    while(dr->write_a_position == dr->read_a_position){
        (dr->not_empty_a).wait(lock2);// 消费者等待"产品库缓冲区不为空"这一条件发生.
    }
    for(int i=0;i<MAX_N_SIZE;i++){
        for (double d:(dr->data_b_buffer)[dr->read_b_position][i].data) {
            tmp+=d;
        }
        ((dr->data_a_buffer)[dr->read_a_position][i].a%2==0)? dr->sum_b_all += -1*tmp : dr->sum_b_all += tmp;
    }
}
```


##四、实验结果
####&#8195;&#8195;假设线程运行以时间间隔作为线程任务的执行周期标志
####&#8195;&#8195;为了便于观察，设置生产者和消费者每执行一次sleep三秒，而日志进程仍以1秒的间隔做打印,程序运行效果如下
![avatar](result.png)  
####&#8195;&#8195;去除生产和消费线程的sleep限制，模拟任务场景得到的结果如下:
![avatar](result2.png)