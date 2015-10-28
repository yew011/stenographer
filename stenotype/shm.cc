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
#include <string.h>    // memset() strncpy()
#include <sys/mman.h>  // mmap() munmap() shm_open() shm_unlink()
#include <sys/stat.h>
#include <sys/socket.h>// socket() bind() listen() accept()
#include <sys/types.h>
#include <sys/un.h>    // sockaddr_un
#include <unistd.h>    // close() unlink()

#include <cerrno>
#include <string>

namespace st {

Shm::Shm(std::string shm_file, uint32_t blocks, std::string punix_file) :
  shm_file_(shm_file),
  blocks_(blocks),
  cur_idx_(0),
  punix_file_(punix_file),
  connected_(false) {
  // Sanity check.
  CHECK(!shm_file_.empty() && !punix_file_.empty());

  // Create shared memory.
  map_ = new Bitmap(blocks_);
  shm_fd_ = shm_open(shm_file_.c_str(), O_CREAT | O_RDWR,
                     S_IRUSR | S_IWUSR | S_IRGRP);
  CHECK_SUCCESS(Errno(shm_fd_));
  CHECK_SUCCESS(Errno(ftruncate(shm_fd_, blocks_ << 20)));
  shm_ptr_ = static_cast<char*>(mmap(NULL, blocks_ << 20,
                                     PROT_READ | PROT_WRITE,
                                     MAP_SHARED, shm_fd_, 0));
  CHECK(shm_ptr_ != MAP_FAILED);

  // Create listening (i.e. passive unix domain) socket (non-blocking).
  unlink(punix_file_.c_str());
  struct sockaddr_un addr;
  punix_sock_ = socket(AF_UNIX, SOCK_STREAM, 0);
  CHECK_SUCCESS(Errno(punix_sock_));
  memset(&addr, 0, sizeof addr);
  addr.sun_family = AF_UNIX;
  strncpy(addr.sun_path, punix_file_.c_str(), punix_file_.length());
  CHECK_SUCCESS(Errno(bind(punix_sock_, (struct sockaddr*) &addr,
                           sizeof addr)));
  CHECK_SUCCESS(Errno(listen(punix_sock_, 1)));
  int flags = fcntl(punix_sock_, F_GETFL, 0);
  CHECK_SUCCESS(Errno(fcntl(punix_sock_, F_SETFL, flags | O_NONBLOCK)));
}

Shm::~Shm() {
  delete map_;
  CHECK_SUCCESS(Errno(munmap(shm_ptr_, blocks_ << 20)));
  CHECK_SUCCESS(Errno(close(shm_fd_)));
  CHECK_SUCCESS(Errno(shm_unlink(shm_file_.c_str())));
  CHECK_SUCCESS(Errno(close(punix_sock_)));
  if (connected_) {
    CHECK_SUCCESS(Errno(close(accept_sock_)));
  }
  CHECK_SUCCESS(Errno(unlink(punix_file_.c_str())));
}

void Shm::Run(void) {
  if (connected_) {
    return;
  }

  accept_sock_ = accept(punix_sock_, NULL, NULL);
  if (accept_sock_ == -1) {
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      // Pass.
    } else {
      CHECK_SUCCESS(Errno(accept_sock_));
    }
  } else {
    LOG(INFO) << "Shm: new connection accepted\n";
    connected_ = true;
    // Accepted socket should also be non-blocking.
    CHECK_SUCCESS(Errno(fcntl(accept_sock_, F_SETFL, O_NONBLOCK)));
  }
  LOG(INFO) << "Shm: after accept\n";
}

void Shm::ShareBlock(char* base) {
  if (!connected_) {
    return;
  }
  if (map_->Isset(cur_idx_)) {
    LOG(ERROR) << "Shm: shared memory no more space\n";
    return;
  }

  // Copy block to shared memory.
  map_->Set(cur_idx_);
  memcpy(base, shm_ptr_ + (cur_idx_ << 20), 1 << 20);

  // Notify peer process.
  ssize_t cnt;
  cnt = send(accept_sock_, &cur_idx_, sizeof cur_idx_, 0);
  if (cnt == -1) {
    if (errno != EAGAIN && errno !=EWOULDBLOCK) {
      LOG(FATAL) << "Shm: send blocked\n";
    } else if (errno == ECONNRESET) {
      LOG(ERROR) << "Shm: connection lost\n";
      CHECK_SUCCESS(Errno(close(accept_sock_)));
      connected_ = false;
      map_->ResetAll();
    } else {
      LOG(FATAL) << "Shm: unexpected error\n";
    }
  } else {
    if (cnt != sizeof cur_idx_) {
      LOG(FATAL) << "Shm: partial write to 'accept_sock_'\n";
    }
  }

  // Move to next index.
  cur_idx_ = (cur_idx_ + 1) % blocks_;
}

void Shm::ReclaimBlock(void) {
  if (!connected_) {
    return;
  }

  uint32_t freed_idx;
  ssize_t cnt;
  for (;;) {
    cnt = recv(accept_sock_, &freed_idx, sizeof freed_idx, 0);
    if (cnt == -1) {
      if (errno != EAGAIN && errno !=EWOULDBLOCK) {
        // Pass.
      } else if (errno == ECONNRESET) {
        LOG(ERROR) << "Shm: connection lost\n";
        CHECK_SUCCESS(Errno(close(accept_sock_)));
        connected_ = false;
        map_->ResetAll();
      } else {
        LOG(FATAL) << "Shm: unexpected error\n";
      }
      break;
    } else {
      if (cnt != sizeof cur_idx_) {
        LOG(FATAL) << "Shm: partial read from 'accept_sock_'\n";
      }
    }
    map_->Unset(freed_idx);
  }
}

// Only supports single thread.
Shm* ShmSetUp(int32_t threads, std::string shm_file, uint32_t blocks,
              std::string punix_file) {
  if (threads > 1) {
    return NULL;
  }

  return new Shm(shm_file, blocks, punix_file);
}

}  // namespace st
