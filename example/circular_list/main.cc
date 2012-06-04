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

// define the circular list data structure
typedef struct list_node_st list_node;
struct list_node_st {
  void *data;
  list_node *next;
};

typedef struct {
  list_node *head;
  list_node *tail;
  int size;
  pthread_mutex_t lock;
} circular_list;

typedef struct {
  int num;
} data_type;

pthread_mutex_t mem_lock;

list_node *new_list_node() {
  pthread_mutex_lock(&mem_lock);
  list_node *node = new list_node;
  pthread_mutex_unlock(&mem_lock);
  return node;
}

void free_list_node(list_node *node) {
  pthread_mutex_lock(&mem_lock);
  delete node;
  pthread_mutex_unlock(&mem_lock);
}

void list_push_back(circular_list *list, void *data) {
  list_node *node = new_list_node();
  node->data = data;
  node->next = NULL;
  pthread_mutex_lock(&list->lock);
  if (list->tail) {
    list->tail->next = node;
    list->tail = node;
  } else {
    list->head = node;
    list->tail = node;
  }
  list->size++;
  pthread_mutex_unlock(&list->lock);
}

void *list_pop_front(circular_list *list) {
  void *ret_val;
  list_node *node = NULL;
  pthread_mutex_lock(&list->lock);
  if (list->head) {
    ret_val = list->head->data;
    node = list->head;
    if (list->head == list->tail) {
      list->head = list->head->next;
      list->tail = list->head;
    } else {
      list->head = list->head->next;
    }
    list->size--;
  } else {
    ret_val = NULL;
  }
  pthread_mutex_unlock(&list->lock);
  if (node) {
    free_list_node(node);
  }
  return ret_val;
}

void list_init(circular_list *list) {
  list->head = NULL;
  list->tail = NULL;
  list->size = 0;
  pthread_mutex_init(&list->lock, NULL);
}

void process(circular_list * list) {
  data_type *data = (data_type *)list_pop_front(list);
  data->num += 10;
  list_push_back(list, data);;
}

void *t1_main(void *args) {
  circular_list *list = (circular_list *)args;
  printf("t1 is rotating the list\n");
  process(list);
  printf("t1 done\n");
  return NULL;
}

void *t2_main(void *args) {
  circular_list *list = (circular_list *)args;
  printf("t2 is rotating the list\n");
  process(list);
  printf("t2 done\n");
  return NULL;
}

int main(int argc, char *argv[]) {
  pthread_mutex_init(&mem_lock, NULL);
  circular_list *work_list = new circular_list;
  list_init(work_list);
  for (int i = 0; i < 10; i++) {
    data_type *data = new data_type;
    data->num = i;
    list_push_back(work_list, data);
  }

  pthread_t *tids = new pthread_t[2];
  pthread_create(&tids[0], NULL, t1_main, work_list);
  pthread_create(&tids[1], NULL, t2_main, work_list);
  pthread_join(tids[0], NULL);
  pthread_join(tids[1], NULL);

  // print and verify results
  list_node *iterator = work_list->head;
  int prev_num = -1;
  while (iterator != NULL) {
    data_type *data = (data_type *)iterator->data;
    printf("%d ", data->num);
    if (prev_num != -1) {
      assert(data->num > prev_num);
    }
    prev_num = data->num;
    iterator = iterator->next;
  }
  printf("\n");

  return 0;
}

