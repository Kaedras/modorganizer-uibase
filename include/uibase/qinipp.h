#pragma once

#include <QString>
#include <QStringList>
#include <QTextStream>
#include <map>
#include <ranges>
#include <vector>

// modified version of https://github.com/mcmtroffaes/inipp
namespace qinipp
{

class Format
{
public:
  virtual ~Format() = default;

  // used for generating
  const QChar char_section_start;
  const QChar char_section_end;
  const QChar char_assign;
  const QChar char_comment;

  // used for parsing
  virtual bool is_section_start(QChar ch) const { return ch == char_section_start; }
  virtual bool is_section_end(QChar ch) const { return ch == char_section_end; }
  virtual bool is_assign(QChar ch) const { return ch == char_assign; }
  virtual bool is_comment(QChar ch) const { return ch == char_comment; }

  // used for interpolation
  const QChar char_interpol;
  const QChar char_interpol_start;
  const QChar char_interpol_sep;
  const QChar char_interpol_end;

  Format(QChar section_start, QChar section_end, QChar assign, QChar comment,
         QChar interpol, QChar interpol_start, QChar interpol_sep, QChar interpol_end)
      : char_section_start(section_start), char_section_end(section_end),
        char_assign(assign), char_comment(comment), char_interpol(interpol),
        char_interpol_start(interpol_start), char_interpol_sep(interpol_sep),
        char_interpol_end(interpol_end)
  {}

  Format() : Format('[', ']', '=', ';', '$', '{', ':', '}') {}

  QString local_symbol(const QString& name) const
  {
    return char_interpol + (char_interpol_start + name + char_interpol_end);
  }

  QString global_symbol(const QString& sec_name, const QString& name) const
  {
    return local_symbol(sec_name + char_interpol_sep + name);
  }
};

class Ini
{
public:
  using Section  = std::map<QString, QString>;
  using Sections = std::map<QString, Section>;

  Sections sections;
  QStringList errors;
  std::shared_ptr<Format> format;

  static constexpr int max_interpolation_depth = 10;

  Ini() : format(std::make_shared<Format>()) {}
  Ini(const std::shared_ptr<Format>& fmt) : format(fmt) {}

  void generate(QTextStream& os) const
  {
    for (auto const& sec : sections) {
      os << format->char_section_start << sec.first << format->char_section_end << '\n';
      for (auto const& val : sec.second) {
        os << val.first << format->char_assign << val.second << '\n';
      }
      os << '\n';
    }
  }

  void parse(QTextStream& is)
  {
    QString line;
    QString section;
    while (is.readLineInto(&line)) {
      line              = line.trimmed();
      const auto length = line.length();
      if (length > 0) {
        const auto pos    = line.indexOf(format->char_assign);
        const auto& front = line.front();
        if (format->is_comment(front)) {
          continue;
        }

        if (format->is_section_start(front)) {
          if (format->is_section_end(line.back()))
            section = line.sliced(1, length - 2);
          else
            errors.push_back(line);
        } else if (pos != 0 && pos != -1) {
          QString variable = line.first(pos);
          QString value    = line.sliced(pos + 1);
          variable         = variable.trimmed();
          value            = value.trimmed();

          auto& sec = sections[section];
          if (!sec.contains(variable))
            sec.emplace(variable, value);
          else
            errors.push_back(line);
        } else {
          errors.push_back(line);
        }
      }
    }
  }

  void interpolate()
  {
    int global_iteration = 0;
    auto changed         = false;
    // replace each "${variable}" by "${section:variable}"
    for (auto& sec : sections)
      replace_symbols(local_symbols(sec.first, sec.second), sec.second);
    // replace each "${section:variable}" by its value
    do {
      changed         = false;
      const auto syms = global_symbols();
      for (auto& section : sections | std::views::values)
        changed |= replace_symbols(syms, section);
    } while (changed && (max_interpolation_depth > global_iteration++));
  }

  void default_section(const Section& sec)
  {
    for (auto& section : sections | std::views::values)
      for (const auto& val : sec)
        section.insert(val);
  }

  void strip_trailing_comments()
  {
    for (auto& section : sections | std::views::values)
      for (auto& string : section | std::views::values) {
        auto pos = string.indexOf(format->char_comment);
        if (pos != -1) {
          string.truncate(pos);
        }
        string = string.trimmed();
      }
  }

  void clear()
  {
    sections.clear();
    errors.clear();
  }

private:
  using Symbols = std::vector<std::pair<QString, QString>>;

  Symbols local_symbols(const QString& sec_name, const Section& sec) const
  {
    Symbols result;
    for (const auto& symbol : sec | std::views::keys)
      result.emplace_back(format->local_symbol(symbol),
                          format->global_symbol(sec_name, symbol));
    return result;
  }

  Symbols global_symbols() const
  {
    Symbols result;
    for (const auto& sec : sections)
      for (const auto& val : sec.second)
        result.emplace_back(format->global_symbol(sec.first, val.first), val.second);
    return result;
  }

  static bool replace_symbols(const Symbols& syms, Section& sec)
  {
    auto changed = false;
    for (auto& sym : syms)
      for (auto& string : sec | std::views::values) {
        changed |= string.contains(sym.first);
        string.replace(sym.first, sym.second);
      }
    return changed;
  }
};

}  // namespace qinipp
