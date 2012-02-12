#ifndef SPACE_HPP
#define SPACE_HPP

#include <cstdint>
#include <cassert>

template<typename T>
struct space {

protected:
  typedef std::uint32_t length_t;
  typedef std::size_t size_t;

  length_t log_buffer_capacity : 5;// = 1;
  bool big_buffer : 1;// = true;
  bool extra_buffer : 1;// = false
  length_t last_buffer_size;// = 0;
  length_t dir_size;// = 1;
  T ** dir;

  length_t dir_capacity() const {
    return (static_cast<length_t>(1) << (log_buffer_capacity - (big_buffer ? 1 : 0)));
  }

  length_t buffer_capacity() const {
    return (static_cast<length_t>(1) << log_buffer_capacity);
  }

  bool valid() const {
#ifndef NDEBUG
    assert (0 != dir);
    assert (0 < log_buffer_capacity);
    assert (dir_size <= dir_capacity());
    assert (dir_size > 0);
    if (0 == last_buffer_size) {
      assert (not extra_buffer);
    }
    assert (last_buffer_size < buffer_capacity());
    if (extra_buffer) {
      assert (0 < last_buffer_size);
      assert (dir_size < dir_capacity());
    }
#endif
    return true;
  }
public:  
  space() :
    log_buffer_capacity(1),
    big_buffer(true),
    extra_buffer(false),
    last_buffer_size(0),
    dir_size(1),
    dir(new T*[1])
  {
    dir[0] = new T[buffer_capacity()];
    assert (valid());
  }
  
  size_t size() const {
    assert (dir_size > 0);
    return (static_cast<size_t>(dir_size-1) << log_buffer_capacity) + static_cast<size_t>(last_buffer_size);
  }

  T & get(const size_t i) {
    assert (valid());
    assert (i < size());

    const size_t big = i >> log_buffer_capacity;
    const size_t little = i & ((static_cast<size_t>(1) << log_buffer_capacity)-1);
    assert (((big << log_buffer_capacity) + little) == i);
    assert (big < static_cast<size_t>(dir_size));
    assert (little < static_cast<size_t>(buffer_capacity()));

    return dir[big][little];
  }
    
  void push_back(const T & x) {
#ifndef NDEBUG
    const size_t old_size = size();
#endif
    assert (valid());

    dir[dir_size-1][last_buffer_size] = x;
    ++last_buffer_size;

    if (last_buffer_size == buffer_capacity()) {
      if (not extra_buffer) {
	if (dir_size == dir_capacity()) {
	  upsize();
	}
	dir[dir_size] = new T[buffer_capacity()];
	//extra_buffer = true;
      }
      ++dir_size;
      extra_buffer = false;
      last_buffer_size = 0;
    }
    
    assert (valid());
    assert (size() == old_size + 1);
  }

  void pop_back() {
#ifndef NDEBUG
    const size_t old_size = size();
#endif
    assert (valid());
    assert (size() > 0);

    if (0 == last_buffer_size) {
      assert (not extra_buffer);
      last_buffer_size = buffer_capacity() - 1;
      --dir_size;
      extra_buffer = true;
    } else {
      --last_buffer_size;
      if ((0 == last_buffer_size)
	  and (extra_buffer)) {
	delete[] dir[dir_size];
	extra_buffer = false;
	//--dir_size;
	if (dir_size * 4 <= dir_capacity()) {
	  assert (dir_size * 4 == dir_capacity());
	  downsize();
	}
      }
    }
    assert (valid());
    assert (size() +1 == old_size);
  }

protected:

  void upsize() {
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

  void downsize() {
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


  void upsize_dir() {
    const length_t old_dir_capacity = dir_capacity();
    T ** old_dir = dir;
    dir = new T*[2*old_dir_capacity];
    for(length_t i = 0; i < old_dir_capacity; ++i) {
      dir[i] = old_dir[i];
    }
    delete[] old_dir;
    big_buffer = false;
  }

  void downsize_dir() {
    const length_t old_dir_capacity = dir_capacity();
    T ** old_dir = dir;
    dir = new T*[old_dir_capacity/2];
    for(length_t i = 0; i < dir_size; ++i) {
      dir[i] = old_dir[i];
    }
    delete[] old_dir;
    big_buffer = true;
  }


  void upsize_buffers() {   
    const length_t buf_cap = buffer_capacity();
  
    for(length_t i = 0; i < dir_size; i += 2) {
      T * const buf1  = dir[i];
      T * const buf2 = dir[i+1];
      T * const bigdir = new T[2 * buf_cap];
      dir[i/2] = bigdir;
      for (length_t j = 0; j < buf_cap; ++j) {
	bigdir[j] = buf1[j];
      }
      for (length_t j = 0; j < buf_cap; ++j) {
	bigdir[buf_cap + j] = buf2[j];
      }
      delete[] buf1;
      delete[] buf2;
    }
  
    ++log_buffer_capacity;
    last_buffer_size *= 2;
    big_buffer = true;
    dir_size /= 2;
  }

  void downsize_buffers() { 
    assert (0 == last_buffer_size);
    assert (not extra_buffer);
    const length_t buf_cap = buffer_capacity();
  
    for(length_t i = 0; i < dir_size; ++i) {
      const auto k = dir_size-i-1;
      T * const oldbuf  = dir[k];
      dir[2*k]   = new T[buf_cap/2];
      dir[2*k+1] = new T[buf_cap/2];
      for (length_t j = 0; j < buf_cap/2; ++j) {
	dir[2*k][j] = oldbuf[j];
      }
      for (length_t j = 0; j < buf_cap/2; ++j) {
	dir[2*k+1][j] = oldbuf[buf_cap/2+j];
      }
      delete[] oldbuf;
    }
    

    
    --log_buffer_capacity;
    //last_buffer_size /= 2;
    big_buffer = false;
    dir_size = 2*dir_size - 1;
    delete[] dir[dir_size];
  }


};

#endif
