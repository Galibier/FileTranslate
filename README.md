Abstract

Performance optimization for large-scale applications has recently become more important as computation continues to move towards data centers. Data-center applications are generally very large and complex, which makes code layout an important optimization to improve their performance. This has motivated recent investigation of practical techniques to improve code layout at both compile time and link time. Although post-link optimizers had some success in the past, no recent work has explored their benefits in the context of modern data-center applications. 

随着计算继续向数据中心发展，最近大规模应用程序的性能优化变得更加重要。数据中心应用程序通常非常庞大和复杂，这使得代码布局成为提高其性能的重要优化。这促使最近研究了在编译时和链接时改进代码布局的实用技术。尽管后期链接优化器在过去取得了一些成功，但最近的工作并没有在现代数据中心应用程序的背景下探索它们的优势。

In this paper, we present BOLT, a post-link optimizer built on top of the LLVM framework. Utilizing samplebased profiling, BOLT boosts the performance of real-world applications even for highly optimized binaries built with both feedback-driven optimizations (FDO) and link-time optimizations (LTO). We demonstrate that post-link performance improvements are complementary to conventional compiler optimizations, even when the latter are done at a whole-program level and in the presence of profile information. We evaluated BOLT on both Facebook data-center workloads and open-source compilers. For data-center applications, BOLT achieves up to 8.0% performance speedups on top of profile-guided function reordering and LTO. For the GCC and Clang compilers, our evaluation shows that BOLT speeds up their binaries by up to 20.4% on top of FDO and LTO, and up to 52.1% if the binaries are built without FDO and LTO.

在本文中，我们介绍了BOLT，一种建立在LLVM框架之上的后链接优化器。利用基于样本的分析，BOLT提升了实际应用程序的性能，即使对于使用反馈驱动优化（FDO）和链接时优化（LTO）构建的高度优化的二进制文件也是如此。我们证明，链接后性能改进是对传统编译器优化的补充，即使后者是在整个程序级别和存在配置文件信息的情况下完成的。我们在Facebook数据中心工作负载和开源编译器上评估了BOLT。对于数据中心应用程序，BOLT在配置文件引导的功能重新排序和LTO之上可实现高达8.0％的性能加速。对于GCC和Clang编译器，我们的评估表明，BOLT在FDO和LTO之上加速其二进制文件的速度提高了20.4％，如果二进制文件在没有FDO和LTO的情况下构建，则高达52.1％。

1. Introduction

Given the large scale of data centers, optimizing their workloads has recently gained a lot of interest. Modern datacenter applications tend to be very large and complex programs. Due to their sheer amount of code, optimizing the code locality for these applications is very important to improve their performance. 

鉴于数据中心规模庞大，最近优化其工作负载已引起很大兴趣。现代数据中心应用程序往往是非常庞大和复杂的程序。由于代码量很大，优化这些应用程序的代码位置对于提高其性能非常重要。

The large size and performance bottlenecks of data-center applications make them good targets for feedback-driven optimizations (FDO), also called profile-guided optimizations (PGO), particularly code layout. At the same time, the large sizes of these applications also impose scalability challenges to apply FDO to them. Instrumentationbased profilers incur significant memory and computational performance costs, often making it impractical to gather accurate profiles from a production system. To simplify deployment and increase adoption, it is desirable to have a system that can obtain profile data for FDO from unmodified binaries running in their normal production environments. This is possible through the use of sample-based profiling, which enables high-quality profiles to be gathered with minimal operational complexity. This is the approach taken by tools such as Ispike [21], AutoFDO [6], and HFSort [25]. This same principle is used as the basis of the BOLT tool presented in this paper. 

数据中心应用程序的大尺寸和性能瓶颈使其成为反馈驱动优化（FDO）的良好目标，也称为配置文件引导优化（PGO），尤其是代码布局。与此同时，这些应用程序的大尺寸也对将FDO应用于它们带来了可扩展性挑战。基于仪器的分析器会产生大量内存和计算性能成本，通常会使从生产系统收集准确的配置文件变得不切实际。为了简化部署并提高采用率，需要一种能够从正常生产环境中运行的未修改二进制文件获取FDO的配置文件数据的系统。这可以通过使用基于样本的分析来实现，该分析能够以最小的操作复杂性收集高质量的分布。这是Ispike [21]，AutoFDO [6]和HFSort [25]等工具采用的方法。同样的原理被用作本文提出的BOLT工具的基础。

Profile data obtained via sampling can be retrofitted to multiple points in the compilation chain. The point into which the profile data is used can vary from compilation time (e.g. AutoFDO [6]), to link time (e.g. LIPO [18] and HFSort [25]), to post-link time (e.g. Ispike [21]). In general, the earlier in the compilation chain the profile information is inserted, the larger is the potential for its impact, because more phases and optimizations can benefit from this information. This benefit has motivated recent work on compile-time and link-time FDO techniques. At the same time, post-link optimizations, which in the past were explored by a series of proprietary tools such as Spike [8], Etch [28], FDPR [11], and Ispike [21], have not attracted much attention in recent years.We believe that the lack of interest in post-link optimizations is due to folklore and the intuition that this approach is inferior because the profile data is injected very late in the compilation chain. 

通过采样获得的配置文件数据可以改装到编译链中的多个点。使用配置文件数据的点可以从编译时间（例如AutoFDO [6]）到链接时间（例如LIPO [18]和HFSort [25]）到链接后时间（例如Ispike [21]）不等。 。通常，在编译链中较早插入配置文件信息，其影响的可能性越大，因为更多阶段和优化可以从此信息中受益。这一好处推动了最近关于编译时和链接时FDO技术的工作。与此同时，后期链接优化（过去由Spike [8]，Etch [28]，FDPR [11]和Ispike [21]等一系列专有工具进行探索）并没有引起太多关注近年来，我们认为对后链接优化缺乏兴趣是由于民间传说和直觉导致这种方法较差，因为配置文件数据在编译链中很晚才注入。

In this paper, we demonstrate that the intuition described above is incorrect. The important insight that we leverage in this work is that, although injecting profile data earlier in the compilation chain enables its use by more optimizations, injecting this data later enables more accurate use of the information for better code layout. In fact, one of the main challenges with AutoFDO is to map the profile data, collected at the binary level, back to the compiler’s intermediate representations [6]. In the original compilation used to produce the binary where the profile data is collected, many optimizations are applied to the code by the compiler and linker before the machine code is emitted. In a post-link optimizer, which operates at the binary level, this problem is much simpler, resulting in more accurate use of the profile data. This accuracy is particularly important for low-level optimizations such as code layout. 

在本文中，我们证明了上述直觉是不正确的。我们在这项工作中利用的重要见解是，虽然在编译链的早期注入配置文件数据可以通过更多优化来使用，但是稍后注入这些数据可以更准确地使用信息以实现更好的代码布局。实际上，AutoFDO的主要挑战之一是将在二进制级别收集的配置文件数据映射回编译器的中间表示[6]。在用于生成收集配置文件数据的二进制文件的原始编译中，在发出机器代码之前，编译器和链接器会对代码应用许多优化。在以二进制级别运行的后链接优化器中，这个问题要简单得多，从而可以更准确地使用配置文件数据。这种准确性对于代码布局等低级优化尤为重要。

We demonstrate the finding above in the context of a static binary optimizer we built, called BOLT. BOLT is a modern, retargetable binary optimizer built on top of the LLVM compiler infrastructure [16]. Our experimental evaluation on large real-world applications shows that BOLT can improve performance by up to 20.41% on top of FDO and LTO. Furthermore, our analysis demonstrates that this improvement is mostly due to the improved code layout that is enabled by the more accurate usage of sample-based profile data at the binary level. 

我们在我们构建的静态二进制优化器（称为BOLT）的上下文中演示了上述发现。 BOLT是一个基于LLVM编译器基础架构构建的现代可重定向二进制优化器[16]。我们对大型实际应用的实验评估表明，在FDO和LTO之上，BOLT可以将性能提高多达20.41％。此外，我们的分析表明，这种改进主要是由于改进的代码布局，通过在二进制级别更准确地使用基于样本的配置文件数据来实现。

Overall, this paper makes the following contributions: 

总的来说，本文做出以下贡献：

1. It describes the design of a modern, open-source postlink optimizer built on top of the LLVM infrastructure.

1. 它描述了在LLVM基础设施之上构建的现代开源后链接优化器的设计。

2. It demonstrates empirically that a post-link optimizer is able to better utilize sample-based profiling data to improve code layout compared to a compiler-based approach. 

2. 根据经验证明，与基于编译器的方法相比，后链接优化器能够更好地利用基于样本的分析数据来改进代码布局。

3. It shows that neither compile-time, link-time, nor postlink- time FDO supersedes the others but, instead, they are complementary. This paper is organized as follows. 

3. 它表明，无论是编译时间，链接时间还是后链接时间FDO都不会取代其他FDO，而是相互补充。本文的结构如下。
Section 2 motivates the case for using sample-based profiling and static binary optimization to improve performance of large-scale applications. Section 3 then describes the architecture of the BOLT binary optimizer, followed by a description of the optimizations that BOLT implements in Section 4 and a discussion about profiling techniques in Section 5. An evaluation of BOLT and a comparison with other techniques is presented in Section 6. Finally, Section 7 discusses related work and Section 8 concludes the paper.

第2节激发了使用基于样本的分析和静态二进制优化来提高大规模应用程序性能的案例。然后，第3节描述了BOLT二进制优化器的体系结构，随后描述了BOLT在第4节中实现的优化以及第5节中关于分析技术的讨论。第6节介绍了BOLT的评估以及与其他技术的比较。最后，第7节讨论了相关工作，第8节总结了论文。

2. Motivation 

In this section, we motivate the post-link optimization approach used by BOLT.

在本节中，我们将激励BOLT使用的链接后优化方法。

2.1 Why sample-based profiling? 

Feedback-driven optimizations (FDO) have been proved to help increase the impact of code optimizations in a variety of systems (e.g. [6, 9, 13, 18, 24]). Early developments in this area relied on instrumentation-based profiling, which requires a special instrumented build of the application to collect profile data. This approach has two drawbacks. First, it complicates the build process, since it requires a special build for profile collection. Second, instrumentation typically incurs very significant CPU and memory overheads. These overheads generally render instrumented binaries inappropriate for running in real production environments. 

反馈驱动的优化（FDO）已被证明有助于增加各种系统中代码优化的影响（例如[6,9,13,18,24]）。该领域的早期开发依赖于基于仪器的分析，该分析需要特殊的仪器化构建应用程序来收集配置文件数据。这种方法有两个缺点。首先，它使构建过程复杂化，因为它需要特殊的构建来进行概要文件收集。其次，检测通常会产生非常大的CPU和内存开销。这些开销通常会使得检测的二进制文件不适合在实际生产环境中运行。

In order to increase the adoption of FDO in production environments, recent work has investigated FDO-style techniques based on sample-based profiling [6, 7, 25]. Instead of instrumentation, these techniques rely on much cheaper sampling using hardware profile counters available in modern CPUs, such as Intel’s Last Branch Records (LBR) [15]. This approach is more attractive not only because it does not require a special build of the application, but also because the profile-collection overheads are negligible. By addressing the two main drawbacks of instrumentation-based FDO techniques, sample-based profiling has increased the adoption of FDO-style techniques in complex, real-world production systems [6, 25]. For these same practical reasons, we opted to use sample-based profiling in this work. 

为了增加FDO在生产环境中的采用，最近的工作已经研究了基于样本分析的FDO式技术[6,7,25]。这些技术不是使用仪器，而是使用现代CPU中可用的硬件配置文件计数器进行更便宜的采样，例如Intel的Last Branch Records（LBR）[15]。这种方法更具吸引力，不仅因为它不需要应用程序的特殊构建，而且因为概要文件收集开销可以忽略不计。通过解决基于仪器的FDO技术的两个主要缺点，基于样本的分析增加了在复杂的真实世界生产系统中采用FDO式技术[6,25]。出于同样的实际原因，我们选择在这项工作中使用基于样本的分析。

2.2 Why a binary optimizer? 

Sample-based profile data can be leveraged at various levels in the compilation pipeline. Figure 1 shows a generic compilation pipeline to convert source code into machine code. As illustrated in Figure 1, the profile data may be injected at different program-representation levels, ranging from source code, to the compiler’s intermediate representations (IR), to the linker, to post-link optimizers. In general, the designers of any FDO tool are faced with the following trade-off. On the one hand, injecting profile data earlier in the pipeline allows more optimizations along the pipeline to benefit from this data. On the other hand, since sample-based profile data must be collected at the binary level, the closer a level is to this representation, the higher the accuracy with which the data can be mapped back to this level’s program representation. Therefore, a post-link binary optimizer allows the profile data to be used with the greatest level of accuracy.

可以在编译管道中的各个级别利用基于样本的概要文件数据。图1显示了将源代码转换为机器代码的通用编译管道。如图1所示，可以以不同的程序表示级别注入概要文件数据，范围从源代码到编译器的中间表示（IR），到链接器，到后链接优化器。一般而言，任何FDO工具的设计者都面临着以下权衡。一方面，在管道中较早地注入配置文件数据允许沿管道的更多优化从该数据中受益。另一方面，由于必须在二进制级别收集基于样本的简档数据，因此级别越接近该表示，数据可以映射回该级别的程序表示的准确度越高。因此，后链接二进制优化器允许以最高精度使用配置文件数据。

AutoFDO [6] retrofits profile data back into a compiler’s intermediate representation (IR). Chen et al. [7] quantified the precision of the profile data that is lost by retrofitting profile data even at a reasonably low-level representation in the GCC compiler. They quantified that the profile data had 84.1% accuracy, which they were able to improve to 92.9% with some techniques described in that work.

AutoFDO [6]将配置文件数据改造回编译器的中间表示（IR）。陈等人[7]量化了通过改进轮廓数据而丢失的轮廓数据的精度，即使在GCC编译器中的合理低级表示也是如此。他们量化了剖面数据的准确率为84.1％，使用该工作中描述的一些技术可以将其提高到92.9％。

The example in Figure 2 illustrates the difficulty in mapping binary-level performance events back to a higher-level representation. In this example, both functions bar and baz call function foo, which gets inlined in both callers. Function foo contains a conditional branch for the if statement on line (02). For forward branches like this, on modern processors, it is advantageous to make the most common successor be the fall-through, which can lead to better branch prediction and instruction-cache locality. This means that, when foo is inlined into bar, block B1 should be placed before B2, but the blocks should be placed in the opposite order when inlined into baz. When this program is profiled at the binary level, two branches corresponding to the if in line (02) will be profiled, one within bar and one within baz. Assume that functions bar and baz execute the same number of times at runtime. Then, when mapping the branch frequencies back to the source code in Figure 2, one will conclude that the branch at line (02) has a 50% chance of branching to both B1 and B2. And, after foo is inlined in both bar and baz, the compiler will not be able to tell what layout is best in each case. Notice that, although this problem can be mitigated by injecting the profile data into a lower-level representation after function inlining has been performed, this does not solve the problem in case foo is declared in a different module than bar and baz because in this case inlining cannot happen until link time. 

图2中的示例说明了将二进制级性能事件映射回更高级别表示的难度。在这个例子中，函数bar和baz都调用函数foo，它在两个调用者中都被内联。函数foo包含第（02）行的if语句的条件分支。对于这样的前向分支，在现代处理器上，最常见的后继者是落空，这可以导致更好的分支预测和指令缓存局部性。这意味着，当foo被内联到bar中时，块B1应该放在B2之前，但是当内联到baz时，块应该以相反的顺序放置。当该程序在二进制级别进行分析时，将分析对应于行（02）中的if的两个分支，一个在bar内，一个在baz内。假设函数bar和baz在运行时执行相同的次数。然后，当将分支频率映射回图2中的源代码时，可以得出结论，第（02）行的分支有50％的机会分支到B1和B2。并且，在bar和baz中内联foo之后，编译器将无法分辨哪种布局在每种情况下都是最佳的。请注意，虽然在执行函数内联后将配置文件数据注入较低级别的表示可以缓解此问题，但这并不能解决foo在与bar和baz不同的模块中声明的问题，因为在这种情况下在链接时间之前不能进行内联。

Since our initial motivation for BOLT was to improve large-scale data-center applications, where code layout plays a major role, a post-link binary optimizer was very appealing. Traditional code-layout techniques are highly dependent on accurate branch frequencies [26], and using inaccurate profile data can actually lead to performance degradation [7]. Nevertheless, as we mentioned earlier, feeding profile information at a very low level prevents earlier optimizations in the compilation pipeline from leveraging this information. Therefore, with this approach, any optimization that we want to benefit from the profile data needs to be applied at the binary level. Fortunately, code layout algorithms are relatively simple and easy to apply at the binary level. 

由于我们BOLT的最初动机是改进大规模数据中心应用程序，其中代码布局起着重要作用，因此后链接二进制优化器非常吸引人。传统的代码布局技术高度依赖于准确的分支频率[26]，使用不准确的配置文件数据实际上会导致性能下降[7]。然而，正如我们之前提到的那样，以非常低的水平提供概况信息可以防止早期的优化编译管道中的ons利用这些信息。因此，使用此方法，我们希望从配置文件数据中受益的任何优化都需要在二进制级别应用。幸运的是，代码布局算法相对简单，易于在二进制级别应用。

2.3 Why a static binary optimizer? 

The benefits of a binary-level optimizer outlined above can be exploited either statically or dynamically. We opted for a static approach for two reasons. The first one is the simplicity of the approach. The second was the absence of runtime overheads. Even though dynamic binary optimizers have had some success in the past (e.g. Dynamo [2], DynamoRIO [5], StarDBT [29]), these systems incur nontrivial overheads that go against the main goal of improving the overall performance of the target application. In other words, these systems need to perform really well in order to recover their overheads and achieve a net performance win. Unfortunately, since they need to keep their overheads low, these systems often have to implement faster, sub-optimal code optimization passes. This has been a general challenge to the adoption of dynamic binary optimizers, as they are not suited for all applications and can easily degrade performance if not tuned well. The main benefit of a dynamic binary optimizer over a static one is the ability to handle dynamically generated and self-modifying code.

上面概述的二进制级优化器的好处可以静态或动态地利用。我们选择静态方法有两个原因。第一个是方法的简单性。第二个是没有运行时开销。尽管动态 功（例如Dynamo [2]，DynamoRIO [5]，StarDBT [29]），但这些系统会产生非常重要的开销，这与提高目标应用程序整体性能的主要目标相悖。 。换句话说，这些系统需要非常好地执行才能恢复其开销并实现净性能提升。不幸的是，由于他们需要将开销保持在较低水平，因此这些系统通常必须实现更快，次优的代码优化过程。这对采用动态二进制优化器一直是一个普遍的挑战，因为它们并不适用于所有应用程序，如果调整得不好，很容易降低性能。动态二进制优化器相对于静态优化器的主要优点是能够处理动态生成和自修改的代码。

3. Architecture 

Large-scale data-center binaries may contain over 100 MB of code from multiple source-code languages, including assembly language. In this section, we discuss the design of the BOLT binary optimizer that we created to operate in this scenario. 

大规模数据中心二进制文件可能包含来自多种源代码语言的超过100MB的代码，包括汇编语言。在本节中，我们将讨论我们为在此场景中运行而创建的BOLT二进制优化器的设计。

3.1 Initial Design 

We developed BOLT by incrementally increasing its binary code coverage. At first, BOLT was only able to optimize the code layout of a limited set of functions. With time, code coverage gradually increased by adding support for more complex functions. Even today, BOLT is still able to leave some functions in the binary untouched while processing and optimizing others, conservatively skipping code that violates its current assumptions. 

我们通过逐步增加其二进制代码覆盖率来开发BOLT。起初，BOLT只能优化一组有限功能的代码布局。随着时间的推移，代码覆盖率通过增加对更复杂功能的支即使在今天，BOLT仍然能够在处理和优化其他功能时保持二进制中的某些功能不受影响，保守地跳过违反其当前假设的代码。

The initial implementation targeted x86 64 Linux ELF binaries and relied exclusively on ELF symbol tables to guide binary content identification. By doing that, BOLT was able to optimize code layout within existing function boundaries. When BOLT was not able to reconstruct the control-flow graph of a given function with full confidence, it would just leave the function untouched. 

初始实现针对x86 64 Linux ELF二进制文件，并且完全依赖于ELF符号表来指导二进制内容标识。通过这样做，BOLT能够优化现有功能边界内的代码布局。当BOLT无法完全自信地重建给定函数的控制流图时，它只会使函数保持不变。

Due to the nature of code layout optimizations, the effective code size may increase for a couple of reasons. First, this may happen due to an increase in the number of branches on cold paths. Second, there is a peculiarity of x86’s conditional branch instruction, which occupies 2 bytes if a (signed) offset to a destination fits in 8 bits but otherwise takes 6 bytes for 32-bit offsets. Naturally, moving cold code further away showed a tendency to increase the hot code size. If an optimized function would not fit into the original function’s allocated space, BOLT would split the cold code and move it to a newly created ELF segment. Note that such function splitting was involuntary and did not provide any extra benefit beyond allowing code straightening optimizations as BOLT was not filling out the freed space between the split point and the next function.

由于代码布局优化的性质，有效代码大小可能由于几个原因而增加。首先，这可能是由于冷路径上分支数量的增加而发生的。其次，x86的条件分支指令具有特殊性，如果到目的地的（带符号）偏移量适合8位，则占用2个字节，否则对于32位偏移量需要6个字节。自然地，将冷代码移动得更远，显示出增加热代码大小的趋势。如果优化函数不适合原始函数的已分配空间，BOLT将拆分冷代码并将其移动到新创建的ELF段。请注意，此类函数拆分是非自愿的，并且除了允许代码校正优化之外没有提供任何额外的好处，因为BOLT没有填充拆分点和下一个函数之间的释放空间。

3.2 Relocations Mode 

A second and more ambitious mode was later added to operate by changing the position of all functions in the binary. While multiple approaches were considered, the most obvious and straightforward one was to rely on relocations recorded and saved by the linker in an executable. Both BFD and Gold linkers provide such an option (--emit-relocs). However, even with this option, there are still some missing pieces of information. An example is the relative offsets for PIC jump tables which are removed by the linker. Other examples are some relocations that are not visible even to the linker, such as cross-function references for local functions within a single compilation unit (they are processed internally by the compiler). Therefore, in order to detect and fix such references, it is important to disassemble all the code correctly before trying to rearrange the functions in the binary. Nevertheless, with relocations, the job of gaining complete control over code re-writing became much easier. Handling relocations gives BOLT the ability to change the order of functions in the binary and split function bodies to further improve code locality. 

后来通过改变二进制中所有函数的位置来添加第二个更雄心勃勃的模式。虽然考虑了多种方法，但最明显和最直接的方法是依赖于链接器在可执行文件中记录和保存的重定位。 BFD和Gold链接器都提供了这样的选项（--emit-relocs）。但是，即使使用此选项，仍然会丢失一些信息。一个例子是链接器删除的PIC跳转表的相对偏移量。其他示例是即使对链接器也不可见的一些重定位，例如单个编译单元内的本地函数的交叉函数引用（它们由编译器在内部处理）。因此，为了检测和修复这些引用，在尝试重新排列二进制函数之前，正确地反汇编所有代码是很重要的。然而，通过重新定位，获得对代码重写的完全控制的工作变得更加容易。处理重定位使BOLT能够更改二进制和拆分函数体中的函数顺序，以进一步改善代码局部性。

Since linkers have access to relocations, it would be possible to use them for similar binary optimizations. However, there are multiple open-source linkers for x86 Linux alone, and which one is being used for any particular application depends on a number of circumstances that may also change over time. Therefore, in order to facilitate the tool’s adoption, we opted for writing an independent post-link optimizer instead of being tied to a specific linker. 

由于链接器可以访问重定位，因此可以将它们用于类似的二进制优化。但是，单独的x86 Linux有多个开源链接器，并且哪个用于任何特定应用程序取决于许多情况，这些情况也可能随时间而变化。因此，为了便于工具的采用，我们选择编写一个独立的后链接优化器，而不是绑定到特定的链接器。

3.3 Rewriting Pipeline 

Analyzing an arbitrary binary and locating code and data is not trivial. In fact, the problem of precisely disassembling machine code is undecidable in the general case. In practice, there is more information than just an entry point available, and BOLT relies on correct ELF symbol table information for code discovery. Since BOLT works with 64-bit Linux binaries, the ABI requires an inclusion of function frame information that contains function boundaries as well. While BOLT could have relied on this information, it is often the case that functions written in assembly omit frame information. Thus, we decided to employ a hybrid approach using both symbol table and frame information when available. 

分析任意二进制文件并定位代码和数据并非易事。事实上，在一般情况下，精确拆卸机器代码的问题是不可判定的。实际上，除了可用的入口点之外，还有更多的信息，并且BOLT依赖于正确的ELF符号表信息来进行代码发现。由于BOLT适用于64位Linux二进制文件，因此ABI要求包含包含函数边界的函数框架信息。虽然BOLT可能依赖于这些信息，但通常情况下，用汇编语言编写的函数会省略框架信息。因此，我们决定在可用时使用符号表和帧信息的混合方法。

Figure 3 shows a diagram with BOLT’s rewriting steps. Function discovery is the very first step, where function names are bound to addresses. Later, debug information and profile data are retrieved so that disassembly of individual functions can start. 

图3显示了BOLT重写步骤的图表。函数发现是第一步，其中函数名称绑定到地址。稍后，将检索调试信息和配置文件数据，以便可以开始反汇编各个函数。

BOLT uses the LLVM compiler infrastructure [16] to handle disassembly and modification of binary files. There are a couple of reasons LLVM is well suited for BOLT. First, LLVM has a nice modular design that enables relatively easy development of tools based on its infrastructure. Second, LLVM supports multiple target architectures, which allows for easily retargetable tools. To illustrate this point, a working prototype for the ARM architecture was implemented in less than a month. In addition to the assembler and disassembler, many other components of LLVM proved to be useful while building BOLT. Overall, this decision to use LLVM has worked out well. The LLVM infrastructure has enabled a quick implementation of a robust and easily retargetable binary optimizer. 

BOLT使用LLVM编译器基础结构[16]来处理二进制文件的反汇编和修改。 LLVM非常适合BOLT，这有几个原因。首先，LLVM具有良好的模块化设计，可以基于其基础架构相对容易地开发工具。其次，LLVM支持多种目标体系结构，可以轻松实现可重定向的工具。为了说明这一点，ARM架构的工作原型在不到一个月的时间内就实现了。除了汇编程序和反汇编程序之外，LLVM的许多其他组件在构建BOLT时证明是有用的。总的来说，这个使用LLVM的决定效果很好。 LLVM基础架构支持快速实现强大且易于重定向的二进制优化器。

As Figure 3 shows, the next step in the rewriting pipeline is to build the control-flow graph (CFG) representation for each of the function. The CFG is constructed using the MCInst objects provided by LLVM’s Tablegen-generated disassembler. BOLT reconstructs the control-flow information by analyzing any branch instructions encountered during disassembly. Then, in the CFG representation, BOLT runs its optimization pipeline, which is explained in detail in Section 4. For BOLT, we have added a generic annotation mechanism to MCInst in order to facilitate certain optimizations, e.g. as a way of recording dataflow information. The final steps involve emitting functions and using LLVM’s runtime dynamic linker (created for the LLVM JIT systems) to resolve references among functions and local symbols (such as basic blocks). Finally, the binary is rewritten with the new contents while also updating ELF structures to reflect the new sizes.

如图3所示，重写管道的下一步是为每个函数构建控制流图（CFG）表示。 CFG是使用LLVM的Tablegen生成的反汇编程序提供的MCInst对象构造的。 BOLT通过分析在反汇编期间遇到的任何分支指令来重建控制流信息。然后，在CFG表示中，BOLT运行其优化管道，这将在第4节中详细解释。对于BOLT，我们向MCInst添加了通用注释机制，以便于某些优化，例如，作为记录数据流信息的一种方式。最后的步骤涉及发出函数并使用LLVM的运行时动态链接器（为LLVM JIT系统创建）来解析函数和本地符号（例如基本块）之间的引用。最后，使用新内容重写二进制文件，同时更新ELF结构以反映新的大小。

3.4 C++ Exceptions and Debug Information 

BOLT is able to recognize DWARF [10] information and update it to reflect the code modifications and relocations performed during the rewriting pass. Figure 4 shows an example of a CFG dump demonstrating BOLT’s internal representation of the binary for the first two basic blocks of a function with C++ exceptions and a throw statement. The function is quite small with only five basic blocks in total, and each basic block is free to be relocated to another position, except the entry point. Placeholders for DWARF Call Frame Information (CFI) instructions are used to annotate positions where the frame state changes (for example, when the stack pointer advances). BOLT rebuilds all CFI for the new binary based on these annotations so the frame unwinder works properly when an exception is thrown. The callq instruction at offset 0x00000010 can throw an exception and has a designated landing pad as indicated by a landing-pad annotation displayed next to it (handler: .LLP0; action: 1). The last annotation on the line indicates a source line origin for every machine-level instruction. 

3.4 C ++异常和调试信息

BOLT能够识别DWARF [10]信息并更新它以反映重写过程中执行的代码修改和重定位。图4显示了一个CFG转储的示例，演示了BOLT对具有C ++异常和throw语句的函数的前两个基本块的二进制内部表示。该功能非常小，总共只有五个基本块，每个基本块可以自由地重新定位到另一个位置，除了入口点。 DWARF调用帧信息（CFI）指令的占位符用于注释帧状态更改的位置（例如，堆栈指针前进时）。 BOLT根据这些注释重建新二进制文件的所有CFI，以便在抛出异常时帧取消器正常工作。偏移量0x00000010处的callq指令可以抛出异常并具有指定的着陆点，如旁边显示的着陆键注释所示（处理程序：.LLP0;操作：1）。该行的最后一个注释表示每个机器级指令的源行原点。

4. Optimizations 

BOLT runs passes with either code transformations or analyses, similar to a compiler. BOLT is also equipped with a dataflow-analysis framework to feed information to passes that need it. This enables BOLT to check register liveness at a given program point, a technique also used by Ispike [21]. Some passes are architecture-independent while others are not. In this section, we discuss the passes applied to the Intel x86_64 target. 

BOLT运行代码转换或分析，类似于编译器。 BOLT还配备了数据流分析框架，可将信息提供给需要它的通行证。这使得BOLT能够检查给定程序点的寄存器活跃度，这也是Ispike [21]使用的技术。有些通行证是独立于建筑的，而有些则则不是。在本节中，我们将讨论应用于Intel x86_64目标的过程。

Table 1 shows each individual BOLT optimization pass in the order they are applied. For example, the first line presents strip-rep-ret at the start of the pipeline. Notice that passes 1 and 4 are focused on leveraging precise target architecture information to remove or mutate some instructions. A use case of BOLT for data-center applications is to allow the user to trade any optional choices in the instruction space in favor of I-cache space, such as removing alignment NOPs and AMD-friendly REPZ bytes, or using shorter versions of instructions. Our findings show that, for large applications, it is better to aggressively reduce I-cache occupation, except if the change incurs D-cache overhead, since cache is one of the most constrained resources in the datacenter space. This explains BOLT’s policy of discarding all NOPs after reading the input binary. Even though compilergenerated alignment NOPs are generally useful, the extra space required by them does not pay off and simply stripping them from the binary provides a small but measurable performance improvement. 

表1按照它们应用的顺序显示了每个单独的BOLT优化过程。例如，第一行在管道开始处显示strip-rep-ret。请注意，第1和第4遍的重点是利用精确的目标体系结构信息来删除或改变某些指令。用于数据中心应用程序的BOLT用例是允许用户在指令空间中交换任何可选选项以支持I-cache空间，例如删除对齐NOP和AMD友好的REPZ字节，或使用更短版本的指令。我们的研究结果表明，对于大型应用程序，最好大力减少I-cache占用，除非更改导致D-cache开销，因为缓存是数据中心空间中受约束最多的资源之一。这解释了BOLT在读取输入二进制文件后丢弃所有NOP的策略。即使编译对齐NOP通常是有用的，它们所需的额外空间也不会得到回报，并且简单地从二进制文件中剥离它们提供了小但可测量的性能改进。

BOLT features identical code folding (ICF) to complement the ICF optimization done by the linker. An additional benefit of doing ICF at the binary level is the abil- 5 ity to optimize functions that were compiled without the -ffunction-sections flag and functions that contain jump tables. As a result, BOLT is able to fold more identical functions than the linkers. We have measured the reduction of code size for the HHVM binary [1] to be about 3% on top of the linker’s ICF pass. 

BOLT具有相同的代码折叠（ICF），以补充链接器完成的ICF优化。在二进制级别执行ICF的另一个好处是可以优化在没有-ffunction-sections标志和包含跳转表的函数的情况下编译的函数。因此，BOLT能够折叠比链接器更多相同的功能。我们已经测量了HHVM二进制文件[1]的代码大小减少，在链接器的ICF传递之上约为3％。

Passes 3 (indirect call promotion), 5 (inline small functions), and 7 (PLT call optimization) leverage call frequency information to either eliminate or mutate a function call into a more performant version. We note that BOLT’s function inlining is a limited version of what compilers perform at higher levels. We expect that most of the inlining opportunities will be leveraged by the compiler (potentially using FDO). The remaining inlining opportunities for BOLT are typically exposed by more accurate profile data, BOLT’s indirect-call promotion (ICP) optimization, crossmodule nature, or a combination of these factors. 

通过3（间接呼叫促进），5（内联小功能）和7（PLT呼叫优化）利用呼叫频率信息来消除或将函数调用变为更高性能的版本。我们注意到BOLT的函数内联是编译器在更高级别执行的有限版本。我们预计大多数内联机会将由编译器利用（可能使用FDO）。 BOLT的剩余内联机会通常通过更准确的配置文件数据，BOLT的间接呼叫促销（ICP）优化，交叉模块性质或这些因素的组合来暴露。

Pass 6, simplification of load instructions, explores a tricky tradeoff by fetching data from statically known values (in read-only sections). In these cases, BOLT may convert loads into immediate-loading instructions, relieving pressure from the D-cache but possibly increasing pressure on the I-cache, since the data is now encoded in the instruction stream. BOLT’s policy in this case is to abort the promotion if the new instruction encoding is larger than the original load instruction, even if it means avoiding an arguably more computationally expensive load instruction. However, we found that such opportunities are not very frequent in our workloads. 

通过6，简化加载指令，通过从静态已知值（在只读部分中）获取数据来探索棘手的权衡。在这些情况下，BOLT可以将负载转换为即时加载指令，从而缓解来自D-cache的压力，但可能增加I-cache的压力，因为数据现在在指令流中编码。在这种情况下，BOLT的策略是，如果新指令编码大于原始加载指令，则中止提升，即使这意味着避免使用可称为计算量更高的加载指令。但是，我们发现这些机会在我们的工作量中并不常见。

Pass 9, reorder and split hot/cold basic blocks, reorders basic blocks according to the most frequently executed paths, so the hottest successor will most likely be a fallthough, reducing taken branches and relieving pressure from the branch predictor unit. 

通过9，重新排序和拆分热/冷基本块，根据最频繁执行的路径重新排序基本块，因此最热门的后继将很可能是一个降低，减少分支和减轻分支预测单元的压力。

Finally, pass 13 reorders the functions via the HFSort technique [25]. This optimization mainly improves I-TLB performance, but it also helps with I-cache to a smaller extent. Combined with pass 9, these are the most effective ones in BOLT because they directly optimize the code layout.

最后，传递13通过HFSort技术重新排序函数[25]。这种优化主要是为了提高I-TLB的性能，但它也有助于在较小程度上实现I-cache。结合第9遍，这些是BOLT中最有效的，因为它们直接优化了代码布局。

5. Profiling Techniques 

This section discusses pitfalls and caveats of different samplebased profiling techniques when trying to produce accurate profiling data. 

本节讨论了在尝试生成准确的分析数据时，不同基于样本的分析技术的缺陷和注意事项。

5.1 Techniques 

In recent Intel microprocessors, LBR is a list of the last 32 taken branches. LBRs are important for profile-guided optimizations not only because they provide accurate counts for critical edges (which cannot be inferred even with perfect basic block count profiling [17]), but also because they make block-layout algorithms more resilient to bad sampling. When evaluating several different sampling events to collect LBRs for BOLT, we found that the performance impact in LBR mode is very consistent even for different sampling events. We have experimented with collecting LBR data with multiple hardware events on Intel x86, including retired instructions, taken branches, and cycles, and also experimented with different levels of Precise Event Based Sampling (PEBS) [15]. In all these cases, for a workload for which BOLT provided a 5.4% speedup, the performance differences were within 1%. In non-LBR mode, using biased events with a non-ideal algorithm to infer edge counts can cause as much as 5% performance penalty when compared to LBR, meaning it misses nearly all optimization opportunities. An investigation showed that non-LBR techniques can be tuned to stay under 1% worse than LBR in this example workload, but if LBR is available in the processor, one is better off using it to obtain higher and more robust performance numbers. We also evaluate this effect for HHVM in Section 6.5. 

在最近的英特尔微处理器中，LBR是最近32个分支机构的列表。 LBR对于轮廓引导优化非常重要，不仅因为它们提供了关键边缘的精确计数（即使使用完美的基本块计数分析也无法推断[17]），而且因为它们使块布局算法对不良采样更具弹性。在评估几个不同的采样事件以收集BOLT的LBR时，我们发现即使对于不同的采样事件，LBR模式下的性能影响也非常一致。我们已经尝试在Intel x86上收集包含多个硬件事件的LBR数据，包括退役指令，分支和周期，还尝试了不同级别的基于精确事件的采样（PEBS）[15]。在所有这些情况下，对于BOLT提供5.4％加速的工作负载，性能差异在1％以内。在非LBR模式下，与LBR相比，使用具有非理想算法的偏向事件来推断边缘计数可能导致高达5％的性能损失，这意味着它几乎错过了所有优化机会。调查显示，在此示例工作负载中，非LBR技术可以调整为比LBR低1％，但如果处理器中有LBR，则最好使用它来获得更高和更强大的性能数字。我们还在6.5节中评估了HHVM的这种效果。

5.2 Consequences for Block 

Layout Using LBRs, in a hypothetical worst-case biasing scenario where all samples in a function are recorded in the same basic block, BOLT will lay out blocks in the order of the path that leads to this block. It is an incomplete layout that misses the ordering of successor blocks, but it is not an invalid nor a cold path. In contrast, when trying to infer the same edge counts with non-LBR samples, the scenario is that of a single hot basic block with no information about which path was taken to get to it. 

布局使用LBR，在假设的最坏情况偏置情况下，函数中的所有样本都记录在同一个基本块中，BOLT将按照通向该块的路径的顺序布置块。这是一个不完整的布局，错过了后继块的顺序，但它不是无效的，也不是冷路。相反，当尝试使用非LBR样本推断相同的边缘计数时，该场景是单个热基本块的情况，没有关于到达哪个路径的信息。

In practice, even in LBR mode, many times the collected profile is contradictory by stating that predecessors execute many times more than its single successor, among other violations of flow equations.2 Previous work [17, 23], which includes techniques implemented in IBM’s FDPR [12], report handling the problem of reconstructing edge counts by solving an instance of minimum cost flow (MCF [17]), a graph network flow problem. However, these reports predate LBRs. LBRs only store taken branches, so when handling very skewed data such as the cases mentioned above, BOLT satisfies the flow equation by attributing all surplus flow to the non-taken path that is naturally missing from the LBR, similarly to Chen et al. [7]. BOLT also benefits from being applied after the static compiler: to cope with uncertainty, by putting weight on the fall-through path, it trusts the original layout done by the static compiler. Therefore, the program trace needs to show a significant number of taken branches, which contradict the original layout done by the compiler, to convince BOLT to reorder the blocks and change the original fall-through path. Without LBRs, it is not possible to take advantage of this: algorithms start with guesses for both taken and non-taken branches without being sure if the taken branches, those taken for granted in LBR mode, are real or the result of bad edge-count inference.

在实践中，即使在LBR模式下，收集的配置文件很多时候都是矛盾的，说明前任执行的次数比其单个后继者多出许多次，其中包括违反流量方程式.2之前的工作[17,23]，其中包括在IBM中实现的技术FDPR [12]，报告通过求解最小成本流（MCF [17]），图形网络流问题来处理重建边缘计数的问题。但是，这些报告早于LBR。 LBR仅存储分支，因此当处理非常偏斜的数据（例如上述情况）时，BOLT通过将所有剩余流量归因于LBR中自然缺失的非采集路径来满足流动方程，类似于Chen等人。 [7]。 BOLT也受益于在静态编译器之后应用：为了应对不确定性，通过在重复路径上加权，它信任静态编译器完成的原始布局。因此，程序跟踪需要显示大量采用的分支，这与编译器完成的原始布局相矛盾，以说服BOLT重新排序块并更改原始的直通路径。如果没有LBR，就不可能利用这一点：算法从捕获和未捕获分支的猜测开始，而不确定所采用的分支，在LBR模式中被认为是理所当然的，或者是坏边缘的结果 - 算推断。

5.3 Consequences for Function Layout 

BOLT uses HFSort [25] to perform function reordering based on a weighted call graph. If LBRs are used, the edge weights of the call graph are directly inferred from the branch records, which may also include function calls and returns. However, without LBRs, BOLT is still able to build an incomplete call graph by looking at the direct calls in the binary and creating caller-callee edges with weights corresponding to the number of samples recorded in the blocks containing the corresponding call instructions. However, this approach cannot take indirect calls into account. Even with these limitations, we did not observe a performance penalty as severe as using non-LBR mode for basic block reordering (Section 6.5)

BOLT使用HFSort [25]基于加权调用图执行函数重新排序。如果使用LBR，则从分支记录直接推断出调用图的边权重，该分支记录还可以包括函数调用和返回。但是，如果没有LBR，BOLT仍然可以通过查看二进制中的直接调用并创建调用者 - 被调用者边缘来构建不完整的调用图，其权重对应于包含相应调用指令的块中记录的样本数。但是，这种方法不能考虑间接调用。即使存在这些限制，我们也没有观察到像使用非LBR模式进行基本块重新排序那样严重的性能损失（第6.5节）。
