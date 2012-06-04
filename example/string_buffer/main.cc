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
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <limits.h>
#include <pthread.h>

class StringBuffer {
 public:
  StringBuffer() {
    value = new char[16];
    value_length = 16;
    count = 0;
    pthread_mutex_init(&mutex_lock, NULL);
  }

  explicit StringBuffer(int length) {
    value = new char[length];
    value_length = length;
    count = 0;
    pthread_mutex_init(&mutex_lock, NULL);
  }

  explicit StringBuffer(const char *str) {
    int length = strlen(str) + 16;
    value = new char[length];
    value_length = length;
    count = 0;
    pthread_mutex_init(&mutex_lock, NULL);
    append(str);
  }

  ~StringBuffer() {
    delete[] value;
  }

  int length() {
    pthread_mutex_lock(&mutex_lock);
    int ret = count;
    pthread_mutex_unlock(&mutex_lock);
    return ret;
  }

  void getChars(int srcBegin, int srcEnd, char *dst, int dstBegin) {
    pthread_mutex_lock(&mutex_lock);
    if (srcBegin < 0) {
      assert(0);
    }
    if ((srcEnd < 0) || (srcEnd > count)) {
      assert(0);
    }
    if (srcBegin > srcEnd) {
      assert(0);
    }
    memcpy(dst + dstBegin, value + srcBegin, srcEnd - srcBegin);
    pthread_mutex_unlock(&mutex_lock);
  }

  StringBuffer *append(StringBuffer *sb) {
    pthread_mutex_lock(&mutex_lock);
    if (sb == NULL) {
      sb = null_buffer;
    }

    int len = sb->length();
    int newcount = count + len;
    if (newcount > value_length)
      expandCapacity(newcount);
    sb->getChars(0, len, value, count);
    count = newcount;
    pthread_mutex_unlock(&mutex_lock);
    return this;
  }

  StringBuffer *append(const char *str) {
    pthread_mutex_lock(&mutex_lock);
    if (str == NULL) {
      str = "null";
    }

    int len = strlen(str);
    int newcount = count + len;
    if (newcount > value_length)
      expandCapacity(newcount);
    memcpy(value + count, str, len);
    count = newcount;
    pthread_mutex_unlock(&mutex_lock);
    return this;
  }

  StringBuffer *erase(int start, int end) {
    pthread_mutex_lock(&mutex_lock);
    if (start < 0)
      assert(0);
    if (end > count)
      end = count;
    if (start > end)
      assert(0);

    int len = end - start;
    if (len > 0) {
      memcpy(value + start, value + start + len, count - end);
      count -= len;
    }
    pthread_mutex_unlock(&mutex_lock);
    return this;
  }

  void print() {
    for (int i = 0; i < count; i++) {
      printf("%c", *(value + i));
    }
    printf("\n");
  }

 private:
  char *value;
  int value_length;
  int count;
  pthread_mutex_t mutex_lock;

  static StringBuffer *null_buffer;

  void expandCapacity(int minimumCapacity) {
    int newCapacity = (value_length + 1) * 2;
    if (newCapacity < 0) {
      newCapacity = INT_MAX;
    } else if (minimumCapacity > newCapacity) {
      newCapacity = minimumCapacity;
    }

    char *newValue = new char[newCapacity];
    memcpy(newValue, value, count);
    delete[] value;
    value = newValue;
    value_length = newCapacity;
  }
};

// global variables and functions
StringBuffer *StringBuffer::null_buffer = new StringBuffer("null");
StringBuffer *buffer = new StringBuffer("abc");

void *thread_main(void *args) {
  printf("erasing the buffer\n");
  buffer->erase(0, 3);
  //buffer->append("abc");
  printf("erasing done\n");
  return NULL;
}

int main(int argc, char *argv[]) {
  pthread_t tid;
  pthread_create(&tid, NULL, thread_main, NULL);

  StringBuffer *sb = new StringBuffer;
  printf("appending the buffer\n");
  sb->append(buffer);
  printf("appending done\n");

  pthread_join(tid, NULL);

  return 0;
}

