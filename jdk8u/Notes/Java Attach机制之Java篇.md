---
source: https://www.jianshu.com/p/7a6e873aa93d
---
## Java Attach机制之Java篇

[![](https://cdn2.jianshu.io/assets/default_avatar/14-0651acff782e7a18653d7530d6b27661.jpg)](https://www.jianshu.com/u/8e688d954134)

2019.05.07 23:39:52字数 457阅读 996

Java Attach机制，初次接触是长成以下这样的：

VirtualMachine.attach(pid);

当时只是惊叹于java提供了这样强大的Api，但是不知道背后究竟发生了什么；带着一种程序员天生具备的使命感开始进行了探索之旅，最终跟进去后其实是下面一段Java代码：

```
 BsdVirtualMachine(AttachProvider var1, String var2) throws AttachNotSupportedException, IOException {
        super(var1, var2);

        int var3;
        try {
            var3 = Integer.parseInt(var2);
        } catch (NumberFormatException var22) {
            throw new AttachNotSupportedException("Invalid process identifier");
        }

        this.path = this.findSocketFile(var3);
        if(this.path == null) {
            File var4 = new File(tmpdir, ".attach_pid" + var3);
            createAttachFile(var4.getPath());

            try {
                sendQuitTo(var3);
                int var5 = 0;
                long var6 = 200L;
                int var8 = (int)(this.attachTimeout() / var6);

                do {
                    try {
                        Thread.sleep(var6);
                    } catch (InterruptedException var21) {
                        ;
                    }

                    this.path = this.findSocketFile(var3);
                    ++var5;
                } while(var5 <= var8 && this.path == null);

                if(this.path == null) {
                    throw new AttachNotSupportedException("Unable to open socket file: target process not responding or HotSpot VM not loaded");
                }
            } finally {
                var4.delete();
            }
        }

        checkPermissions(this.path);
        int var24 = socket();

        try {
            connect(var24, this.path);
        } finally {
            close(var24);
        }

    }
```

然后解析下上面这段代码：  
首先去查询socket文件，如果文件不存在，就会再去创建一个.attach_pid<pid>相关的文件，然后向这个<pid>的进程发送了SIGQUIT命令，接着进了一个循环中，不断等待一段时间，并且去检查socket文件的存在性，一旦超时后再没有等到socket文件的出现，则会抛出“Unable to open socket file: target process not responding or HotSpot VM not loaded”的异常了；但是如果socket文件此时存在了或者说从刚开始的时候socket文件就一直存在了，那么就会去直接检查socket文件的权限，然后进行socket连接，最后关闭socket连接；其实到这里，也只是稍微描述了这个attach的过程，但是其实过程中还是有很多疑问点没有去跟踪到的：  
1.这个.attach_pid<pid>文件是干嘛的？  
2.为什么要去等待这个socket文件的生成，这个文件生成了会怎么样？  
3.为什么还会向<pid>这个进程发送SIGQUIT命令呢，怎么看出来是发送SIGQUIT命令的呢？  
要解决以上的问题，就要再次深入Native的代码去看底层实现的细节，会在下一篇《Java Attach机制之Native篇》中跟踪到

最后编辑于

：2019.05.07 23:54:03

更多精彩内容，就在简书APP

![](https://upload.jianshu.io/images/js-qrc.png)

"小礼物走一走，来简书关注我"

还没有人赞赏，支持一下

[![  ](https://cdn2.jianshu.io/assets/default_avatar/14-0651acff782e7a18653d7530d6b27661.jpg)](https://www.jianshu.com/u/8e688d954134)

-   序言：七十年代末，一起剥皮案震惊了整个滨河市，随后出现的几起案子，更是在滨河造成了极大的恐慌，老刑警刘岩，带你破解...
    
-   1. 周嘉洛拥有一个不好说有没有用的异能。 异能管理局的人给他的异能起名为「前方高能预警」。 2. 周嘉洛第一次发...
    
-   序言：滨河连续发生了三起死亡事件，死亡现场离奇诡异，居然都是意外死亡，警方通过查阅死者的电脑和手机，发现死者居然都...
    
-   文/潘晓璐 我一进店门，熙熙楼的掌柜王于贵愁眉苦脸地迎上来，“玉大人，你说我怎么就摊上这事。” “怎么了？”我有些...
    
-   文/不坏的土叔 我叫张陵，是天一观的道长。 经常有香客问我，道长，这世上最难降的妖魔是什么？ 我笑而不...
    
-   正文 为了忘掉前任，我火速办了婚礼，结果婚礼上，老公的妹妹穿的比我还像新娘。我一直安慰自己，他们只是感情好，可当我...
    
    [![](https://upload.jianshu.io/users/upload_avatars/4790772/388e473c-fe2f-40e0-9301-e357ae8f1b41.jpeg?imageMogr2/auto-orient/strip|imageView2/1/w/48/h/48/format/webp)茶点故事](https://www.jianshu.com/u/0f438ff0a55f)阅读 12,886评论 0赞 101
    
-   文/花漫 我一把揭开白布。 她就那样静静地躺着，像睡着了一般。 火红的嫁衣衬着肌肤如雪。 梳的纹丝不乱的头发上，一...
    
-   那天，我揣着相机与录音，去河边找鬼。 笑死，一个胖子当着我的面吹牛，可吹牛的内容都是我干的。 我是一名探鬼主播，决...
    
-   文/苍兰香墨 我猛地睁开眼，长吁一口气：“原来是场噩梦啊……” “哼！你这毒妇竟也来了？” 一声冷哼从身侧响起，我...
    
-   想象着我的养父在大火中拼命挣扎，窒息，最后皮肤化为焦炭。我心中就已经是抑制不住地欢快，这就叫做以其人之道，还治其人...
    
-   序言：老挝万荣一对情侣失踪，失踪者是张志新（化名）和其女友刘颖，没想到半个月后，有当地人在树林里发现了一具尸体，经...
    
-   正文 独居荒郊野岭守林人离奇死亡，尸身上长有42处带血的脓包…… 初始之章·张勋 以下内容为张勋视角 年9月15日...
    
    [![](https://upload.jianshu.io/users/upload_avatars/4790772/388e473c-fe2f-40e0-9301-e357ae8f1b41.jpeg?imageMogr2/auto-orient/strip|imageView2/1/w/48/h/48/format/webp)茶点故事](https://www.jianshu.com/u/0f438ff0a55f)阅读 5,602评论 1赞 91
    
-   正文 我和宋清朗相恋三年，在试婚纱的时候发现自己被绿了。 大学时的朋友给我发了我未婚夫和他白月光在一起吃饭的照片。...
    
    [![](https://upload.jianshu.io/users/upload_avatars/4790772/388e473c-fe2f-40e0-9301-e357ae8f1b41.jpeg?imageMogr2/auto-orient/strip|imageView2/1/w/48/h/48/format/webp)茶点故事](https://www.jianshu.com/u/0f438ff0a55f)阅读 6,028评论 0赞 80
    
-   白月光回国，霸总把我这个替身辞退。还一脸阴沉的警告我。[不要出现在思思面前， 不然我有一百种方法让你生不如死。]我...
    
-   序言：一个原本活蹦乱跳的男人离奇死亡，死状恐怖，灵堂内的尸体忽然破棺而出，到底是诈尸还是另有隐情，我是刑警宁泽，带...
    
-   正文 年R本政府宣布，位于F岛的核电站，受9级特大地震影响，放射性物质发生泄漏。R本人自食恶果不足惜，却给世界环境...
    
    [![](https://upload.jianshu.io/users/upload_avatars/4790772/388e473c-fe2f-40e0-9301-e357ae8f1b41.jpeg?imageMogr2/auto-orient/strip|imageView2/1/w/48/h/48/format/webp)茶点故事](https://www.jianshu.com/u/0f438ff0a55f)阅读 6,251评论 3赞 77
    
-   文/蒙蒙 一、第九天 我趴在偏房一处隐蔽的房顶上张望。 院中可真热闹，春花似锦、人声如沸。这庄子的主人今日做“春日...
    
-   文/苍兰香墨 我抬头看了看天上的太阳。三九已至，却和暖如春，着一层夹袄步出监牢的瞬间，已是汗流浃背。 一阵脚步声响...
    
-   我被黑心中介骗来泰国打工， 没想到刚下飞机就差点儿被人妖公主榨干…… 1. 我叫王不留，地道东北人。 一个月前我还...
    
-   正文 我出身青楼，却偏偏与公主长得像，于是被迫代替她去往敌国和亲。 传闻我的和亲对象是个残疾皇子，可洞房花烛夜当晚...
    
    [![](https://upload.jianshu.io/users/upload_avatars/4790772/388e473c-fe2f-40e0-9301-e357ae8f1b41.jpeg?imageMogr2/auto-orient/strip|imageView2/1/w/48/h/48/format/webp)茶点故事](https://www.jianshu.com/u/0f438ff0a55f)阅读 7,509评论 1赞 99
    

### 推荐阅读[更多精彩内容](https://www.jianshu.com/)

-   本文转自http://www.fanyilun.me/2017/07/18/%E8%B0%88%E8%B0%88J...
    
    [![](https://upload.jianshu.io/users/upload_avatars/5387388/514320cf-634b-4dbf-9bfc-e7699d83ac92.jpg?imageMogr2/auto-orient/strip|imageView2/1/w/48/h/48/format/webp)一帅](https://www.jianshu.com/u/56a64219b193)阅读 1,944评论 0赞 9
    
-   0 前言 前面文章，我们已讲述了《基于JVMTI的Agent实现》和《基于Java Instrument的Agen...
    
    [![](https://upload.jianshu.io/users/upload_avatars/4892983/a806a511-11f1-492c-a5a5-df0fb243b03a?imageMogr2/auto-orient/strip|imageView2/1/w/48/h/48/format/webp)西华子](https://www.jianshu.com/u/34102f334639)阅读 684评论 1赞 0
    
-   0 前言 前面文章，我们已讲述了《基于JVMTI的Agent实现》和《基于Java Instrument的Agen...
    
    [![](https://upload.jianshu.io/users/upload_avatars/2062729/404b7397-cbe2-40cf-8f36-6c8d019c9788.jpeg?imageMogr2/auto-orient/strip|imageView2/1/w/48/h/48/format/webp)七寸知架构](https://www.jianshu.com/u/657c611b2e07)阅读 3,092评论 0赞 50
    
-   一、什么是Attach机制？ 简单点说就是jdk的一些工具类提供的一种jvm进程间通信的能力，能让一个进程传命令给...
    
    [![](https://cdn2.jianshu.io/assets/default_avatar/2-9636b13945b9ccf345bc98d0d81074eb.jpg)侠客杨歌](https://www.jianshu.com/u/b40e57a6f9fd)阅读 9,371评论 0赞 6
    
-   从JDK6开始引入，除了Solaris平台的Sun JVM支持远程的Attach，在其他平台都只允许Attach到...
