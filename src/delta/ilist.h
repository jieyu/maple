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

// File: delta/ilist.h - Define of ilist and ilist database.

#ifndef DELTA_ILIST_H_
#define DELTA_ILIST_H_

#include <tr1/unordered_set>

#include "core/basictypes.h"
#include "core/static_info.h"
#include "core/sync.h"
#include "idiom/iroot.h"
#include "delta/ilist.pb.h"

namespace delta { 
	
class iList {
 public:
  explicit iList(Mutex *lock) : internal_lock_(lock) {}
  ~iList() {}

  void Update(idiom::iRoot *iroot, bool locking);
  void Save(const std::string &db_name, StaticInfo *sinfo);

 protected:
  typedef std::tr1::unordered_set<idiom::iRoot *> iRootSet;

  Mutex *internal_lock_;
  iRootSet iroot_set_;
  iListProto proto_;

 private:
  DISALLOW_COPY_CONSTRUCTORS(iList);
};

}

#endif
