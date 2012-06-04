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

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <assert.h>
 
unsigned NUM_THREADS = 1;
unsigned global_count = 0;
void *thread(void *);
               
int main(int argc, char *argv[]) {
  long i;
  pthread_t pthread_id[200];
  NUM_THREADS = atoi(argv[1]);

  for(i = 0; i < NUM_THREADS; i++)
    pthread_create(&pthread_id[i], NULL, thread, (void *) i);
  for(i = 0; i < NUM_THREADS; i++)
    pthread_join(pthread_id[i], NULL);

  assert(global_count==NUM_THREADS);
  return 0;
}
 
void *thread(void * num) {
  unsigned temp = global_count;
  temp++;
  global_count = temp;
  return NULL;
}

