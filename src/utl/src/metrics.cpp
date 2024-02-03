#include "utl/metrics.hpp"
#include "fassert.hpp"
#include <spdlog/spdlog.h>
#include <fstream>
#include <iomanip>

using namespace std;
namespace fs = std::filesystem;

namespace metrics {

namespace {

template<class... Ts>
struct VariantVisitor: Ts... { using Ts::operator()...; };

void make_indent(ostream& out, int64_t spaces) {
    FASSERT(spaces >= 0)
        (spaces);
    for(; spaces > 0; --spaces) {
        out << ' ';
    }
}

void dump_dict(ostream& out, const map<string, impl::Value>& dict, int64_t indent) {
    out << "{";
    auto it = dict.cbegin();
    if(it != dict.cend()) {
        out << endl;
        make_indent(out, indent);
        out << '"' << it->first << "\": ";
        it->second.dump(out, indent);
        ++it;
    }
    for(; it != dict.cend(); ++it) {
        out << ',' << endl;
        make_indent(out, indent);
        out << '"' << it->first << "\": ";
        it->second.dump(out, indent);
    }
    out << endl;
    make_indent(out, indent - 2);
    out << "}";
}

void dump_array(ostream& out, const deque<impl::Value>& arr, int64_t indent) {
    out << "[";
    auto it = arr.cbegin();
    if(it != arr.cend()) {
        out << endl;
        make_indent(out, indent);
        it->dump(out, indent);
        ++it;
    }
    for(; it != arr.cend(); ++it) {
        out << ',' << endl;
        make_indent(out, indent);
        it->dump(out, indent);
    }
    out << endl;
    make_indent(out, indent - 2);
    out << "]";
}

} // namespace


Config::Config()
{}

Config Config::sink_to_file(const fs::path& file_path) noexcept
{
    Config res;
    res.file_path_ = file_path;
    return res;
}

Config Config::sink_to_stderr() noexcept {
    return Config();
}

Reporter::Reporter(const Config& cfg) noexcept
:   config_(cfg)
{}

Reporter::~Reporter()
{
    if (config_.file_path_) {
        string filepath = *config_.file_path_;
        ofstream fout(filepath);
        dump_dict(fout, values_, 2);
        fout.close();
        if (fout.fail()) {
            spdlog::warn("Fail to save out metrics to {}.", filepath);
        }
    } else {
        dump_dict(cerr, values_, 2);
    }
}

impl::Value& Reporter::operator[](const string_view& key) noexcept {
    FASSERT(key.find('.') == key.npos)
        (key)
        .what("Keys are disallowed to contain '.'. Use hierarchical keys instead.");
    return values_[string(key)];
}

namespace impl {

PrimitiveAssign::PrimitiveAssign(Value& v)
:   v_(v)
{}

void PrimitiveAssign::set_int(int64_t v)
{
    v_.value_ = v;
}

void PrimitiveAssign::set_str(std::string_view v)
{
    v_.value_ = string(v);
}

void PrimitiveAssign::set_bool(bool v)
{
    v_.value_ = v;
}

void PrimitiveAssign::set_float(double v)
{
    v_.value_ = v;
}

Value::Value()
:   value_(Unset())
{}

Value::~Value() {}

void Value::dump(ostream& out, int64_t indent) const noexcept {
    std::visit(
        VariantVisitor{
            [&](Unset) {
                FASSERT(false)
                    .what("Value must be set.");
            },
            [&](int64_t v) {
                out << v;
            },
            [&](double v) {
                out << defaultfloat << v;
            },
            [&](bool v) {
                out << boolalpha << v;
            },
            [&](const string& v) {
                out << "\"" << v << "\"";
            },
            [&](const map<string, Value>& vs) {
                dump_dict(out, vs, indent + 2);
            },
            [&](const deque<Value>& vs) {
                dump_array(out, vs, indent + 2);
            }
        },
        value_);
}

Value& Value::operator[](const string_view& key) noexcept {
    FASSERT(key.find('.') == key.npos)
        (key)
        .what("Keys are disallowed to contain '.'. Use hierarchical keys instead.");
    if (!std::holds_alternative<map<string, Value>>(value_)) {
        value_ = map<string, Value>();
    }
    map<string, Value>& dict = std::get<map<string, Value>>(value_);
    return dict[string(key)];
}

Value& Value::append() noexcept {
    if (!std::holds_alternative<deque<Value>>(value_)) {
        value_ = deque<Value>();
    }
    deque<Value>& array = std::get<deque<Value>>(value_);
    array.push_back(Value());
    return array.back();
}

} // namespace impl


} // namespace metrics
