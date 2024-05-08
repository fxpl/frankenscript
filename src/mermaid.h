#pragma once

#include <fstream>
#include <map>
#include <string>
#include <vector>

#include "objects.h"

namespace objects {
// TODO: Make this nicer
  std::string mermaid_path{};
  void set_output(std::string path) { mermaid_path = path; }

  inline void mermaid(std::vector<Edge> &roots) {
    auto out = std::ofstream(mermaid_path);
    std::map<DynObject *, std::size_t> visited;
    std::map<Region *, std::vector<std::size_t>> region_strings;
    std::vector<std::size_t> immutable_objects;
    size_t id = 2;
    visited[DynObject::frame()] = 0;
    visited[nullptr] = 1;
    immutable_objects.push_back(1);
    out << "```mermaid" << std::endl;
    out << "graph TD" << std::endl;
    out << "id0[frame]" << std::endl;
    out << "id1[null]" << std::endl;
    for (auto &root : roots) {
      DynObject::visit({root.src, root.key, root.dst},
            [&](Edge e) {
              DynObject *dst = e.dst;
              std::string key = e.key;
              DynObject *src = e.src;
              out << "  id" << visited[src] << " -->|" << key << "| ";
              size_t curr_id;
              if (visited.find(dst) != visited.end()) {
                out << "id" << visited[dst] << std::endl;
                return false;
              }

              curr_id = id++;
              visited[dst] = curr_id;
              out << "id" << curr_id << "[ " << dst->name << " - " << dst->rc
                  << " ]" << std::endl;

              auto region = DynObject::get_region(dst);
              if (region != nullptr) {
                region_strings[region].push_back(curr_id);
              }

              if (dst->is_immutable()) {
                immutable_objects.push_back(curr_id);
              }
              return true;
            },
            {});
    }

    for (auto [region, objects] : region_strings) {
      if (region->parent == nullptr)
        continue;
      out << "  region" << region << "  -->|parent| region" << region->parent
          << std::endl; 
    }

    for (auto [region, objects] : region_strings) {
      out << "subgraph  " << std::endl;
      out << "  region" << region << "[meta\\nlrc=" << region->local_reference_count << "\\nprc=" << region->parent_reference_count << "]"
          << std::endl;
      for (auto obj : objects) {
        out << "  id" << obj << std::endl;
      }
      out << "end" << std::endl;
    }

    out << "subgraph Immutable" << std::endl;
    for (auto obj : immutable_objects) {
      out << "  id" << obj << std::endl;
    }
    out << "end" << std::endl;
    out << "```" << std::endl;
  }
} // namespace objects