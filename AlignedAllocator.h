#pragma once

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <limits>
#include <new>
#include <type_traits>

namespace AllocatorBuilder {
namespace AlignedAllocator {
template <class T, size_t Alignment = alignof(T)>
class AlignedAllocator {
public:
    static_assert(Alignment >= alignof(T), "Requested Alignment should be larger than or equal to the minimum required for T");

    using value_type = T;
    using pointer = T*;
    using const_pointer = const T*;
    using reference = T&;
    using const_reference = const T&;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using propagate_on_container_move_assignment = std::true_type;

    template< class U, size_t OtherAlignment >
    struct rebind {
        typedef AlignedAllocator<U, OtherAlignment> other;
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
        void * mem = nullptr;

        int ret = posix_memalign(&mem, Alignment, n * sizeof(T));

        if (ret != 0) {
            throw std::bad_alloc();
        }

        return reinterpret_cast<T*>(mem);
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
} // namespace AlignedAllocator
} // namespace AllocatorBuilder
