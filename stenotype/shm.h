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
  Shm(std::string shm_file, uint32_t blocks, std::string punix_file);

  ~Shm();

  // Connection check and restoration.
  void Run(void);

  // Put the memory block in the shared memory.
  void ShareBlock(char* base);

  // Try reclaiming a memory block returned by the peer process.
  void ReclaimBlock(void);

 private:
  Bitmap* map_;           // Bitmap for tracking the shared memory usage.
  uint32_t blocks_;       // Number of 1MB blocks in the shared memory.
  uint32_t cur_idx_;      // Current index in bitmap.
  std::string shm_file_;  // Name of the shm_open file.
  int shm_fd_;            // fd returned after shm_open().
  char* shm_ptr_;         // ptr returned after mmap().
  int punix_sock_;        // listening non-blocking unix domain socket.
  std::string punix_file_;// file path to bind the unix ddomain socket.
  int accept_sock_;       // socket accepted from 'punix_sock_'.
  bool connected_;        // true if the 'accpet_sock_' is still connected.
};

Shm* ShmSetUp(int32_t threads, std::string shm_file, uint32_t blocks,
              std::string punix_fix);
}

#endif  // STENOGRAPHER_SHM_H_
