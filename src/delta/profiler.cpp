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

// File: delta/profiler.cpp - Implementation of the PSet profile controller.

#include "delta/profiler.hpp"

namespace delta {

void Profiler::HandlePreSetup() {
  ExecutionControl::HandlePreSetup();

  knob_->RegisterBool("ignore_lib", "Ignore all the accesses in the common libraries.", "0");
  knob_->RegisterStr("iroot_in", "The input memorization database.", "iroot.db");
  knob_->RegisterStr("iroot_out", "The output memorization database.", "iroot.db");
  knob_->RegisterStr("ilist_out", "The output memorization database.", "ilist.db");  

  observer_ = new Observer(knob_);
  observer_->Register();
}

void Profiler::HandlePostSetup() {
  ExecutionControl::HandlePostSetup();

  iroot_db_ = new idiom::iRootDB(CreateMutex());
  iroot_db_->Load(knob_->ValueStr("iroot_in"), sinfo_);

  ilist_ = new iList(CreateMutex());

  if (observer_->Enabled()){
    observer_->Setup(CreateMutex(), sinfo_, iroot_db_, ilist_);
    AddAnalyzer(observer_);
  }
}

/*bool Profiler::HandleIgnoreMemAccess(IMG img) {
  if (!IMG_Valid(img))
    return true;

  if (IMG_Name(img).find("libpthread") != std::string::npos)
    return true;

  if (knob_->ValueBool("ignore_lib")) {
    if ((IMG_Name(img).find("libc") != std::string::npos) ||
        (IMG_Name(img).find("libpthread") != std::string::npos) ||
        (IMG_Name(img).find("ld-") != std::string::npos) ||
        (IMG_Name(img).find("libstdc++") != std::string::npos) ||
        (IMG_Name(img).find("libgcc_s") != std::string::npos) ||
        (IMG_Name(img).find("libm") != std::string::npos) ||
        (IMG_Name(img).find("libnsl") != std::string::npos) ||
        (IMG_Name(img).find("librt") != std::string::npos) ||
        (IMG_Name(img).find("libdl") != std::string::npos) ||
        (IMG_Name(img).find("libz") != std::string::npos) ||
        (IMG_Name(img).find("libcrypt") != std::string::npos) ||
        (IMG_Name(img).find("libdb") != std::string::npos) ||
        (IMG_Name(img).find("libexpat") != std::string::npos) ||
        (IMG_Name(img).find("libbz2") != std::string::npos))
      return true;
  }

  return false;
}
*/

void Profiler::HandleProgramExit(){
  ExecutionControl::HandleProgramExit();

  // output iroot.db
  iroot_db_->Save(knob_->ValueStr("iroot_out"), sinfo_);
  // output ilist.db
  ilist_->Save(knob_->ValueStr("ilist_out"), sinfo_);
} 

} // namespace delta

