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
  // Give a nice id to each object. 
  std::map<DynObject *, std::size_t> visited;
  // Keep track of all the objects in a region.
  std::map<Region *, std::vector<std::size_t>> region_strings;
  // Keep track of the immutable objects.
  std::vector<std::size_t> immutable_objects;
  // Add frame as local region
  visited[DynObject::frame()] = 0;
  region_strings[DynObject::get_local_region()] = {0};
  // Add nullptr as immutable
  visited[nullptr] = 1;
  immutable_objects.push_back(1);
  // Account for frame and nullptr objects.
  size_t id = 2;
  // Header
  out << "```mermaid" << std::endl;
  out << "graph TD" << std::endl;
  out << "id0[frame]" << std::endl;
  out << "id1[null]" << std::endl;

  bool unreachable = false;

  auto explore = [&](Edge e) {
                       DynObject *dst = e.dst;
                       std::string key = e.key;
                       DynObject *src = e.src;
                       if (src != nullptr) {
                         out << "  id" << visited[src] << " -->|" << key
                             << "| ";
                       }
                       if (visited.find(dst) != visited.end()) {
                         out << "id" << visited[dst] << std::endl;
                         return false;
                       }
                       auto curr_id = id++;
                       visited[dst] = curr_id;
                       out << "id" << curr_id << "[ " << dst << "\\n"
                           << dst->name << "\\nrc=" << dst->rc << " ]"
                           << (unreachable ? ":::unreachable" : "")
                           << std::endl;

                       auto region = DynObject::get_region(dst);
                       if (region != nullptr) {
                         region_strings[region].push_back(curr_id);
                       }

                       if (dst->is_immutable()) {
                         immutable_objects.push_back(curr_id);
                       }
                       return true;
                     };
  // Output all reachable edges
  for (auto &root : roots) {
    DynObject::visit({root.src, root.key, root.dst},
                     explore,
                     {});
  }

  // Output the unreachable parts of the graph
  unreachable = true;
  for (auto &root : DynObject::all_objects) {
    DynObject::visit({nullptr, "", root},
                     explore,
                     {});
  }

  // Output any region parent edges.
  for (auto [region, objects] : region_strings) {
    if (region->parent == nullptr)
      continue;
    out << "  region" << region << "  -.->|parent| region" << region->parent
        << std::endl;
  }

  // Output all the region membership information
  for (auto [region, objects] : region_strings) {
    out << "subgraph  ";

    if (region == DynObject::get_local_region()) {
      out << "local region" << std::endl;
      out << "  id0" << std::endl;
    }
    else 
    {
        out << std::endl;
        out << "  region" << region << "{" << region
            << "\\nlrc=" << region->local_reference_count
            << "\\nprc=" << region->parent_reference_count << "}" << std::endl;
    }
    for (auto obj : objects) {
      out << "  id" << obj << std::endl;
    }
    out << "end" << std::endl;
  }

  // Output the immutable region.
  out << "subgraph Immutable" << std::endl;
  for (auto obj : immutable_objects) {
    out << "  id" << obj << std::endl;
  }
  out << "end" << std::endl;

  // Output object count as very useful.
  out << "subgraph Count " << DynObject::get_count() << std::endl;
  out << "end" << std::endl;
  out << "classDef unreachable stroke:red,stroke-width:2px" << std::endl;
  // Footer (end of mermaid graph)
  out << "```" << std::endl;
}
} // namespace objects