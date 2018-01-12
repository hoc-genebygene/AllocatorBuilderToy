#include "AlignedAllocator.h"
#include "BuddyAllocator.h"
#include "Mallocator.h"
#include "SlabAllocator.h"
#include "ThreadCachingAllocator.h"
#include "ThreadSafeAllocator.h"

#include <iostream>

using namespace AllocatorBuilder;

void ExerciseMallocator() {
    Mallocator::Mallocator<int> mallocator_instance;
    mallocator_instance.allocate(4);
}


void ExerciseAlignedAllocator() {
    AlignedAllocator::AlignedAllocator<int, 4096> aligned_allocator_instance;
    aligned_allocator_instance.allocate(4);
}

void ExerciseSlabAllocator() {
    SlabAllocator::SlabAllocator<int> slab_allocator_instance;
    slab_allocator_instance.allocate(4);
}

void ExerciseBuddyAllocator() {
    BuddyAllocator::BuddyAllocator<int, 16, 32> buddy_allocator_instance;
    int * array1 = buddy_allocator_instance.allocate(4);
    int * array2 = buddy_allocator_instance.allocate(8);
    int * array3 = buddy_allocator_instance.allocate(4);


    std::cout << sizeof(int) << std::endl;
    std::cout << (void *)std::addressof(array1[0]) << std::endl;
    std::cout << (void *)std::addressof(array2[0]) << std::endl;
    std::cout << (void *)std::addressof(array3[0]) << std::endl;
}

void ExerciseThreadSafeAllocator() {
    ThreadSafeAllocator::ThreadSafeAllocator<SlabAllocator::SlabAllocator<int>> thread_safe_slab_allocator;
    thread_safe_slab_allocator.allocate(4);
}

void ExerciseThreadCachingAllocator() {
    ThreadCachingAllocator::ThreadCachingAllocator<int, BuddyAllocator::BuddyAllocator<int, 16, 32>> thread_caching_allocator;
    thread_caching_allocator.allocate(4);
}

int main() {
    ExerciseMallocator();

    ExerciseAlignedAllocator();
    ExerciseSlabAllocator();
    ExerciseBuddyAllocator();
    ExerciseThreadSafeAllocator();
    ExerciseThreadCachingAllocator();
}
