#pragma once

#include <optional>
#include <functional>
#include <deque>

namespace odb {
class dbInst;
class dbITerm;
} // namespace odb

namespace sta {
class LibertyCell;
class LibertyPort;
} // namespace sta

namespace utl {
class Logger;
} // namespace utl

namespace linden {

struct PairedPin {
    /// will not own both parameters.
    PairedPin(odb::dbITerm*, sta::LibertyPort*);

    odb::dbITerm& db() const noexcept {
        return db_pin_.get();
    }

    sta::LibertyPort& sta() const noexcept {
        return sta_pin_.get();
    }

    const std::string& name() const noexcept {
        return name_;
    }

private:
    std::reference_wrapper<odb::dbITerm> db_pin_;
    std::reference_wrapper<sta::LibertyPort> sta_pin_;
    std::string name_;
};

struct PairedCell {
    /// will not own both parameters.
    PairedCell(odb::dbInst*, sta::LibertyCell*);

    odb::dbInst& db() const noexcept {
        return odb_inst_.get();
    }

    sta::LibertyCell& sta() const noexcept {
        return sta_cell_.get();
    }

    const std::string& name() const noexcept {
        return name_;
    }

    const std::optional<PairedPin>& scan_enable() const noexcept {
        return scan_enable_;
    }

private:
    void init_scan_enable();

private:
    std::reference_wrapper<odb::dbInst> odb_inst_;
    std::reference_wrapper<sta::LibertyCell> sta_cell_;
    std::string name_;
    std::optional<PairedPin> scan_enable_;
};

std::deque<PairedCell> cells_on_scanchains();

} // namespace linden
