// Copyright 2019-2023 The Regents of the University of California, Google LLC
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#pragma once

#include <functional>

namespace odb {

class dbBlock;
class dbDatabase;
class dbLib;
class dbTech;

}  // namespace odb

namespace ord {

class OpenRoad;

// Observer interface
class OpenRoadObserver
{
 public:
  virtual ~OpenRoadObserver()
  {
    if (unregister_observer_) {
      unregister_observer_();
    }
  }
  virtual void postReadLef(const char *filename, odb::dbTech* tech, odb::dbLib* library, bool make_tech) {
    postReadLef(tech, library);
  }
  // Either pointer could be null
  virtual void postReadLef(odb::dbTech* tech, odb::dbLib* library) {}
  virtual void postReadDef(odb::dbBlock* block) {}
  virtual void postReadDb(odb::dbDatabase* db) {}

  void set_unregister_observer(std::function<void()> unregister_observer)
  {
    unregister_observer_ = std::move(unregister_observer);
  }

 private:
  std::function<void()> unregister_observer_;
};

}  // namespace ord
