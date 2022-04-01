///////////////////////////////////////////////////////////////////////////////
// BSD 3-Clause License
//
// Copyright (c) 2018-2020, The Regents of the University of California
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// * Redistributions of source code must retain the above copyright notice, this
//   list of conditions and the following disclaimer.
//
// * Redistributions in binary form must reproduce the above copyright notice,
//   this list of conditions and the following disclaimer in the documentation
//   and/or other materials provided with the distribution.
//
// * Neither the name of the copyright holder nor the names of its
//   contributors may be used to endorse or promote products derived from
//   this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
///////////////////////////////////////////////////////////////////////////////

#include "initialPlaceGuide.h"

#include <iostream>
#include <fstream>
#include <map>

#include "placerBase.h"

namespace gpl {
using namespace std;

void InitialPlaceGuide::run() {
  assert(guide_file_ != "");
#ifndef NDEBUG // FIXME @bangxiang: to cleanup
  {
    const int centerX = pb_->die().coreCx();
    const int centerY = pb_->die().coreCy();

    for(auto& inst: pb_->placeInsts()) {
      if (!inst->isLocked()) {
        if (inst->dbInst()->isPlaced()) {
          auto p = inst->dbInst();
          std::cout << p->getName() << ":(" << inst->lx() << "," << inst->ly() << ")" << std::endl;
        } else {
          std::cout << "inst->setCenterLocation(): (" << centerX << "," << centerY << ")" << std::endl;
          inst->setCenterLocation(centerX, centerY);
        }
      }
    }
    return;
  }
#endif
  std::ifstream fin(guide_file_);
  string line;
  std::map<std::string, std::pair<int, int> > m;
  while (getline(fin, line)) {
      stringstream ss(line);
      string name = "";
      int x = 0;
      int y = 0;
      ss >> name >> x >> y;
      m[name] = std::make_pair(x, y);
  }
  fin.close();

  assert(pb_);
  const int centerX = pb_->die().coreCx();
  const int centerY = pb_->die().coreCy();
  for(auto& inst : pb_->placeInsts()) {
    if (!inst->isLocked()) {
      auto k = inst->dbInst()->getName();
      if (m.count(k) != 0) {
        auto v = m[k];
        inst->dbSetCenterLocation(v.first, v.second);
      } else {
        inst->dbSetCenterLocation(centerX, centerY);
      }
    }
  }
}

}
