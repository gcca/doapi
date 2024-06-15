#pragma once

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <memory>
#include <mutex>
#include <ostream>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace divi {

class Result {
public:
  explicit Result(const std::string &s) : a_{std::make_unique<_unm_res>(s)} {}
  explicit Result(const char *s) : a_{std::make_unique<_unm_res>(s)} {}
  explicit Result(std::size_t c)
      : a_{std::make_unique<_mat_res>(std::to_string(c))} {}

  [[nodiscard]] const std::string_view &view() const noexcept {
    return a_->v_;
  };

private:
  struct _unm_res {
    explicit _unm_res(const std::string_view &&v)
        : v_{std::forward<const std::string_view>(v)} {}
    std::string_view v_;
  };
  struct _mat_res : _unm_res {
    explicit _mat_res(const std::string &&s)
        : s_{std::forward<const std::string>(s)}, _unm_res{{nullptr, 0}} {
      v_ = s_;
    }
    std::string s_;
  };

  std::unique_ptr<_unm_res> a_;
};

static const char *const NOOP_CSTR = "$noop";
static const char *const OK_CSTR = "$ok";
static const char *const C0_CSTR = "0";

class Visitor {
public:
  virtual ~Visitor() = default;
  Visitor(const Visitor &) = delete;
  Visitor(const Visitor &&) = delete;
  Visitor &operator=(const Visitor &) = delete;
  Visitor &operator=(const Visitor &&) = delete;

  virtual Result On(std::unordered_map<std::string, std::string> & /**/) {
    return Result(NOOP_CSTR);
  }

  virtual Result On(std::unordered_map<std::string, std::size_t> & /**/) {
    return Result(NOOP_CSTR);
  }

protected:
  Visitor() = default;
};

class Bucket {
public:
  Result AcceptS(Visitor &v) { return v.On(s_map_); }

  Result AcceptC(Visitor &v) { return v.On(c_map_); }

  std::mutex &butex() { return butex_; }

  [[nodiscard]] std::string Dump() const {
    std::ostringstream oss;
    if (not s_map_.empty()) {
      oss << "s_map:";
      for (const auto &p : s_map_) {
        oss << '\n' << p.first << '=' << p.second;
      }
    }
    if (not c_map_.empty()) {
      oss << "c_map:";
      for (const auto &p : c_map_) {
        oss << '\n' << p.first << '=' << p.second;
      }
    }
    return oss.str();
  }

private:
  std::mutex butex_;
  std::unordered_map<std::string, std::string> s_map_;
  std::unordered_map<std::string, std::size_t> c_map_;
};

class Op {
  friend std::ostream &operator<<(std::ostream &os, const Op &op) {
    op.Explain(os);
    return os;
  }

public:
  virtual ~Op() = default;
  Op(const Op &) = delete;
  Op(const Op &&) = delete;
  Op &operator=(const Op &) = delete;
  Op &operator=(const Op &&) = delete;

  virtual void Explain(std::ostream &) const noexcept = 0;
  virtual Result ApplyOn(Bucket &bucket) = 0;

protected:
  Op() = default;
};

class SOp : public Op, public Visitor {
public:
  Result ApplyOn(Bucket &bucket) final { return bucket.AcceptS(*this); }
};

class GetOp : public SOp {
public:
  explicit GetOp(const std::string_view &key) : key_{key} {}

  void Explain(std::ostream &os) const noexcept final { os << "GET:" << key_; }

  Result On(std::unordered_map<std::string, std::string> &s_map) final {
    return Result(s_map[std::string(key_)]);
  }

private:
  std::string_view key_;
};

class PutOp : public SOp {
public:
  explicit PutOp(const std::string_view &key, const std::string_view &value)
      : key_{key}, value_{value} {}

  void Explain(std::ostream &os) const noexcept final {
    os << "PUT:" << key_ << ',' << value_;
  }

  Result On(std::unordered_map<std::string, std::string> &s_map) final {
    s_map[std::string{key_}] = value_;
    return Result(OK_CSTR);
  }

private:
  std::string_view key_;
  std::string_view value_;
};

class COp : public Op, public Visitor {
public:
  Result ApplyOn(Bucket &bucket) final { return bucket.AcceptC(*this); }
};

class CountOp : public COp {
public:
  explicit CountOp(const std::string_view &key) : key_{key} {}

  void Explain(std::ostream &os) const noexcept final {
    os << "COUNT:" << key_;
  }

  Result On(std::unordered_map<std::string, std::size_t> &c_map) final {
    return Result(c_map[std::string{key_}]);
  }

private:
  std::string_view key_;
};

class IncOp : public COp {
public:
  explicit IncOp(const std::string_view &key) : key_{key} {}

  void Explain(std::ostream &os) const noexcept final { os << "INC:" << key_; }

  Result On(std::unordered_map<std::string, std::size_t> &c_map) final {
    return Result(++c_map[std::string{key_}]);
  }

private:
  std::string_view key_;
};

class LogicalPlan {
public:
  explicit LogicalPlan(std::vector<std::unique_ptr<Op>> &&ops)
      : ops_{std::move(ops)} {}

  [[nodiscard]] std::string Explain() const {
    std::ostringstream oss;
    auto op_it = ops_.cbegin();
    while (std::next(op_it) != ops_.cend()) { oss << **op_it++ << '\n'; }
    oss << **op_it;
    return oss.str();
  }

  std::vector<Result> Execute(Bucket &bucket) {
    const std::lock_guard<std::mutex> lock{bucket.butex()};

    std::vector<Result> results;
    results.reserve(ops_.size());
    std::transform(ops_.cbegin(), ops_.cend(), std::back_inserter(results),
                   [&](const auto &op) { return op->ApplyOn(bucket); });

    return results;
  }

private:
  std::vector<std::unique_ptr<Op>> ops_;
};

class TextPlan {
  class Action {
    friend std::ostream &operator<<(std::ostream &os, const Action &action) {
      os << "op:" << action.op << " args:[";

      auto arg_it = action.args.cbegin();
      while (std::next(arg_it) != action.args.cend()) {
        os << *arg_it++ << ", ";
      }
      os << *arg_it;
      return os;
    }

  public:
    std::string_view op;
    std::vector<std::string_view> args;
  };

public:
  constexpr static std::size_t MAX_ACTIONS_LENGTH = 50;

  explicit TextPlan(std::string &&text, std::vector<Action> &&actions)
      : text_{std::move(text)}, actions_{std::move(actions)} {}

  explicit TextPlan(std::string &&text) : text_{std::move(text)} {
    std::string_view const vtext{text_};
    actions_.reserve(MAX_ACTIONS_LENGTH);

    std::size_t end = text_.find('\n'), carry = 0;

    while (end != std::string::npos) {
      std::string_view const line = vtext.substr(carry, end - carry);
      actions_.push_back(FromLine(line));

      carry = end + 1;
      end = text_.find('\n', carry);
    }

    std::string_view const line = vtext.substr(carry, vtext.size() - carry);
    actions_.push_back(FromLine(line));
  }

  [[nodiscard]] std::string Explain() const {
    std::ostringstream oss;

    auto action_it = actions_.cbegin();
    while (std::next(action_it) != actions_.cend()) {
      oss << *action_it++ << ']' << std::endl;
    }
    oss << *action_it++ << ']';

    return oss.str();
  }

  [[nodiscard]] LogicalPlan ToLogicalPlan() const {
    std::vector<std::unique_ptr<Op>> ops;
    ops.reserve(actions_.size());

    std::transform(actions_.cbegin(), actions_.cend(), std::back_inserter(ops),
                   [](const Action &action) -> std::unique_ptr<Op> {
                     if (action.op == "GET") {
                       return std::make_unique<GetOp>(action.args[0]);
                     }
                     if (action.op == "PUT") {
                       return std::make_unique<PutOp>(action.args[0],
                                                      action.args[1]);
                     }
                     if (action.op == "COUNT") {
                       return std::make_unique<CountOp>(action.args[0]);
                     }
                     if (action.op == "INC") {
                       return std::make_unique<IncOp>(action.args[0]);
                     }
                     std::ostringstream oss;
                     oss << "Invalid op=" << action.op;
                     throw std::runtime_error{oss.str()};
                   });

    return LogicalPlan(std::move(ops));
  }

  static std::unique_ptr<TextPlan> MakeFromLRaw(std::string &text) {
    return std::make_unique<TextPlan>(std::move(text));
  }

private:
  std::string text_;
  std::vector<Action> actions_;

  static Action FromLine(const std::string_view &line) {
    std::size_t end = line.find(' ');
    const std::string_view op{line.data(), end};

    std::vector<std::string_view> args;
    args.reserve(4);

    std::size_t start = end + 1;
    end = line.find(' ', start);

    while (end != std::string_view::npos) {
      args.emplace_back(line.data() + start, end - start);
      start = end + 1;
      end = line.find(' ', start);
    }

    args.emplace_back(line.data() + start, line.size() - start);

    return Action{std::move(op), std::move(args)};
  }
};

} // namespace divi
