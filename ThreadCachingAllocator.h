
#pragma once

#include <stdlib.h>

#include <limits>
#include <new>
#include <thread>
#include <type_traits>
#include <unordered_map>

namespace AllocatorBuilder {
namespace ThreadCachingAllocator {
namespace detail {
class ThreadCache {

};
} // namespace detail


template <class T, class BackingAllocator, size_t NumArenas = 8>
class ThreadCachingAllocator {
public:
    // std::allocator_traits
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

    // custom allocator traits
    using thread_safe = std::true_type;

//    static_assert(std::is_same<typename BackingAllocator::thread_safe, std::true_type>::value, "Backing allocator must be lock-safe");

    pointer address(reference x) const noexcept {
        return std::addressof(x);
    }

    const_pointer address(const_reference x) const noexcept {
        return std::addressof(x);
    }

    ThreadCachingAllocator() {
//        int res = pthread_key_create(thread_cache_key_);
//        if (res != 0) {
//            throw std::runtime_error("Could not create pthread thread-specific-data key");
//        }
    }

    pointer allocate(std::size_t n, const void * hint) {
        // purposefully ignore hint
        return allocate(n);
    }

    pointer allocate(std::size_t n) {
        // TODO
        return nullptr;
    }

    void deallocate(pointer p, std::size_t n) {
        // TODO
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
    BackingAllocator allocator_;

    pthread_key_t thread_cache_key_;
};
} // namespace ThreadCachingAllocator
} // namespace AllocatorBuilder
