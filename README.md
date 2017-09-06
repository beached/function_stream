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


## [Future's](./include/future_result.h)

## [Parallel Stream/Pipeline](./include/function_stream.h)
