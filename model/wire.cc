/*
  @copyright Steve Keen 2015
  @author Russell Standish
  This file is part of Minsky.

  Minsky is free software: you can redistribute it and/or modify it
  under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Minsky is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with Minsky.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "wire.h"
#include "port.h"
#include "group.h"
#include <ecolab_epilogue.h>

using namespace std;

namespace minsky
{
  vector<float> Wire::coords() const
  {
    vector<float> c;
    assert(from() && to());
    assert(m_coords.size() % 2 == 0);
    if (auto f=from())
      if (auto t=to())
        {
          c.push_back(f->x());
          c.push_back(f->y());
          for (size_t i=0; m_coords.size()>1 && i<m_coords.size()-1; i+=2)
            {
              c.push_back(f->x() + (t->x()-f->x())*m_coords[i]);
              c.push_back(f->y() + (t->y()-f->y())*m_coords[i+1]);
            }
          c.push_back(t->x());
          c.push_back(t->y());
        }
    return c;
  }

  vector<float> Wire::coords(const vector<float>& coords)
  {
    if (coords.size()<6) 
      m_coords.clear();
    else
      {
        assert(coords.size() % 2 == 0);
        
        float dx=coords[coords.size()-2]-coords[0];
        float dy=coords[coords.size()-1]-coords[1];
        m_coords.resize(coords.size()-4);
        for (size_t i=2; i<coords.size()-3; i+=2)
          {
            m_coords[i-2] = (coords[i]-coords[0])/dx;
            m_coords[i-1] = (coords[i+1]-coords[1])/dy;
          }
      }
    return this->coords();
  }


  Wire::Wire(const shared_ptr<Port>& from, const shared_ptr<Port>& to, 
         const vector<float>& a_coords): 
      m_from(from), m_to(to) 
  {
    if (!from || !to) throw error("wiring defunct ports");
    coords(a_coords);
    m_from.lock()->wires.push_back(this);
    m_to.lock()->wires.push_back(this);
  }

  Wire::~Wire()
  {
    if (auto toPort=to())
      toPort->eraseWire(this);
    if (auto fromPort=from())
      fromPort->eraseWire(this);
  }

  bool Wire::visible() const
  {
    auto f=from(), t=to();
    return f && t && (f->item.visible() || (t->item.visible())); 
  }

  void Wire::moveToPorts(const shared_ptr<Port>& from, const shared_ptr<Port>& to)
  {
    if (auto f=this->from())
      f->wires.erase(remove(f->wires.begin(), f->wires.end(), this), f->wires.end());
    if (auto t=this->to())
      t->wires.erase(remove(t->wires.begin(), t->wires.end(), this), t->wires.end());
    m_from=from;
    m_to=to;
    from->wires.push_back(this);
    to->wires.push_back(this);
  }

  
  void Wire::moveIntoGroup(Group& dest)
  {
    WirePtr wp;
    // one hit find and remove wire from its map, saving the wire
    dest.globalGroup().recursiveDo
      (&Group::wires, 
       [&](Wires& wires, Wires::iterator i) {
        if (i->get()==this) 
          {
            wp=*i;
            wires.erase(i);
            return true;
          }
        else
          return false;
      }); 
    if (wp)
      dest.addWire(wp);
  }

  void Wire::split()
  {
    // add I/O variables if this wire crosses a group boundary
    if (auto fg=from()->item.group.lock())
      if (auto tg=to()->item.group.lock())
        if (fg!=tg && !from()->item.ioVar() && !to()->item.ioVar()) // crosses boundary
          {
            // check if this wire is in from group
            auto cmp=[&](WirePtr w) {return w.get()==this;};
            auto i=find_if(fg->wires.begin(), fg->wires.end(), cmp);
            if (i==fg->wires.end())
              {
                fg->addOutputVar();
                assert(fg->outVariables.back()->ports.size()>1);
                fg->addWire(new Wire(from(),fg->outVariables.back()->ports[1]));
                m_from=fg->outVariables.back()->ports[0];
              }
            // check if this wire is in to group
            i=find_if(tg->wires.begin(), tg->wires.end(), cmp);
            if (i==tg->wires.end())
              {
                tg->addInputVar();
                assert(tg->inVariables.back()->ports.size()>1);
                tg->addWire(new Wire(tg->inVariables.back()->ports[0],to()));
                m_to=tg->inVariables.back()->ports[1];
              }
          }
  }
}
