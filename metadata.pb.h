// Generated by the protocol buffer compiler.  DO NOT EDIT!
// source: metadata.proto

#ifndef PROTOBUF_metadata_2eproto__INCLUDED
#define PROTOBUF_metadata_2eproto__INCLUDED

#include <string>

#include <google/protobuf/stubs/common.h>

#if GOOGLE_PROTOBUF_VERSION < 2005000
#error This file was generated by a newer version of protoc which is
#error incompatible with your Protocol Buffer headers.  Please update
#error your headers.
#endif
#if 2005000 < GOOGLE_PROTOBUF_MIN_PROTOC_VERSION
#error This file was generated by an older version of protoc which is
#error incompatible with your Protocol Buffer headers.  Please
#error regenerate this file with a newer version of protoc.
#endif

#include <google/protobuf/generated_message_util.h>
#include <google/protobuf/message.h>
#include <google/protobuf/repeated_field.h>
#include <google/protobuf/extension_set.h>
#include <google/protobuf/generated_enum_reflection.h>
#include <google/protobuf/unknown_field_set.h>
// @@protoc_insertion_point(includes)

namespace posixok {

// Internal implementation detail -- do not call these.
void  protobuf_AddDesc_metadata_2eproto();
void protobuf_AssignDesc_metadata_2eproto();
void protobuf_ShutdownFile_metadata_2eproto();

class Metadata;
class Metadata_ReachabilityEntry;

enum Metadata_ReachabilityType {
  Metadata_ReachabilityType_UID = 0,
  Metadata_ReachabilityType_GID = 1,
  Metadata_ReachabilityType_UID_OR_GID = 2,
  Metadata_ReachabilityType_NOT_UID = 3,
  Metadata_ReachabilityType_NOT_GID = 4,
  Metadata_ReachabilityType_GID_REQ_UID = 5
};
bool Metadata_ReachabilityType_IsValid(int value);
const Metadata_ReachabilityType Metadata_ReachabilityType_ReachabilityType_MIN = Metadata_ReachabilityType_UID;
const Metadata_ReachabilityType Metadata_ReachabilityType_ReachabilityType_MAX = Metadata_ReachabilityType_GID_REQ_UID;
const int Metadata_ReachabilityType_ReachabilityType_ARRAYSIZE = Metadata_ReachabilityType_ReachabilityType_MAX + 1;

const ::google::protobuf::EnumDescriptor* Metadata_ReachabilityType_descriptor();
inline const ::std::string& Metadata_ReachabilityType_Name(Metadata_ReachabilityType value) {
  return ::google::protobuf::internal::NameOfEnum(
    Metadata_ReachabilityType_descriptor(), value);
}
inline bool Metadata_ReachabilityType_Parse(
    const ::std::string& name, Metadata_ReachabilityType* value) {
  return ::google::protobuf::internal::ParseNamedEnum<Metadata_ReachabilityType>(
    Metadata_ReachabilityType_descriptor(), name, value);
}
// ===================================================================

class Metadata_ReachabilityEntry : public ::google::protobuf::Message {
 public:
  Metadata_ReachabilityEntry();
  virtual ~Metadata_ReachabilityEntry();

  Metadata_ReachabilityEntry(const Metadata_ReachabilityEntry& from);

  inline Metadata_ReachabilityEntry& operator=(const Metadata_ReachabilityEntry& from) {
    CopyFrom(from);
    return *this;
  }

  inline const ::google::protobuf::UnknownFieldSet& unknown_fields() const {
    return _unknown_fields_;
  }

  inline ::google::protobuf::UnknownFieldSet* mutable_unknown_fields() {
    return &_unknown_fields_;
  }

  static const ::google::protobuf::Descriptor* descriptor();
  static const Metadata_ReachabilityEntry& default_instance();

  void Swap(Metadata_ReachabilityEntry* other);

  // implements Message ----------------------------------------------

  Metadata_ReachabilityEntry* New() const;
  void CopyFrom(const ::google::protobuf::Message& from);
  void MergeFrom(const ::google::protobuf::Message& from);
  void CopyFrom(const Metadata_ReachabilityEntry& from);
  void MergeFrom(const Metadata_ReachabilityEntry& from);
  void Clear();
  bool IsInitialized() const;

  int ByteSize() const;
  bool MergePartialFromCodedStream(
      ::google::protobuf::io::CodedInputStream* input);
  void SerializeWithCachedSizes(
      ::google::protobuf::io::CodedOutputStream* output) const;
  ::google::protobuf::uint8* SerializeWithCachedSizesToArray(::google::protobuf::uint8* output) const;
  int GetCachedSize() const { return _cached_size_; }
  private:
  void SharedCtor();
  void SharedDtor();
  void SetCachedSize(int size) const;
  public:

  ::google::protobuf::Metadata GetMetadata() const;

  // nested types ----------------------------------------------------

  // accessors -------------------------------------------------------

  // required .posixok.Metadata.ReachabilityType type = 1;
  inline bool has_type() const;
  inline void clear_type();
  static const int kTypeFieldNumber = 1;
  inline ::posixok::Metadata_ReachabilityType type() const;
  inline void set_type(::posixok::Metadata_ReachabilityType value);

  // optional uint32 gid = 2;
  inline bool has_gid() const;
  inline void clear_gid();
  static const int kGidFieldNumber = 2;
  inline ::google::protobuf::uint32 gid() const;
  inline void set_gid(::google::protobuf::uint32 value);

  // optional uint32 uid = 3;
  inline bool has_uid() const;
  inline void clear_uid();
  static const int kUidFieldNumber = 3;
  inline ::google::protobuf::uint32 uid() const;
  inline void set_uid(::google::protobuf::uint32 value);

  // @@protoc_insertion_point(class_scope:posixok.Metadata.ReachabilityEntry)
 private:
  inline void set_has_type();
  inline void clear_has_type();
  inline void set_has_gid();
  inline void clear_has_gid();
  inline void set_has_uid();
  inline void clear_has_uid();

  ::google::protobuf::UnknownFieldSet _unknown_fields_;

  int type_;
  ::google::protobuf::uint32 gid_;
  ::google::protobuf::uint32 uid_;

  mutable int _cached_size_;
  ::google::protobuf::uint32 _has_bits_[(3 + 31) / 32];

  friend void  protobuf_AddDesc_metadata_2eproto();
  friend void protobuf_AssignDesc_metadata_2eproto();
  friend void protobuf_ShutdownFile_metadata_2eproto();

  void InitAsDefaultInstance();
  static Metadata_ReachabilityEntry* default_instance_;
};
// -------------------------------------------------------------------

class Metadata : public ::google::protobuf::Message {
 public:
  Metadata();
  virtual ~Metadata();

  Metadata(const Metadata& from);

  inline Metadata& operator=(const Metadata& from) {
    CopyFrom(from);
    return *this;
  }

  inline const ::google::protobuf::UnknownFieldSet& unknown_fields() const {
    return _unknown_fields_;
  }

  inline ::google::protobuf::UnknownFieldSet* mutable_unknown_fields() {
    return &_unknown_fields_;
  }

  static const ::google::protobuf::Descriptor* descriptor();
  static const Metadata& default_instance();

  void Swap(Metadata* other);

  // implements Message ----------------------------------------------

  Metadata* New() const;
  void CopyFrom(const ::google::protobuf::Message& from);
  void MergeFrom(const ::google::protobuf::Message& from);
  void CopyFrom(const Metadata& from);
  void MergeFrom(const Metadata& from);
  void Clear();
  bool IsInitialized() const;

  int ByteSize() const;
  bool MergePartialFromCodedStream(
      ::google::protobuf::io::CodedInputStream* input);
  void SerializeWithCachedSizes(
      ::google::protobuf::io::CodedOutputStream* output) const;
  ::google::protobuf::uint8* SerializeWithCachedSizesToArray(::google::protobuf::uint8* output) const;
  int GetCachedSize() const { return _cached_size_; }
  private:
  void SharedCtor();
  void SharedDtor();
  void SetCachedSize(int size) const;
  public:

  ::google::protobuf::Metadata GetMetadata() const;

  // nested types ----------------------------------------------------

  typedef Metadata_ReachabilityEntry ReachabilityEntry;

  typedef Metadata_ReachabilityType ReachabilityType;
  static const ReachabilityType UID = Metadata_ReachabilityType_UID;
  static const ReachabilityType GID = Metadata_ReachabilityType_GID;
  static const ReachabilityType UID_OR_GID = Metadata_ReachabilityType_UID_OR_GID;
  static const ReachabilityType NOT_UID = Metadata_ReachabilityType_NOT_UID;
  static const ReachabilityType NOT_GID = Metadata_ReachabilityType_NOT_GID;
  static const ReachabilityType GID_REQ_UID = Metadata_ReachabilityType_GID_REQ_UID;
  static inline bool ReachabilityType_IsValid(int value) {
    return Metadata_ReachabilityType_IsValid(value);
  }
  static const ReachabilityType ReachabilityType_MIN =
    Metadata_ReachabilityType_ReachabilityType_MIN;
  static const ReachabilityType ReachabilityType_MAX =
    Metadata_ReachabilityType_ReachabilityType_MAX;
  static const int ReachabilityType_ARRAYSIZE =
    Metadata_ReachabilityType_ReachabilityType_ARRAYSIZE;
  static inline const ::google::protobuf::EnumDescriptor*
  ReachabilityType_descriptor() {
    return Metadata_ReachabilityType_descriptor();
  }
  static inline const ::std::string& ReachabilityType_Name(ReachabilityType value) {
    return Metadata_ReachabilityType_Name(value);
  }
  static inline bool ReachabilityType_Parse(const ::std::string& name,
      ReachabilityType* value) {
    return Metadata_ReachabilityType_Parse(name, value);
  }

  // accessors -------------------------------------------------------

  // optional uint32 atime = 1 [default = 0];
  inline bool has_atime() const;
  inline void clear_atime();
  static const int kAtimeFieldNumber = 1;
  inline ::google::protobuf::uint32 atime() const;
  inline void set_atime(::google::protobuf::uint32 value);

  // required uint32 mtime = 2;
  inline bool has_mtime() const;
  inline void clear_mtime();
  static const int kMtimeFieldNumber = 2;
  inline ::google::protobuf::uint32 mtime() const;
  inline void set_mtime(::google::protobuf::uint32 value);

  // required uint32 ctime = 3;
  inline bool has_ctime() const;
  inline void clear_ctime();
  static const int kCtimeFieldNumber = 3;
  inline ::google::protobuf::uint32 ctime() const;
  inline void set_ctime(::google::protobuf::uint32 value);

  // required uint32 id_user = 4;
  inline bool has_id_user() const;
  inline void clear_id_user();
  static const int kIdUserFieldNumber = 4;
  inline ::google::protobuf::uint32 id_user() const;
  inline void set_id_user(::google::protobuf::uint32 value);

  // required uint32 id_group = 5;
  inline bool has_id_group() const;
  inline void clear_id_group();
  static const int kIdGroupFieldNumber = 5;
  inline ::google::protobuf::uint32 id_group() const;
  inline void set_id_group(::google::protobuf::uint32 value);

  // required uint32 mode = 6;
  inline bool has_mode() const;
  inline void clear_mode();
  static const int kModeFieldNumber = 6;
  inline ::google::protobuf::uint32 mode() const;
  inline void set_mode(::google::protobuf::uint32 value);

  // optional uint32 link_count = 7 [default = 1];
  inline bool has_link_count() const;
  inline void clear_link_count();
  static const int kLinkCountFieldNumber = 7;
  inline ::google::protobuf::uint32 link_count() const;
  inline void set_link_count(::google::protobuf::uint32 value);

  // optional uint32 size = 8 [default = 0];
  inline bool has_size() const;
  inline void clear_size();
  static const int kSizeFieldNumber = 8;
  inline ::google::protobuf::uint32 size() const;
  inline void set_size(::google::protobuf::uint32 value);

  // optional uint32 blocks = 9 [default = 0];
  inline bool has_blocks() const;
  inline void clear_blocks();
  static const int kBlocksFieldNumber = 9;
  inline ::google::protobuf::uint32 blocks() const;
  inline void set_blocks(::google::protobuf::uint32 value);

  // optional string data_unique_id = 11;
  inline bool has_data_unique_id() const;
  inline void clear_data_unique_id();
  static const int kDataUniqueIdFieldNumber = 11;
  inline const ::std::string& data_unique_id() const;
  inline void set_data_unique_id(const ::std::string& value);
  inline void set_data_unique_id(const char* value);
  inline void set_data_unique_id(const char* value, size_t size);
  inline ::std::string* mutable_data_unique_id();
  inline ::std::string* release_data_unique_id();
  inline void set_allocated_data_unique_id(::std::string* data_unique_id);

  // optional int64 path_permission_verified = 13 [default = 0];
  inline bool has_path_permission_verified() const;
  inline void clear_path_permission_verified();
  static const int kPathPermissionVerifiedFieldNumber = 13;
  inline ::google::protobuf::int64 path_permission_verified() const;
  inline void set_path_permission_verified(::google::protobuf::int64 value);

  // repeated .posixok.Metadata.ReachabilityEntry path_permission = 14;
  inline int path_permission_size() const;
  inline void clear_path_permission();
  static const int kPathPermissionFieldNumber = 14;
  inline const ::posixok::Metadata_ReachabilityEntry& path_permission(int index) const;
  inline ::posixok::Metadata_ReachabilityEntry* mutable_path_permission(int index);
  inline ::posixok::Metadata_ReachabilityEntry* add_path_permission();
  inline const ::google::protobuf::RepeatedPtrField< ::posixok::Metadata_ReachabilityEntry >&
      path_permission() const;
  inline ::google::protobuf::RepeatedPtrField< ::posixok::Metadata_ReachabilityEntry >*
      mutable_path_permission();

  // repeated .posixok.Metadata.ReachabilityEntry path_permission_children = 15;
  inline int path_permission_children_size() const;
  inline void clear_path_permission_children();
  static const int kPathPermissionChildrenFieldNumber = 15;
  inline const ::posixok::Metadata_ReachabilityEntry& path_permission_children(int index) const;
  inline ::posixok::Metadata_ReachabilityEntry* mutable_path_permission_children(int index);
  inline ::posixok::Metadata_ReachabilityEntry* add_path_permission_children();
  inline const ::google::protobuf::RepeatedPtrField< ::posixok::Metadata_ReachabilityEntry >&
      path_permission_children() const;
  inline ::google::protobuf::RepeatedPtrField< ::posixok::Metadata_ReachabilityEntry >*
      mutable_path_permission_children();

  // @@protoc_insertion_point(class_scope:posixok.Metadata)
 private:
  inline void set_has_atime();
  inline void clear_has_atime();
  inline void set_has_mtime();
  inline void clear_has_mtime();
  inline void set_has_ctime();
  inline void clear_has_ctime();
  inline void set_has_id_user();
  inline void clear_has_id_user();
  inline void set_has_id_group();
  inline void clear_has_id_group();
  inline void set_has_mode();
  inline void clear_has_mode();
  inline void set_has_link_count();
  inline void clear_has_link_count();
  inline void set_has_size();
  inline void clear_has_size();
  inline void set_has_blocks();
  inline void clear_has_blocks();
  inline void set_has_data_unique_id();
  inline void clear_has_data_unique_id();
  inline void set_has_path_permission_verified();
  inline void clear_has_path_permission_verified();

  ::google::protobuf::UnknownFieldSet _unknown_fields_;

  ::google::protobuf::uint32 atime_;
  ::google::protobuf::uint32 mtime_;
  ::google::protobuf::uint32 ctime_;
  ::google::protobuf::uint32 id_user_;
  ::google::protobuf::uint32 id_group_;
  ::google::protobuf::uint32 mode_;
  ::google::protobuf::uint32 link_count_;
  ::google::protobuf::uint32 size_;
  ::std::string* data_unique_id_;
  ::google::protobuf::int64 path_permission_verified_;
  ::google::protobuf::RepeatedPtrField< ::posixok::Metadata_ReachabilityEntry > path_permission_;
  ::google::protobuf::RepeatedPtrField< ::posixok::Metadata_ReachabilityEntry > path_permission_children_;
  ::google::protobuf::uint32 blocks_;

  mutable int _cached_size_;
  ::google::protobuf::uint32 _has_bits_[(13 + 31) / 32];

  friend void  protobuf_AddDesc_metadata_2eproto();
  friend void protobuf_AssignDesc_metadata_2eproto();
  friend void protobuf_ShutdownFile_metadata_2eproto();

  void InitAsDefaultInstance();
  static Metadata* default_instance_;
};
// ===================================================================


// ===================================================================

// Metadata_ReachabilityEntry

// required .posixok.Metadata.ReachabilityType type = 1;
inline bool Metadata_ReachabilityEntry::has_type() const {
  return (_has_bits_[0] & 0x00000001u) != 0;
}
inline void Metadata_ReachabilityEntry::set_has_type() {
  _has_bits_[0] |= 0x00000001u;
}
inline void Metadata_ReachabilityEntry::clear_has_type() {
  _has_bits_[0] &= ~0x00000001u;
}
inline void Metadata_ReachabilityEntry::clear_type() {
  type_ = 0;
  clear_has_type();
}
inline ::posixok::Metadata_ReachabilityType Metadata_ReachabilityEntry::type() const {
  return static_cast< ::posixok::Metadata_ReachabilityType >(type_);
}
inline void Metadata_ReachabilityEntry::set_type(::posixok::Metadata_ReachabilityType value) {
  assert(::posixok::Metadata_ReachabilityType_IsValid(value));
  set_has_type();
  type_ = value;
}

// optional uint32 gid = 2;
inline bool Metadata_ReachabilityEntry::has_gid() const {
  return (_has_bits_[0] & 0x00000002u) != 0;
}
inline void Metadata_ReachabilityEntry::set_has_gid() {
  _has_bits_[0] |= 0x00000002u;
}
inline void Metadata_ReachabilityEntry::clear_has_gid() {
  _has_bits_[0] &= ~0x00000002u;
}
inline void Metadata_ReachabilityEntry::clear_gid() {
  gid_ = 0u;
  clear_has_gid();
}
inline ::google::protobuf::uint32 Metadata_ReachabilityEntry::gid() const {
  return gid_;
}
inline void Metadata_ReachabilityEntry::set_gid(::google::protobuf::uint32 value) {
  set_has_gid();
  gid_ = value;
}

// optional uint32 uid = 3;
inline bool Metadata_ReachabilityEntry::has_uid() const {
  return (_has_bits_[0] & 0x00000004u) != 0;
}
inline void Metadata_ReachabilityEntry::set_has_uid() {
  _has_bits_[0] |= 0x00000004u;
}
inline void Metadata_ReachabilityEntry::clear_has_uid() {
  _has_bits_[0] &= ~0x00000004u;
}
inline void Metadata_ReachabilityEntry::clear_uid() {
  uid_ = 0u;
  clear_has_uid();
}
inline ::google::protobuf::uint32 Metadata_ReachabilityEntry::uid() const {
  return uid_;
}
inline void Metadata_ReachabilityEntry::set_uid(::google::protobuf::uint32 value) {
  set_has_uid();
  uid_ = value;
}

// -------------------------------------------------------------------

// Metadata

// optional uint32 atime = 1 [default = 0];
inline bool Metadata::has_atime() const {
  return (_has_bits_[0] & 0x00000001u) != 0;
}
inline void Metadata::set_has_atime() {
  _has_bits_[0] |= 0x00000001u;
}
inline void Metadata::clear_has_atime() {
  _has_bits_[0] &= ~0x00000001u;
}
inline void Metadata::clear_atime() {
  atime_ = 0u;
  clear_has_atime();
}
inline ::google::protobuf::uint32 Metadata::atime() const {
  return atime_;
}
inline void Metadata::set_atime(::google::protobuf::uint32 value) {
  set_has_atime();
  atime_ = value;
}

// required uint32 mtime = 2;
inline bool Metadata::has_mtime() const {
  return (_has_bits_[0] & 0x00000002u) != 0;
}
inline void Metadata::set_has_mtime() {
  _has_bits_[0] |= 0x00000002u;
}
inline void Metadata::clear_has_mtime() {
  _has_bits_[0] &= ~0x00000002u;
}
inline void Metadata::clear_mtime() {
  mtime_ = 0u;
  clear_has_mtime();
}
inline ::google::protobuf::uint32 Metadata::mtime() const {
  return mtime_;
}
inline void Metadata::set_mtime(::google::protobuf::uint32 value) {
  set_has_mtime();
  mtime_ = value;
}

// required uint32 ctime = 3;
inline bool Metadata::has_ctime() const {
  return (_has_bits_[0] & 0x00000004u) != 0;
}
inline void Metadata::set_has_ctime() {
  _has_bits_[0] |= 0x00000004u;
}
inline void Metadata::clear_has_ctime() {
  _has_bits_[0] &= ~0x00000004u;
}
inline void Metadata::clear_ctime() {
  ctime_ = 0u;
  clear_has_ctime();
}
inline ::google::protobuf::uint32 Metadata::ctime() const {
  return ctime_;
}
inline void Metadata::set_ctime(::google::protobuf::uint32 value) {
  set_has_ctime();
  ctime_ = value;
}

// required uint32 id_user = 4;
inline bool Metadata::has_id_user() const {
  return (_has_bits_[0] & 0x00000008u) != 0;
}
inline void Metadata::set_has_id_user() {
  _has_bits_[0] |= 0x00000008u;
}
inline void Metadata::clear_has_id_user() {
  _has_bits_[0] &= ~0x00000008u;
}
inline void Metadata::clear_id_user() {
  id_user_ = 0u;
  clear_has_id_user();
}
inline ::google::protobuf::uint32 Metadata::id_user() const {
  return id_user_;
}
inline void Metadata::set_id_user(::google::protobuf::uint32 value) {
  set_has_id_user();
  id_user_ = value;
}

// required uint32 id_group = 5;
inline bool Metadata::has_id_group() const {
  return (_has_bits_[0] & 0x00000010u) != 0;
}
inline void Metadata::set_has_id_group() {
  _has_bits_[0] |= 0x00000010u;
}
inline void Metadata::clear_has_id_group() {
  _has_bits_[0] &= ~0x00000010u;
}
inline void Metadata::clear_id_group() {
  id_group_ = 0u;
  clear_has_id_group();
}
inline ::google::protobuf::uint32 Metadata::id_group() const {
  return id_group_;
}
inline void Metadata::set_id_group(::google::protobuf::uint32 value) {
  set_has_id_group();
  id_group_ = value;
}

// required uint32 mode = 6;
inline bool Metadata::has_mode() const {
  return (_has_bits_[0] & 0x00000020u) != 0;
}
inline void Metadata::set_has_mode() {
  _has_bits_[0] |= 0x00000020u;
}
inline void Metadata::clear_has_mode() {
  _has_bits_[0] &= ~0x00000020u;
}
inline void Metadata::clear_mode() {
  mode_ = 0u;
  clear_has_mode();
}
inline ::google::protobuf::uint32 Metadata::mode() const {
  return mode_;
}
inline void Metadata::set_mode(::google::protobuf::uint32 value) {
  set_has_mode();
  mode_ = value;
}

// optional uint32 link_count = 7 [default = 1];
inline bool Metadata::has_link_count() const {
  return (_has_bits_[0] & 0x00000040u) != 0;
}
inline void Metadata::set_has_link_count() {
  _has_bits_[0] |= 0x00000040u;
}
inline void Metadata::clear_has_link_count() {
  _has_bits_[0] &= ~0x00000040u;
}
inline void Metadata::clear_link_count() {
  link_count_ = 1u;
  clear_has_link_count();
}
inline ::google::protobuf::uint32 Metadata::link_count() const {
  return link_count_;
}
inline void Metadata::set_link_count(::google::protobuf::uint32 value) {
  set_has_link_count();
  link_count_ = value;
}

// optional uint32 size = 8 [default = 0];
inline bool Metadata::has_size() const {
  return (_has_bits_[0] & 0x00000080u) != 0;
}
inline void Metadata::set_has_size() {
  _has_bits_[0] |= 0x00000080u;
}
inline void Metadata::clear_has_size() {
  _has_bits_[0] &= ~0x00000080u;
}
inline void Metadata::clear_size() {
  size_ = 0u;
  clear_has_size();
}
inline ::google::protobuf::uint32 Metadata::size() const {
  return size_;
}
inline void Metadata::set_size(::google::protobuf::uint32 value) {
  set_has_size();
  size_ = value;
}

// optional uint32 blocks = 9 [default = 0];
inline bool Metadata::has_blocks() const {
  return (_has_bits_[0] & 0x00000100u) != 0;
}
inline void Metadata::set_has_blocks() {
  _has_bits_[0] |= 0x00000100u;
}
inline void Metadata::clear_has_blocks() {
  _has_bits_[0] &= ~0x00000100u;
}
inline void Metadata::clear_blocks() {
  blocks_ = 0u;
  clear_has_blocks();
}
inline ::google::protobuf::uint32 Metadata::blocks() const {
  return blocks_;
}
inline void Metadata::set_blocks(::google::protobuf::uint32 value) {
  set_has_blocks();
  blocks_ = value;
}

// optional string data_unique_id = 11;
inline bool Metadata::has_data_unique_id() const {
  return (_has_bits_[0] & 0x00000200u) != 0;
}
inline void Metadata::set_has_data_unique_id() {
  _has_bits_[0] |= 0x00000200u;
}
inline void Metadata::clear_has_data_unique_id() {
  _has_bits_[0] &= ~0x00000200u;
}
inline void Metadata::clear_data_unique_id() {
  if (data_unique_id_ != &::google::protobuf::internal::kEmptyString) {
    data_unique_id_->clear();
  }
  clear_has_data_unique_id();
}
inline const ::std::string& Metadata::data_unique_id() const {
  return *data_unique_id_;
}
inline void Metadata::set_data_unique_id(const ::std::string& value) {
  set_has_data_unique_id();
  if (data_unique_id_ == &::google::protobuf::internal::kEmptyString) {
    data_unique_id_ = new ::std::string;
  }
  data_unique_id_->assign(value);
}
inline void Metadata::set_data_unique_id(const char* value) {
  set_has_data_unique_id();
  if (data_unique_id_ == &::google::protobuf::internal::kEmptyString) {
    data_unique_id_ = new ::std::string;
  }
  data_unique_id_->assign(value);
}
inline void Metadata::set_data_unique_id(const char* value, size_t size) {
  set_has_data_unique_id();
  if (data_unique_id_ == &::google::protobuf::internal::kEmptyString) {
    data_unique_id_ = new ::std::string;
  }
  data_unique_id_->assign(reinterpret_cast<const char*>(value), size);
}
inline ::std::string* Metadata::mutable_data_unique_id() {
  set_has_data_unique_id();
  if (data_unique_id_ == &::google::protobuf::internal::kEmptyString) {
    data_unique_id_ = new ::std::string;
  }
  return data_unique_id_;
}
inline ::std::string* Metadata::release_data_unique_id() {
  clear_has_data_unique_id();
  if (data_unique_id_ == &::google::protobuf::internal::kEmptyString) {
    return NULL;
  } else {
    ::std::string* temp = data_unique_id_;
    data_unique_id_ = const_cast< ::std::string*>(&::google::protobuf::internal::kEmptyString);
    return temp;
  }
}
inline void Metadata::set_allocated_data_unique_id(::std::string* data_unique_id) {
  if (data_unique_id_ != &::google::protobuf::internal::kEmptyString) {
    delete data_unique_id_;
  }
  if (data_unique_id) {
    set_has_data_unique_id();
    data_unique_id_ = data_unique_id;
  } else {
    clear_has_data_unique_id();
    data_unique_id_ = const_cast< ::std::string*>(&::google::protobuf::internal::kEmptyString);
  }
}

// optional int64 path_permission_verified = 13 [default = 0];
inline bool Metadata::has_path_permission_verified() const {
  return (_has_bits_[0] & 0x00000400u) != 0;
}
inline void Metadata::set_has_path_permission_verified() {
  _has_bits_[0] |= 0x00000400u;
}
inline void Metadata::clear_has_path_permission_verified() {
  _has_bits_[0] &= ~0x00000400u;
}
inline void Metadata::clear_path_permission_verified() {
  path_permission_verified_ = GOOGLE_LONGLONG(0);
  clear_has_path_permission_verified();
}
inline ::google::protobuf::int64 Metadata::path_permission_verified() const {
  return path_permission_verified_;
}
inline void Metadata::set_path_permission_verified(::google::protobuf::int64 value) {
  set_has_path_permission_verified();
  path_permission_verified_ = value;
}

// repeated .posixok.Metadata.ReachabilityEntry path_permission = 14;
inline int Metadata::path_permission_size() const {
  return path_permission_.size();
}
inline void Metadata::clear_path_permission() {
  path_permission_.Clear();
}
inline const ::posixok::Metadata_ReachabilityEntry& Metadata::path_permission(int index) const {
  return path_permission_.Get(index);
}
inline ::posixok::Metadata_ReachabilityEntry* Metadata::mutable_path_permission(int index) {
  return path_permission_.Mutable(index);
}
inline ::posixok::Metadata_ReachabilityEntry* Metadata::add_path_permission() {
  return path_permission_.Add();
}
inline const ::google::protobuf::RepeatedPtrField< ::posixok::Metadata_ReachabilityEntry >&
Metadata::path_permission() const {
  return path_permission_;
}
inline ::google::protobuf::RepeatedPtrField< ::posixok::Metadata_ReachabilityEntry >*
Metadata::mutable_path_permission() {
  return &path_permission_;
}

// repeated .posixok.Metadata.ReachabilityEntry path_permission_children = 15;
inline int Metadata::path_permission_children_size() const {
  return path_permission_children_.size();
}
inline void Metadata::clear_path_permission_children() {
  path_permission_children_.Clear();
}
inline const ::posixok::Metadata_ReachabilityEntry& Metadata::path_permission_children(int index) const {
  return path_permission_children_.Get(index);
}
inline ::posixok::Metadata_ReachabilityEntry* Metadata::mutable_path_permission_children(int index) {
  return path_permission_children_.Mutable(index);
}
inline ::posixok::Metadata_ReachabilityEntry* Metadata::add_path_permission_children() {
  return path_permission_children_.Add();
}
inline const ::google::protobuf::RepeatedPtrField< ::posixok::Metadata_ReachabilityEntry >&
Metadata::path_permission_children() const {
  return path_permission_children_;
}
inline ::google::protobuf::RepeatedPtrField< ::posixok::Metadata_ReachabilityEntry >*
Metadata::mutable_path_permission_children() {
  return &path_permission_children_;
}


// @@protoc_insertion_point(namespace_scope)

}  // namespace posixok

#ifndef SWIG
namespace google {
namespace protobuf {

template <>
inline const EnumDescriptor* GetEnumDescriptor< ::posixok::Metadata_ReachabilityType>() {
  return ::posixok::Metadata_ReachabilityType_descriptor();
}

}  // namespace google
}  // namespace protobuf
#endif  // SWIG

// @@protoc_insertion_point(global_scope)

#endif  // PROTOBUF_metadata_2eproto__INCLUDED
