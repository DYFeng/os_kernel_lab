## 文档规范

### 一些标准的架构、软件名词写法
- 语言相关
  - Rust
  - C
  - C++
- 教程
  - rCore-Tutorial
- 操作系统相关
  - uCore
  - rCore
  - Linux
  - macOS
  - Windows
  - Ubuntu
- 操作系统技术相关
  - 物理页（而不是物理页帧）
  - 虚拟页（而不是物理页帧）
- 架构相关
  - x86_64
  - RISC-V 64
- 其他一些名词
  - ABI
  - GitHub
  - virtio
- Rust 相关
  - rustup
  - cargo
  - rustc
- 其他软件
  - QEMU
  - Homebrew

### 内容控制
- 我们的目标针对于「做实验的同学」，对想完整实现一个 rCore 的同学来说可能不太友好；
- 但是我们也相信，这部分想完整实现的同学也不会因为我们在文档中少些了一部分非常细节的诸如模块调用的内容就放弃，而且从头复制到尾也不是一个好的做法，这不会让你对操作系统有更深刻的理解；
- 所以，在文档开发过程中，我们需要对清晰和全面做很多的权衡和考虑，需要省略掉大量语法层面而 OS 无关的代码来带来更多的可读性和精简性；
- 所以，在文档中引用的代码，只需要写主体的函数，不需要把一系列调用、头部注释全部加入进去。在最后，可能会再利用代码折叠的方式对这个问题进一步权衡。

### 书写格式
- 在数字、英文、独立的标点或记号两侧的中文之间要加空格，如：
  - 安装 QEMU
  - 分为 2 个部分
- 命令、宏名、寄存器、类型名、函数名、变量名、行间输出、编译选项、路径和文件名需要使用 `记号`
  - 寄存器 `a0` 这样的也需要使用 `记号`，而不是 $$a_0$$，这是为了和 `sepc` 统一
- 行内命令或运行输出引用使用 \`\` 记号，并在两侧加入空格，如：
  - `cargo run`
  - 出现 `ERROR: pkg-config binary 'pkg-config' not found` 时
- 行间命令使用 \`\`\` 记号并加入语言记号：
  - 命令使用 bash 记号
  - Rust 语言使用 rust 记号
  - cargo 的配置使用 toml 记号
  - 如何命令只是命令，则不需要 $ 记号（方便同学复制），如：
    ```bash
    echo "Hello, world."
    ```
  - 如果在展示一个命令带来的输出效果，需要加入 $ 记号表示一个命令的开始，如：
    ```bash
    $ echo "Hello, world."
    Hello, world.
    ```
- 粗体使用 \*\* **粗体** \*\* 记号
  - 一些重要的概念最好进行加粗
- 斜体使用 \* *斜体* \* 记号，而不要混合使用 \_ _Italic_ \_ 记号
- 在正式的段落中要加入标点符号，在 - 记号开始的列表中的单独名词表项不加入标点符号（但是如果是段落需要加），如：
  - 操作系统有（名词罗列）：
    - macOS
    - Windows
  - 我们需要（连贯段落）：
    - 先打开 QEMU；
    - 再关闭 QEMU。
- 在 / 记号两侧添加空格，如：
  - Linux / Windows WSL
- 中文名词的英文解释多用大写，如：
  - 裸机（Bare Metal）
  - `sepc` （Supervisor Exception Program Counter）
- 只要是主体是中文的段落，括号统一使用中文括号（），如果主体是英文则使用英文括号 ()
  - 值得注意的是中文括号两侧本来就会又留白，这里不会在括号两侧加入空格
  - 英文空格两侧最好加上空格
- 在文档中引用成段的代码时，需要填写上文件的路径，如：
  
  {% label %}os/src/sbi.rs{% endlabel %}
  ```rust
  /// 向控制台输出一个字符
  ///
  /// 需要注意我们不能直接使用 Rust 中的 char 类型
  pub fn console_putchar(c: usize) {
      sbi_call(SBI_CONSOLE_PUTCHAR, c, 0, 0);
  }
  ```
- 在使用伪代码时，不使用 `$` `%` 等符号描述寄存器，使用 `:=` 表示赋值，例如 `pc := sepc`
- 代码过长或会让文档显得很长时，需要进行折叠

### 小节格式

- 章节的标题为使用 `#` 一级标题，后面的子标题依次加级别
- 小节的标题统一使用 `##` 二级标题，后面的子标题依次加级别