// Eden - A dynamic program analysis framework.
//
// Copyright (C) 2011 Jie Yu
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
// Authors - Jie Yu (jieyu@umich.edu)

// File: delta/slist.h - Define suspect list database.

#ifndef DELTA_SLIST_H_
#define DELTA_SLIST_H_

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <utility>

#include "core/basictypes.h"
#include "core/static_info.h"
#include "core/sync.h"
#include "idiom/iroot.h"
#include "delta/slist.pb.h"

class Inst;
class Image;
class StaticInfo;

namespace delta {

class SuspectList {
 public:
  SuspectList(Mutex *lock, idiom::iRootDB *iroot_db) 
  	         : internal_lock_(lock),
  	           iroot_db_(iroot_db){}
  ~SuspectList() {delete internal_lock_;}

  bool Empty();
  void Load(const std::string &file_name, StaticInfo *sinfo);
  std::vector<inst_id_type> GetSuspect(address_t suspect_iterator);
  bool IsExistInObjectMap(inst_id_type Inst_id);
  idiom::iRoot *ChooseForTest(idiom::iroot_id_t iroot_id);

 protected:
  Mutex *internal_lock_; 	
  idiom::iRootDB *iroot_db_;

 private:
  typedef std::vector< inst_id_type > PatternVector;
  typedef std::vector<PatternVector> SuspectListVector;
  typedef std::map<inst_id_type, void *> SuspectListObjectMap;

  SuspectListVector _suspect_list_vector;
  SuspectListObjectMap _suspect_list_object_map;

  void AddSuspectListVector(std::istream &in);
  void AddSuspcetListObjectMap(std::istream &in);

  DISALLOW_COPY_CONSTRUCTORS(SuspectList);
};

} // namespace

#endif

