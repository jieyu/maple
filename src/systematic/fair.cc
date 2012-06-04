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

// File: systematic/fair.cc - The implementation of the fair schedule
// control module. Please refer to the paper "Madanlal Musuvathi,
// Shaz Qadeer: Fair stateless model checking. PLDI 2008: 362-371" for
// more info.

#include "systematic/fair.h"

#include "core/logging.h"

namespace systematic {

bool FairControl::Enabled(State *curr_state, Action *next_action) {
  // return whether action is fairly enabled
  Thread *thd = next_action->thd();
  DEBUG_ASSERT(curr_state->IsEnabled(thd));
  // check whether there exists an enabled thread that has a
  // relatively higher priority that thd (i.e. check whether
  // (thd, x) exists in P such that x is enabled). if yes, return
  // false, otherwise return true
  for (ThreadRelation::iterator it = p_.begin(); it != p_.end(); ++it) {
    if (it->first == thd && curr_state->IsEnabled(it->second)) {
      return false;
    }
  }
  return true;
}

void FairControl::Update(State *curr_state) {
  // get the previous state
  State *prev_state = curr_state->Prev();
  if (!prev_state)
    return;

  // get the taken thread t of the state (line 11)
  Action *t_action = prev_state->taken();
  DEBUG_ASSERT(t_action);
  Thread *t = t_action->thd();

  // removes all edges with sink t from P to decrease the
  // relative priority of t (line 13)
  for (ThreadRelation::iterator pit = p_.begin(); pit != p_.end(); ) {
    if (pit->second == t)
      pit = p_.erase(pit);
    else
      ++pit;
  }

  // updates the auxiliary predicates for each thread
  // update E[u] (line 15)
  for (ThreadSetMap::iterator eit = e_.begin(); eit != e_.end(); ++eit) {
    ThreadSet &eu = eit->second;
    ThreadSet eu_td; // to delete
    for (ThreadSet::iterator it = eu.begin(); it != eu.end(); ++it) {
      if (!curr_state->IsEnabled(*it))
        eu_td.insert(*it);
    }
    for (ThreadSet::iterator it = eu_td.begin(); it != eu_td.end(); ++it) {
      eu.erase(*it);
    }
  }
  // update D[u] (line 16 - 20)
  for (ThreadSetMap::iterator dit = d_.begin(); dit != d_.end(); ++dit) {
    if (dit->first == t) {
      ThreadSet &du = dit->second;
      Action::Map *prev_es = prev_state->enabled();
      for (Action::Map::iterator it = prev_es->begin();
           it != prev_es->end(); ++it) {
        Thread *thd = it->first;
        if (!curr_state->IsEnabled(thd))
          du.insert(thd);
      }
    }
  }
  // update S[u] (line 21)
  for (ThreadSetMap::iterator sit = s_.begin(); sit != s_.end(); ++sit) {
    ThreadSet &su = sit->second;
    su.insert(t);
  }

  // update P if t is a yield
  if (t_action->IsYieldOp()) {
    // calculate H (line 24)
    ThreadSet h;
    ThreadSet &et = e_[t];
    ThreadSet &dt = d_[t];
    ThreadSet &st = s_[t];
    for (ThreadSet::iterator it = et.begin(); it != et.end(); ++it) {
      h.insert(*it);
    }
    for (ThreadSet::iterator it = dt.begin(); it != dt.end(); ++it) {
      h.insert(*it);
    }
    for (ThreadSet::iterator it = st.begin(); it != st.end(); ++it) {
      h.erase(*it);
    }
    // update P (line 25)
    for (ThreadSet::iterator hit = h.begin(); hit != h.end(); ++hit) {
      Thread *h = *hit;
      // check whether (t, h) is in P
      bool exist = false;
      for (ThreadRelation::iterator pit = p_.begin(); pit != p_.end(); ++pit) {
        if (pit->first == t && pit->second == h) {
          exist = true;
          break;
        }
      }
      if (!exist) {
        p_.push_back(ThreadPair(t, h));
      }
    }
    // update E[t] (line 26)
    et.clear();
    Action::Map *curr_es = curr_state->enabled();
    for (Action::Map::iterator it = curr_es->begin();
         it != curr_es->end(); ++it) {
      et.insert(it->first);
    }
    // clear D[t] (line 27)
    dt.clear();
    // clear S[t] (line 28)
    st.clear();
  }
}

std::string FairControl::ToString() {
  std::stringstream ss;
  ss << std::dec;
  // display E
  ss << "E:" << std::endl;
  for (ThreadSetMap::iterator eit = e_.begin(); eit != e_.end(); ++eit) {
    ss << "  [" << eit->first->uid() << "] ";
    for (ThreadSet::iterator it = eit->second.begin();
         it != eit->second.end(); ++it) {
      ss << (*it)->uid() << " ";
    }
    ss << std::endl;
  }
  // display D
  ss << "D:" << std::endl;
  for (ThreadSetMap::iterator dit = d_.begin(); dit != d_.end(); ++dit) {
    ss << "  [" << dit->first->uid() << "] ";
    for (ThreadSet::iterator it = dit->second.begin();
         it != dit->second.end(); ++it) {
      ss << (*it)->uid() << " ";
    }
    ss << std::endl;
  }
  // display S
  ss << "S:" << std::endl;
  for (ThreadSetMap::iterator sit = s_.begin(); sit != s_.end(); ++sit) {
    ss << "  [" << sit->first->uid() << "] ";
    for (ThreadSet::iterator it = sit->second.begin();
         it != sit->second.end(); ++it) {
      ss << (*it)->uid() << " ";
    }
    ss << std::endl;
  }
  // display P
  ss << "P:" << std::endl << "  ";
  for (ThreadRelation::iterator pit = p_.begin(); pit != p_.end(); ++pit) {
    ss << "(" << pit->first->uid() << ", " << pit->second->uid() << ") ";
  }
  ss << std::endl;
  return ss.str();
}

} // namespace systematic

