project(AllocatorBuilderToy CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(AllocatorBuilderToy_HDRS
    AlignedAllocator.h
    BuddyAllocator.h
    Mallocator.h
    SlabAllocator.h
    ThreadCachingAllocator.h
    ThreadSafeAllocator.h
)

set(AllocatorBuilderToy_SRCS
    main.cpp
)

add_executable(main ${AllocatorBuilderToy_SRCS} ${AllocatorBuilderToy_HDRS})
