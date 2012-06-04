// Copyright 2011 The University of Michigan
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Authors - Jie Yu (jieyu@umich.edu)

// File: core/static_info.h - Static information for program binary or
// library binaries.

#ifndef CORE_STATIC_INFO_H_
#define CORE_STATIC_INFO_H_

#include <iostream>
#include <map>
#include <set>
#include <tr1/unordered_map>

#include "core/basictypes.h"
#include "core/sync.h"
#include "core/static_info.pb.h" // protobuf head file

class Inst;
class StaticInfo;

typedef uint32 image_id_type;
#define INVALID_IMAGE_ID static_cast<image_id_type>(-1)
#define PSEUDO_IMAGE_NAME  "PSEUDO_IMAGE"

// An image can be a main executable, or a library image.
class Image {
 public:
  Inst *Find(address_t offset);
  bool IsCommonLib();
  bool IsLibc();
  bool IsPthread();
  std::string ShortName();
  std::string ToString() { return ShortName(); }

  image_id_type id() { return proto_->id(); }
  const std::string &name() { return proto_->name(); }

 private:
  typedef std::tr1::unordered_map<address_t, Inst *> InstAddrMap;

  explicit Image(ImageProto *proto) : proto_(proto) {}
  ~Image() {}

  void Register(Inst *inst);

  InstAddrMap inst_offset_map_; // store static instructions for the image
  ImageProto *proto_;

 private:
  friend class StaticInfo;

  DISALLOW_COPY_CONSTRUCTORS(Image);
};

typedef uint32 inst_id_type;
typedef uint32 opcode_type;
#define INVALID_INST_ID static_cast<inst_id_type>(-1)
#define INVALID_OPCODE static_cast<opcode_type>(0)

// A static instruction represented by the image that contains it and
// the offset in the image.
class Inst {
 public:
  bool HasOpcode() { return proto_->has_opcode(); }
  bool HasDebugInfo() { return proto_->has_debug_info(); }
  void SetOpcode(opcode_type c) { proto_->set_opcode(c); }
  void SetDebugInfo(const std::string &file_name, int line, int column);
  std::string DebugInfoStr();
  std::string ToString();

  inst_id_type id() { return proto_->id(); }
  Image *image() { return image_; }
  address_t offset() { return proto_->offset(); }
  opcode_type opcode() { return proto_->opcode(); }

 protected:
  Inst(Image *image, InstProto *proto) : image_(image), proto_(proto) {}
  ~Inst() {}

  Image *image_;
  InstProto *proto_;

 private:
  friend class StaticInfo;

  DISALLOW_COPY_CONSTRUCTORS(Inst);
};

// The static information for all executables and library images.
class StaticInfo {
 public:
  explicit StaticInfo(Mutex *lock);
  ~StaticInfo() {}

  Image *CreateImage(const std::string &name);
  Inst *CreateInst(Image *image, address_t offset);
  Image *FindImage(const std::string &name);
  Image *FindImage(image_id_type id);
  Inst *FindInst(inst_id_type id);
  void Load(const std::string &db_name);
  void Save(const std::string &db_name);

 private:
  typedef std::map<image_id_type, Image *> ImageMap;
  typedef std::tr1::unordered_map<inst_id_type, Inst *> InstMap;

  image_id_type GetNextImageID() { return ++curr_image_id_; }
  inst_id_type GetNextInstID() { return ++curr_inst_id_; }

  Mutex *lock_;
  image_id_type curr_image_id_;
  inst_id_type curr_inst_id_;
  ImageMap image_map_;
  InstMap inst_map_;
  StaticInfoProto proto_;

 private:
  DISALLOW_COPY_CONSTRUCTORS(StaticInfo);
};

#endif

