---
source: https://blog.csdn.net/weixin_39977547/article/details/114201043?spm=1001.2101.3001.6650.5&utm_medium=distribute.pc_relevant.none-task-blog-2%7Edefault%7EBlogCommendFromBaidu%7ERate-5-114201043-blog-114201047.235%5Ev38%5Epc_relevant_sort_base2&depth_1-utm_source=distribute.pc_relevant.none-task-blog-2%7Edefault%7EBlogCommendFromBaidu%7ERate-5-114201043-blog-114201047.235%5Ev38%5Epc_relevant_sort_base2
---
最近在项目中需要写一个数据转换引擎服务，每过5分钟同步一次数据。具体实现是启动engine server后会初始化一个ScheduledExecutorService和一个ThreadPoolExecutor[线程池](https://so.csdn.net/so/search?q=%E7%BA%BF%E7%A8%8B%E6%B1%A0&spm=1001.2101.3001.7020)。schduel executor每过5分钟将dataTransformList中每一个tranform加入到线程池中运行。每一个数据转化器负责转换一组数据库数据。在执行过程中存在服务重启并且此时tranform正在转换数据并且数据没有全部操作完，此时希望正在执行的work能正常完成作业后再退出。优雅停机在服务重启，服务关闭显得比较重要了(尽管不能解决服务器突然断电导致服务瞬间不可用等原因)。

普通的优雅停机：当使用kill PID的时候jvm会收到服务停止信号并执行shutdownHook的线程

Runtime.getRuntime().addShutdownHook(new Thread() {

public void run() {

synchronized (EngineBootstrap.class) {

EngineServer.getInstance().shutdown();

running = false;

EngineBootstrap.class.notify();

}

}

});

EngineServer的shutdown方法

public void shutdown() {

this.transformExecutor.shutdown();

this.scheduledExecutorService.shutdown();

}ThreadPoolExecutor的shutdown方法会中断所有的空闲任务，保持正在运行中的任务执行完毕，但是由于kill PID一段时间后jvm就退出了导致正在执行的任务还没有完成就停止了。

改进后的优雅停机：

Signal sig = new Signal(getOSSignalType());

Signal.handle(sig, new SignalHandler() {

public void handle(Signal signal) {

synchronized (EngineBootstrap.class) {

EngineServer.getInstance().shutdown();

running = false;

EngineBootstrap.class.notify();

}

}

});

private static String getOSSignalType() {

return System.getProperties().getProperty("os.name").

toLowerCase().startsWith("win") ? "INT" : "USR2";

}

linux下通过kill -l查看 31 对应于 SIGUSR2 执行kill -31 PID, SignalHander会接收到signal number为31的信号并执行server shutdown,此时jvm并不会退出直到线程池所有正在执行的线程全部执行完毕才会安全退出。

java -jar data-engine-1.0.0-SNAPSHOT.jar

sh shutdown.sh

可以看到主线程安全退出后，线程池中的work执行完毕后java进程才结束

chenbanghongs-MacBook-Pro:nbugs-data-engine sylar$ java -jar target/data-engine-1.0.0-SNAPSHOT.jar.zip

1

2

3

4

5

31

优雅停机

主线程安全退出

6

7

8

9

10

11

12

13

14

15

16

17

18

19

20

transform
