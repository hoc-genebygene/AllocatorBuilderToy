#pragma once

#include <stdlib.h>

#include <cassert>
#include <deque>
#include <limits>
#include <list>
#include <new>
#include <type_traits>

namespace AllocatorBuilder {
namespace SlabAllocator {
template <class T>
class SlabAllocator {
public:
    using value_type = T;
    using pointer = T*;                     
    using const_pointer = const T*;         
    using reference = T&;                   
    using const_reference = const T&;       
    using size_type = std::size_t;          
    using difference_type = std::ptrdiff_t; 
    using propagate_on_container_move_assignment = std::true_type;

    template<class U>
    struct rebind {
        typedef SlabAllocator<U> other;
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

    T * allocate(std::size_t n) {
        if (n > Slab::NUM_SLAB_ELEMENTS) {
            return nullptr;
        }

        if (!empty_slabs_.empty()) {
            // Prefer to fill empty slabs first (TODO, determine if this is the best strategy)
            Slab * slab = empty_slabs_.back();
            empty_slabs_.pop_back();

            pointer ptr = slab->allocate(n);
            switch (slab->getSlabStatus()) {
                case Slab::SlabMetadata::SlabStatus::EMPTY:
                    assert(0); // We just allocated to slab, it can never be empty!
                case Slab::SlabMetadata::SlabStatus::PARTIAL:
                    partial_slabs_.push_back(slab);
                case Slab::SlabMetadata::SlabStatus::FULL:
                    full_slabs_.push_back(slab);
            }

            return ptr;
        } else if (!partial_slabs_.empty()) {
            auto it = std::find_if(partial_slabs_.begin(), partial_slabs_.end(), [n](Slab * slab){ return n <= slab->getNumFree(); });

            if (it != partial_slabs_.end()) {
                Slab * slab = *it;
                pointer ptr = slab->allocate(n);

                switch (slab->getSlabStatus()) {
                    case Slab::SlabMetadata::SlabStatus::EMPTY:
                        assert(0); // We just allocated to slab, it can never be empty!
                    case Slab::SlabMetadata::SlabStatus::PARTIAL:
                        break; // We started as a partial slab, so do nothing
                    case Slab::SlabMetadata::SlabStatus::FULL:
                        // Slab is now full, so erase from partially full slabs and put into full slab
                        partial_slabs_.erase(it);
                        full_slabs_.push_back(slab);
                }

                return ptr;
            }
        }
        // Purposefully no else here, we can fall through from partial slab case if we couldn't find a partial slab with enough free space

        allocated_slabs_.emplace_back();
        Slab * slab = &allocated_slabs_.back();
        pointer ptr = slab->allocate(n);

        if (n < Slab::NUM_SLAB_ELEMENTS) {
            partial_slabs_.push_back(slab);
        } else {
            assert(n == Slab::NUM_SLAB_ELEMENTS); // The case where n > Slab::NUM_SLAB_ELEMENTS is handled earlier
            full_slabs_.push_back(slab);
        }

        return ptr;
    }

    void deallocate(T * p, std::size_t n) {
        auto partial_it = std::find_if(partial_slabs_.begin(), partial_slabs_.end(), [p, n](const_pointer slab) { return slab->wasAllocatedHere(p, n); });
        if (partial_it != partial_slabs_.end()) {
            Slab * slab = *partial_it;
            slab->deallocate(p, n);
            if (slab->getSlabStatus() == Slab::SlabMetadata::SlabStatus::EMPTY) {
                partial_slabs_.erase(partial_it);
                empty_slabs_.push_back(slab);
            }

            return;
        }

        auto full_it = std::find_if(full_slabs_.begin(), full_slabs_.end(), [p, n](const_pointer slab) { return slab->wasAllocatedHere(p, n); });
        if (full_it != full_slabs_.end()) {
            Slab * slab = *full_it;
            full_slabs_.erase(full_it);
            slab->deallocate(p, n);
            if (slab->getSlabStatus() == Slab::SlabMetadata::SlabStatus::EMPTY) {
                empty_slabs_.push_back(slab);
            } else {
                partial_slabs_.push_back(slab);
            }
            return;
        }

        assert(0); // p was not found in partial_slabs_ or full_slabs_, something is wrong...
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
    class Slab {
    public:
        class SlabMetadata{
        public:
            enum class SlabStatus {
                EMPTY,
                PARTIAL,
                FULL
            };

            SlabStatus getSlabStatus() const {
                if (next_free_index_ == 0) {
                    return SlabStatus::EMPTY;
                } else if (next_free_index_ == NUM_SLAB_ELEMENTS) {
                    return SlabStatus::FULL;
                } else {
                    return SlabStatus::PARTIAL;
                }
            }

            size_t getNumFree() const {
                return NUM_SLAB_ELEMENTS - next_free_index_;
            }

            off_t getNextFreeIndex() const {
                return next_free_index_;
            }

            void incrementNextFreeIndex(std::size_t n) {
                assert(n >= getNumFree());
                next_free_index_ += n;
            }

            void deallocate(std::size_t n) {
                num_deallocated_ += n;
                assert(num_deallocated_ <= NUM_SLAB_ELEMENTS);

                if (num_deallocated_ == NUM_SLAB_ELEMENTS) {
                    clear(); // Reset the slab to empty!
                }
            }

            void clear() {
                next_free_index_ = 0;
                num_deallocated_ = 0;
            }

            SlabMetadata() : next_free_index_(0) {
            }

        private:
            off_t next_free_index_;
            size_t num_deallocated_;
        };

        T* allocate(std::size_t n) {
            assert(n >= metadata_.getNumFree()); // We should be looking at other slab if we cant find enough space
            T* ptr = &slab_space[metadata_.getNextFreeIndex()];
            metadata_.incrementNextFreeIndex(n);
            return ptr;
        }

        void deallocate(T* p, std::size_t n) {
            assert(wasAllocatedHere(p, n));
            metadata_.deallocate(n);
        }

        typename SlabMetadata::SlabStatus getSlabStatus() {
            return metadata_.getSlabStatus();
        }

        size_t getNumFree() {
            return metadata_.getNumFree();
        }

        bool wasAllocatedHere(pointer p, std::size_t n) {
            return p >= &slab_space[0] && p <= &slab_space[NUM_SLAB_ELEMENTS - 1];
            // TODO: For now ignore n, but we can write code in the future to assert for the cases for n > 1.
        }

    private:
        static const constexpr size_t SLAB_SIZE = 4096; // The size of the slab including its metadata
    public:
        static const constexpr size_t NUM_SLAB_ELEMENTS = (SLAB_SIZE - sizeof(SlabMetadata)) / sizeof(T);

    private:
        T slab_space[NUM_SLAB_ELEMENTS];

        SlabMetadata metadata_;
    };

    std::deque<Slab> allocated_slabs_;

    std::list<Slab *> empty_slabs_;
    std::list<Slab *> partial_slabs_;
    std::list<Slab *> full_slabs_;
};
} // namespace SlabAllocator
} // namespace AllocatorBuilder
