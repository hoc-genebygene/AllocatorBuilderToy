#pragma once

#include <stdlib.h>

#include <limits>
#include <new>
#include <type_traits>

namespace AllocatorBuilder {
namespace Mallocator {
template <class T>
class Mallocator {
public:
    using value_type = T;
    using pointer = T*;                     
    using const_pointer = const T*;         
    using reference = T&;                   
    using const_reference = const T&;       
    using size_type = std::size_t;          
    using difference_type = std::ptrdiff_t; 
    using propagate_on_container_move_assignment = std::true_type;

    template< class U >
    struct rebind {
        typedef Mallocator<U> other;
    };

    using is_always_equal = std::true_type;

    pointer address(reference x) const noexcept {
        return std::addressof(x);
    }

    const_pointer address(const_reference x) const noexcept {
        return std::addressof(x);
    }

    T* allocate(std::size_t n, const void * hint) {
        // purposefully ignore hint
        return allocate(n);
    }

    T* allocate(std::size_t n) {
        // Because we cant rely on having Dynamic memory allocation for over-aligned data (http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2016/p0035r4.html) being available until GCC7, we have to use posix_memalign
        if (n > max_size()) {
            throw std::length_error("Tried to allocate more than the allocator will support");
        }

        T* mem_ptr;
        int res = posix_memalign((void **)&mem_ptr, alignof(T), n * sizeof(T));

        if (res == 0) {
            throw std::bad_alloc();
        }

        return mem_ptr;
    }

    void deallocate(T* p, std::size_t n) {
        free(p);
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
};
} // namespace Mallocator
} // namespace AllocatorBuilder
