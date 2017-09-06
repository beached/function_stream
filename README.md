# Function Stram

A parallel library

## [High Level Algorithms](./include/algorithms.h)

### for_each, for_each_n
``` C++
template<typename RandomIterator, typename Func> 
void for_each( RandomIterator const first, RandomIterator const last, Func func );			

template<typename Iterator, typename Func> 
void for_each_n( Iterator first, size_t N, Func func );
```

### fill
``` C++
template<typename Iterator, typename T> 
void fill( Iterator first, Iterator last, T const &value );
```

### sort, stable_sort
``` C++
template<typename Iterator, typename Compare> 
void sort( Iterator first, Iterator last, Compare compare );

template<typename Iterator, typename Compare> 
void stable_sort( Iterator first, Iterator last, Compare compare );
```

### reduce      
``` C++
template<typename T, typename Iterator, typename BinaryOp> 
T reduce( Iterator first, Iterator last, T init, BinaryOp binary_op );

template<typename T, typename Iterator> 
auto reduce( Iterator first, Iterator last, T init );

template<typename Iterator> 
auto reduce( Iterator first, Iterator last );
```

### min/max element
``` C++
template<typename Iterator, typename LessCompare> 
auto min_element( Iterator first, Iterator last, LessCompare compare = LessCompare{} );

template<typename Iterator, typename LessCompare> 
auto max_element( Iterator first, Iterator last, LessCompare compare = LessCompare{} );
```

### transform(map)
``` C++
template<typename Iterator, typename OutputIterator, typename UnaryOperation> 
void transform( Iterator first1, Iterator const last1, OutputIterator first2, UnaryOperation unary_op );

template<typename Iterator, typename UnaryOperation> 
void transform( Iterator first, Iterator last, UnaryOperation unary_op );
```

### combined map reduce
``` C++
template<typename Iterator, typename MapFunction, typename ReduceFunction> 
auto map_reduce( Iterator first, Iterator last, MapFunction map_function, ReduceFunction reduce_function );

template<typename Iterator, typename T, typename MapFunction, typename ReduceFunction> 
auto map_reduce( Iterator first, Iterator last, T const &init, MapFunction map_function, ReduceFunction reduce_function );
```

### scan(prefix sum)
``` C++
template<typename Iterator, typename OutputIterator, typename BinaryOp> 
void scan( Iterator first, Iterator last, OutputIterator first_out, OutputIterator last_out, BinaryOp binary_op );

template<typename Iterator, typename OutputIterator, typename BinaryOp> 
void scan( Iterator first, Iterator last, BinaryOp binary_op );
```

## [Task Based Parallelism](./include/task_scheduler.h)


## [Future's](./include/future_result.h)

## [Parallel Stream/Pipeline](./include/function_stream.h)
