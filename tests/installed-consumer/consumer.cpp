#include <plainroot/validation/validation.hpp>

int main() {
  using namespace plainroot::validation;

  const auto parsed = Validators::Numbers::parseInteger<int>("42");
  if (!parsed)
    return 1;

  const auto result = Validators::Numbers::with(parsed.value)
                          .isInRange(0, 130)
                          .validate("profile.age");
  return result.valid() ? 0 : 2;
}
