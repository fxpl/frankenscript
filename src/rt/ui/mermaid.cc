#include "../core.h"
#include "../objects/dyn_object.h"
#include "../ui.h"

#include <filesystem>
#include <fstream>
#include <limits>
#include <map>
#include <string>
#include <vector>

typedef std::shared_ptr<verona::interpreter::Behavior> behavior_ptr;

namespace rt::ui
{
  namespace fs = std::filesystem;

  const char* TAINT_NODE_COLOR = "#afa8db";
  const char* TAINT_EDGE_COLOR = "#9589dc";

  const char* CROSS_REGION_EDGE_COLOR = "orange";
  const char* UNREACHABLE_NODE_COLOR = "red";
  const char* HIGHLIGHT_NODE_COLOR = "yellow";
  const char* ERROR_NODE_COLOR = "red";

  const char* IMMUTABLE_NODE_COLOR = "#94f7ff";
  const char* IMMUTABLE_REGION_COLOR = "#e9fdff";
  const char* IMMUTABLE_EDGE_COLOR = "#76c5cc";

  const char* LOCAL_REGION_COLOR = "#eefcdd";
  const char* DEFAULT_EDGE_COLOR = "#777777";

  const char* REGION_COLORS[] = {
    "#fcfbdd",
    "#f9f7bc",
    "#f7f39b",
    "#f4ef7a",
    "#f2ec59",
    "#d9d450",
  };

  const char* LOCAL_REGION_ID = "LocalReg";
  const char* IMM_REGION_ID = "ImmReg";
  const char* COWN_REGION_ID = "CownReg";

  const char* FONT_SIZE = "16px";
  const int EDGE_WIDTH = 2;
  const int ERROR_EDGE_WIDTH = 4;

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
    replace(text, "\n", "<br/>");
    return text;
  }

  struct NodeInfo
  {
    size_t id;
    bool is_opaque;
    std::map<size_t, objects::DynObject*> edges;
  };

  std::ostream& operator<<(std::ostream& os, const NodeInfo& node)
  {
    os << "id" << node.id;
    return os;
  }

  struct RegionInfo
  {
    std::vector<size_t> nodes;
    std::vector<objects::Region*> regions;
    bool drawn;
  };

  class MermaidDiagram
  {
  protected:
    MermaidUI* info;
    std::ofstream& out;
    size_t edge_counter = 0;

    MermaidDiagram(MermaidUI* info_) : info(info_), out(info->out) {}

    void draw_header()
    {
      // Header
      out << "<div style='background: #fff'>" << std::endl;
      out << std::endl;
      out << "```mermaid" << std::endl;
      out << "%%{init: {'theme': 'neutral', 'themeVariables': { 'fontSize': '"
          << FONT_SIZE << "' }}}%%" << std::endl;
      out << "graph TD" << std::endl;
    }

    void draw_footer()
    {
      // Footer (end of mermaid graph)
      out << "```" << std::endl;
      out << "</div>" << std::endl;
      out << "<div style='break-after:page'></div>" << std::endl;
      out << std::endl;
    }

    void color_edge(size_t edge_id, const char* color, int width = EDGE_WIDTH)
    {
      out << "  linkStyle " << edge_id << " stroke:" << color
          << ",stroke-width:" << width << "px" << std::endl;
    }
  };

  class ScheduleDiagram : protected MermaidDiagram
  {
  public:
    ScheduleDiagram(MermaidUI* info_) : MermaidDiagram(info_) {}

    void draw(std::vector<behavior_ptr>& roots)
    {
      // Header
      this->draw_header();
      out << "  id0(Scheduler):::immutable" << std::endl;
      // Footer
      this->draw_footer();
    }
  };

  class ObjectGraphDiagram : protected MermaidDiagram
  {
    size_t id_counter = 1;

    // Give a nice id to each object.
    std::map<objects::DynObject*, NodeInfo> nodes;
    std::map<objects::Region*, RegionInfo> regions;

  public:
    ObjectGraphDiagram(MermaidUI* info_) : MermaidDiagram(info_)
    {
      // Add nullptr
      nodes[nullptr] = {0};
      regions[objects::immutable_region].nodes.push_back(0);
    }

    void draw(std::vector<objects::DynObject*>& roots)
    {
      // header
      this->draw_header();
      out << "  id0(None):::immutable" << std::endl;

      draw_nodes(roots);
      draw_regions();
      draw_taint();
      draw_highlight();
      draw_error();

      // Classes
      out << "classDef unreachable stroke-width:2px,stroke:"
          << UNREACHABLE_NODE_COLOR << std::endl;
      out << "classDef highlight stroke-width:4px,stroke:"
          << HIGHLIGHT_NODE_COLOR << std::endl;
      out << "classDef error stroke-width:4px,stroke:" << ERROR_NODE_COLOR
          << std::endl;
      out << "classDef tainted fill:" << TAINT_NODE_COLOR << std::endl;
      out << "classDef tainted_immutable stroke-width:4px,stroke:"
          << TAINT_NODE_COLOR << std::endl;
      out << "classDef immutable fill:" << IMMUTABLE_NODE_COLOR << std::endl;

      // Footer
      this->draw_footer();
    }

  private:
    std::pair<const char*, const char*> get_node_style(objects::DynObject* obj)
    {
      if (obj->get_prototype() == core::cownPrototypeObject())
      {
        return {"[[", "]]"};
      }

      if (obj->get_prototype() == objects::regionPrototypeObject())
      {
        return {"[\\", "/]"};
      }

      if (obj->is_immutable())
      {
        // Make sure to also update the None node, when editing these
        return {"(", ")"};
      }

      return {"[", "]"};
    }

    bool is_borrow_edge(objects::Edge e)
    {
      return e.src != nullptr && e.target != nullptr &&
        objects::get_region(e.src) != objects::get_region(e.target) &&
        objects::get_region(e.src) == objects::get_local_region();
    }

    std::string node_decoration(objects::DynObject* dst, bool reachable)
    {
      if (dst->is_immutable())
      {
        return ":::immutable";
      }
      if (!reachable && info->highlight_unreachable)
      {
        return ":::unreachable";
      }
      return "";
    }

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
        out << *src_node;
        out << (is_borrow_edge(e) ? "-.->" : "-->");
        out << " |" << escape(e.key) << "| ";
        edge_id = edge_counter;
        edge_counter += 1;
        src_node->edges[edge_id] = dst;
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
        auto markers = get_node_style(dst);

        // Header
        out << *node;
        out << markers.first;

        // Content
        out << escape(dst->get_name());
        out << "<br/>rc=" << dst->rc;
        out << (rt::core::globals()->contains(dst) ? " #40;global#41;" : "");

        // Footer
        out << markers.second;
        out << node_decoration(dst, reachable);
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
        else
        {
          color_edge(edge_id, DEFAULT_EDGE_COLOR);
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
          regions[region].nodes.push_back(node->id);
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

    void
    draw_region_body(objects::Region* r, RegionInfo* info, std::string& indent)
    {
      indent += "  ";
      for (auto obj : info->nodes)
      {
        out << indent << "id" << obj << std::endl;
      }
      if (MermaidUI::pragma_draw_regions_nested)
      {
        for (auto reg : info->regions)
        {
          draw_region(reg, indent);
        }
      }
      indent.erase(indent.size() - 2);
    }

    void draw_region(objects::Region* r, std::string& indent)
    {
      auto info = &regions[r];
      if (info->drawn)
      {
        return;
      }
      info->drawn = true;
      auto depth = indent.size() / 2;

      // Header
      out << indent << "subgraph ";
      out << "reg" << r << "[\" \"]" << std::endl;

      // Content
      draw_region_body(r, info, indent);

      // Footer
      out << indent << "end" << std::endl;
      out << indent << "style reg" << r
          << " fill:" << REGION_COLORS[depth % std::size(REGION_COLORS)]
          << std::endl;
    }

    void draw_regions()
    {
      // Build parent relations
      for (auto [reg, _] : regions)
      {
        regions[reg->parent].regions.push_back(reg);
      }

      std::string indent;
      if (info->draw_cown_region)
      {
        auto region = objects::cown_region;
        out << "subgraph " << COWN_REGION_ID << "[\"Cown region\"]"
            << std::endl;
        draw_region_body(region, &regions[region], indent);
        out << "end" << std::endl;
        out << "style " << COWN_REGION_ID << " fill:" << REGION_COLORS[0]
            << std::endl;
      }
      regions[objects::cown_region].drawn = true;

      if (info->draw_immutable_region)
      {
        auto region = objects::immutable_region;
        out << "subgraph " << IMM_REGION_ID << "[\"Immutable region\"]"
            << std::endl;
        draw_region_body(region, &regions[region], indent);
        out << "end" << std::endl;
        out << "style " << IMM_REGION_ID << " fill:" << IMMUTABLE_REGION_COLOR
            << std::endl;
      }
      regions[objects::immutable_region].drawn = true;

      // Local region
      {
        auto region = objects::get_local_region();
        out << "subgraph " << LOCAL_REGION_ID << "[\"Local region\"]"
            << std::endl;
        draw_region_body(objects::cown_region, &regions[region], indent);
        out << "end" << std::endl;
        out << "style " << LOCAL_REGION_ID << " fill:" << LOCAL_REGION_COLOR
            << std::endl;
      }
      regions[objects::get_local_region()].drawn = true;

      // Draw all other regions
      if (MermaidUI::pragma_draw_regions_nested)
      {
        for (auto reg : regions[nullptr].regions)
        {
          draw_region(reg, indent);
        }
        for (auto reg : regions[objects::cown_region].regions)
        {
          draw_region(reg, indent);
        }
      }
      else
      {
        for (const auto& [reg, _] : regions)
        {
          if (reg != nullptr)
          {
            draw_region(reg, indent);
          }
        }
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
        auto css_class = "tainted";
        if (dst->is_immutable() || dst->is_opaque())
        {
          css_class = "tainted_immutable";
        }
        out << "class " << *node << " " << css_class << ";" << std::endl;
        tainted.insert(dst);

        if (dst->is_opaque())
        {
          return false;
        }

        for (auto [edge_id, _] : node->edges)
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

    void draw_highlight()
    {
      for (auto node : info->highlight_objects)
      {
        auto node_info = &this->nodes[node];
        out << "class " << *node_info << " highlight;" << std::endl;
      }
    }

    void draw_error()
    {
      for (auto node : info->error_objects)
      {
        auto node_info = &this->nodes[node];
        out << "class " << *node_info << " error;" << std::endl;
      }

      for (auto e : info->error_edges)
      {
        auto dst = e.target;
        auto src_info = &this->nodes[e.src];
        auto edge = std::find_if(
          src_info->edges.begin(),
          src_info->edges.end(),
          [dst](const auto& pair) { return pair.second == dst; });
        size_t edge_id;
        if (edge != src_info->edges.end())
        {
          edge_id = edge->first;
        }
        else
        {
          out << *src_info << "--> | ERROR | " << this->nodes[dst] << std::endl;
          edge_id = edge_counter;
          edge_counter += 1;
        }
        color_edge(edge_id, ERROR_NODE_COLOR, ERROR_EDGE_WIDTH);
      }
    }
  };

  MermaidUI::MermaidUI()
  {
    hide_cown_region();
  }

  void MermaidUI::prep_output()
  {
    // Reset the file if this is a breakpoint
    if (should_break() && out.is_open())
    {
      out.close();
    }

    // Open the file if it's not open
    if (!out.is_open())
    {
      // Check if the directory exists, and create it if not
      fs::path dir = fs::path(path).parent_path();
      if (!dir.empty() && !fs::exists(dir))
      {
        fs::create_directories(dir);
      }

      // Open the actual file
      out.open(path);

      if (!out.is_open())
      {
        std::cout << "Unable to open output file: " << path << std::endl;
        std::abort();
      }
    }
  }

  void MermaidUI::output(
    std::vector<rt::objects::DynObject*>& roots, std::string message)
  {
    this->prep_output();

    out << "<pre><code>" << message << "</code></pre>" << std::endl;

    ObjectGraphDiagram diag(this);
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

  void MermaidUI::draw_schedule(
    std::vector<behavior_ptr> behaviors, std::string message)
  {
    this->prep_output();

    out << "### " << message << std::endl;

    ScheduleDiagram diag(this);
    diag.draw(behaviors);

    // Make sure the output is available.
    out.flush();
  }

  void MermaidUI::highlight(
    std::string message, std::vector<objects::DynObject*>& highlight)
  {
    std::swap(this->highlight_objects, highlight);
    auto objs = local_root_objects();
    output(objs, message);
    std::swap(highlight, this->highlight_objects);
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

  void MermaidUI::error(std::string info)
  {
    // Make sure ui doesn't pause
    steps += 10;
    highlight_unreachable = false;

    // Construct message
    std::stringstream ss;
    ss << "Error: " << info;
    auto msg = ss.str();

    // Get roots
    std::vector<objects::DynObject*> nodes_vec = local_root_objects();
    for (auto item : error_objects)
    {
      always_hide.erase(item);
      nodes_vec.push_back(item);
    }
    for (auto edge : error_edges)
    {
      always_hide.erase(edge.src);
      always_hide.erase(edge.target);
      nodes_vec.push_back(edge.src);
      nodes_vec.push_back(edge.target);
    }

    // Output
    std::cerr << msg << std::endl;
    output(nodes_vec, msg);
  }

  std::vector<objects::DynObject*> MermaidUI::local_root_objects()
  {
    auto local_set = &objects::get_local_region()->objects;
    std::vector<objects::DynObject*> nodes_vec;
    for (auto item : *local_set)
    {
      if (always_hide.contains(item) || unreachable_hide.contains(item))
      {
        continue;
      }
      nodes_vec.push_back(item);
    }

    return nodes_vec;
  }
} // namespace rt::ui
