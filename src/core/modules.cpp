#include "core/project.hpp"
#include "util/util.hpp"

#include <algorithm>
#include <functional>
#include <map>
#include <regex>
#include <set>

namespace project {

namespace {

struct ModuleUnit {
  fs::path source;
  std::string name;
  std::string contents;
  std::vector<std::string> imports;
};

using ModuleIndex = std::map<std::string, ModuleUnit>;

std::optional<ModuleUnit> read_module_unit(const fs::path& source) {
  auto contents = util::read_file(source);
  if (!contents) return std::nullopt;

  static const std::regex module_declaration(
      R"((?:^|\n)\s*(?:export\s+)?module\s+([A-Za-z_][A-Za-z0-9_.:]*)\s*;)");
  static const std::regex import_declaration(
      R"((?:^|\n)\s*(?:export\s+)?import\s+(:?[A-Za-z_][A-Za-z0-9_.:]*)\s*;)");

  std::smatch declaration;
  if (!std::regex_search(*contents, declaration, module_declaration)) {
    return std::nullopt;
  }

  ModuleUnit unit;
  unit.source = source;
  unit.name = declaration[1].str();
  unit.contents = std::move(*contents);
  for (std::sregex_iterator it(unit.contents.begin(), unit.contents.end(),
                               import_declaration), end;
       it != end; ++it) {
    unit.imports.push_back((*it)[1].str());
  }
  return unit;
}

bool is_skipped_directory(const fs::path& path) {
  const auto name = path.filename().string();
  return name == ".git" || name == "build" || name == "dist" ||
         name == "node_modules" || name == ".venv";
}

void collect_cppm_files(const fs::path& root, std::vector<fs::path>& files) {
  std::error_code ec;
  if (fs::is_regular_file(root, ec)) {
    if (root.extension() == ".cppm") files.push_back(root);
    return;
  }
  if (!fs::is_directory(root, ec)) return;

  const fs::recursive_directory_iterator end;
  for (fs::recursive_directory_iterator it(
           root, fs::directory_options::skip_permission_denied, ec);
       it != end; it.increment(ec)) {
    if (ec) {
      ec.clear();
      continue;
    }
    if (it->is_directory(ec)) {
      if (is_skipped_directory(it->path())) it.disable_recursion_pending();
      continue;
    }
    if (it->is_regular_file(ec) && it->path().extension() == ".cppm") {
      files.push_back(it->path());
    }
  }
}

void collect_sibling_cppm_files(const fs::path& source,
                                std::vector<fs::path>& files) {
  std::error_code ec;
  for (fs::directory_iterator it(source.parent_path(), ec), end;
       it != end && !ec; it.increment(ec)) {
    if (it->is_regular_file(ec) && it->path().extension() == ".cppm") {
      files.push_back(it->path());
    }
  }
}

bool index_module_sources(const fs::path& root,
                          const std::optional<fs::path>& extra_source,
                          ModuleIndex& index) {
  std::vector<fs::path> candidates;
  collect_cppm_files(root, candidates);
  if (extra_source &&
      extra_source->parent_path().lexically_normal() !=
          (root / "modules").lexically_normal()) {
    collect_sibling_cppm_files(*extra_source, candidates);
  }
  if (extra_source) candidates.push_back(*extra_source);

  std::sort(candidates.begin(), candidates.end());
  candidates.erase(std::unique(candidates.begin(), candidates.end()), candidates.end());

  for (const auto& candidate : candidates) {
    auto unit = read_module_unit(candidate);
    if (!unit) continue;

    auto [existing, inserted] = index.emplace(unit->name, *unit);
    if (!inserted && existing->second.contents != unit->contents) {
      std::error_code ec;
      const auto existing_path = fs::weakly_canonical(existing->second.source, ec);
      const auto candidate_path = fs::weakly_canonical(candidate, ec);
      if (existing_path != candidate_path) {
        util::error("multiple sources declare module '" + unit->name + "': " +
                    existing->second.source.string() + " and " + candidate.string());
        return false;
      }
    }
  }
  return true;
}

std::string resolve_relative_partition(std::string import_name,
                                       const std::string& current_module) {
  if (!import_name.starts_with(':')) return import_name;
  const auto partition = current_module.find(':');
  const std::string primary = current_module.substr(0, partition);
  return primary + import_name;
}

bool collect_dependency_closure(const ModuleUnit& unit,
                                const ModuleIndex& index,
                                std::set<std::string>& visited,
                                std::vector<ModuleUnit>& closure) {
  if (!visited.insert(unit.name).second) return true;

  for (const auto& raw_import : unit.imports) {
    const auto import_name = resolve_relative_partition(raw_import, unit.name);
    if (import_name == "std" || import_name == "std.compat") continue;

    const auto dependency = index.find(import_name);
    if (dependency == index.end()) {
      util::error("module '" + unit.name + "' imports '" + import_name +
                  "', but no matching .cppm source was found. Put it beside the "
                  "source or somewhere in the project.");
      return false;
    }
    if (!collect_dependency_closure(dependency->second, index, visited, closure)) {
      return false;
    }
  }

  closure.push_back(unit);
  return true;
}

bool update_module_manifest(const fs::path& modules_dir) {
  std::vector<fs::path> module_files;
  collect_cppm_files(modules_dir, module_files);
  std::sort(module_files.begin(), module_files.end());
  if (module_files.empty()) {
    util::error("no .cppm module interfaces found in modules/");
    return false;
  }

  std::string manifest = "set(CPPUP_MODULE_SOURCES\n";
  for (const auto& source : module_files) {
    const auto relative = source.lexically_relative(modules_dir).generic_string();
    manifest += "  \"${CMAKE_CURRENT_LIST_DIR}/" + relative + "\"\n";
  }
  manifest += ")\n";

  if (!util::write_file(modules_dir / "modules.cmake", manifest)) {
    util::error("could not write modules/modules.cmake");
    return false;
  }
  return true;
}

bool synchronize_modules(const fs::path& root,
                         const std::vector<fs::path>& roots,
                         const std::optional<fs::path>& extra_source) {
  ModuleIndex index;
  if (!index_module_sources(root, extra_source, index)) return false;

  std::set<std::string> visited;
  std::vector<ModuleUnit> closure;
  for (const auto& root_source : roots) {
    auto unit = read_module_unit(root_source);
    if (!unit) {
      util::error("module source must declare a named module: " + root_source.string());
      return false;
    }
    if (!collect_dependency_closure(*unit, index, visited, closure)) return false;
  }

  const fs::path modules_dir = root / "modules";
  for (const auto& unit : closure) {
    const std::string filename = unit.source.filename().string();
    if (filename.find_first_of("\\\";$") != std::string::npos) {
      util::error("module filename contains unsupported CMake characters: " + filename);
      return false;
    }
    const fs::path destination = modules_dir / filename;
    if (fs::exists(destination)) {
      const auto existing = util::read_file(destination);
      if (!existing || *existing != unit.contents) {
        std::error_code ec;
        if (!fs::equivalent(unit.source, destination, ec) || ec) {
          util::error("module destination already exists with different content: " +
                      destination.string());
          return false;
        }
      }
    }
  }

  for (const auto& unit : closure) {
    const fs::path destination = modules_dir / unit.source.filename();
    std::error_code ec;
    if (fs::exists(destination) && fs::equivalent(unit.source, destination, ec) && !ec) {
      continue;
    }
    const auto existing = util::read_file(destination);
    if (existing && *existing == unit.contents) continue;
    if (!util::write_file(destination, unit.contents)) {
      util::error("could not copy module source to " + destination.string());
      return false;
    }
  }
  return update_module_manifest(modules_dir);
}

std::vector<fs::path> module_roots(const fs::path& modules_dir) {
  std::vector<fs::path> roots;
  collect_cppm_files(modules_dir, roots);
  std::sort(roots.begin(), roots.end());
  return roots;
}

} // namespace

bool add_module(const fs::path& root, const fs::path& source) {
  const fs::path modules_dir = root / "modules";
  if (!fs::is_regular_file(modules_dir / "modules.cmake")) {
    util::error("cppup add currently requires a project created with the modules template");
    return false;
  }

  const fs::path source_path = fs::absolute(source).lexically_normal();
  if (!fs::is_regular_file(source_path) || source_path.extension() != ".cppm") {
    util::error("module source must be an existing .cppm file");
    return false;
  }

  std::vector<fs::path> roots = {source_path};
  if (!synchronize_modules(root, roots, source_path)) return false;
  util::success("Added module dependencies and refreshed modules/modules.cmake");
  util::info("Run 'cppup build' to generate PCM files for this toolchain.");
  return true;
}

bool update_modules(const fs::path& root) {
  const fs::path modules_dir = root / "modules";
  if (!fs::is_regular_file(modules_dir / "modules.cmake")) {
    util::error("cppup update requires a project with modules/modules.cmake");
    return false;
  }

  const auto roots = module_roots(modules_dir);
  if (roots.empty()) {
    util::error("no .cppm module interfaces found in modules/");
    return false;
  }
  if (!synchronize_modules(root, roots, std::nullopt)) return false;

  util::success("Resolved local imports and refreshed modules/modules.cmake");
  util::info("Run 'cppup build' to generate PCM files for this toolchain.");
  return true;
}

} // namespace project
