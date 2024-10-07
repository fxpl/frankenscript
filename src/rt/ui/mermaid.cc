#include <fstream>
#include <map>
#include <string>
#include <vector>

#include "../ui.h"
#include "../objects/dyn_object.h"

namespace rt::ui {

void mermaid(std::vector<objects::Edge> &roots, std::ostream &out) {
  // Give a nice id to each object.
  std::map<objects::DynObject *, std::size_t> visited;
  // Keep track of all the objects in a region.
  std::map<objects::Region *, std::vector<std::size_t>> region_strings;
  // Keep track of the immutable objects.
  std::vector<std::size_t> immutable_objects;
  // // Add frame as local region
  // visited[objects::DynObject::frame()] = 0;
  // region_strings[objects::DynObject::get_local_region()] = {0};
  // Add nullptr as immutable
  visited[nullptr] = 0;
  immutable_objects.push_back(0);
  // Account for frame and nullptr objects.
  size_t id = 1; // was 2
  // Header
  out << "```mermaid" << std::endl;
  out << "graph TD" << std::endl;

  bool unreachable = false;

  auto explore = [&](objects::Edge e) {
    objects::DynObject *dst = e.target;
    std::string key = e.key;
    objects::DynObject *src = e.src;
    if (src != nullptr) {
      out << "  id" << visited[src] << " -->|" << key << "| ";
    }
    if (visited.find(dst) != visited.end()) {
      out << "id" << visited[dst] << std::endl;
      return false;
    }
    auto curr_id = id++;
    visited[dst] = curr_id;
    out << "id" << curr_id << "[ ";
    out << dst->get_name();
    out << "<br/>rc=" << dst->rc;

    out << " ]" << (unreachable ? ":::unreachable" : "") << std::endl;

    auto region = objects::DynObject::get_region(dst);
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
    objects::visit({root.src, root.key, root.target}, explore);
  }

  // Output the unreachable parts of the graph
  unreachable = true;
  for (auto &root : objects::DynObject::all_objects) {
    objects::visit({nullptr, "", root}, explore);
  }

  // Output any region parent edges.
  for (auto [region, objects] : region_strings) {
    if (region->parent == nullptr)
      continue;
    out << "  region" << region->parent << "  <-.-o region" << region
        << std::endl;
  }

  // Output all the region membership information
  for (auto [region, objects] : region_strings) {
    out << "subgraph  ";

    if (region == objects::DynObject::get_local_region()) {
      out << "local region" << std::endl;
    } else {
      out << std::endl;
      out << "  region" << region << "[\\" << region
          << "<br/>lrc=" << region->local_reference_count
          << "<br/>sbrc=" << region->sub_region_reference_count
          << "<br/>prc=" << region->parent_reference_count << "/]" << std::endl;
    }
    for (auto obj : objects) {
      out << "  id" << obj << std::endl;
    }
    out << "end" << std::endl;
  }

  // Output the immutable region.
  out << "subgraph Immutable" << std::endl;
  out << "  id0[nullptr]" << std::endl;
  for (auto obj : immutable_objects) {
    out << "  id" << obj << std::endl;
  }
  out << "end" << std::endl;

  // Output object count as very useful.
  out << "subgraph Count " << objects::DynObject::get_count() << std::endl;
  out << "end" << std::endl;
  out << "classDef unreachable stroke:red,stroke-width:2px" << std::endl;
  // Footer (end of mermaid graph)
  out << "```" << std::endl;
}
} // namespace rt::ui