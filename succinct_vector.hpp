/*
A modified Brodnik array following

"An Empirical Evaluation of Extendible Arrays", Stelios Joannou and Rajeev Raman

This vector uses no more than n + O(\sqrt{n}) space when it contains n
items. The idea is to keep about \sqrt{n} buffers, each of which holds
about \sqrt{n} items. No more than O(\sqrt{n}) of the item slots are
empty. The total space not holding items is this plus a buffer index
(describing the locations of the O(\sqrt{n})) buffers.

When n doubles or quarters, we rebuild either the buffer index or the
buffers themselves.

 */

/*
TODOs: 

copy std::vector behavior for default constructors of items
more constructors
use allocators
exception safety, especially on new and delete
move constructor, for C++11
iterators

 */

#ifndef SUCCINCT_VECTOR_HPP
#define SUCCINCT_VECTOR_HPP

#include <cstdint>
#include <cassert>

namespace succinct {

template<typename T>
struct vector {
public:
  typedef std::size_t size_t;
  //default constructor:
  vector();
  // copy constructor:
  vector(const vector &);
  // assignment operator
  vector &  operator=(const vector &);
  size_t size() const;
  ~vector();
  const T & operator[](const size_t i) const;
  T & operator[](const size_t i);
  
protected:

  // Since the buffers and the buffer index have size about \sqrt{n},
  // as long as size_t is uint64_t or smaller, we only need half as
  // many bits to decribe locations in the buffer and buffer index as
  // we do to describe locations in the vector.
  typedef std::uint32_t length_t;
  
  // The buffer directory, each entry of which is a pointer to a buffer:
  T ** dir; 
  length_t dir_size; 
  // The last buffer may not be filled to capacity, so we need to keep
  // track of where the last item in it is:
  length_t last_buffer_size; 
  
  // The log_2 of the buffer capacity. Buffers always have a size that
  // is a power of 2:
  length_t log_buffer_capacity : 5; 
  // The buffers can be twice as large as the directory. This is false
  // if they have the same size:
  bool big_buffer : 1; 
  // There may be an extra buffer pre-allocated. This prevents
  // thrashing at a buffer boundary. The Joannou & Raman and Brodnik
  // et al. papers assume that allocating a new block of memory takes
  // O(1) time. If it instead takes Θ(k) time, where k is the number
  // of bytes allocated, then this thrashing can make push_back and
  // pop_back ω(1) amortized.
  //
  // If there is an extra buffer, then the directory must have an
  // extra slot in which a pointer to that buffer is stored.
  bool extra_buffer : 1; 

  // These two capacity functions could be stored as member variables,
  // but that would take extra space.
  length_t 
  dir_capacity() const {
    assert (log_buffer_capacity > 0);
    const length_t log_dir_capacity = log_buffer_capacity - (big_buffer ? 1 : 0);
    return (static_cast<length_t>(1) << log_dir_capacity);
  }

  length_t 
  buffer_capacity() const {
    return (static_cast<length_t>(1) << log_buffer_capacity);
  }

  void
  assert_valid() const {
#ifdef NDEBUG
    return;
#endif
    assert (0 != dir);
    assert (0 < log_buffer_capacity);
    assert (dir_size <= dir_capacity());
    assert (dir_size > 0);
    assert (last_buffer_size < buffer_capacity());

    if (0 == last_buffer_size) {
      // Since the last buffer has no items in it, it actually *IS* an
      // extra buffer. We don't want two.
      assert (not extra_buffer);
    }

    if (extra_buffer) {
      assert (0 < last_buffer_size);
      // The pointer o the extra buffer is stored one-past-the end of
      // the dir, so there must be space for it.
      assert (dir_size < dir_capacity());
    }

    assert ((dir_size + (extra_buffer ? 1 : 0)) * 4 >= dir_capacity());
  }



  // In an array that is valid except for the directory (and thus, the
  // buffers), make a valid directory and buffers by copying the ones
  // from another vector with the intended shape and size.
  void
  construct(const vector & that) {
    assert_valid();
    for(length_t i = 0; i < dir_size; ++i) {
      dir[i] = new T[buffer_capacity()];
      //Note: this copies everything, even the uninitialized items
      std::copy(that.dir[i], that.dir[i] + that.buffer_capacity(), dir[i]);
    }
    if (extra_buffer) {
      dir[dir_size] = new T[buffer_capacity()];
    }
    assert_valid();
  }

  // Deallocate (and destruct the items in) every buffer and the directory
  void
  destuct() {
    for (size_t i = 0; i < dir_size; ++i) {
      delete[] dir[i];
    }
    if (extra_buffer) {
      delete[] dir[dir_size];
    }
    delete[] dir;
  }

public:  

  // Default constructor: size 0, capacity 2, max capacity before rebuild 4

protected:

  // Returns a reference to the ith item in the vector. Note that this
  // is actually const-unsafe - having a reference to that item allows
  // setting the value of that item. This is wrapped safely by the
  // operator[]s.
  T & 
  pget(const size_t i) const {
    assert_valid();
    assert (i < size());

    // The pointer into the dir
    const size_t big = i >> log_buffer_capacity;
    const size_t little = i & (buffer_capacity() - 1);
    assert (((big << log_buffer_capacity) + little) == i);
    // This is a rough bounds check - if it fails, it probably
    // indicates a programmer error, rather than a library error
    assert (big < static_cast<size_t>(dir_size)); 
    assert (little < static_cast<size_t>(buffer_capacity()));

    return dir[big][little];
  }

public:

  // Add an item to the end of the vector. O(1) amortized, Θ(n) worst
  // case.
  void 
  push_back(const T & x) {
#ifndef NDEBUG
    const size_t old_size = size();
#endif
    assert_valid();

    // The last_buffer_size is less than buffer_capactiy as an
    // invariant of the data structure.
    assert (last_buffer_size < buffer_capacity());
    dir[dir_size-1][last_buffer_size] = x;
    // After this statement, the data structure invariants may no
    // longer be valid:
    ++last_buffer_size;

    if (last_buffer_size == buffer_capacity()) {
      if (not extra_buffer) {
	if (dir_size == dir_capacity()) {
	  // We don't have an extra buffer, and we don't have any room
	  // to add another buffer to the directory. We must rebuild:
	  upsize();
	}
	assert (dir_size < dir_capacity());
	dir[dir_size] = new T[buffer_capacity()];
	// At this point, we have an extra buffer, but we are just
	// about to put it in the directory proper, thus making it not
	// "extra" at all.
	
	// extra_buffer = true;
      }
      // Expand the directory, using the extra buffer as a new empty buffer
      ++dir_size;
      extra_buffer = false;
      last_buffer_size = 0;
    }
    
    assert_valid();
    assert (size() == old_size + 1);
  }

  // Delete an item to the end of the vector. The vector must be
  // non-empty when this function is called. O(1) amortized, Θ(n)
  // worst case.
  void 
  pop_back() {
#ifndef NDEBUG
    const size_t old_size = size();
#endif
    assert_valid();
    // This is actually a precondition of pop_back(). If it fails,
    // this is probably a user error, not a library error.
    assert (size() > 0);

    if (0 == last_buffer_size) {
      // The last buffer is about to become the extra buffer.
      assert (not extra_buffer);
      last_buffer_size = buffer_capacity() - 1;
      --dir_size;
      extra_buffer = true;
    } else {
      --last_buffer_size;
      if ((0 == last_buffer_size)
	  and extra_buffer) {
	// Since the last buffer now has 0 items, to preserve the
	// invariants of the structure, we cannot have an extra
	// buffer. We cannot turn this last buffer with 0 items into
	// an empty buffer because the structure invariants ensure
	// that last_buffer_size < buffer_capacity().
	delete[] dir[dir_size];
	extra_buffer = false;

      }
    }
    if ((dir_size + (extra_buffer ? 1 : 0)) * 4 <= dir_capacity()) {
      // If the inequality was strict, we must have been equal before
      // this push_back, which means we should have already downsized.
      assert ((dir_size + (extra_buffer ? 1 : 0)) * 4 == dir_capacity());
      downsize();
    }

    assert_valid();
    assert (size() +1 == old_size);
  }

protected:

  // upsize gives gome empty space in the dir between dir_size and
  // dir_capacity
  void 
  upsize() {
#ifndef NDEBUG
    const size_t old_size = size();
#endif
    if (big_buffer) {
      upsize_dir();
    } else {
      upsize_buffers();
    }
    assert (size() == old_size);
  }

  void 
  downsize() {
#ifndef NDEBUG
    const size_t old_size = size();
#endif
    if (big_buffer) {
      downsize_buffers();
    } else {
      downsize_dir();
    }
    assert (size() == old_size);
  }


  // increase the dir_capacity
  void 
  upsize_dir() {
    assert (big_buffer);
    const length_t old_dir_capacity = dir_capacity();
    T ** old_dir = dir;
    dir = new T*[2*old_dir_capacity];
    std::copy(old_dir, old_dir + old_dir_capacity, dir);
    delete[] old_dir;
    big_buffer = false;
  }

  // decrease the dir_capacity
  void 
  downsize_dir() {
    const length_t old_dir_capacity = dir_capacity();
    T ** old_dir = dir;
    dir = new T*[old_dir_capacity/2];
    std::copy(old_dir, old_dir + dir_size, dir);
    delete[] old_dir;
    big_buffer = true;
  }


  // increase the buffer_capacity
  void 
  upsize_buffers() {   
    assert (not big_buffer);
    assert (not extra_buffer);
    assert (dir_size = dir_capacity());
    assert ((dir_capacity() & 1) == 0);
    assert (last_buffer_size == buffer_capacity()); 
    // note that this means the vector invariants do not hold at this point
    
    const length_t buf_cap = buffer_capacity();
  
    // We proceed along the directory, taking pairs of buffers and
    // merging them into one large buffer
    for(length_t i = 0; i < dir_size; i += 2) {
      T * const buf1  = dir[i];
      T * const buf2 = dir[i+1];
      T * const bigdir = new T[2 * buf_cap];
      dir[i/2] = bigdir;
      std::copy(buf1, buf1 + buf_cap, bigdir);
      std::copy(buf2, buf2 + buf_cap, bigdir + buf_cap);
      delete[] buf1;
      delete[] buf2;
    }
  
    ++log_buffer_capacity;
    last_buffer_size *= 2;
    big_buffer = true;
    dir_size /= 2;
  }

  // decrease the buffer_capacity
  void 
  downsize_buffers() { 
    assert (0 == last_buffer_size);
    assert (not extra_buffer);
    assert ((buffer_capacity() & 1) == 0);
    assert (dir_size * 2 < dir_capacity());
    assert (big_buffer);
    //assert ((dir_size & 1) == 1);

    const length_t buf_cap = buffer_capacity();

    // The last buffer has no items, and we don't need it any more
    delete[] dir[dir_size-1];
  
    // We proceed backward along the dir, splitting each buuffer intwo
    // two smaller buffers. We never overwrite a buffer of the larger
    // size before we store it in oldbuf on some iteration because the
    // directory is at least half empty.
    for(length_t i = 1; i < dir_size; ++i) {
      const auto k = dir_size-i-1;
      assert (k < dir_size - 1);
      T * const oldbuf  = dir[k];
      dir[2*k]   = new T[buf_cap/2];
      dir[2*k+1] = new T[buf_cap/2];
      std::copy(oldbuf,             oldbuf + buf_cap/2, dir[2*k  ]);
      std::copy(oldbuf + buf_cap/2, oldbuf + buf_cap,   dir[2*k+1]);
      delete[] oldbuf;
    }  

    // The buffers are now smaller:
    big_buffer = false;
    --log_buffer_capacity;

    // The directory filled up twice as many full blocks as before (2
    // (dir_size-1) = 2*dir_size - 2) and we're going to add one more
    // block to return to the vector invariant that last_buffer_size <
    // buffer_capacity(), so 2*dir_size - 1
    dir_size = 2*dir_size - 1;

    dir[dir_size-1] = new T[buffer_capacity()];
    assert (0 == last_buffer_size);
  }


}; // struct vector


  //default constructor
template<typename T> 
vector<T>::vector() :
  dir(new T*[1]),
  dir_size(1),
  last_buffer_size(0),
  log_buffer_capacity(1),
  big_buffer(true),
  extra_buffer(false)
{
  dir[0] = new T[buffer_capacity()];
  assert_valid();
}
  
  // copy constructor
template<typename T> 
vector<T>::vector(const vector<T> & that) :
  dir(new T*[that.dir_capacity()]),
  dir_size(that.dir_size),
  last_buffer_size(that.last_buffer_size),
  log_buffer_capacity(that.log_buffer_capacity),
  big_buffer(that.big_buffer),
  extra_buffer(that.extra_buffer)
{
  construct(that);
}

// assignment operator
template<typename T> 
vector<T> &
vector<T>::operator=(const vector<T> & that) {
  destuct();
  
  dir = new T*[that.dir_capacity()];
  dir_size = that.dir_size;
  last_buffer_size = that.last_buffer_size;
  log_buffer_capacity = that.log_buffer_capacity;
  big_buffer = that.big_buffer;
  extra_buffer = that.extra_buffer;
  
  construct(that);
  return *this;
}

template<typename T> 
size_t 
vector<T>::size() const {
  assert (dir_size > 0);
  return 
    (static_cast<size_t>(dir_size-1) << log_buffer_capacity) 
    + static_cast<size_t>(last_buffer_size);
}

template<typename T> 
vector<T>::~vector() {
  destuct();
}

template<typename T> 
const T & 
vector<T>::operator[](const size_t i) const {
  return pget(i);
}

template<typename T> 
T & 
vector<T>::operator[](const size_t i) {
  return pget(i);
}


} // namespace succinct
#endif
