#include "stringutility.h"

#include <algorithm>
#include <locale>

namespace MOBase
{

// this is strongly inspired from boost
class is_iequal
{
  std::locale m_loc;

public:
  is_iequal(const std::locale& loc = std::locale()) : m_loc{loc} {}

  template <typename T1, typename T2>
  bool operator()(const T1& Arg1, const T2& Arg2) const
  {
    return std::toupper<T1>(Arg1, m_loc) == std::toupper<T2>(Arg2, m_loc);
  }
};

bool iequals(std::string_view lhs, std::string_view rhs)
{
  return std::ranges::equal(lhs, rhs, is_iequal());
}

void ireplace_all(std::string& input, std::string_view search,
                  std::string_view replace) noexcept
{
  auto result = std::ranges::search(input, search, is_iequal());
  while (!result.empty()) {
    input.replace(result.begin(), result.end(), replace);
    result = std::ranges::search(input, search, is_iequal());
  }
}

}  // namespace MOBase
