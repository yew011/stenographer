// Copyright 2014 Google Inc. All rights reserved.
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

#ifndef STENOGRAPHER_SHM_H_
#define STENOGRAPHER_SHM_H_

// Contains types and functions for conduct inter-process memory sharing.
// Only support single thread.

#include "util.h"

#include <string>   // string

namespace st {

// Information on a single packet in an AF_PACKET block.
class Shm {
 public:
  Shm(std::string file, size_t blocks);

  ~Shm();

  // Put the memory block in the shared memory.0
  Error ShareBlock(char* base);

 private:
  Bitmap* map_;          // Bitmap for tracking the shared memory usage.
  size_t blocks_;        // Number of 1MB blocks in the shared memory.
  size_t cur_idx_;       // Current index in bitmap.
  std::string shm_file_; // Name of the shm_open file.
  int shm_fd_;           // fd returned after shm_open().
  char* shm_ptr_;        // ptr returned after mmap().
};

Shm* ShmSetUp(int32_t threads, std::string file, size_t blocks);

}

#endif  // STENOGRAPHER_SHM_H_
