/*
  Copyright (c) 2016, Antonio SJ Musumeci <trapexit@spawn.link>

  Permission to use, copy, modify, and/or distribute this software for any
  purpose with or without fee is hereby granted, provided that the above
  copyright notice and this permission notice appear in all copies.

  THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
  WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
  MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
  ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
  ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
  OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

#include "config.hpp"
#include "errno.hpp"
#include "fs_lremovexattr.hpp"
#include "fs_path.hpp"
#include "policy_rv.hpp"
#include "ugid.hpp"

#include "fuse.h"

#include <string>
#include <vector>

using std::string;
using std::vector;


namespace l
{
  static
  int
  get_error(const PolicyRV &prv_,
            const string   &basepath_)
  {
    for(int i = 0, ei = prv_.success.size(); i < ei; i++)
      {
        if(prv_.success[i].basepath == basepath_)
          return prv_.success[i].rv;
      }

    for(int i = 0, ei = prv_.error.size(); i < ei; i++)
      {
        if(prv_.error[i].basepath == basepath_)
          return prv_.error[i].rv;
      }

    return 0;
  }

  static
  void
  removexattr_loop_core(const string &basepath_,
                        const char   *fusepath_,
                        const char   *attrname_,
                        PolicyRV     *prv_)
  {
    string fullpath;

    fullpath = fs::path::make(basepath_,fusepath_);

    errno = 0;
    fs::lremovexattr(fullpath,attrname_);

    prv_->insert(errno,basepath_);
  }

  static
  void
  removexattr_loop(const vector<string> &basepaths_,
                   const char           *fusepath_,
                   const char           *attrname_,
                   PolicyRV             *prv_)
  {
    for(size_t i = 0, ei = basepaths_.size(); i != ei; i++)
      {
        l::removexattr_loop_core(basepaths_[i],fusepath_,attrname_,prv_);
      }
  }

  static
  int
  removexattr(const Policy::Action &actionFunc_,
              const Policy::Search &searchFunc_,
              const Branches       &branches_,
              const char           *fusepath_,
              const char           *attrname_)
  {
    int rv;
    PolicyRV prv;
    vector<string> basepaths;

    rv = actionFunc_(branches_,fusepath_,&basepaths);
    if(rv == -1)
      return -errno;

    l::removexattr_loop(basepaths,fusepath_,attrname_,&prv);
    if(prv.error.empty())
      return 0;
    if(prv.success.empty())
      return prv.error[0].rv;

    basepaths.clear();
    rv = searchFunc_(branches_,fusepath_,&basepaths);
    if(rv == -1)
      return -errno;

    return l::get_error(prv,basepaths[0]);
  }
}

namespace FUSE
{
  int
  removexattr(const char *fusepath_,
              const char *attrname_)
  {
    Config::Read cfg;

    if(fusepath_ == CONTROLFILE)
      return -ENOATTR;

    if(cfg->xattr.to_int())
      return -cfg->xattr.to_int();

    const fuse_context *fc = fuse_get_context();
    const ugid::Set     ugid(0,0);

    return l::removexattr(cfg->func.removexattr.policy,
                          cfg->func.getxattr.policy,
                          cfg->branches,
                          fusepath_,
                          attrname_);
  }
}
