#include "settings_activity.hpp"

#include "frame_poller.hpp"
#include "gba_buttons.hpp"

namespace {

const char *controllerButtonName(int button) {
    switch (static_cast<brls::ControllerButton>(button)) {
        case brls::BUTTON_LT: return "ZL";
        case brls::BUTTON_LB: return "L";
        case brls::BUTTON_LSB: return "Linker Stick";
        case brls::BUTTON_UP: return "Dpad Oben";
        case brls::BUTTON_RIGHT: return "Dpad Rechts";
        case brls::BUTTON_DOWN: return "Dpad Unten";
        case brls::BUTTON_LEFT: return "Dpad Links";
        case brls::BUTTON_BACK: return "Minus";
        case brls::BUTTON_GUIDE: return "Home";
        case brls::BUTTON_START: return "Plus";
        case brls::BUTTON_RSB: return "Rechter Stick";
        case brls::BUTTON_Y: return "Y";
        case brls::BUTTON_B: return "B";
        case brls::BUTTON_A: return "A";
        case brls::BUTTON_X: return "X";
        case brls::BUTTON_RB: return "R";
        case brls::BUTTON_RT: return "ZR";
        default: return "?";
    }
}

// Only the buttons a real controller exposes as discrete digital presses
// (not sticks/nav aliases) are offered as bindable targets.
bool isBindable(int button) {
    switch (static_cast<brls::ControllerButton>(button)) {
        case brls::BUTTON_LT:
        case brls::BUTTON_LB:
        case brls::BUTTON_UP:
        case brls::BUTTON_RIGHT:
        case brls::BUTTON_DOWN:
        case brls::BUTTON_LEFT:
        case brls::BUTTON_BACK:
        case brls::BUTTON_START:
        case brls::BUTTON_Y:
        case brls::BUTTON_B:
        case brls::BUTTON_A:
        case brls::BUTTON_X:
        case brls::BUTTON_RB:
        case brls::BUTTON_RT:
            return true;
        default:
            return false;
    }
}

} // namespace

std::string SettingsActivity::describeBinding(const std::string &prefKey, int defaultController) const {
    int bound = prefs.keyBinding(prefKey);
    if (bound == Prefs::kExplicitlyUnbound) {
        return "Nicht belegt";
    }
    if (bound == Prefs::kNoOverride) {
        return std::string(controllerButtonName(defaultController)) + " (Standard)";
    }
    return controllerButtonName(bound);
}

void SettingsActivity::refreshBindingRow(const std::string &prefKey) {
    auto it = bindingLabels.find(prefKey);
    if (it == bindingLabels.end()) {
        return;
    }
    for (const auto &button : GBA_BUTTONS) {
        if (button.prefKey == prefKey) {
            it->second->setText(describeBinding(prefKey, button.defaultController));
            break;
        }
    }
}

void SettingsActivity::onControllerButtonPressed(brls::ControllerButton button) {
    if (pendingBindPrefKey.empty() || !isBindable(button)) {
        return;
    }
    std::string prefKey = pendingBindPrefKey;
    pendingBindPrefKey.clear();
    prefs.setKeyBinding(prefKey, static_cast<int>(button));
    refreshBindingRow(prefKey);
}

brls::View *SettingsActivity::createContentView() {
    auto *column = new brls::Box();
    column->setAxis(brls::Axis::COLUMN);
    column->setAlignItems(brls::AlignItems::STRETCH);
    column->setPadding(24, 32, 24, 32);

    auto *onScreenControls = new brls::BooleanCell();
    onScreenControls->init("Bildschirmsteuerung", prefs.onScreenControlsEnabled, [this](bool value) {
        prefs.onScreenControlsEnabled = value;
        prefs.save();
    });
    column->addView(onScreenControls);

    auto *divider1 = new brls::Rectangle(nvgRGBA(255, 255, 255, 40));
    divider1->setWidthPercentage(100);
    divider1->setHeight(1);
    divider1->setMarginTop(12);
    divider1->setMarginBottom(12);
    column->addView(divider1);

    auto *anzeigeLabel = new brls::Label();
    anzeigeLabel->setText("Anzeige");
    anzeigeLabel->setFontSize(20);
    anzeigeLabel->setMarginBottom(8);
    column->addView(anzeigeLabel);

    auto *bilinearFilter = new brls::BooleanCell();
    bilinearFilter->init("Bilineare Filterung", prefs.bilinearVideoFilter, [this](bool value) {
        prefs.bilinearVideoFilter = value;
        prefs.save();
    });
    column->addView(bilinearFilter);

    auto *divider2 = new brls::Rectangle(nvgRGBA(255, 255, 255, 40));
    divider2->setWidthPercentage(100);
    divider2->setHeight(1);
    divider2->setMarginTop(12);
    divider2->setMarginBottom(12);
    column->addView(divider2);

    auto *bindingsLabel = new brls::Label();
    bindingsLabel->setText("Tastenbelegung");
    bindingsLabel->setFontSize(20);
    bindingsLabel->setMarginBottom(8);
    column->addView(bindingsLabel);

    bindingsList = new brls::Box();
    bindingsList->setAxis(brls::Axis::COLUMN);
    column->addView(bindingsList);

    for (const auto &button : GBA_BUTTONS) {
        std::string prefKey = button.prefKey;

        auto *row = new brls::Box();
        row->setAxis(brls::Axis::ROW);
        row->setAlignItems(brls::AlignItems::CENTER);
        row->setMarginBottom(8);

        auto *nameLabel = new brls::Label();
        nameLabel->setText(button.label);
        nameLabel->setWidth(80);
        row->addView(nameLabel);

        auto *bindingLabel = new brls::Label();
        bindingLabel->setText(describeBinding(prefKey, button.defaultController));
        bindingLabel->setGrow(1.0f);
        row->addView(bindingLabel);
        bindingLabels[prefKey] = bindingLabel;

        auto *bindButton = new brls::Button();
        bindButton->setText("Zuweisen");
        bindButton->setMarginRight(8);
        bindButton->registerClickAction([this, prefKey, bindingLabel](brls::View *) {
            pendingBindPrefKey = prefKey;
            captureWaitingForRelease = true;
            bindingLabel->setText("Taste drücken...");
            return true;
        });
        row->addView(bindButton);

        auto *clearButton = new brls::Button();
        clearButton->setText("Löschen");
        clearButton->registerClickAction([this, prefKey](brls::View *) {
            prefs.clearKeyBinding(prefKey);
            refreshBindingRow(prefKey);
            return true;
        });
        row->addView(clearButton);

        bindingsList->addView(row);
    }

    // Polls the unified controller state every frame while a binding
    // capture is pending. Requires all buttons to be released first (see
    // header comment) so the same physical press that opened the capture
    // (e.g. A, to click "Zuweisen") can't also be read as the new binding.
    auto *poller = new FramePoller([this]() {
        if (pendingBindPrefKey.empty()) {
            return;
        }
        brls::ControllerState state {};
        brls::Application::getPlatform()->getInputManager()->updateUnifiedControllerState(&state);

        int pressedButton = -1;
        for (int i = 0; i < brls::_BUTTON_MAX; i++) {
            if (state.buttons[i]) {
                pressedButton = i;
                break;
            }
        }

        if (captureWaitingForRelease) {
            if (pressedButton < 0) {
                captureWaitingForRelease = false;
            }
            return;
        }
        if (pressedButton >= 0) {
            onControllerButtonPressed(static_cast<brls::ControllerButton>(pressedButton));
        }
    });
    column->addView(poller);

    auto *scroll = new brls::ScrollingFrame();
    scroll->setGrow(1.0f);
    scroll->setContentView(column);

    auto *frame = new brls::AppletFrame(scroll);
    frame->setTitle("Einstellungen");
    return frame;
}
