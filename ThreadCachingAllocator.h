
#pragma once

#include <stdlib.h>

#include <limits>
#include <new>
#include <thread>
#include <type_traits>
#include <unordered_map>

namespace AllocatorBuilder {
namespace ThreadCachingAllocator {
template <class T, class BackingAllocator, size_t NumArenas = 8>
class ThreadCachingAllocator {
public:
    using value_type = T;
    using pointer = T*;
    using const_pointer = const T*;
    using reference = T&;
    using const_reference = const T&;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using propagate_on_container_move_assignment = std::true_type;

    template<class U, class OtherBackingAllocator, size_t OtherNumArenas>
    struct rebind {
        typedef ThreadCachingAllocator<U, OtherBackingAllocator, OtherNumArenas> other;
    };

    using is_always_equal = std::true_type;

    ThreadCachingAllocator() : arenas_(NumArenas) {}

    pointer address(reference x) const noexcept {
        return std::addressof(x);
    }

    const_pointer address(const_reference x) const noexcept {
        return std::addressof(x);
    }

    pointer allocate(std::size_t n, const void * hint) {
        // purposefully ignore hint
        return allocate(n);
    }

    pointer allocate(std::size_t n) {
        std::thread::id thread_id = std::this_thread::get_id();
        

        auto it = arenas_.find(thread_id);
        if (it == arenas_.end()) {
            auto res = arenas_.emplace(thread_id, BackingAllocator{});
            it = res.first;

            assert(res.second);
        }

        // it should now point to an allocator
        auto & allocator = it->second;
        pointer mem = allocator.allocate(n);

        memory_to_allocator_map_.emplace(mem, &allocator);

        return mem;
    }

    void deallocate(pointer p, std::size_t n) {
        auto it = memory_to_allocator_map_.find(p);
        if (it == memory_to_allocator_map_.end()) {
            assert(0);
            throw std::logic_error("Deallocated something that wasn't allocated here");
            return;
        }

        auto & allocator = it->second;
        allocator.deallocate(p);
    }

    size_type max_size() const noexcept {
        return std::numeric_limits<size_type>::max() / sizeof(value_type);
    }

    template <class U, class... Args>
    void construct(U * p, Args&&... args) {
        ::new((void *)p) U(std::forward<Args>(args)...);
    }

    template <class U>
    void destroy(U * p) {
        p->~U();
    }
private:
    std::unordered_map<std::thread::id, BackingAllocator> arenas_;
    std::unordered_map<pointer, BackingAllocator *> memory_to_allocator_map_;
};
} // namespace ThreadCachingAllocator
} // namespace AllocatorBuilder
