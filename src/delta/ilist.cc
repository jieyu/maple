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

// File: delta/ilist.h - Implementation of ilist and ilist database.

#include "delta/ilist.h"

#include <fstream>

namespace delta {

void iList::Update(idiom::iRoot *iroot, bool locking) {
  ScopedLock locker(internal_lock_, locking);

  iRootSet::iterator it = iroot_set_.find(iroot);
  if(it == iroot_set_.end()){
    iroot_set_.insert(iroot);
    uint32 iroot_id = iroot->id();
    iListEntryProto *ilist_entry = proto_.add_entry();
    ilist_entry->set_iroot_id(iroot_id);
  }
}

void iList::Save(const std::string & db_name, StaticInfo * sinfo) {
  std::fstream out(db_name.c_str(),
                   std::ios::out | std::ios::trunc | std::ios::binary);
  proto_.SerializeToOstream(&out);
  out.close();
}

}  // namespace delta
