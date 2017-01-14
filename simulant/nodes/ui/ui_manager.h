
#pragma once

#include "../../types.h"
#include "widget.h"

namespace smlt {
namespace ui {


typedef generic::TemplatedManager<Widget, WidgetID> WidgetManager;

class UIManager {
public:
    UIManager(Stage* stage);

    WidgetID new_widget_as_button(const unicode& text, float width=.0f, float height=.0f);

    WidgetID new_widget_as_label(const unicode& text, float width=.0f, float height=.0f);
    WidgetID new_widget_as_progress_bar(float min=.0f, float max=100.0f, float value=.0f);

    void delete_widget(WidgetID widget);

    Stage* stage() const { return stage_; }

private:
    Stage* stage_ = nullptr;
    std::unique_ptr<WidgetManager> manager_;
    UIConfig config_;
};

}
}
