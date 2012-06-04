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

#include <vector>
#include <map>
#include <cassert>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <pthread.h>

class MYSQL_LOG {
 public:
  MYSQL_LOG() {
    pthread_mutex_init(&mutex_, NULL);
  }

  ~MYSQL_LOG() {}

  void write(const char *content) {
    Lock();
    contents_.push_back(content);
    Unlock();
  }

  const char *get_content(int i) {
    return contents_[i];
  }

 private:
  void Lock() { pthread_mutex_lock(&mutex_); }
  void Unlock() { pthread_mutex_unlock(&mutex_); }

  pthread_mutex_t mutex_;
  std::vector<const char *> contents_;
};

class MYSQL_TABLE {
 public:
  MYSQL_TABLE() {
    pthread_mutex_init(&mutex_, NULL);
  }

  ~MYSQL_TABLE() {}

  void insert_entry(int key, int val) {
    Lock();
    contents_[key] = val;
    Unlock();
  }

  void remove_entries() {
    Lock();
    contents_.clear();
    Unlock();
  }

  bool empty() {
    return contents_.empty();
  }

 private:
  void Lock() { pthread_mutex_lock(&mutex_); }
  void Unlock() { pthread_mutex_unlock(&mutex_); }

  pthread_mutex_t mutex_;
  std::map<int, int> contents_;
};

MYSQL_LOG mysql_log;
MYSQL_TABLE table;

void *delete_thread_main(void *args) {
  printf("removing\n");
  table.remove_entries();
  mysql_log.write("remove");
  printf("removing done\n");
  return NULL;
}

void *insert_thread_main(void *args) {
  printf("inserting\n");
  table.insert_entry(1, 2);
  mysql_log.write("insert");
  printf("inserting done\n");
  return NULL;
}

int main(int argc, char *argv[]) {
  pthread_t delete_tid;
  pthread_t insert_tid;
  pthread_create(&delete_tid, NULL, delete_thread_main, NULL);
  pthread_create(&insert_tid, NULL, insert_thread_main, NULL);
  pthread_join(delete_tid, NULL);
  pthread_join(insert_tid, NULL);

  // validate results
  if (table.empty()) {
    assert(!strcmp(mysql_log.get_content(0), "insert"));
  } else {
    assert(!strcmp(mysql_log.get_content(0), "remove"));
  }

  printf("Program exit normally\n");

  return 0;
}

