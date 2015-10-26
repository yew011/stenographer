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

#include "shm.h"
#include "util.h"

#include <fcntl.h>
#include <sys/mman.h>  // mmap() munmap() shm_open() shm_unlink()
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>    // close()

#include <cerrno>
#include <string>

namespace st {

Shm::Shm(std::string file, size_t blocks) {
  CHECK(!file.empty());

  cur_idx_ = 0;
  shm_file_ = file;
  blocks_ = blocks;
  map_ = new Bitmap(blocks_);
  shm_fd_ = shm_open(shm_file_.c_str(), O_CREAT | O_RDWR,
                     S_IRUSR | S_IWUSR | S_IRGRP);
  CHECK_SUCCESS(Errno(shm_fd_));
  CHECK_SUCCESS(Errno(ftruncate(shm_fd_, blocks_ << 20)));
  shm_ptr_ = static_cast<char*>(mmap(NULL, blocks_ << 20,
                                     PROT_READ | PROT_WRITE,
                                     MAP_SHARED, shm_fd_, 0));
  CHECK(shm_ptr_ != MAP_FAILED);
}

Shm::~Shm() {
  delete map_;
  CHECK_SUCCESS(Errno(munmap(shm_ptr_, blocks_ << 20)));
  CHECK_SUCCESS(Errno(close(shm_fd_)));
  CHECK_SUCCESS(Errno(shm_unlink(shm_file_.c_str())));
}

Error Shm::ShareBlock(char* base) {
  RETURN_IF_ERROR(map_->Isset(cur_idx_) ? Errno(ENOBUFS) : SUCCESS,
                  "Shared memory filled up.\n");

  map_->Set(cur_idx_);
  memcpy(base, shm_ptr_ + (cur_idx_ << 20), 1 << 20);
  cur_idx_ = (cur_idx_ + 1) % blocks_;

  return SUCCESS;
}

// Only supports single thread.
Shm* ShmSetUp(int32_t threads, std::string file, size_t blocks) {
  if (threads > 1) {
    return NULL;
  }

  return new Shm(file, blocks);
}

}  // namespace st
