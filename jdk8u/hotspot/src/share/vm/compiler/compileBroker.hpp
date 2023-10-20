/*
 * Copyright (c) 1999, 2019, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 *
 */

#ifndef SHARE_VM_COMPILER_COMPILEBROKER_HPP
#define SHARE_VM_COMPILER_COMPILEBROKER_HPP

#include "ci/compilerInterface.hpp"
#include "compiler/abstractCompiler.hpp"
#include "runtime/perfData.hpp"

class nmethod;
class nmethodLocker;

// CompileTask
//
// An entry in the compile queue.  It represents a pending or current
// compilation.
// CompileTask继承自CHeapObj，表示编译队列中的一个编译任务，其定义位于hospot/src/share/vm/compiler/compileBroker.hpp中。
// CompileTask定义方法大部分是属性操作相关的和日志打印相关的，重点关注用来创建CompileTask实例的allocate方法，
// 用来释放CompileTask实例内存的free方法，用来初始化CompileTask的initialize方法的实现。
class CompileTask : public CHeapObj<mtCompiler> {
  friend class VMStructs;

 private:
    // 空闲的编译任务队列
  static CompileTask* _task_free_list;
#ifdef ASSERT
  static int          _num_allocated_tasks;
#endif

    // 该任务的锁
  Monitor*     _lock;
  uint         _compile_id;
    // 该编译任务对应的方法
  Method*      _method;
    // _method所属的klass的引用
  jobject      _method_holder;
    // 栈上替换方法相对于字节码基地址的偏移
  int          _osr_bci;
    // 编译任务的状态
  bool         _is_complete;
  bool         _is_success;
  bool         _is_blocking;
    // 编译级别
  int          _comp_level;
    // 栈上替换方法对应的字节码的字节数
  int          _num_inlined_bytecodes;
    // nmethodLocker是nmethod的一个容器，用来保存编译完成的代码。
    // 负责保存编译结果的_code_handle属性不是在initialize方法中完成初始化的，而是通过另外的set_code/set_code_handle方法完成初始化的
  nmethodLocker* _code_handle;  // holder of eventual result
    // 编译任务队列中的下一个和上一个任务
  CompileTask* _next, *_prev;
  bool         _is_free;
  // Fields used for logging why the compilation was initiated:
    // 取值os::elapsed_counter()，表示JVM启动到现在的累计时间
  jlong        _time_queued;  // in units of os::elapsed_counter()
    // 触发这个方法编译的方法
  Method*      _hot_method;   // which method actually triggered this task
    // _hot_method所属的klass的引用
  jobject      _hot_method_holder;
    // 触发方法编译的调用次数
  int          _hot_count;    // information about its invocation counter
    // 编译任务的备注
  const char*  _comment;      // more info about the task
    // 编译任务失败原因
  const char*  _failure_reason;

 public:
  CompileTask() {
    _lock = new Monitor(Mutex::nonleaf+2, "CompileTaskLock");
  }

  void initialize(int compile_id, methodHandle method, int osr_bci, int comp_level,
                  methodHandle hot_method, int hot_count, const char* comment,
                  bool is_blocking);

  static CompileTask* allocate();
  static void         free(CompileTask* task);

  int          compile_id() const                { return _compile_id; }
  Method*      method() const                    { return _method; }
  int          osr_bci() const                   { return _osr_bci; }
  bool         is_complete() const               { return _is_complete; }
  bool         is_blocking() const               { return _is_blocking; }
  bool         is_success() const                { return _is_success; }

  nmethodLocker* code_handle() const             { return _code_handle; }
    //  set_code_handle的调用方有两个：
    // 其中CompileBroker::compiler_thread_loop方法调用此方法初始化_code_handle，CompileTaskWrapper析构方法中将 _code_handle置为NULL。
  void         set_code_handle(nmethodLocker* l) { _code_handle = l; }
  nmethod*     code() const;                     // _code_handle->code()
    // set_code的调用方有两个：
    // ciEnv::register_method方法是在nmethod初始化完成后将其设置到_code_handle的_nm属性，CompileTask::free是将 _code_handle的_nm属性置空。
  void         set_code(nmethod* nm);            // _code_handle->set_code(nm)

  Monitor*     lock() const                      { return _lock; }

  void         mark_complete()                   { _is_complete = true; }
  void         mark_success()                    { _is_success = true; }

  int          comp_level()                      { return _comp_level;}
  void         set_comp_level(int comp_level)    { _comp_level = comp_level;}

  int          num_inlined_bytecodes() const     { return _num_inlined_bytecodes; }
  void         set_num_inlined_bytecodes(int n)  { _num_inlined_bytecodes = n; }

  CompileTask* next() const                      { return _next; }
  void         set_next(CompileTask* next)       { _next = next; }
  CompileTask* prev() const                      { return _prev; }
  void         set_prev(CompileTask* prev)       { _prev = prev; }
  bool         is_free() const                   { return _is_free; }
  void         set_is_free(bool val)             { _is_free = val; }

private:
  static void  print_compilation_impl(outputStream* st, Method* method, int compile_id, int comp_level,
                                      bool is_osr_method = false, int osr_bci = -1, bool is_blocking = false,
                                      const char* msg = NULL, bool short_form = false);

public:
  void         print_compilation(outputStream* st = tty, const char* msg = NULL, bool short_form = false);
  static void  print_compilation(outputStream* st, const nmethod* nm, const char* msg = NULL, bool short_form = false) {
    print_compilation_impl(st, nm->method(), nm->compile_id(), nm->comp_level(),
                           nm->is_osr_method(), nm->is_osr_method() ? nm->osr_entry_bci() : -1, /*is_blocking*/ false,
                           msg, short_form);
  }

  static void  print_inlining(outputStream* st, ciMethod* method, int inline_level, int bci, const char* msg = NULL);
  static void  print_inlining(ciMethod* method, int inline_level, int bci, const char* msg = NULL) {
    print_inlining(tty, method, inline_level, bci, msg);
  }

  // Redefine Classes support
  void mark_on_stack();

  static void  print_inline_indent(int inline_level, outputStream* st = tty);

  void         print();
  void         print_line();
  void         print_line_on_error(outputStream* st, char* buf, int buflen);

  void         log_task(xmlStream* log);
  void         log_task_queued();
  void         log_task_start(CompileLog* log);
  void         log_task_done(CompileLog* log);

  void         set_failure_reason(const char* reason) {
    _failure_reason = reason;
  }
};

// CompilerCounters
//
// Per Compiler Performance Counters.
//
class CompilerCounters : public CHeapObj<mtCompiler> {

  public:
    enum {
      cmname_buffer_length = 160
    };

  private:

    char _current_method[cmname_buffer_length];
    PerfStringVariable* _perf_current_method;

    int  _compile_type;
    PerfVariable* _perf_compile_type;

    PerfCounter* _perf_time;
    PerfCounter* _perf_compiles;

  public:
    CompilerCounters(const char* name, int instance, TRAPS);

    // these methods should be called in a thread safe context

    void set_current_method(const char* method) {
      strncpy(_current_method, method, (size_t)cmname_buffer_length-1);
      _current_method[cmname_buffer_length-1] = '\0';
      if (UsePerfData) _perf_current_method->set_value(method);
    }

    char* current_method()                  { return _current_method; }

    void set_compile_type(int compile_type) {
      _compile_type = compile_type;
      if (UsePerfData) _perf_compile_type->set_value((jlong)compile_type);
    }

    int compile_type()                       { return _compile_type; }

    PerfCounter* time_counter()              { return _perf_time; }
    PerfCounter* compile_counter()           { return _perf_compiles; }
};

// CompileQueue
//
// A list of CompileTasks.
// CompileQueue同样继承自CHeapObj，表示CompileTask队列，其同样定义位于compileBroker.hpp中。CompileQueue定义的属性比较简单，主要是实现队列的必要属性
class CompileQueue : public CHeapObj<mtCompiler> {
 private:
  const char* _name;
  Monitor*    _lock;

  CompileTask* _first;
  CompileTask* _last;

    // _first_stale属性表示一段时间内不再被调用的方法的编译任务队列，通过CompileTask本身的_next/_prev属性构成队列
    // CompileQueue的构造方法将_first_stale属性置为空，purge_stale_tasks方法是遍历_first_stale队列，remove_and_mark_stale方法用于将某个任务从编译任务队列中移除，然后添加到_first_stale队列中
  CompileTask* _first_stale;

  int _size;

  void purge_stale_tasks();
 public:
  CompileQueue(const char* name, Monitor* lock) {
    _name = name;
    _lock = lock;
    _first = NULL;
    _last = NULL;
    _size = 0;
    _first_stale = NULL;
  }

  const char*  name() const                      { return _name; }
  Monitor*     lock() const                      { return _lock; }

  void         add(CompileTask* task);
  void         remove(CompileTask* task);
  void         remove_and_mark_stale(CompileTask* task);
  CompileTask* first()                           { return _first; }
  CompileTask* last()                            { return _last;  }

  CompileTask* get();

  bool         is_empty() const                  { return _first == NULL; }
  int          size()     const                  { return _size;          }


  // Redefine Classes support
  void mark_on_stack();
  void free_all();
  NOT_PRODUCT (void print();)

  ~CompileQueue() {
    assert (is_empty(), " Compile Queue must be empty");
  }
};

// CompileTaskWrapper
//
// Assign this task to the current thread.  Deallocate the task
// when the compilation is complete.
// CompileTaskWrapper就是一个包装器，通过构造方法将编译任务设置为编译线程的当前执行任务，通过析构函数完成编译任务的释放清理
class CompileTaskWrapper : StackObj {
public:
  CompileTaskWrapper(CompileTask* task);
  ~CompileTaskWrapper();
};


// Compilation
//
// The broker for all compilation requests.
// 编译器管理器  CompileBroker 就负责编译器的初始化，管理等等，根据注释 The broker for all compilation requests 就能看出他负责所有编译请求的处理
// CompileBroker定义在compileBroker.hpp中，表示一个负责接收并处理编译请求的经纪人，提供对外的编译相关的高度封装的方法。CompileBroker定义的属性都是静态属性
class CompileBroker: AllStatic {
 friend class Threads;
  friend class CompileTaskWrapper;

 public:
  enum {
    name_buffer_length = 100
  };

  // Compile type Information for print_last_compile() and CompilerCounters
  enum { no_compile, normal_compile, osr_compile, native_compile };
  static int assign_compile_id (methodHandle method, int osr_bci);


 private:
    // CompileBroker状态
  static bool _initialized;
  static volatile bool _should_block;

  // This flag can be used to stop compilation or turn it back on
    // 打标用，用来表示是否停止编译或者开启编译
  static volatile jint _should_compile_new_jobs;

  // The installed compiler(s)
    // 长度固定为2，安装的编译器实例
  static AbstractCompiler* _compilers[2];

  // These counters are used for assigning id's to each compilation
    // 临时保存的编译ID
  static volatile jint _compilation_id;
  static volatile jint _osr_compilation_id;

    // 上一次编译的编译类型，是一个枚举值，no_compile, normal_compile, osr_compile, native_compile
  static int  _last_compile_type;
    // 上一次的编译级别
  static int  _last_compile_level;
    // char数组，上一次编译的方法名
  static char _last_method_compiled[name_buffer_length];

    // C1和C2的编译任务队列
  static CompileQueue* _c2_compile_queue;
  static CompileQueue* _c1_compile_queue;

    // 编译线程数组
  static GrowableArray<CompilerThread*>* _compiler_threads;

    // 除此之外还有统计编译次数等数据的PerfCounter，PerfVariable类的属性。
  // performance counters
  static PerfCounter* _perf_total_compilation;
  static PerfCounter* _perf_native_compilation;
  static PerfCounter* _perf_osr_compilation;
  static PerfCounter* _perf_standard_compilation;

  static PerfCounter* _perf_total_bailout_count;
  static PerfCounter* _perf_total_invalidated_count;
  static PerfCounter* _perf_total_compile_count;
  static PerfCounter* _perf_total_native_compile_count;
  static PerfCounter* _perf_total_osr_compile_count;
  static PerfCounter* _perf_total_standard_compile_count;

  static PerfCounter* _perf_sum_osr_bytes_compiled;
  static PerfCounter* _perf_sum_standard_bytes_compiled;
  static PerfCounter* _perf_sum_nmethod_size;
  static PerfCounter* _perf_sum_nmethod_code_size;

  static PerfStringVariable* _perf_last_method;
  static PerfStringVariable* _perf_last_failed_method;
  static PerfStringVariable* _perf_last_invalidated_method;
  static PerfVariable*       _perf_last_compile_type;
  static PerfVariable*       _perf_last_compile_size;
  static PerfVariable*       _perf_last_failed_type;
  static PerfVariable*       _perf_last_invalidated_type;

  // Timers and counters for generating statistics
  static elapsedTimer _t_total_compilation;
  static elapsedTimer _t_osr_compilation;
  static elapsedTimer _t_standard_compilation;

  static int _total_compile_count;
  static int _total_bailout_count;
  static int _total_invalidated_count;
  static int _total_native_compile_count;
  static int _total_osr_compile_count;
  static int _total_standard_compile_count;
  static int _sum_osr_bytes_compiled;
  static int _sum_standard_bytes_compiled;
  static int _sum_nmethod_size;
  static int _sum_nmethod_code_size;
  static long _peak_compilation_time;

  static volatile jint _print_compilation_warning;

  static CompilerThread* make_compiler_thread(const char* name, CompileQueue* queue, CompilerCounters* counters, AbstractCompiler* comp, TRAPS);
  static void init_compiler_threads(int c1_compiler_count, int c2_compiler_count);
  static bool compilation_is_prohibited(methodHandle method, int osr_bci, int comp_level);
  static bool is_compile_blocking      ();
  static void preload_classes          (methodHandle method, TRAPS);

  static CompileTask* create_compile_task(CompileQueue* queue,
                                          int           compile_id,
                                          methodHandle  method,
                                          int           osr_bci,
                                          int           comp_level,
                                          methodHandle  hot_method,
                                          int           hot_count,
                                          const char*   comment,
                                          bool          blocking);
  static void wait_for_completion(CompileTask* task);

  static void invoke_compiler_on_method(CompileTask* task);
  static void set_last_compile(CompilerThread *thread, methodHandle method, bool is_osr, int comp_level);
  static void push_jni_handle_block();
  static void pop_jni_handle_block();
  static bool check_break_at(methodHandle method, int compile_id, bool is_osr);
  static void collect_statistics(CompilerThread* thread, elapsedTimer time, CompileTask* task);

  static void compile_method_base(methodHandle method,
                                  int osr_bci,
                                  int comp_level,
                                  methodHandle hot_method,
                                  int hot_count,
                                  const char* comment,
                                  Thread* thread);
  static CompileQueue* compile_queue(int comp_level) {
    if (is_c2_compile(comp_level)) return _c2_compile_queue;
    if (is_c1_compile(comp_level)) return _c1_compile_queue;
    return NULL;
  }
  static bool init_compiler_runtime();
  static void shutdown_compiler_runtime(AbstractCompiler* comp, CompilerThread* thread);

 public:
  enum {
    // The entry bci used for non-OSR compilations.
    standard_entry_bci = InvocationEntryBci
  };

  static AbstractCompiler* compiler(int comp_level) {
    if (is_c2_compile(comp_level)) return _compilers[1]; // C2
    if (is_c1_compile(comp_level)) return _compilers[0]; // C1
    return NULL;
  }

  static bool compilation_is_complete(methodHandle method, int osr_bci, int comp_level);
  static bool compilation_is_in_queue(methodHandle method);
  static int queue_size(int comp_level) {
    CompileQueue *q = compile_queue(comp_level);
    return q != NULL ? q->size() : 0;
  }
  static void compilation_init();
  static void init_compiler_thread_log();
  static nmethod* compile_method(methodHandle method,
                                 int osr_bci,
                                 int comp_level,
                                 methodHandle hot_method,
                                 int hot_count,
                                 const char* comment, Thread* thread);

  static void compiler_thread_loop();
  static uint get_compilation_id() { return _compilation_id; }

  // Set _should_block.
  // Call this from the VM, with Threads_lock held and a safepoint requested.
  static void set_should_block();

  // Call this from the compiler at convenient points, to poll for _should_block.
  static void maybe_block();

  enum {
    // Flags for toggling compiler activity
    stop_compilation    = 0,
    run_compilation     = 1,
    shutdown_compilaton = 2
  };

  static bool should_compile_new_jobs() { return UseCompiler && (_should_compile_new_jobs == run_compilation); }
  static bool set_should_compile_new_jobs(jint new_state) {
    // Return success if the current caller set it
    jint old = Atomic::cmpxchg(new_state, &_should_compile_new_jobs, 1-new_state);
    return (old == (1-new_state));
  }

  static void disable_compilation_forever() {
    UseCompiler               = false;
    AlwaysCompileLoopMethods  = false;
    Atomic::xchg(shutdown_compilaton, &_should_compile_new_jobs);
  }

  static bool is_compilation_disabled_forever() {
    return _should_compile_new_jobs == shutdown_compilaton;
  }
  static void handle_full_code_cache();
  // Ensures that warning is only printed once.
  static bool should_print_compiler_warning() {
    jint old = Atomic::cmpxchg(1, &_print_compilation_warning, 0);
    return old == 0;
  }
  // Return total compilation ticks
  static jlong total_compilation_ticks() {
    return _perf_total_compilation != NULL ? _perf_total_compilation->get_value() : 0;
  }

  // Redefine Classes support
  static void mark_on_stack();

  // Print a detailed accounting of compilation time
  static void print_times();

  // Debugging output for failure
  static void print_last_compile();

  static void print_compiler_threads_on(outputStream* st);

  // compiler name for debugging
  static const char* compiler_name(int comp_level);

  static int get_total_compile_count() {          return _total_compile_count; }
  static int get_total_bailout_count() {          return _total_bailout_count; }
  static int get_total_invalidated_count() {      return _total_invalidated_count; }
  static int get_total_native_compile_count() {   return _total_native_compile_count; }
  static int get_total_osr_compile_count() {      return _total_osr_compile_count; }
  static int get_total_standard_compile_count() { return _total_standard_compile_count; }
  static int get_sum_osr_bytes_compiled() {       return _sum_osr_bytes_compiled; }
  static int get_sum_standard_bytes_compiled() {  return _sum_standard_bytes_compiled; }
  static int get_sum_nmethod_size() {             return _sum_nmethod_size;}
  static int get_sum_nmethod_code_size() {        return _sum_nmethod_code_size; }
  static long get_peak_compilation_time() {       return _peak_compilation_time; }
  static long get_total_compilation_time() {      return _t_total_compilation.milliseconds(); }
};

#endif // SHARE_VM_COMPILER_COMPILEBROKER_HPP
