# SPONGE: MIT CS144

an demo implementation for TCP/IP/Ethernet, repo for learning mit cs144

### Full Resource:

https://cs144.github.io/

### Lab WriteUp

There exist 8 labs totally, which will give you a chance to implement a complete Internet infrastructure step by step;

- Lab 0: Environment configuration and ByteStream
- Lab 1: StreamReassembler
- Lab 2: TCP Receiver
- Lab 3: TCP Sender
- Lab 4: TCP Connection
- Lab 5: Network Interface
- Lab 6: IP routing
- Lab 7: Put all together

##### PreWork:

强烈建议学习Modern C++的基本语法，推荐[这本书](https://changkun.de/modern-cpp/zh-cn/00-preface/index.html#%E5%BC%95%E8%A8%80)
的前五章看懂了即可，配合c++的基础知识即可完成，一定要注意 shared_ptr的问题，框架的代码写的很nice，如果不懂modern c++，基本上是看不懂的。

配置linux开发环境，这里不再多提。

##### Lab 0:

我使用了wsl2+clion 作为开发环境，事实证明除了开发体验非常好之外没什么别的优点，测试的时候wsl2奇怪的**virtual network**已经给人整自闭了，如果有双系统，强烈建议使用双系统 or 虚拟机 or wsl1。
整个配置安装基本没什么难点，参照这个[bash](https://web.stanford.edu/class/cs144/vm_howto/setup_dev_env.sh) 安点软件就ok，注意gcc需要8.0以上。

ByteStream代码没有什么难点，不到100行就解决问题，预计时间：2h以内。

##### Lab 1:

**StreamReassembler** 实现对乱序接收字符串的重新排序，我的实现效率比较低，使用容器将未顺序的段存起来，每次接收到新的段时尝试合并成最大的段，同时判断是否能继续滑动窗口即可。

该实验的难点一个是真正理解EOF作用，每次接收到FIN时更新EOF_POS，及时判断何时EOF。另一个是理解什么是窗口？个人理解就是[pos, pos+ Cap - ByteStream.size()]
这个剩余的范围才是可以接受的，如果[index,index+data.size()] 部分或者全部落到了这个范围之外，扔掉之外的部分，具体可以参考代码。预计时间：2h以内。

##### Lab 2:

**WrappingInt** 实现对isn的包装和拆包，其实可以把tcp流看成是一串字符串，只不过字符串的第一个字母是特殊字符SYN，最后一个字符是FIN，那么isn的relative和absolute就非常好转换了，注意+2(
SYN/FIN)
+1(SYN) 的问题。

**TCPReceiver**
利用前两个类实现接收方，注意不用理会没有连接之前/断开连接之后的包，以及及时根据StreamReassembler判断是否FIN已经进入顺序接收的ByteStream（此时ack号要算FIN）。预计时间：4h以内。边界条件非常多。。。

##### Lab 3：

**TCPSender**
所有部分中最难的那部分，整体代码量只有150行，但是实验指导前前后后我读了4遍以上。。需要理解的是，fill_windows干了所有发包的动作，包括SYN/FIN包，也就是说只要该类被创建，就意味着waiting状态，第一次fill_windows直接发送SYN。
另一个坑：shared_ptr，不多说，建议阅读Buffer和BufferList代码。我被这个坑坑了整整一天时间。。。预计时间：一天

##### Lab 4:

**TCPConnection** 也比较难，实验指导一定认真仔细多读几遍，很多实现细节藏在其中。需要注意：在tick的时候发包或者每个函数中发包都是被允许的，都可以过CI。我绝大多数的时间都花到了理解到底什么时候active，什么时候no
active，具体细节请看代码。

重要：该部分测试涉及TUN设备的创建与使用，请在根目录下使用授予执行权限

~~~shell
chmod +x tun.sh
chmod +x txrx.sh
~~~

同时，务必使用非root用户，build后check，否则会出现各种奇怪的问题。另一方面请将CRLF->LF，wsl与git看起来配合的不是很好.jpg，我clone下来全是CRLF。。。可以使用vim中的命令解决

~~~shell
set ff=unix
~~~

这个check也折磨了我很久。。预计时间：5h以内

##### Lab 5:

**NetworkInterface**: 可以理解为一个二层设备（交换机/网桥），本实验中只负责处理包装IP包和ARP包，非常简单和前面相比。注意一点，实验指导中没有说明一点：如果此次发送IP包目的IP对应的MAC地址cache
miss，同时距离上次相同目的IP cache miss 发送ARP 的 时间超过了5000ms，则重发ARP。 其他就非常简单了。预计时间：2h

##### Lab 6:

**Router** :可以理解为一个三层设备（路由器），本实验中就一个根据路由表进行路由的需求，比较简单，代码量20行，注意一定要特判 0.0.0.0/0。。预计时间：1h

### Sponge quickstart

To set up your build directory:

    $ mkdir -p <path/to/sponge>/build
    $ cd <path/to/sponge>/build
    $ cmake ..

**Note:** all further commands listed below should be run from the `build` dir.

To build:

    $ make

To test (after building; make sure you've got the [build prereqs](https://web.stanford.edu/class/cs144/vm_howto)
installed!)

    $ make check_labN *(replacing N with a checkpoint number)*

在我的实现中，通过了所有测试点，除了webget由于wsl2的原因无法进行测试，lab7中的测试运行的非常好，借助cs144.keithw.org本地可以正常进行通信。 以一张图结尾
![detail](https://pic.raynor.top/images/2022/01/20/image-20220120225331531.png)