#ifndef ALIGNMENT_ALLOCATOR_H
#define ALIGNMENT_ALLOCATOR_H

#include <cstddef>
#include <cstdlib>
#include <new>

#if defined(__GNUC__) || defined(__clang__)
  #include <mm_malloc.h>    // provides _mm_malloc / _mm_free
#elif defined(_MSC_VER)
  #include <malloc.h>       // MSVC: _aligned_malloc / _aligned_free
#endif

template <typename T, std::size_t Alignment = 32>
class AlignmentAllocator {
public:
  using value_type      = T;
  using size_type       = std::size_t;
  using difference_type = std::ptrdiff_t;
  using pointer         = T*;
  using const_pointer   = const T*;
  using reference       = T&;
  using const_reference = const T&;

  template <typename U>
  struct rebind { using other = AlignmentAllocator<U, Alignment>; };

  AlignmentAllocator() noexcept {}
  template <typename U>
  AlignmentAllocator(const AlignmentAllocator<U, Alignment>&) noexcept {}

  pointer allocate(size_type n) {
    if (n == 0) return nullptr;
    void* ptr = nullptr;
  #if defined(__GNUC__) || defined(__clang__)
    ptr = _mm_malloc(n * sizeof(value_type), Alignment);
    if (!ptr) throw std::bad_alloc();
  #elif defined(_MSC_VER)
    ptr = _aligned_malloc(n * sizeof(value_type), Alignment);
    if (!ptr) throw std::bad_alloc();
  #else
    // fallback to posix_memalign
    if (posix_memalign(&ptr, Alignment, n * sizeof(value_type)) != 0)
      throw std::bad_alloc();
  #endif
    return static_cast<pointer>(ptr);
  }

  void deallocate(pointer p, size_type) noexcept {
    if (!p) return;
  #if defined(__GNUC__) || defined(__clang__)
    _mm_free(p);
  #elif defined(_MSC_VER)
    _aligned_free(p);
  #else
    free(p);
  #endif
  }

  // construction/destruction
  void construct(pointer p, const_reference val) { ::new ((void*)p) value_type(val); }
  void destroy(pointer p) { p->~value_type(); }

  size_type max_size() const noexcept {
    return static_cast<size_type>(-1) / sizeof(value_type);
  }

  // comparisons
  bool operator==(const AlignmentAllocator&) const noexcept { return true; }
  bool operator!=(const AlignmentAllocator& other) const noexcept { return !(*this == other); }
};

#endif // ALIGNMENT_ALLOCATOR_H
