# Function Stram

A parallel library

## [High Level Algorithms](./include/algorithms.h)

### for_each, for_each_n
Applies the given function object func to the result of dereferencing every iterator in the range [first, last) (not necessarily in order)
``` C++
template<typename RandomIterator, typename Func> 
void for_each( RandomIterator const first, RandomIterator const last, Func func );			

template<typename Iterator, typename Func> 
void for_each_n( Iterator first, size_t N, Func func );
```

### fill
Assigns value to the result of dereferencing every iterator in the range [first, last) (not necessarily in order)
``` C++
template<typename Iterator, typename T> 
void fill( Iterator first, Iterator last, T const &value );
```

### sort
Sorts the elements in the range [first, last) in ascending order. The order of equal elements is not guaranteed to be preserved.  Elements are compared using the given binary comparison function compare.
``` C++
template<typename Iterator, typename Compare> 
void sort( Iterator first, Iterator last, Compare compare );
```

### stable_sort
Sorts the elements in the range [first, last) in ascending order. The order of equal elements is guaranteed to be preserved.  Elements are compared using the given comparison function compare.
``` C++
template<typename Iterator, typename Compare> 
void stable_sort( Iterator first, Iterator last, Compare compare );
```
### min/max element
Finds the smallest(min) or largest(max) element in the range [first, last).  Elements are compared using the given binary comparison function compare.
``` C++
template<typename Iterator, typename LessCompare> 
auto min_element( Iterator first, Iterator last, LessCompare compare = LessCompare{} );

template<typename Iterator, typename LessCompare> 
auto max_element( Iterator first, Iterator last, LessCompare compare = LessCompare{} );
```

### reduce      
Reduces the range [first; last), possibly permuted and aggregated in unspecified manner, along with the initial value init over binary_op.
``` C++
template<typename T, typename Iterator, typename BinaryOp> 
T reduce( Iterator first, Iterator last, T init, BinaryOp binary_op );

template<typename T, typename Iterator> 
auto reduce( Iterator first, Iterator last, T init );

template<typename Iterator> 
auto reduce( Iterator first, Iterator last );
```

### transform(map)
Apply the given function unary_op to the result of dereferencing every iterator in the range [first, last) (not necessarily in order).  If supplied the result is stored in range [first2, first2 + std::distance( first, last )), or in place otherwise.
``` C++
template<typename Iterator, typename OutputIterator, typename UnaryOperation> 
void transform( Iterator first1, Iterator const last1, OutputIterator first2, UnaryOperation unary_op );

template<typename Iterator, typename UnaryOperation> 
void transform( Iterator first, Iterator last, UnaryOperation unary_op );
```

### combined map reduce
Apply the given function map_function to the result of dereferencing every iterator in the range [first, last) (not necessarily in order).  The results are passed to the binary_function reduce_function and returned.
``` C++
template<typename Iterator, typename MapFunction, typename ReduceFunction> 
auto map_reduce( Iterator first, Iterator last, MapFunction map_function, ReduceFunction reduce_function );

template<typename Iterator, typename T, typename MapFunction, typename ReduceFunction> 
auto map_reduce( Iterator first, Iterator last, T const &init, MapFunction map_function, ReduceFunction reduce_function );
```

### scan(prefix sum)
Computes the result of binary_op with the elements in the subranges of the range [first, last) and writes them to the range [first_out, last_out).   
``` C++
template<typename Iterator, typename OutputIterator, typename BinaryOp> 
void scan( Iterator first, Iterator last, OutputIterator first_out, OutputIterator last_out, BinaryOp binary_op );

template<typename Iterator, typename OutputIterator, typename BinaryOp> 
void scan( Iterator first, Iterator last, BinaryOp binary_op );
```

## [Task Based Parallelism](./include/task_scheduler.h)
Implementation of a task stealing work queue.  The default is to have 1 thread per core and block on destruction.

### get default task scheduler
``` C++
task_scheduler get_task_scheduler( );
```

### adding a task to queue
Add a simple task of the form void( ) to the queue.  
``` C++
void task_scheduler::add_task( task_t task ) noexcept;
```

### starting task scheduler
``` C++
void task_scheduler::start( );
```

### stopping task scheduler
``` C++
void task_scheduler::stop( bool block = true );
```

### check if task scheduler is started
``` C++
bool task_scheduler::started( ) const;
```

### check how many task queues are processing jobs
``` C++
size_t task_scheduler::size( ) const;
```

### indicate code sections that will block thread
Use blocking section to indicate that another thread can start processing the work queues while this one is blocked.  If the current thread is on of the task_scheduler's own, otherwise it will not start a new thread.  Function is of the form of void( ).
``` C++
template<typename Function>
void task_scheduler::blocking_section( Function func );

template<typename Function>
void blocking_section( task_scheduler & ts, Function func );

template<typename Function>
void blocking_section( Function func );
```

### Scheduling tasks
Add supplied task to task_scheduler ts and notify supplied semaphore when completed
``` C++
template<typename Task>
void schedule_task( daw::shared_semaphore semaphore, Task task, task_scheduler &ts );
```

Add supplied task to default task_scheduler and notify supplied semaphore when completed
``` C++
template<typename Task>
void schedule_task( daw::shared_semaphore semaphore, Task task );
```

Add supplied task to task_scheduler ts and return a semaphore that will block until it completes
``` C++
template<typename Task>
daw::shared_semaphore create_waitable_task( Task task, task_scheduler & ts );
```

Add supplied tasks to default task_scheduler and return a semaphore that will block until it completes 
``` C++
template<typename Task>
daw::shared_semaphore create_waitable_task( Task task );
```

Add supplied tasks to the default task_scheduler and return a semaphore that will block until all complete
``` C++
template<typename... Tasks>
daw::shared_semaphore create_task_group( Tasks &&... tasks );
```

Add supplied tasks to the default task_scheduler and return when they all complete
``` C++
template<typename... Tasks>
void invoke_tasks( Tasks &&... tasks );
```
## [Future's](./include/future_result.h)

## [Parallel Stream/Pipeline](./include/function_stream.h)
