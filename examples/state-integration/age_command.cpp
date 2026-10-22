#include <plainroot/state/state.hpp>
#include <plainroot/validation/validation.hpp>

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <optional>
#include <string_view>
#include <variant>

namespace {

using plainroot::state::Key;
using plainroot::state::Snapshot;
using plainroot::state::Store;
using plainroot::state::Transaction;
using plainroot::state::UpdateCallbacks;
using plainroot::validation::Issue;
using plainroot::validation::ValidationResult;
using StateResult = plainroot::state::Result;

constexpr std::string_view kAgeField = "profile.age";
constexpr int64_t kInitialAge = 30;

int state_error(StateResult result) {
  const char* name = plainroot_state_result_name_v1(result);
  std::cerr << "state operation failed: " << (name ? name : "unknown")
            << '\n';
  return 1;
}

void print_issue(const Issue& issue) {
  std::cout << plainroot::validation::issue_code_name(issue.code);
  if (!issue.field.empty())
    std::cout << " field=" << issue.field;
  for (const auto& parameter : issue.parameters) {
    std::cout << ' ' << parameter.name << '=';
    std::visit([](const auto& value) { std::cout << value; }, parameter.value);
  }
}

bool read_age(const Snapshot& snapshot, const Key<int64_t>& age_key,
              int64_t& value, uint64_t& revision) {
  std::optional<int64_t> maybe_age;
  if (snapshot.get(age_key, maybe_age) != PLAINROOT_STATE_OK || !maybe_age ||
      snapshot.revision(revision) != PLAINROOT_STATE_OK)
    return false;
  value = *maybe_age;
  return true;
}

int report_rejected(const Issue& issue, Store& store,
                    const Key<int64_t>& age_key, uint64_t expected_revision) {
  Snapshot after;
  if (store.snapshot(after) != PLAINROOT_STATE_OK)
    return 1;
  int64_t stored_age = 0;
  uint64_t actual_revision = 0;
  if (!read_age(after, age_key, stored_age, actual_revision) ||
      stored_age != kInitialAge || actual_revision != expected_revision) {
    std::cerr << "rejected command changed the stored candidate\n";
    return 1;
  }

  std::cout << "rejected ";
  print_issue(issue);
  std::cout << " stored=" << stored_age << " revision=" << actual_revision
            << '\n';
  return 2;
}

}  // namespace

int main(int argc, char** argv) {
  using namespace plainroot::validation;

  if (argc != 2) {
    std::cerr << "usage: plainroot-validation-state-command AGE\n";
    return 64;
  }

  Store store;
  StateResult state_status =
      Store::create({{"profile.age", PLAINROOT_STATE_VALUE_I64}}, store);
  if (state_status != PLAINROOT_STATE_OK)
    return state_error(state_status);
  Key<int64_t> age_key;
  state_status = store.key(kAgeField, age_key);
  if (state_status != PLAINROOT_STATE_OK)
    return state_error(state_status);

  Snapshot initial;
  state_status = store.snapshot(initial);
  if (state_status != PLAINROOT_STATE_OK)
    return state_error(state_status);
  Transaction seed;
  state_status = initial.begin(seed);
  if (state_status != PLAINROOT_STATE_OK)
    return state_error(state_status);
  state_status = seed.set(age_key, kInitialAge);
  if (state_status != PLAINROOT_STATE_OK)
    return state_error(state_status);
  uint64_t initial_revision = 0;
  const StateResult seed_result = seed.commit(initial_revision);
  if (seed_result != PLAINROOT_STATE_OK)
    return state_error(seed_result);

  ParseOptions parse_options;
  parse_options.trim_ascii_whitespace = true;
  const auto parsed_age = parse_integer<int64_t>(argv[1], parse_options);
  if (parsed_age.result != PLAINROOT_VALIDATION_OK) {
    std::cerr << "validation parse failed: "
              << plainroot::validation::result_name(parsed_age.result) << '\n';
    return 1;
  }
  if (!parsed_age) {
    Issue issue;
    issue.code = parsed_age.failure_code;
    issue.field = kAgeField;
    return report_rejected(issue, store, age_key, initial_revision);
  }

  std::optional<ValidationResult> validation_result;
  UpdateCallbacks command;
  command.transform = [&](const Snapshot&, Transaction& draft) {
    return draft.set(age_key, parsed_age.value);
  };
  command.check = [&](const Transaction& draft) {
    std::optional<int64_t> candidate;
    const StateResult read_result = draft.get(age_key, candidate);
    if (read_result != PLAINROOT_STATE_OK || !candidate)
      return PLAINROOT_STATE_INTERNAL_ERROR;
    validation_result.emplace(Validators::Numbers::with(*candidate)
                                  .isInRange(int64_t{0}, int64_t{130})
                                  .validate(kAgeField));
    if (validation_result->status() != PLAINROOT_VALIDATION_OK)
      return PLAINROOT_STATE_CALLBACK_FAILED;
    return validation_result->valid() ? PLAINROOT_STATE_OK
                                      : PLAINROOT_STATE_REJECTED;
  };

  uint64_t committed_revision = initial_revision;
  const StateResult update_result = store.update(command, committed_revision);
  if (update_result == PLAINROOT_STATE_REJECTED) {
    if (!validation_result)
      return 1;
    Issue issue;
    std::size_t count = 0;
    if (validation_result->issue_count(count) != PLAINROOT_VALIDATION_OK ||
        count == 0 || validation_result->issue(0, issue) !=
                          PLAINROOT_VALIDATION_OK)
      return 1;
    return report_rejected(issue, store, age_key, initial_revision);
  }
  if (update_result != PLAINROOT_STATE_OK)
    return state_error(update_result);

  Snapshot after;
  if (store.snapshot(after) != PLAINROOT_STATE_OK)
    return state_error(PLAINROOT_STATE_INTERNAL_ERROR);
  int64_t stored_age = 0;
  uint64_t actual_revision = 0;
  if (!read_age(after, age_key, stored_age, actual_revision) ||
      stored_age != parsed_age.value || actual_revision != committed_revision) {
    std::cerr << "committed state does not match the validated candidate\n";
    return 1;
  }
  std::cout << "accepted profile.age=" << stored_age
            << " revision=" << actual_revision << '\n';
  return 0;
}
