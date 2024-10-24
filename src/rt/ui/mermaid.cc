#include "../core.h"
#include "../objects/dyn_object.h"
#include "../ui.h"

#include <fstream>
#include <map>
#include <string>
#include <vector>

namespace rt::ui
{

  void replace(std::string& text, std::string from, std::string replace)
  {
    size_t pos = 0;
    while ((pos = text.find(from, pos)) != std::string::npos)
    {
      text.replace(pos, from.length(), replace);
      pos += replace.length(); // Move past the last replaced position
    }
  }

  std::string escape(std::string text)
  {
    replace(text, "[", "#91;");
    replace(text, "]", "#93;");
    replace(text, "_", "#95;");
    replace(text, "<", "#60;");
    replace(text, ">", "#62;");
    replace(text, "\"", "#34;");
    return text;
  }

  class MermaidDiagram
  {
    MermaidUI* info;
    std::ofstream& out;

    // Shared state across draws
    std::map<objects::Region*, std::vector<std::size_t>> region_strings;
    std::vector<std::size_t> immutable_objects;

  public:
    MermaidDiagram(MermaidUI* info_) : info(info_), out(info->out)
    {
      // Add nullptr as immutable
      immutable_objects.push_back(0);
    }

    void draw(std::vector<objects::DynObject*>& roots)
    {
      // Header
      out << "```mermaid" << std::endl;
      out << "graph TD" << std::endl;

      draw_nodes(roots);
      draw_regions();
      draw_immutable_region();

      out << "subgraph Count " << objects::DynObject::get_count() << std::endl;
      out << "end" << std::endl;
      out << "classDef unreachable stroke:red,stroke-width:2px" << std::endl;
      // Footer (end of mermaid graph)
      out << "```" << std::endl;
    }

  private:
    void draw_nodes(std::vector<objects::DynObject*>& roots)
    {
      // Give a nice id to each object.
      std::map<objects::DynObject*, std::size_t> visited;
      visited[nullptr] = 0;
      size_t id = 1;

      bool unreachable = false;

      auto explore = [&](objects::Edge e) {
        objects::DynObject* dst = e.target;
        std::string key = e.key;
        objects::DynObject* src = e.src;
        if (
          info->always_hide.contains(dst) ||
          unreachable && info->unreachable_hide.contains(dst))
        {
          return false;
        }
        if (src != nullptr)
        {
          out << "  id" << visited[src] << " -->|" << escape(key) << "| ";
        }
        if (visited.find(dst) != visited.end())
        {
          out << "id" << visited[dst] << std::endl;
          return false;
        }
        auto curr_id = id++;
        visited[dst] = curr_id;
        out << "id" << curr_id << "[ ";
        out << escape(dst->get_name());
        out << "<br/>rc=" << dst->rc;

        out << " ]" << (unreachable ? ":::unreachable" : "") << std::endl;

        auto region = objects::get_region(dst);
        if (region != nullptr)
        {
          region_strings[region].push_back(curr_id);
        }

        if (dst->is_immutable())
        {
          immutable_objects.push_back(curr_id);
        }
        return true;
      };
      // Output all reachable nodes
      for (auto& root : roots)
      {
        objects::visit(root, explore);
      }

      // Output the unreachable parts of the graph
      unreachable = true;
      for (auto& root : objects::DynObject::all_objects)
      {
        objects::visit({nullptr, "", root}, explore);
      }
    }

    void draw_regions()
    {
      // Output any region parent edges.
      for (auto [region, objects] : region_strings)
      {
        if (region->parent == nullptr)
          continue;
        out << "  region" << region->parent << "  <-.-o region" << region
            << std::endl;
      }

      // Output all the region membership information
      for (auto [region, objects] : region_strings)
      {
        out << "subgraph  ";

        if (region == objects::get_local_region())
        {
          out << "local region" << std::endl;
        }
        else
        {
          out << std::endl;
          out << "  region" << region << "[\\" << region
              << "<br/>lrc=" << region->local_reference_count
              << "<br/>sbrc=" << region->sub_region_reference_count << "/]"
              << std::endl;
        }
        for (auto obj : objects)
        {
          out << "  id" << obj << std::endl;
        }
        out << "end" << std::endl;
      }
    }

    void draw_immutable_region()
    {
      // Output the immutable region.
      out << "subgraph Immutable" << std::endl;
      out << "  id0[nullptr]" << std::endl;
      for (auto obj : immutable_objects)
      {
        out << "  id" << obj << std::endl;
      }
      out << "end" << std::endl;
    }
  };

  MermaidUI::MermaidUI(bool interactive_) : interactive(interactive_)
  {
    path = "mermaid.md";

    if (!interactive)
    {
      // Will be opened by the output function.
      out.open(path);
    }
  }

  void MermaidUI::output(
    std::vector<rt::objects::DynObject*>& roots, std::string message)
  {
    if (interactive)
    {
      out.open(path);
    }

    out << "```" << std::endl;
    out << message << std::endl;
    out << "```" << std::endl;

    MermaidDiagram diag(this);
    diag.draw(roots);

    if (interactive)
    {
      out.close();
      std::cout << "Press a key!" << std::endl;
      getchar();
    }
  }
} // namespace rt::ui
