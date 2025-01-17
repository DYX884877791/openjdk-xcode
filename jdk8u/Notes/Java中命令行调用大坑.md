---
source: https://www.toutiao.com/article/6932793532562588168/?log_from=61401768a4f44_1690269647541
---
## **Java中命令行调用大坑**

## **背景**

我司有一个查询服务接口机，QPS大概40~50，调用方式是Java调用Shell命令行的方式，核心代码如下：

```
Process ps = Runtime.getRuntime().exec("your command");
ps.getInputStream();//处理输入流
```

以前用的好好的，最有一次直接把接口机堵死了，ssh都很难登陆上，登陆上去之后发现全是同一个java进程，起码得上百个，而且还有很多defunct僵尸进程，没办法只有重启服务器等了20分钟才恢复服务

后来才知道是接口机调用量上升了，因为有新的服务在调用，现在QPS大概在70~100左右，也就是说接口机只能承受住40~50 QPS，超过就会导致接口机堵住，后来经常堵住，从发现问题到不断尝试解决问题整个过程花了几个月的时间

承受不了更高的QPS意味着接口机该扩容了，但是有一个十分关键的问题是：接口机资源还够！每次刚开始堵住的时候都发现内存还剩5~6个G，而且随着流量不断打过来，内存100M，100M的降，最后降到只有几百M内存，而且全是同一个java进程，接口机便动弹不得

这明显是程序有问题而不是接口机资源不够的问题嘛，于是就开始排查程序

## **第一版**

第一版程序是这么写的

```
try {
   Process ps = Runtime.getRuntime().exec(vCmd);
   InputStream in = ps.getInputStream();
   in = new BufferedInputStream(in);
   StringBuffer buffer = new StringBuffer();
   while ((ptr = in.read()) != -1) {
       buffer.append((char) ptr);
   }
   return buffer.toString();
} catch (IOException e) {
    log.error(e);
}
return null;
```

就是很朴实无华的调用命令行而已，不要问我为什么非得调用命令行，因为这个服务很老旧，只能调命令行，调用命令行本来就是开销很大的操作，大量调用要尽量避免或改成其他调用

## **第二版(加线程池)**

第一版出问题之后想的是加一个线程池，让调用shell的代码放到线程池里面去执行，让线程池来控制资源，于是诞生了第二版

1.  定义线程池

```
public  static ThreadPoolTaskExecutor pool = new ThreadPoolTaskExecutor();

static {
    // 线程池维护线程的最少数量
    pool.setCorePoolSize(100);
    // 线程池维护线程的最大数量
    pool.setMaxPoolSize(600);
    pool.setQueueCapacity(50);
    pool.setKeepAliveSeconds(20);//除核心线程外的线程存活时间
    pool.setRejectedExecutionHandler(new ThreadPoolExecutor.AbortPolicy());
    pool.setThreadNamePrefix("THREAD-POOL-");
    pool.initialize();
}
```

2.  用线程池调用

```
Future<String> future = pool.submit(()-> {
    try {
        Process ps = Runtime.getRuntime().exec(vCmd);
        InputStream in = ps.getInputStream();
        in = new BufferedInputStream(in);
        StringBuffer buffer = new StringBuffer();
        while ((ptr = in.read()) != -1) {
            buffer.append((char) ptr);
        }
        return buffer.toString();
    } catch (IOException e) {
        log.error(e);
    }
    return null;
});

try {
    return future.get();
} catch (Exception e) {
    log.error("thread pool excetion -> {}",e.getMessage());
    future.cancel(true);
    return null;
}
```

运行了一段时间发现调用量一上来接口机还是会堵住，于是便放弃了线程池（后来想想其实线程池应该是可以的，只是我们设置的线程数量不对，QPS最大100，每个查询都算做1s，那么设置100个线程就应该足够了，大多数时候每个查询可能就200ms~500ms左右，线程数肯定要小于100的，上面设置的最大600，等于没限制住）

## **第三版(加超时)**

线程池方案失败之后我们一度认为是Java调用shell命令行不能设置超时导致的，如果某条查询超过了1s，那么就直接返回了，不继续查，于是有了第三版

这一版的核心在于调用ps.exitValue()这个非阻塞方法，它可以告诉我们shell命令是否执行完成，于是下面的while循环每隔100ms就去调用ps.exitValue()得知是否调用完成，如果超过了1s，则直接返回给调用者了，不进行调用

```
Process ps = Runtime.getRuntime().exec(vCmd);
long start = System.currentTimeMillis();
BufferedReader inputReader = null;
try {
    inputReader = new BufferedReader(new InputStreamReader(ps.getErrorStream()));
    boolean processFinished = false;
    StringBuilder sb = new StringBuilder();
    int cnt = 0;
    while (System.currentTimeMillis() - start < 1000 && !processFinished) {
        cnt ++;
        processFinished = true;
        long cuStart = System.currentTimeMillis();
        try {
            ps.exitValue();
        } catch (IllegalThreadStateException e) {
            // process hasn't finished yet
            processFinished = false;

            try {
                Thread.sleep(100);
            } catch (InterruptedException e1) {
                logger.error("Process, failed [" + e.getMessage() + "]", e);
            }
        }
    }


    if (!processFinished) {
        logger.error(" timeout used " +(System.currentTimeMillis() - start));
        return null;
    }
    
    
    String line;
    StringBuilder rtn = new StringBuilder();
    while (inputReader.ready() && ( (line = inputReader.readLine()) != null) ){
        rtn.append(line);
    }
    return rtn.toString();
} catch (Exception e) {
    String error = "Command process, failed [" + e.getMessage() + "]";
    logger.error(error, e);
} finally {
    if (inputReader != null) {
        try {
            inputReader.close();
        } catch (IOException e) {
            //ignore
        }
    }
}
return null;
```

后来发现这样这这是换汤不换药，接口机照常堵住，因为就算调用超时了返回给了调用者，但是命令行的操作还是在执行，一样会耗费系统资源，此时还加了shell脚本来监控进程数，如果太多就杀进程，但是一样效果不明显，shell脚本如下

```
#!/usr/bin/env bash

# 主线程ID
mainid=23105
# 最大线程阈值
max_size=5

while true;do
    pids=(`ps aux | grep '/serviceShell/conf/logging.properties' | grep -v 'grep' | grep -v $mainid | awk '{print $2}'`)
    pids_size=${#pids[@]}
    if [ $pids_size -gt $max_size ];then
        echo $(date +%F%n%T)
        # 打印内存信息
        free -m
        echo "当前主id是 $mainid"
        echo "当前线程个数为$pids_size 大于$max_size个，杀死线程："
        for pid in ${pids[@]}
        do
          if [[ "$pid" != "$mainid" ]];then
                echo $pid
                kill -9 $pid
          fi
        done
        # 打印内存信息
        free -m
    fi
    sleep 1
done
```

而且在堵住的时候发现这句话Process ps = Runtime.getRuntime().exec(vCmd);竟然要调用100多秒！后来想想这应该是调用量一上来，超过系统的负载，就会导致命令行调用很难申请到资源，一直在等操作系统排队处理

## **第四版(找到真正的瓶颈：缓冲区)**

经过上面几版的瞎折腾，我们似乎遗忘了最不正常的问题：接口机资源还够为什么调用命令行会卡住？究竟什么才是Java调用命令行的瓶颈？经过网上搜寻，发现JDK文档上关于Process有这么一段说明

> By default, the created subprocess does not have its own terminal or console. All its standard I/O (i.e. stdin, stdout, stderr) operations will be redirected to the parent process, where they can be accessed via the streams obtained using the methods getOutputStream(), getInputStream(), and getErrorStream(). The parent process uses these streams to feed input to and get output from the subprocess. Because some native platforms only provide limited buffer size for standard input and output streams, failure to promptly write the input stream or read the output stream of the subprocess may cause the subprocess to block, or even deadlock.
> 
> 中文翻译：
> 
> 默认情况下，创建的子进程没有自己的终端或控制台，子进程所有的标准IO操作会被重定向到父进程（也就是JVM），JVM里可以用getOutputStream()、getInputStream()和getErrorStream()来获取子进程的标准输出、输入和错误流
> 
> **下面重点来了**
> 
> 由于有些本机平台仅针对标准输入和输出流提供有限的缓冲区大小，当标准输出或者标准错误输出写满缓存池时，程序没法继续写入，子进程没法正常退出。读写子进程的输出流或输入流迅速出现失败，则可能致使子进程阻塞，甚至产生死锁。

所以瓶颈很有可能就是缓冲区太小！

后来还发现了Java命令行的框架Apache Commons Exec：https://commons.apache.org/proper/commons-exec/，它可以在命令行执行之后新开线程去及时消费子进程输入、输出和错误流里的数据，避免缓冲区阻塞或死锁

基于上面两点，第四版改动如下

## **使用Apache Commons Exec框架调用命令行**

```
long s = System.currentTimeMillis();
StringBuilder sb = new StringBuilder();
try {
    ByteArrayOutputStream outputStream = new ByteArrayOutputStream();
    ByteArrayOutputStream errorStream = new ByteArrayOutputStream();
    CommandLine commandline = CommandLine.parse(vCmd);
    
    //看门狗，可设置超时
    ExecuteWatchdog watchdog = new ExecuteWatchdog(1000);

    DefaultExecutor exec = new DefaultExecutor();
    exec.setExitValues(null);
    PumpStreamHandler streamHandler = new PumpStreamHandler(outputStream,errorStream);
    exec.setStreamHandler(streamHandler);
    exec.setWatchdog(watchdog);

    //调用命令行
    exec.execute(commandline);
    sb.append(" execute used " + (System.currentTimeMillis() - s + /*后面为0 赋值*/((s = System.currentTimeMillis())-s)  )  );

    //消费数据
    String out = outputStream.toString("gbk");
    String error = errorStream.toString("gbk");
    sb.append(" stream used " + (System.currentTimeMillis() - s + /*后面为0 赋值*/((s = System.currentTimeMillis())-s)  )  );
    return out;
} catch (Exception e) {
    logger.error(e.getMessage(),e);
    sb.append("  exception info "+e.getMessage()+" used " + (System.currentTimeMillis() - s + /*后面为0 赋值*/((s = System.currentTimeMillis())-s)  )  );
    return "F";
} finally {
    logger.info(" exec info " + sb.toString());
}
```

通过查看Apache Commons Exec的源码发现它就是每次调用就新开线程去处理三个流的

```
try {
    streams.setProcessInputStream(process.getOutputStream());
    streams.setProcessOutputStream(process.getInputStream());
    streams.setProcessErrorStream(process.getErrorStream());
} catch (final IOException e) {
    process.destroy();
    throw e;
}

protected Thread createPump(final InputStream is, final OutputStream os, final boolean closeWhenExhausted) {
    //此处新开线程
    final Thread result = new Thread(new StreamPumper(is, os, closeWhenExhausted), "Exec Stream Pumper");
    result.setDaemon(true);
    return result;
}
```

通过上面的操作解决了单个JVM的缓冲区可能出现的阻塞/死锁问题

## **使用Nginx负载均衡**

这一步很简单但是也很重要，这是解决瓶颈的根本，一个JVM缓冲区太小，咱来两个JVM，来三个JVM不就行了？Nginx负载均衡核心配置如下：

```
#user  nobody;

worker_processes  4;
worker_cpu_affinity 0001 0010 0100 1000;
worker_rlimit_nofile 65536;

error_log  logs/error.log;

events {
    use epoll;
    worker_connections  65536;
    multi_accept on;
}


http {
    include       mime.types;
    default_type  application/octet-stream;

    access_log logs/access.log main;


    //后端三个JVM
    upstream app_backend {
        server 127.0.0.1:8081 weight=1 max_fails=3 fail_timeout=300;
        server 127.0.0.1:8082 weight=1 max_fails=3 fail_timeout=300;
        server 127.0.0.1:8083 weight=1 max_fails=3 fail_timeout=300;
    }

    server {
        listen       80;
        server_name  localhost;


        location ~ \/(app) {
            proxy_set_header X-real-ip $remote_addr;
            proxy_set_header REMOTE-HOST $remote_addr;
            proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
            proxy_set_header Host $host;
            proxy_connect_timeout 300;
            proxy_read_timeout 300;
            proxy_send_timeout 300;
            proxy_pass http://app_backend;
            client_max_body_size  10m;
            proxy_http_version 1.1;
            proxy_set_header Connection "";
            limit_conn addr 100;
            limit_rate 10000k;
        }
        #error_page  404              /404.html;

        # redirect server error pages to the static page /50x.html
        error_page 400 404 413  500 502 503 504  /50x.html;
        location = /50x.html {
            root   html;
        }
    }
}
```

## **总结**

如果是调用量很大的服务一般不建议采用Java调命令行，如果费要调用的话注意下面两个问题

-   单个JVM注意处理好子进程的输出、输入和错误三个流，避免单JVM缓冲区阻塞或者死锁，使用Apache Commons Exec
-   如果单个JVM支撑不了调用，并且服务器资源剩余很多的话可以考虑用Nginx负载均衡，将单机单JVM变成单机多JVM
