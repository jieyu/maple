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

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <pthread.h>

int counter = 0;
pthread_barrier_t barrier;

void *thread0(void *arg) {
  counter = 1;
  pthread_barrier_wait(&barrier);
  pthread_barrier_wait(&barrier);
  counter = 3;
  return NULL;
}

void *thread1(void *arg) {
  pthread_barrier_wait(&barrier);
  counter = 10;
  counter = 30;
  pthread_barrier_wait(&barrier);
  return NULL;
}

int main(int argc, char *argv[]) {
  pthread_t tids[2];
  pthread_barrier_init(&barrier, NULL, 2);
  pthread_create(&tids[0], NULL, thread0, NULL);
  pthread_create(&tids[1], NULL, thread1, NULL);
  pthread_join(tids[0], NULL);
  pthread_join(tids[1], NULL);
  return 0;
}

