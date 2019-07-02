Abstract

Modern data-center applications often comprise a large amount of code, with substantial working sets, making them good candidates for code-layout optimizations. Although recent work has evaluated the impact of profile-guided intramodule optimizations and some cross-module optimizations, no recent study has evaluated the benefit of function placement for such large-scale applications. In this paper, we study the impact of function placement in the context of a simple tool we created that uses sample-based profiling data. By using sample-based profiling, this methodology follows the same principle behind AutoFDO, i.e. using profiling data collected from unmodified binaries running in production, which makes it applicable to large-scale binaries. Using this tool, we first evaluate the impact of the traditional Pettis-Hansen (PH) function-placement algorithm on a set of widely deployed data-center applications. Our experiments show that using the PH algorithm improves the performance of the studied applications by an average of 2.6%. In addition to that, this paper also evaluates the impact of two improvements on top of the PH technique. The first improvement is a new algorithm, called C3, which addresses a fundamental weakness we identified in the PH algorithm. We not only qualitatively illustrate how C3 overcomes this weakness in PH, but also present experimental results confirming that C3 performs better than PH in practice, boosting the performance of our workloads by an average of 2.9% on top of PH. The second improvement we evaluate is the selective use of huge pages. Our evaluation shows that, although aggressively mapping the entire code section of a large binary onto huge pages can be detrimental to performance, judiciously using huge pages can further improve performance of our applications by 2.0% on average.

现代数据中心应用程序通常包含大量代码，具有大量工作集，使其成为代码布局优化的良好候选者。虽然最近的工作已经评估了配置文件引导的模块内优化和一些跨模块优化的影响，但是最近的研究没有评估这种大规模应用的功能放置的好处。在本文中，我们研究了函数放置在我们创建的使用基于样本的分析数据的简单工具的上下文中的影响。通过使用基于样本的分析，该方法遵循AutoFDO背后的相同原理，即使用从生产中运行的未修改二进制文件收集的分析数据，这使其适用于大规模二进制文件。使用此工具，我们首先评估传统的Pettis-Hansen（PH）功能放置算法对一组广泛部署的数据中心应用程序的影响。我们的实验表明，使用PH算法可以将研究应用的性能平均提高2.6％。除此之外，本文还评估了两项改进对PH技术的影响。第一个改进是一种名为C3的新算法，它解决了我们在PH算法中发现的一个基本弱点。我们不仅定性地说明了C3如何克服PH中的这一弱点，而且还提供了实验结果，证实C3在实践中表现优于PH，在PH之上平均提高了我们的工作负载性能2.9％。我们评估的第二个改进是选择性地使用大页面。我们的评估表明，尽管将大型二进制文件的整个代码部分积极地映射到大页面上可能会对性能产生不利影响，但明智地使用大页面可以进一步提高应用程序的性能平均2.0％。

1.Introduction

Modern server workloads are large and complex programs that have been highly tuned over the course of their development. As a result, many such applications lack obvious “hot spots” that an engineer can optimize to deliver large overall performance improvements. Instead, the sheer volume of code that must be executed can be a bottleneck for the system. As a result, code locality is a relevant factor for performance of such systems.

现代服务器工作负载是大型复杂程序，在开发过程中经过高度调整。因此，许多此类应用程序缺少明显的“热点”，工程师可以对其进行优化以提供大的整体性能改进。相反，必须执行的大量代码可能是系统的瓶颈。结果，代码局部性是这种系统性能的相关因素。

While the large size and performance criticality of such applications make them good candidates for profile-guided code-layout optimizations, these characteristics also impose scalability challenges to optimize these applications. Instrumentation-based profilers significantly slow down the applications, often making it impractical to gather accurate profiles from a production system. To simplify deployment, it is beneficial to have a system that can profile unmodified binaries, running in production, and use these data for feedback-directed optimization. This is possible through the use of sample-based profiling, which enables high-quality profiles to be gathered with minimal operational complexity. This is the approach taken by tools such as AutoFDO [1], and which we also follow in this work.

虽然此类应用程序的大尺寸和性能关键性使其成为配置文件引导的代码布局优化的良好候选者，但这些特性也会对优化这些应用程序带来可扩展性挑战。基于仪器的分析器显着减慢了应用程序的速度，通常使从生产系统收集准确的配置文件变得不切实际。为了简化部署，有一个系统可以分析未经修改的二进制文件，在生产中运行，并使用这些数据进行反馈导向的优化是有益的。这可以通过使用基于样本的分析来实现，该分析能够以最小的操作复杂性收集高质量的分布。这是AutoFDO [1]等工具采用的方法，我们也在这项工作中遵循。

The benefit of feedback-directed optimizations for some data-center applications has been evaluated in some previous work, including AutoFDO [1] and LIPO [2]. Chen et al. [1] evaluated the impact of intra-module feedbackdirected optimizations, while Li et al. [2] evaluated the impact of some cross-module optimizations, in particular inlining and indirect-call promotion. However, no recent work has evaluated the benefit of function placement for largescale data-center applications.

一些数据中心应用的反馈导向优化的好处已经在一些以前的工作中得到了评估，包括AutoFDO [1]和LIPO [2]。陈等人。 [1]评估了模块内反馈定向优化的影响，而Li等人。 [2]评估了一些跨模块优化的影响，特别是内联和间接呼叫促销。但是，最近的工作没有评估大型数据中心应用程序的功能放置的好处。

In this paper, we demonstrate the benefit of optimizing function placement for these large-scale server applications. By default, the linker places functions according to the order the object files are specified in the command line, with no particular order within each object file. This arbitrary layout disperses the hot code across the text section, which reduces the efficiency of caches and TLBs. The potential to improve the function order and thus the performance of a binary was demonstrated by Pettis and Hansen [3]. In this work, we first evaluate the performance impact of their technique on a set of widely deployed data-center applications, and then show the impact of two improvements on top of this traditional technique.

在本文中，我们展示了优化这些大规模服务器应用程序的功能放置的好处。默认情况下，链接器根据命令行中指定目标文件的顺序放置函数，每个目标文件中没有特定的顺序。这种任意布局将热代码分散到文本部分，这降低了高速缓存和TLB的效率。 Pettis和Hansen [3]证明了改善函数顺序以及二进制性能的潜力。在这项工作中，我们首先评估其技术对一组广泛部署的数据中心应用程序的性能影响，然后展示两种改进对这种传统技术的影响。

Our study is conducted in the context of hfsort, which is a simple tool we created to sort the functions in a binary. Our methodology was designed to be simple enough to be applied to large-scale production systems with little friction. Like AutoFDO [1], this is achieved by leveraging samplebased profiling.

我们的研究是在hfsort的上下文中进行的，hfsort是我们创建的一个简单工具，用于对二进制函数进行排序。我们的方法设计简单，足以应用于摩擦很小的大型生产系统。与AutoFDO [1]一样，这是通过利用基于样本的分析来实现的。

Overall, this paper makes the following contributions: 

it evaluates the impact of Pettis and Hansen’s traditional function-ordering algorithm on a set of widely deployed data-center applications;

it identifies an opportunity for potential improvement over Pettis and Hansen’s algorithm, and then describes a novel algorithm based on this insight;

it describes a simple, user-level approach to leverage huge pages for a program’s text section on Linux;

it experimentally evaluates the aforementioned techniques, demonstrating measurable performance improvements on our set of data-center applications.

总的来说，本文做出以下贡献：

它评估了Pettis和Hansen的传统功能排序算法对一组广泛部署的数据中心应用程序的影响;

它确定了比Pettis和Hansen算法有潜在改进的机会，然后描述了一种基于这种洞察力的新算法;

它描述了一种简单的用户级方法，可以在Linux上为程序的文本部分利用大页面;

它通过实验评估上述技术，展示了我们的数据中心应用程序的可衡量的性能改进。

This paper is organized as follows.We start by describing the applications studied in this paper and some key performance characteristics in Section 2. Then Section 3 presents an overview of our methodology for improving code layout, followed by a description of techniques for building a dynamic call graph (Section 4) and for sorting the functions (Section 5). Section 6 then describes our technique for leveraging huge pages for the text section on Linux. A thorough evaluation of our techniques on four widely deployed server applications is presented in Section 7. Finally, related work is discussed in Section 8 and Section 9 concludes the paper.

本文的结构如下：我们首先描述本文研究的应用程序和第2节中的一些关键性能特征。然后，第3节概述了我们改进代码布局的方法，然后介绍了构建动态的技术。调用图（第4节）和函数排序（第5节）。然后，第6节描述了我们在Linux上为文本部分利用大页面的技术。第7节介绍了我们对四种广泛部署的服务器应用程序的技术的全面评估。最后，第8节讨论了相关的工作，第9节总结了本文。

2.Studied Applications

In order to demonstrate the importance of both code locality and the proposed techniques for server applications, this paper focuses on four systems that account for a large portion of the computing cycles spent to run some of the most popular websites in the world. The first of these systems is the HipHop Virtual Machine (HHVM) [4], which is the PHP and Hack execution engine powering many servers across the Internet, including three of the top ten websites in the world: Facebook, Wikipedia, and Baidu [5]. The second system evaluated in this paper is TAO [6], a highly distributed, inmemory, data-caching service used at Facebook. The other two systems are AdIndexer, an ads-related service, and Multifeed Aggregator, a service used to determine what is shown in the Facebook News Feed.

为了证明代码局部性和提出的服务器应用技术的重要性，本文重点介绍了四个系统，这些系统占据了运行世界上一些最受欢迎网站所花费的大部分计算周期。这些系统中的第一个是HipHop虚拟机（HHVM）[4]，它是PHP和Hack执行引擎，为互联网上的许多服务器供电，包括世界十大网站中的三个：Facebook，维基百科和百度[ 5]。本文评估的第二个系统是TAO [6]，这是一种在Facebook上使用的高度分布式的内存数据缓存服务。另外两个系统是AdIndexer，一种与广告相关的服务，以及Multifeed Aggregator，一种用于确定Facebook新闻Feed中显示内容的服务。

These four applications contain between 70 and 199 MB of program text. In the case of HHVM, which includes a JIT compiler, there is an even larger portion of dynamically generated code. Note, however, that the function-ordering techniques studied in this paper are applied statically and thus do not impact the dynamically generated code in HHVM. For running Facebook, the breakdown of HHVM’s execution time is 70% in static code and 30% in dynamic code. For all these applications, while the majority of the time is spent in smaller, hot portions of the code, there is an enormous tail of lukewarm code that executes with moderate frequency. This long tail competes for I-TLB, I-cache, and LLC space with the hot code. The resulting cache pressure from this large code footprints leads to high miss rates at all levels.

这四个应用程序包含70到199 MB的程序文本。对于包含JIT编译器的HHVM，动态生成的代码中有更大部分。但请注意，本文研究的函数排序技术是静态应用的，因此不会影响HHVM中动态生成的代码。对于运行Facebook，HHVM的执行时间在静态代码中为70％，在动态代码中为30％。对于所有这些应用程序，虽然大部分时间都花费在代码的较小，较热的部分上，但是存在一个以温和频率执行的不冷不热的代码。这条长尾用热门代码竞争I-TLB，I-cache和LLC空间。由此大代码占用空间产生的高速缓存压力导致所有级别的高失误率。

Frequent misses stall the processor front end and limit opportunities for out-of-order scheduling, leading to low instructions-per-cycle (IPC). Table 1 shows the binary size and cache performance of the studied applications. For example, without the optimizations studied in this paper, HHVM suffers 29.7 I-cache misses and 1.3 I-TLB misses per thousand instructions, and processes only 0.53 instructions per cycle. These high miss rates indicate that the processor’s resources are significantly underutilized due to frequent front-end stalls.

频繁的未命中使处理器前端失速并限制了无序调度的机会，导致每周期低指令（IPC）。表1显示了所研究应用程序的二进制大小和缓存性能。例如，如果没有本文研究的优化，HHVM每千条指令会遭受29.7个I-cache未命中和1.3个I-TLB未命中，并且每个周期仅处理0.53个指令。这些高失误率表明由于频繁的前端停顿，处理器的资源显着未得到充分利用。

3.System Overview

This section gives an overview of the methodology used in this paper to improve binary layout. One of the main design goals of this methodology was to be practical enough to be used in real, large-scale production systems. Figure 1 illustrates the steps and components of this methodology.

本节概述了本文中用于改进二进制布局的方法。该方法的主要设计目标之一是足够实用，可用于实际的大规模生产系统。图1说明了该方法的步骤和组件。

The first step in our methodology is to collect profile data. To do so, we use production servers running unmodified binaries. We select a small set of loaded servers for profiling, and we use a sampling-based tool (the Linux perf utility) to gather profile data. The perf tool uses hardware performance counters and interrupts the process at the specified intervals to collect profile data. As described in Section 4, we use the instructions perf event to collect profile data (either last-branch records or stack traces) at regular intervals measured in number of dynamic instructions executed.

我们方法的第一步是收集个人资料数据。为此，我们使用运行未修改二进制文件的生产服务器。我们选择一小组加载的服务器进行分析，我们使用基于采样的工具（Linux perf实用程序）来收集配置文件数据。 perf工具使用硬件性能计数器并以指定的时间间隔中断进程以收集概要文件数据。如第4节所述，我们使用指令perf事件以定期间隔收集配置文件数据（最后分支记录或堆栈跟踪），以执行的动态指令数量来衡量。

The profile data is fed into a tool that generates an optimized list of hot functions. The tool we built to sort the hot functions is called hfsort, and it is available as open source [7]. This tool starts by processing the profile data to build a dynamic call graph, as described in Section 4. This profile-based call graph is the input to the layout algorithm, which uses the node and edge weights to determine an appropriate ordering for the functions. The layout algorithm proposed in this paper is described in detail in Section 5.2. To enable the actual function reordering by the linker, the program is compiled using gcc -ffunction-sections, which places each function in an appropriately named ELF section [8]. The hfsort tool then generates a customized linker script, which directs the linker to place sections (i.e. functions) in a specific order. Because the linker may employ identical code folding to reduce code size by aliasing functions with identical bodies, the generated linker script should list all aliased functions to ensure proper placement.

配置文件数据被输入到生成热功能优化列表的工具中。我们为热函数排序而构建的工具叫做hfsort，它可以作为开源[7]使用。该工具首先处理配置文件数据以构建动态调用图，如第4节所述。这个基于配置文件的调用图是布局算法的输入，它使用节点和边权重来确定函数的适当顺序。 。本文提出的布局算法将在5.2节中详细介绍。要通过链接器启用实际的函数重新排序，程序是使用gcc -ffunction-sections编译的，它将每个函数放在一个适当命名的ELF部分[8]中。然后，hfsort工具生成一个自定义链接描述文件，该脚本指示链接器按特定顺序放置部分（即函数）。因为链接器可以使用相同的代码折叠来通过使用相同的主体别名函数来减少代码大小，所以生成的链接描述文件应列出所有别名函数以确保正确放置。

This methodology is attractive from a deployment perspective for two main reasons. First, it does not require a special profiling build. Instead, profile data is collected with negligible overhead on unmodified binaries running in their regular production environments. Second, this methodology uses simple, off-the-shelf tools to perform the optimization. perf is a standard utility, and so are linkers such as gold [9] with support for a custom script. The function ordering can be performed by an external utility such as hfsort, and need not be explicitly supported by the linker or the compiler.

从部署的角度来看，这种方法很有吸引力，原因有两个。首先，它不需要特殊的分析构建。相反，在其常规生产环境中运行的未修改二进制文件上收集的配置文件数据可忽略不计。其次，该方法使用简单的现成工具来执行优化。 perf是一个标准的实用程序，像gold [9]这样的连接器也支持自定义脚本。函数排序可以由外部实用程序（如hfsort）执行，并且不需要链接器或编译器明确支持。

4.Building the Call Graph

Application binaries are normally constructed around the concept of functions (or procedures) from a higher-level programming language. The binary code for each function is normally generated contiguously in the binary, and transitions between functions are performed via function calls and returns. In this work, we focus on improving the locality of these transitions among the functions at runtime. To achieve this goal, it is natural to use a call graph representation of the binary. A call graph G = (V,A) contains a set of nodes V , each associated with a corresponding function f in the binary, and also a set of arcs A, where each arc f->g represents the fact that function f calls function g. In order to represent the dynamic occurrence of transitions between functions, we use a weighted call graph. That is, associated with each arc f->g, there is a weight w(f->g) representing the number of times that function f calls g at runtime.

应用程序二进制文件通常围绕来自更高级编程语言的函数（或过程）的概念构建。每个函数的二进制代码通常在二进制文件中连续生成，函数之间的转换通过函数调用和返回来执行。在这项工作中，我们专注于在运行时改进函数之间的这些转换的局部性。为了实现这一目标，使用二进制的调用图表示是很自然的。调用图G =（V，A）包含一组节点V，每个节点与二进制中的相应函数f相关联，还包含一组弧A，其中每个弧f-> g表示函数f调用的事实功能g。为了表示函数之间转换的动态出现，我们使用加权调用图。也就是说，与每个弧f-> g相关联，存在权重w（f-> g），其表示函数f在运行时调用g的次数。

Although a non-weighted call graph for a program can be built statically, obtaining a weighted call graph requires some sort of profiling. The straightforward profiling approach is to instrument the program with counters inserted at every call site, and then to run the program on representative profiling inputs. For binary layout optimizations, which are static transformations, this profiling data is then fed into a pass that rebuilds the binary using the data. This approach is commonly used for profile-guided optimizations, including in the seminal code-layout work by Pettis and Hansen [3].

尽管可以静态地构建程序的非加权调用图，但是获得加权调用图需要某种分析。简单的分析方法是使用在每个调用站点插入的计数器来检测程序，然后在代表性的分析输入上运行程序。对于静态转换的二进制布局优化，然后将此分析数据提供给使用数据重建二进制文件的传递。这种方法通常用于配置文件引导的优化，包括Pettis和Hansen [3]的开创性代码布局工作。

Overall, there are two main drawbacks with this approach based on instrumentation, which complicate its use in production environments. First, it requires intrusive instrumentation of the program and an extra, special build of the application. Furthermore, to instrument the whole application, including libraries, this instrumentation should be done either at link-time or on the final binary. Second, instrumentation incurs significant performance and memory overheads that are often inadequate to be used in real production environments. As a result, a special, controlled environment is often needed to execute the profiling binary, which then limits the amount of profiling data that can be collected. Together, these issues result in many production environments completely opting out of profile-guided optimizations, despite their potential performance benefits.

总的来说，这种基于仪器的方法存在两个主要缺点，这使得它在生产环境中的使用变得复杂。首先，它需要程序的侵入式检测和应用程序的额外特殊构建。此外，要检测整个应用程序（包括库），应在链接时或最终二进制文件上完成此检测。其次，仪器会产生显着的性能和内存开销，这些开销通常不足以在实际生产环境中使用。因此，通常需要一个特殊的受控环境来执行分析二进制文件，然后限制可以收集的分析数据量。总之，这些问题导致许多生产环境完全退出配置文件引导的优化，尽管它们具有潜在的性能优势。

The alternative to overcome the drawbacks of instrumentation is to rely on sampling techniques to build a weighted call graph. Compared to instrumentation, sampling-based techniques are intrinsically less accurate, although this inaccuracy can be limited by collecting enough samples. Furthermore, efficient sampling techniques enable the collection of profiling data in actual production environments, which has the potential to be more representative than instrumentationbased profiles collected in less realistic environments.

克服仪器缺点的替代方案是依靠采样技术来构建加权调用图。与仪器相比，基于采样的技术本质上不太准确，尽管通过收集足够的样本可以限制这种不准确性。此外，有效的采样技术使得能够在实际生产环境中收集剖析数据，这有可能比在不太现实的环境中收集的基于仪器的剖面更具代表性。

We have experimented with two sampling-based techniques that have negligible overheads and thus can be used to obtain profiling data on unmodified production systems. The first one is to use hardware support available on modern Intel x86 processors, called last branch records (LBR) [10]. This is basically a 16-entry buffer that keeps the last 16 executed control-transfer instructions, and which can be programmed to filter the events to only keep records for function calls. The caller-callee addresses from the LBR can be read through a script passed to the Linux perf tool. The second approach we have experimented with is based on sampling stack traces instead of flat profiles, which is also used in other work, e.g. the pprof CPU profiler. This can be done very efficiently, in particular for programs compiled with frame pointers. Stack traces can be obtained via perf’s --call-graph option. From a stack trace, a relatively accurate weighted call graph can be computed by just looking at the top two frames on the stack. More precisely, the weight w(f->g) of arc f->g can be approximated by how many sampled stack traces had function g on the top with f immediately below it. Our experiments with these two samplingbased approaches revealed that they lead to weighted call graphs with similar accuracy.

我们已经尝试了两种基于抽样的技术，这些技术的开销可以忽略不计，因此可用于获取未修改生产系统的分析数据。第一个是使用现代Intel x86处理器上提供的硬件支持，称为最后一个分支记录（LBR）[10]。这基本上是一个16项缓冲区，用于保存最后16个执行的控制传输指令，并且可以对其进行编程以过滤事件，仅保留函数调用的记录。来自LBR的调用者 - 被调用者地址可以通过传递给Linux perf工具的脚本来读取。我们已经尝试过的第二种方法是基于采样堆栈轨迹而不是平面轮廓，这也用于其他工作，例如， pprof CPU分析器。这可以非常有效地完成，特别是对于使用帧指针编译的程序。堆栈跟踪可以通过perf的--call-graph选项获得。从堆栈跟踪中，只需查看堆栈顶部的两个帧即可计算出相对准确的加权调用图。更准确地说，弧f-> g的权重w（f-> g）可以近似为多少采样的堆栈轨迹在顶部具有函数g，其下方有f。我们使用这两种基于抽样的方法进行的实验表明，它们导致加权调用图具有相似的准确性。

Figure 2 illustrates a dynamic call graph that can be built with either of these sampling-based approaches. For example, the weight w(B->C) = 30 means that, via hardware counters, there were 30 call entries for B calling C in the LBR, or, alternatively via stack traces, that 30 sampled stack traces had function C at the top with function B immediately below it.

图2显示了一个动态调用图，可以使用这些基于采样的方法构建。例如，权重w（B-> C）= 30意味着，通过硬件计数器，在LBR中有30个B调用C的调用条目，或者通过堆栈跟踪，30个采样的堆栈跟踪具有函数C at顶部有功能B，紧靠其下方。

5.Function-Ordering Heuristics

Petrank and Rawitz [11] demonstrated that finding an optimal data or code placement that minimizes cache misses is a NP-hard problem. Furthermore, they also showed that this problem is unlikely to have an efficient approximate solution either. Besides that, server applications also typically have a very large set of functions, in the order of hundreds of thousands or more, which render applying an optimal, exponential-time solution impractical. Therefore, in practice, heuristic solutions are applied to these problems.

Petrank和Rawitz [11]证明找到最小化缓存未命中的最佳数据或代码放置是NP难问题。此外，他们还表明，这个问题也不可能有一个有效的近似解决方案。除此之外，服务器应用程序通常还具有非常大的功能集，大约数十万或更多，这使得应用最佳的指数时间解决方案变得不切实际。因此，在实践中，启发式解决方案应用于这些问题。

This section describes two heuristics to obtaining a binary layout. Section 5.1 describes a prior heuristic by Pettis and Hansen [3], while Section 5.2 describes the novel heuristic proposed in this paper. Section 7 presents an experimental evaluation comparing the performance impact of these techniques on the applications described in Section 2.

本节介绍了获取二进制布局的两种启发式方法。第5.1节描述了Pettis和Hansen的先验启发式[3]，而第5.2节描述了本文提出的新型启发式。第7节介绍了一项实验评估，比较了这些技术对第2节中描述的应用程序的性能影响。

5.1 Pettis-Hansen (PH) Heuristic

Pettis and Hansen [3] studied various aspects of code layout, including reordering functions through the linker to improve code locality (Section 3 in [3]). Their function-ordering algorithm is a commonly used technique in practice, having been implemented in compilers, binary optimizers, and performance tools [3, 12–15]. We describe their heuristic for this problem in this section, which we call the PH heuristic, and illustrate how it operates in a simple example.

Pettis和Hansen [3]研究了代码布局的各个方面，包括通过链接器重新排序函数以改进代码局部性（[3]中的第3节）。它们的函数排序算法是实践中常用的技术，已在编译器，二进制优化器和性能工具中实现[3,12-15]。我们在本节中描述了他们对这个问题的启发式方法，我们将其称为PH启发式，并在一个简单的例子中说明它是如何运作的。

The PH heuristic is based on a weighted dynamic call graph. However, the call graph used by PH is undirected, meaning that an arc between functions F and G represents that either function F calls function G, or function G calls F, or both. Although subtle, this indistinction between callers and callees in the call graph can lead to sub-optimal results as illustrated in Section 5.2.

PH启发式基于加权动态调用图。然而，PH使用的调用图是无向的，这意味着函数F和G之间的弧表示函数F调用函数G，或函数G调用F或两者。虽然微妙，但调用图中调用者和被调用者之间的这种不明显可能导致次优结果，如第5.2节所示。

Once the call graph is constructed, PH processes each edge in the graph in decreasing weight order. At each step, PH merges the two nodes connected by the edge in consideration. When two nodes are merged, their edges to the remaining nodes are coalesced and their weights are added up. During the algorithm, a linear list of the original nodes associated with each node in the graph is maintained. When merging two nodes a and b, careful attention is paid to the original connections in the graph involving the first and last nodes in the lists associated with a and b. Reversing of either a or b is evaluated as a mechanism for increasing the weight of the new adjacent nodes that will result from the merge, and the combination that maximizes this metric is chosen. The process repeats until there are no edges left in the graph.

一旦构建了调用图，PH就会以递减的权重顺序处理图中的每个边。在每个步骤中，PH合并由边缘连接的两个节点。当合并两个节点时，它们与剩余节点的边缘将合并，并且它们的权重会相加。在算法期间，保持与图中的每个节点相关联的原始节点的线性列表。合并两个节点a和b时，请注意图中的原始连接，该连接涉及与a和b关联的列表中的第一个和最后一个节点。 a或b的反转被评估为用于增加将由合并产生的新相邻节点的权重的机制，并且选择使该度量最大化的组合。重复该过程，直到图中没有剩余边缘。

We illustrate the operation of the PH heuristic in the example in Figure 2. In the first step, PH processes the heaviest-weight edge, A ! B, merging nodes A and B and obtaining the graph in Figure 3(a). In the second step, the heaviest edge in Figure 3(a), connecting C and D, is selected. In the final step, the only edge remaining is used to merge nodes A;B and C;D. At this point, four different options are considered, corresponding to either reversing or not each of the nodes. The edges in the original graph (Figure 2) are analyzed, and the choice to make A and C adjacent is made because they are connected by the edge with the heaviest weight. To realize this decision, the nodes in the merged node A;B are reversed before making the final merge. The final ordering is illustrated in Figure 3(c).

我们在图2的示例中说明了PH启发式的操作。在第一步中，PH处理最重的边缘，A！ B，合并节点A和B并获得图3（a）中的图。在第二步中，选择连接C和D的图3（a）中最重的边缘。在最后一步中，剩余的唯一边缘用于合并节点A; B和C; D.此时，考虑四个不同的选项，对应于反转或不反转每个节点。分析原始图（图2）中的边缘，并选择使A和C相邻，因为它们通过具有最重权重的边连接。为了实现该决定，在进行最终合并之前，合并节点A; B中的节点被反转。最终的排序如图3（c）所示。

5.2 Call-Chain Clustering (C3) Heuristic

In this section, we describe a new call-graph-based heuristic, which we named Call-Chain Clustering (C3). We first present a key insight that distinguishes C3 from the PH heuristic, and then describe C3 and illustrate its operation with an example.

在本节中，我们描述了一种新的基于调用图的启发式算法，我们将其命名为Call-Chain Clustering（C3）。我们首先提出一个关键的见解，区分C3和PH启发式，然后描述C3并用一个例子来说明它的操作。

Unlike PH, C3 uses a directed call graph, and the role of a function as either the caller or callee at each point is taken into account. One of the key insights of the C3 heuristic is that taking into account the caller/callee relationships matters. We illustrate this insight on a simple example with two functions, F and G, where function F calls G. In order to improve code layout, compilers typically layout a function so that the function entry is at the lower address. During the execution of a function, instructions from higher address are fetched and executed. Representing the size of function F by |F|, the average distance in the address space of any instruction in F from the entry of F is |F|/2. So, assuming this average distance from the entry of F to the call to G within F and the layout where G follows F in the binary, the distance to be jumped in the address space when executing this call G instruction in F is |F|/2. This is illustrated in Figure 4(a). Now consider the layout where G is placed before F. In this case, the distance to be jumped by the call G instruction is |G|+|F|/2. This is illustrated in Figure 4(b). The distance in this second case can be arbitrarily larger than in the first case depending on the size of G. And, the larger the distance, the worse the locality: there is a higher probability of crossing a cache line or a page boundary.

与PH不同，C3使用有向调用图，并且考虑函数作为每个点处的调用者或被调用者的角色。 C3启发式的关键见解之一是考虑到呼叫者/被呼叫者关系的重要性。我们用一个带有两个函数F和G的简单例子来说明这种见解，其中函数F调用G.为了改进代码布局，编译器通常布置一个函数，使函数入口位于较低的地址。在执行函数期间，获取并执行来自较高地址的指令。通过| F |表示函数F的大小，F中的任何指令的地址空间中与F的条目的平均距离为| F | / 2。因此，假设从F的条目到F内的G的调用的平均距离以及G在二进制中跟随F的布局，在F中执行该调用G指令时在地址空间中跳转的距离是| F |。 / 2。这在图4（a）中说明。现在考虑在F之前放置G的​​布局。在这种情况下，通过调用G指令跳转的距离是| G | + | F | / 2。这在图4（b）中说明。根据G的大小，第二种情况下的距离可以任意大于第一种情况。并且，距离越大，局部性越差：越过高速缓存行或页面边界的概率越高。

The C3 heuristic operates as follows. It processes each function in the call graph, in decreasing order of profile weights. Initially, each function is placed in a cluster by itself. Then, when processing each function, its cluster is appended to the cluster containing its most likely predecessor in the call graph. The intuition here is that we want to place a function as close as possible to its most common caller, and we do so following a priority from the hottest to the coldest functions in the program. By following this order, C3 effectively prioritizes the hotter functions, allowing them to be placed next to their preferred predecessor. The only thing that blocks the merge of two clusters is when either of them is larger than the merging threshold. The merging threshold that C3 uses is the page size because, beyond this limit, there is no benefit from further increasing the size of a cluster: it is already too big to fit in either an instruction cache line or a memory page.

C3启发式操作如下。它按照轮廓权重的降序处理调用图中的每个函数。最初，每个函数本身都放在一个集群中。然后，在处理每个函数时，其集群将附加到包含调用图中最可能的前导的集群。这里的直觉是我们希望将函数尽可能地放在最常见的调用者身上，并且我们遵循程序中最热门到最冷的函数的优先级。按照这个顺序，C3有效地优先考虑更热的功能，允许它们放在他们喜欢的前任旁边。阻止两个集群合并的唯一因素是当它们中的任何一个大于合并阈值时。 C3使用的合并阈值是页面大小，因为超出此限制，进一步增加集群的大小没有任何好处：它已经太大而无法放入指令缓存行或内存页面。

C3’s last step is to sort the final clusters. In this step, the clusters are sorted in decreasing order according to a density metric. This metric is the total amount of time spent executing all the functions in the cluster (computed from the profiling samples) divided by the total size in bytes of all the functions in the cluster (available in the binary):

C3的最后一步是对最终的集群进行排序。在该步骤中，根据密度度量以递减顺序对聚类进行排序。此度量标准是执行集群中所有函数（从分析样本计算）所花费的总时间除以集群中所有函数的总大小（以二进制形式提供）：

density(c) = time(c)/size(c) 

The intuition for using this metric is to try to pack most of the execution time in as few code pages as possible, in order to further improve locality. A large, hot function puts more pressure on the cache hierarchy than an equally hot but smaller function. Therefore, preferring the latter will minimize the number of cache lines or TLB pages required to cover most of the program’s execution time.

使用此度量标准的直觉是尝试将尽可能少的代码页中的大部分执行时间打包，以进一步改善局部性。大型热门功能对缓存层次结构施加的压力要大于同样热门但功能较小的功能。因此，优先选择后者将最大限度地减少覆盖程序执行时间大部分所需的缓存行或TLB页数。

Note that, although we limit the cluster sizes through the merging threshold, we still place consecutive clusters adjacently in memory, with no gaps between them.

请注意，尽管我们通过合并阈值限制了簇大小，但我们仍将相邻的簇相邻地放置在内存中，它们之间没有间隙。

We now illustrate how C3 processes the example from Figure 2. For simplicity, let us assume that the amount of time spent in each function equals the sum of the weights of its incoming arcs in the call graph. Therefore, C3 starts by processing function B, which is then appended to the cluster containing A. The result of this merge is illustrated in Figure 5(a). Next, function D is processed, and it is merged with the cluster containing function C. The result of this step is shown in Figure 5(b). At this point, there are two clusters left: A;B and C;D. Next, function C is processed, and its cluster (C;D) is appended to cluster A;B, resulting in the final cluster A;B;C;D, which is illustrated in Figure 5(c).

我们现在说明C3如何处理图2中的示例。为简单起见，我们假设在每个函数中花费的时间量等于调用图中其传入弧的权重之和。因此，C3从处理函数B开始，然后将函数B附加到包含A的集群。该合并的结果如图5（a）所示。接下来，处理函数D，并将其与包含函数C的集群合并。该步骤的结果如图5（b）所示。此时，剩下两个簇：A; B和C; D.接下来，处理函数C，并将其簇（C; D）附加到簇A; B，得到最终簇A; B; C; D，如图5（c）所示。

We now quantitatively compare the final layouts obtained by PH (B;A;C;D) and C3 (A;B;C;D) for the example in Figure 2. For simplicity, we assume that all 4 functions have the same size |f| and that all calls appear exactly in the middle of the caller’s body (i.e. at a |f|/2 distance from the caller’s start). Figures 6(a) and (b) illustrate the code layouts obtained using the PH and C3 heuristics, respectively. These figures also illustrate the distances between the call instructions and their targets with both code layouts. Plugging in the arc weights from the call graph from Figure 2, we obtain the total distance jumped through the calls in each case:

我们现在定量地比较PH（B; A; C; D）和C3（A; B; C; D）获得的最终布局，用于图2中的示例。为简单起见，我们假设所有4个函数具有相同的大小| F |并且所有呼叫都恰好出现在呼叫者身体的中间（即距呼叫者开始的| f | / 2距离）。图6（a）和（b）分别示出了使用PH和C3启发式获得的代码布局。这些图还说明了呼叫指令与其目标之间的距离以及两种代码布局。从图2中的调用图中插入弧权重，我们获得在每种情况下通过调用跳过的总距离：

Therefore, relative to PH, C3 results in a 35% reduction in the total call-distance in this case. In practice, as the experiments in Section 7 demonstrate, such reduction in calldistance results in a reduction in I-cache and I-TLB misses, and therefore an increase in IPC and performance.

因此，相对于PH，C3在这种情况下导致总呼叫距离减少35％。实际上，正如第7节中的实验所示，这种校准的减少导致I-cache和I-TLB未命中的减少，从而导致IPC和性能的提高。

6.Huge Pages for the Text Section

Function layout heuristics like C3 and PH increase performance by improving the efficiency of the processor’s caches, particularly the I-TLB. Once the hot functions have been clustered into a small subsection of the binary using these techniques, it is possible to exploit TLB features to further reduce misses and improve performance. 

功能布局启发式（如C3和PH）通过提高处理器缓存（尤其是I-TLB）的效率来提高性能。一旦使用这些技术将热函数聚集到二进制文件的一小部分中，就可以利用TLB特性来进一步减少未命中并提高性能。

Modern microprocessors support multiple page sizes for program code. For example, Intel’s Ivy Bridge microarchitecture supports both 4 KB and 2 MB pages for instructions. Using these huge pages allows the processor to map larger address ranges simultaneously, which greatly reduces pressure on the I-TLB. There are usually a limited number of huge I-TLB entries available, e.g. Ivy Bridge provides only 8 entries, so it is important for programs to use these entries judiciously.

现代微处理器支持多种页面大小的程序代码。例如，英特尔的Ivy Bridge微体系结构支持4 KB和2 MB页面的指令。使用这些巨大的页面允许处理器同时映射更大的地址范围，这大大减轻了I-TLB的压力。通常存在有限数量的巨大I-TLB条目，例如， Ivy Bridge只提供8个条目，因此程序明智地使用这些条目非常重要。

On systems that support multiple page sizes, the use of huge pages for either data or code is always an opportunity to be evaluated for large-scale applications. On Linux, the libhugetlbfs library [16] provides a simple mechanism to map a program’s entire text section onto huge pages. However, as our experiments in Section 7.5 demonstrate, mapping the entire text section of a binary onto huge pages can put too much pressure on the limited huge I-TLB entries and thus result in a performance degradation.

在支持多种页面大小的系统上，对数据或代码使用大页面始终是评估大型应用程序的机会。在Linux上，libhugetlbfs库[16]提供了一种简单的机制，可以将程序的整个文本部分映射到大页面上。但是，正如我们在7.5节中的实验所示，将二进制文件的整个文本部分映射到大页面上会对有限的巨大I-TLB条目施加太大压力，从而导致性能下降。

In principle, one could add support for partially mapping a binary’s text section onto huge pages by modifying the Linux loader. However, shipping kernel patches has its own drawbacks in practice, as it increases deployment risk and slows down the experimental process compared to an application-level solution.

原则上，可以通过修改Linux加载器来添加对将二进制文本部分部分映射到大页面的支持。但是，运行内核补丁在实践中有其自身的缺点，因为与应用程序级解决方案相比，它会增加部署风险并减慢实验过程。

To avoid this complexity, we implement huge page mapping in user code via a new library. At startup, the application copies the hot function section to scratch space, and unmaps that address range. That range is then re-mapped using anonymous huge pages, and the text is copied back in place.2 This technique allows the application to map the hottest functions using a small number of huge pages. We measure the performance impact of this technique in Section 7.

为了避免这种复杂性，我们通过新库在用户代码中实现了巨大的页面映射。启动时，应用程序将热门功能部分复制到暂存空间，并取消映射该地址范围。然后使用匿名大页面重新映射该范围，并将文本复制回原位.2此技术允许应用程序使用少量大页面映射最热门的函数。我们在第7节中测量了该技术对性能的影响。
