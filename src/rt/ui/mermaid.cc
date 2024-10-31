#include "../core.h"
#include "../objects/dyn_object.h"
#include "../ui.h"

#include <fstream>
#include <limits>
#include <map>
#include <string>
#include <vector>

namespace rt::ui
{
  const char* TAINT_NODE_COLOR = "#43a";
  const char* TAINT_EDGE_COLOR = "#9589dc";

  const char* CROSS_REGION_EDGE_COLOR = "orange";
  const char* UNREACHABLE_NODE_COLOR = "red";

  const char* IMMUTABLE_REGION_COLOR = "#32445d";
  const char* IMMUTABLE_EDGE_COLOR = "#94f7ff";

  const char* LOCAL_REGION_ID = "LocalReg";
  const char* IMM_REGION_ID = "ImmReg";
  const char* COWN_REGION_ID = "CownReg";

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
    replace(text, "(", "#40;");
    replace(text, ")", "#41;");
    replace(text, "_", "#95;");
    replace(text, "<", "#60;");
    replace(text, ">", "#62;");
    replace(text, "\"", "#34;");
    return text;
  }

  struct NodeInfo
  {
    size_t id;
    bool is_opaque;
    std::vector<size_t> edges;
  };

  std::ostream& operator<<(std::ostream& os, const NodeInfo& node)
  {
    os << "id" << node.id;
    return os;
  }

  class MermaidDiagram
  {
    MermaidUI* info;
    std::ofstream& out;

    size_t id_counter = 1;
    size_t edge_counter = 0;

    // Give a nice id to each object.
    std::map<objects::DynObject*, NodeInfo> nodes;
    std::map<objects::Region*, std::vector<std::size_t>> region_strings;

  public:
    MermaidDiagram(MermaidUI* info_) : info(info_), out(info->out)
    {
      // Add nullptr
      nodes[nullptr] = {0};
      region_strings[rt::objects::immutable_region].push_back(0);
    }

    void color_edge(size_t edge_id, const char* color)
    {
      out << "linkStyle " << edge_id << " stroke:" << color
          << ",stroke-width:1px" << std::endl;
    }

    void draw(std::vector<objects::DynObject*>& roots)
    {
      // Header
      out << "```mermaid" << std::endl;
      out << "graph TD" << std::endl;

      draw_nodes(roots);
      draw_regions();
      draw_taint();
      draw_info();

      out << "style " << IMM_REGION_ID << " fill:" << IMMUTABLE_REGION_COLOR
          << std::endl;
      out << "classDef unreachable stroke-width:2px,stroke:"
          << UNREACHABLE_NODE_COLOR << std::endl;
      out << "classDef tainted fill:" << TAINT_NODE_COLOR << std::endl;
      // Footer (end of mermaid graph)
      out << "```" << std::endl;
    }

  private:
    /// @brief Draws the target node and the edge from the source to the target.
    NodeInfo* draw_edge(objects::Edge e, bool reachable)
    {
      NodeInfo* result = nullptr;
      size_t edge_id = -1;
      objects::DynObject* dst = e.target;
      objects::DynObject* src = e.src;

      // Draw edge
      if (src != nullptr)
      {
        auto src_node = &nodes[src];
        out << "  " << *src_node;
        out << ((src_node->is_opaque) ? "-.->" : "-->");
        out << " |" << escape(e.key) << "| ";
        edge_id = edge_counter;
        edge_counter += 1;
        src_node->edges.push_back(edge_id);
      }

      // Draw target
      if (nodes.find(dst) != nodes.end())
      {
        out << nodes[dst] << std::endl;
      }
      else
      {
        // Draw a new node
        nodes[dst] = {id_counter++, dst->is_opaque()};
        auto node = &nodes[dst];

        // Header
        out << *node;
        out << (dst->is_cown() ? "[[" : "[");

        // Content
        out << escape(dst->get_name());
        out << "<br/>rc=" << dst->rc;
        out << (rt::core::globals()->contains(dst) ? " #40;global#41;" : "");

        // Footer
        out << (dst->is_cown() ? "]]" : "]");
        out << (reachable ? "" : ":::unreachable");
        out << std::endl;

        result = node;
      }

      // Color edge
      if (edge_id != -1)
      {
        if (!dst || dst->is_immutable())
        {
          color_edge(edge_id, IMMUTABLE_EDGE_COLOR);
        }
        else if (rt::objects::get_region(src) != rt::objects::get_region(dst))
        {
          color_edge(edge_id, CROSS_REGION_EDGE_COLOR);
        }
      }

      return result;
    }

    void draw_nodes(std::vector<objects::DynObject*>& roots)
    {
      bool reachable = true;

      auto explore = [&](objects::Edge e) {
        objects::DynObject* dst = e.target;
        if (
          info->always_hide.contains(dst) ||
          (!reachable && info->unreachable_hide.contains(dst)) ||
          (!info->draw_funcs && dst &&
           dst->get_prototype() == core::bytecodeFuncPrototypeObject()))
        {
          return false;
        }
        auto node = draw_edge(e, reachable);
        if (!node)
        {
          return false;
        }

        auto region = objects::get_region(dst);
        if (region != nullptr)
        {
          region_strings[region].push_back(node->id);
        }

        return true;
      };

      // Output all reachable nodes
      for (auto& root : roots)
      {
        objects::visit(root, explore);
      }
      // Output the unreachable parts of the graph
      reachable = false;
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
        {
          continue;
        }
        if ((!info->draw_cown_region) && region->parent == objects::cown_region)
        {
          continue;
        }

        out << "  region" << region->parent << "  <-.-o region" << region
            << std::endl;
        edge_counter += 1;
      }

      // Output all the region membership information
      for (auto [region, objects] : region_strings)
      {
        if ((!info->draw_cown_region) && region == objects::cown_region)
        {
          continue;
        }

        out << "subgraph ";

        if (region == objects::get_local_region())
        {
          out << LOCAL_REGION_ID << "[\"Local region\"]" << std::endl;
        }
        else if (region == objects::immutable_region)
        {
          out << IMM_REGION_ID << "[\"Immutable region\"]" << std::endl;
          out << "  id0[nullptr]" << std::endl;
        }
        else if (region == objects::cown_region)
        {
          out << COWN_REGION_ID << "[\"Cown region\"]" << std::endl;
          out << "  region" << region << "[\\" << region
              << "<br/>sbrc=" << region->sub_region_reference_count << "/]"
              << std::endl;
        }
        else
        {
          out << " " << std::endl;
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

    void draw_taint()
    {
      std::set<objects::DynObject*> tainted;
      auto mark_tained = [&](objects::Edge e) {
        objects::DynObject* dst = e.target;

        if (tainted.contains(dst))
        {
          return false;
        }
        auto node = &this->nodes[dst];
        out << "class " << *node << " tainted;" << std::endl;
        tainted.insert(dst);

        if (dst->is_opaque())
        {
          return false;
        }

        for (auto edge_id : node->edges)
        {
          color_edge(edge_id, TAINT_EDGE_COLOR);
        }

        return true;
      };

      for (auto root : info->taint)
      {
        objects::visit(root, mark_tained);
      }
    }

    void draw_info()
    {
      auto globals = rt::core::globals();
      auto local_ctn = objects::DynObject::get_count() - globals->size();

      // Header
      out << "subgraph info" << std::endl;
      out << "  i01[";

      // Info
      out << "Locals: " << local_ctn << "<br/>";
      out << "Globals: " << globals->size() << "<br/>";

      // Footer
      out << "]" << std::endl;
      out << "end" << std::endl;
    }
  };

  MermaidUI::MermaidUI(int step_counter) : steps(step_counter)
  {
    path = "mermaid.md";

    hide_cown_region();
  }

  void MermaidUI::output(
    std::vector<rt::objects::DynObject*>& roots, std::string message)
  {
    // Reset the file if this is a breakpoint
    if (should_break() && out.is_open())
    {
      out.close();
    }

    // Open the file if it's not open
    if (!out.is_open())
    {
      out.open(path);
    }

    out << "```" << std::endl;
    out << message << std::endl;
    out << "```" << std::endl;

    MermaidDiagram diag(this);
    diag.draw(roots);

    if (should_break())
    {
      out.flush();
      next_action();
    }
    else
    {
      steps -= 1;
    }
  }

  void print_help()
  {
    std::cout << "Commands:" << std::endl;
    std::cout << "- s <n>: Run n step (default n = 0) [Default]" << std::endl;
    std::cout << "- r    : Runs until the next break point" << std::endl;
    std::cout << "- h    : Prints this message " << std::endl;
  }

  void MermaidUI::next_action()
  {
    if (first_break)
    {
      print_help();
      first_break = false;
    }

    while (true)
    {
      std::cout << "> ";
      std::string line;
      std::getline(std::cin, line);
      std::istringstream iss(line);
      std::string command;
      iss >> command;

      if (command == "s" || line.empty())
      {
        int n = 0;
        steps = (iss >> n) ? n : 0;

        return;
      }
      else if (command == "r")
      {
        steps = std::numeric_limits<int>::max();
        return;
      }
      else if (command == "h")
      {
        print_help();
      }
      else
      {
        std::cerr << "Unknown command. Type 'h' for help." << std::endl;
      }
    }
  }

  void MermaidUI::hide_cown_region()
  {
    draw_cown_region = false;
    add_always_hide(core::cownPrototypeObject());
  }

  void MermaidUI::show_cown_region()
  {
    draw_cown_region = true;
    remove_always_hide(core::cownPrototypeObject());
  }
} // namespace rt::ui
