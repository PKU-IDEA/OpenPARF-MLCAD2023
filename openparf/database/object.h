/**
 * @file   object.h
 * @author Yibo Lin
 * @date   Mar 2020
 */

#ifndef OPENPARF_DATABASE_OBJECT_H_
#define OPENPARF_DATABASE_OBJECT_H_

#include <algorithm>
#include <limits>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "util/util.h"

OPENPARF_BEGIN_NAMESPACE

namespace database {

class Object {
 public:
  using CoordinateType                    = CoordinateTraits<int32_t>::CoordinateType;
  using IndexType                         = CoordinateTraits<CoordinateType>::IndexType;
  constexpr static IndexType invalidIndex = std::numeric_limits<IndexType>::max();


  /// @brief default constructor
  Object() { setId(std::numeric_limits<IndexType>::max()); }

  /// @brief constructor
  explicit Object(IndexType id) : id_(id) {}

  /// @brief copy constructor
  Object(Object const &rhs) { copy(rhs); }

  /// @brief move constructor
  Object(Object &&rhs) { move(std::move(rhs)); }

  /// @brief copy assignment
  Object &operator=(Object const &rhs) {
    if (this != &rhs) {
      copy(rhs);
    }
    return *this;
  }

  /// @brief move assignment
  Object &operator=(Object &&rhs) {
    if (this != &rhs) {
      move(std::move(rhs));
    }
    return *this;
  }

  /// @brief destructor
  virtual ~Object() {}

  /// @brief get id of the object
  /// @return id
  IndexType            id() const { return id_; }

  /// @brief set id of the object
  /// @param id
  void                 setId(IndexType id) { id_ = id; }

  /// @brief summarize memory usage of the object in bytes
  virtual IndexType    memory() const { return sizeof(*this); }

  /// @brief overload output stream
  friend std::ostream &operator<<(std::ostream &os, Object const &rhs) {
    os << className<decltype(rhs)>() << "("
       << "id_ : " << rhs.id_ << ")";
    return os;
  }

 protected:
  /// @brief copy object
  void      copy(Object const &rhs);
  /// @brief move object
  void      move(Object &&rhs);

  IndexType id_;
};

template<typename T>
struct OutputChar {
  T const &operator()(T const &v) const { return v; }
};

template<>
struct OutputChar<int8_t> {
  int32_t operator()(int8_t const &v) const { return v; }
};

template<>
struct OutputChar<uint8_t> {
  uint32_t operator()(uint8_t const &v) const { return v; }
};

/// @brief output stream for vector
template<typename T>
std::ostream &operator<<(std::ostream &os, std::vector<T> const &rhs) {
  os << "[" << rhs.size() << "][";
  const char *delimiter = "";
  for (auto v : rhs) {
    os << delimiter << OutputChar<T>()(v);
    delimiter = ", ";
  }
  os << "]";
  return os;
}

/// @brief output stream for vector
template<typename T>
std::ostream &operator<<(std::ostream &os, std::vector<T *> const &rhs) {
  os << "[" << rhs.size() << "][";
  const char *delimiter = "";
  for (auto v : rhs) {
    os << delimiter << OutputChar<T>()(*v);
    delimiter = ", ";
  }
  os << "]";
  return os;
}

/// @brief input stream for vector
/// Assume the format is size + content
template<typename T>
std::istream &operator>>(std::istream &is, std::vector<T> &rhs) {
  Object::IndexType size;
  is >> size;
  rhs.resize(size);
  for (Object::IndexType i = 0; i < size; ++i) {
    is >> rhs[i];
  }
  return is;
}


/// @brief map for objects with name
/// @tparam T T must be derived from Object
template<class T>
class ObjectMap {
 public:
  using IndexType     = typename T::IndexType;
  using Iterator      = typename std::vector<T>::iterator;
  using ConstIterator = typename std::vector<T>::const_iterator;

  /// @brief default constructor
  ObjectMap() {}

  /// @brief copy constructor
  ObjectMap(ObjectMap const &rhs) { copy(rhs); }

  /// @brief move constructor
  ObjectMap(ObjectMap &&rhs) { move(std::move(rhs)); }

  /// @brief copy assignment
  ObjectMap &operator=(ObjectMap const &rhs) {
    if (this != &rhs) {
      copy(rhs);
    }
    return *this;
  }

  /// @brief move assignment
  ObjectMap &operator=(ObjectMap &&rhs) {
    if (this != &rhs) {
      move(std::move(rhs));
    }
    return *this;
  }

  T &tryAdd(std::string const &name) {
    auto it = map_.find(name);
    if (it == map_.end()) {
      IndexType id = vec_.size();
      vec_.emplace_back(id);
      map_.insert(std::make_pair(name, id));
      return vec_.back();
    } else {
      return vec_[it->second];
    }
  }

  bool     ifExist(std::string const &name) const { return map_.find(name) != map_.end(); }

  T const &fromStr(std::string const &name) const {
    if (!ifExist(name)) {
      openparfAssertMsg(false, "Cannot find %s\n", name.c_str());
    }
    return vec_[map_.find(name)->second];
  }

  T &fromStr(std::string const &name) {
    if (!ifExist(name)) {
      openparfAssertMsg(false, "Cannot find %s\n", name.c_str());
    }
    return vec_[map_.find(name)->second];
  }

  IndexType size() const { return vec_.size(); }

  T const  &operator[](IndexType id) const {
    openparfAssertMsg(id < vec_.size(), "id is out of range");
    return vec_[id];
  }

  T &operator[](IndexType id) {
    openparfAssertMsg(id < vec_.size(), "id is out of range");
    return vec_[id];
  }

  T const &at(IndexType id) const {
    openparfAssertMsg(id < vec_.size(), "id is out of range");
    return vec_[id];
  }

  T &at(IndexType id) {
    openparfAssertMsg(id < vec_.size(), "id is out of range");
    return vec_[id];
  }

  /// @brief begin iterator
  Iterator              begin() { return vec_.begin(); }

  /// @brief const begin iterator
  ConstIterator         begin() const { return vec_.begin(); }

  /// @brief end iterator
  Iterator              end() { return vec_.end(); }

  /// @brief const end iterator
  ConstIterator         end() const { return vec_.end(); }

  T const              &operator[](std::string const &name) const { return fromStr(name); }

  T                    &operator[](std::string const &name) { return fromStr(name); }

  T const              &at(std::string const &name) const { return fromStr(name); }

  T                    &at(std::string const &name) { return fromStr(name); }

  std::vector<T> const &vec() const { return vec_; }

  std::vector<T>       &vec() { return vec_; }

  IndexType             id(std::string const &name) const {
    if (!ifExist(name)) {
      // throw std::runtime_error("Cannot find " + name);
      openparfAssertMsg(false, "Cannot find %s\n", name.c_str());
    }
    return map_.find(name)->second;
  }

  void clear() {
    vec_.clear();
    map_.clear();
  }

  IndexType memory() const {
    IndexType mem = sizeof(*this);
    mem += vec_.capacity() * sizeof(T);
    mem += map_.size() * sizeof(std::pair<std::string, IndexType>);
    return mem;
  }

 protected:
  /// copy object
  void copy(ObjectMap const &rhs) {
    vec_ = rhs.vec_;
    map_ = rhs.map_;
  }

  /// move object
  void move(ObjectMap &&rhs) {
    vec_ = std::move(rhs.vec_);
    map_ = std::move(rhs.map_);
  }

  friend std::ostream &operator<<(std::ostream &os, ObjectMap const &rhs) {
    os << className<decltype(rhs)>();
    os << ", vec_: (";
    for (auto const &v : rhs.vec_) {
      os << v << ", ";
    }
    os << ")";
    os << ", map_: (";
    for (auto const &v : rhs.map_) {
      os << v.first << ": " << v.second << ", ";
    }
    os << ")";
    return os;
  }

  std::vector<T>                             vec_;
  std::unordered_map<std::string, IndexType> map_;
};
}   // namespace database

OPENPARF_END_NAMESPACE

#endif   // OPENPARF_DATABASE_OBJECT_H_
