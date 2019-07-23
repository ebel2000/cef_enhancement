// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/ui/views/desktop_capture/desktop_media_picker_views.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/task/post_task.h"
#include "build/build_config.h"
#include "libcef/browser/media/webrtc/desktop_media_list.h"
#include "libcef/browser/media/webrtc/desktop_media_picker_manager.h"

#include "libcef/browser/ui/views/desktop_capture/desktop_media_source_view.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/strings/grit/components_strings.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents_delegate.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/tabbed_pane/tabbed_pane.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_client_view.h"
#include "ui/wm/core/shadow_types.h"

using content::DesktopMediaID;

namespace {

#if !defined(OS_CHROMEOS) && defined(USE_AURA)
DesktopMediaID::Id AcceleratedWidgetToDesktopMediaId(
    gfx::AcceleratedWidget accelerated_widget) {
#if defined(OS_WIN)
  return reinterpret_cast<DesktopMediaID::Id>(accelerated_widget);
#else
  return static_cast<DesktopMediaID::Id>(accelerated_widget);
#endif
}
#endif

}  // namespace

namespace Cef {
DesktopMediaPickerDialogView::DesktopMediaPickerDialogView(
    const DesktopMediaPicker::Params& params,
    DesktopMediaPickerViews* parent,
    std::vector<std::unique_ptr<DesktopMediaList>> source_lists)
    : parent_(parent),
      modality_(params.modality),
      description_label_(new views::Label()),
      audio_share_checkbox_(nullptr),
      pane_(new views::TabbedPane()) {

		//@zsf-b
		SetLayoutManager(std::make_unique<views::BoxLayout>(
			views::BoxLayout::kVertical,
			gfx::Insets(8, 16, 24, 16),
			8));
		//@zsf-e

  description_label_->SetMultiLine(true);
  description_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  AddChildView(description_label_);

  for (auto& source_list : source_lists) {
    switch (source_list->GetMediaListType()) {
      case DesktopMediaID::TYPE_NONE: {
        NOTREACHED();
        break;
      }
      case DesktopMediaID::TYPE_SCREEN: {
        source_types_.push_back(DesktopMediaID::TYPE_SCREEN);

        const DesktopMediaSourceViewStyle kSingleScreenStyle(
            1,                                       // columns
            gfx::Size(360, 280),                     // item_size
            gfx::Rect(),                             // icon_rect
            gfx::Rect(),                             // label_rect
            gfx::HorizontalAlignment::ALIGN_CENTER,  // text_alignment
            gfx::Rect(20, 20, 320, 240),             // image_rect
            4,   // selection_border_thickness
            5);  // focus_rectangle_inset

        const DesktopMediaSourceViewStyle kGenericScreenStyle(
            2,                                       // columns
            gfx::Size(270, 220),                     // item_size
            gfx::Rect(),                             // icon_rect
            gfx::Rect(15, 165, 240, 40),             // label_rect
            gfx::HorizontalAlignment::ALIGN_CENTER,  // text_alignment
            gfx::Rect(15, 15, 240, 150),             // image_rect
            2,   // selection_border_thickness
            5);  // focus_rectangle_inset

        views::ScrollView* screen_scroll_view =
            views::ScrollView::CreateScrollViewWithBorder();
        base::string16 screen_title_text = l10n_util::GetStringUTF16(
            IDS_DESKTOP_MEDIA_PICKER_SOURCE_TYPE_SCREEN);
        auto list_controller = std::make_unique<DesktopMediaListController>(
            this, std::move(source_list));
        screen_scroll_view->SetContents(list_controller->CreateView(
            kGenericScreenStyle, kSingleScreenStyle, screen_title_text));
        list_controllers_.push_back(std::move(list_controller));

        screen_scroll_view->ClipHeightTo(
            kGenericScreenStyle.item_size.height(),
            kGenericScreenStyle.item_size.height() * 2);
        screen_scroll_view->set_hide_horizontal_scrollbar(true);

        pane_->AddTab(screen_title_text, screen_scroll_view);
        pane_->set_listener(this);
        break;
      }
      case DesktopMediaID::TYPE_WINDOW: {
        source_types_.push_back(DesktopMediaID::TYPE_WINDOW);

        const DesktopMediaSourceViewStyle kWindowStyle(
            3,                                     // columns
            gfx::Size(180, 160),                   // item_size
            gfx::Rect(10, 120, 20, 20),            // icon_rect
            gfx::Rect(32, 110, 138, 40),           // label_rect
            gfx::HorizontalAlignment::ALIGN_LEFT,  // text_alignment
            gfx::Rect(8, 8, 164, 104),             // image_rect
            2,                                     // selection_border_thickness
            5);                                    // focus_rectangle_inset

        views::ScrollView* window_scroll_view =
            views::ScrollView::CreateScrollViewWithBorder();
        base::string16 window_title_text = l10n_util::GetStringUTF16(
            IDS_DESKTOP_MEDIA_PICKER_SOURCE_TYPE_WINDOW);
        auto list_controller = std::make_unique<DesktopMediaListController>(
            this, std::move(source_list));
        window_scroll_view->SetContents(list_controller->CreateView(
            kWindowStyle, kWindowStyle, window_title_text));
        list_controllers_.push_back(std::move(list_controller));

        window_scroll_view->ClipHeightTo(kWindowStyle.item_size.height(),
                                         kWindowStyle.item_size.height() * 2);
        window_scroll_view->set_hide_horizontal_scrollbar(true);

        pane_->AddTab(window_title_text, window_scroll_view);
        pane_->set_listener(this);
        break;
      }
      case DesktopMediaID::TYPE_WEB_CONTENTS: {
        // source_types_.push_back(DesktopMediaID::TYPE_WEB_CONTENTS);

        // base::string16 title =
        //     l10n_util::GetStringUTF16(IDS_DESKTOP_MEDIA_PICKER_SOURCE_TYPE_TAB);
        // auto list_controller = std::make_unique<DesktopMediaListController>(
        //     this, std::move(source_list));
        // pane_->AddTab(title,
        //               list_controller->CreateTabListView(title).release());
        // list_controllers_.push_back(std::move(list_controller));

        // pane_->set_listener(this);
        break;
      }
    }
  }

  if (params.app_name == params.target_name) {
    description_label_->SetText(l10n_util::GetStringFUTF16(
        IDS_DESKTOP_MEDIA_PICKER_TEXT, params.app_name));
  } else {
    description_label_->SetText(
        l10n_util::GetStringFUTF16(IDS_DESKTOP_MEDIA_PICKER_TEXT_DELEGATED,
                                   params.app_name, params.target_name));
  }

  DCHECK(!source_types_.empty());
  pane_->SetFocusBehavior(views::View::FocusBehavior::NEVER);
  AddChildView(pane_);

  if (params.request_audio) {
    audio_share_checkbox_ = new views::Checkbox(
        l10n_util::GetStringUTF16(IDS_DESKTOP_MEDIA_PICKER_AUDIO_SHARE));
    audio_share_checkbox_->SetChecked(true);
  }

  // Focus on the first non-null media_list.
  OnSourceTypeSwitched(0);

  // If |params.web_contents| is set and it's not a background page then the
  // picker will be shown modal to the web contents. Otherwise the picker is
  // shown in a separate window.
  views::Widget* widget = nullptr;
//  bool modal_dialog =
//      params.web_contents &&
//      !params.web_contents->GetDelegate()->IsNeverVisible(params.web_contents);
//  if (modal_dialog) {
//    widget =
//        constrained_window::ShowWebModalDialogViews(this, params.web_contents);
//  } else {
//#if defined(OS_MACOSX)
//    // On Mac, MODAL_TYPE_CHILD with a null parent isn't allowed - fall back to
//    // MODAL_TYPE_WINDOW.
//    modality_ = ui::MODAL_TYPE_WINDOW;
//#endif
    widget = DialogDelegate::CreateDialogWidget(this, params.context, nullptr);
    widget->Show();
//  }
		//xuyu: todo: open this sentence
//  chrome::RecordDialogCreation(chrome::DialogIdentifier::DESKTOP_MEDIA_PICKER);

//#if defined(OS_MACOSX)
//  // On Mac, even modals are shown using separate native windows.
//  bool is_separate_native_window = true;
//#else
//  bool is_separate_native_window = !modal_dialog;
//#endif

  // If the picker is a separate native window, it should not be shown in the
  // source list, so its id is passed into NativeDesktopMediaList to be ignored.
  DesktopMediaID dialog_window_id;
//  if (is_separate_native_window) {
    dialog_window_id = DesktopMediaID::RegisterNativeWindow(
        DesktopMediaID::TYPE_WINDOW, widget->GetNativeWindow());

#if !defined(OS_CHROMEOS) && defined(USE_AURA)
    // Set native window ID if the windows is outside Ash.
    dialog_window_id.id = AcceleratedWidgetToDesktopMediaId(
        widget->GetNativeWindow()->GetHost()->GetAcceleratedWidget());
#elif defined(OS_MACOSX)
    // On Mac, the window_id in DesktopMediaID is the same as the actual native
    // window ID. Note that assuming this is a bit of a layering violation; the
    // fact that this code makes that assumption is documented at the code that
    // causes it to hold, so hopefully nobody changes that :)
    dialog_window_id.id = dialog_window_id.window_id;
#endif
//  }

  for (const auto& list_controller : list_controllers_)
    list_controller->StartUpdating(dialog_window_id);
}

DesktopMediaPickerDialogView::~DesktopMediaPickerDialogView() {}

void DesktopMediaPickerDialogView::TabSelectedAt(int index) {
  OnSourceTypeSwitched(index);
  list_controllers_[index]->FocusView();
  DialogModelChanged();
}

void DesktopMediaPickerDialogView::OnSourceTypeSwitched(int index) {
  // Set whether the checkbox is visible based on the source type.
  if (audio_share_checkbox_) {
    switch (source_types_[index]) {
      case DesktopMediaID::TYPE_SCREEN:
#if defined(USE_CRAS) || defined(OS_WIN)
        audio_share_checkbox_->SetVisible(true);
#else
        audio_share_checkbox_->SetVisible(false);
#endif
        break;
      case DesktopMediaID::TYPE_WINDOW:
        audio_share_checkbox_->SetVisible(false);
        break;
      case DesktopMediaID::TYPE_WEB_CONTENTS:
        audio_share_checkbox_->SetVisible(true);
        break;
      case DesktopMediaID::TYPE_NONE:
        NOTREACHED();
        break;
    }
  }
}

const DesktopMediaListController*
DesktopMediaPickerDialogView::GetSelectedController() const {
  return list_controllers_[pane_->GetSelectedTabIndex()].get();
}

DesktopMediaListController*
DesktopMediaPickerDialogView::GetSelectedController() {
  return list_controllers_[pane_->GetSelectedTabIndex()].get();
}

void DesktopMediaPickerDialogView::DetachParent() {
  parent_ = nullptr;
}

gfx::Size DesktopMediaPickerDialogView::CalculatePreferredSize() const {
  static const size_t kDialogViewWidth = 600;
  return gfx::Size(kDialogViewWidth, GetHeightForWidth(kDialogViewWidth));
}

ui::ModalType DesktopMediaPickerDialogView::GetModalType() const {
  return modality_;
}

base::string16 DesktopMediaPickerDialogView::GetWindowTitle() const {
  return l10n_util::GetStringUTF16(IDS_DESKTOP_MEDIA_PICKER_TITLE);
}

bool DesktopMediaPickerDialogView::IsDialogButtonEnabled(
    ui::DialogButton button) const {
  return button != ui::DIALOG_BUTTON_OK ||
         GetSelectedController()->GetSelection().has_value();
}

views::View* DesktopMediaPickerDialogView::GetInitiallyFocusedView() {
  return list_controllers_[0]->GetViewForInitialFocus();
}

int DesktopMediaPickerDialogView::GetDefaultDialogButton() const {
  // OK button will be active only when there is a selection by user. So it's OK
  // to set it as the primary.
  return ui::DIALOG_BUTTON_OK;
}

base::string16 DesktopMediaPickerDialogView::GetDialogButtonLabel(
    ui::DialogButton button) const {
  return l10n_util::GetStringUTF16(button == ui::DIALOG_BUTTON_OK
                                       ? IDS_DESKTOP_MEDIA_PICKER_SHARE
                                       : IDS_CANCEL);
}

views::View* DesktopMediaPickerDialogView::CreateExtraView() {
  return audio_share_checkbox_;
}

bool DesktopMediaPickerDialogView::Accept() {
  // Ok button should only be enabled when a source is selected.
  base::Optional<DesktopMediaID> source_optional =
      accepted_source_.has_value() ? accepted_source_
                                   : GetSelectedController()->GetSelection();
  DesktopMediaID source = source_optional.value();
  source.audio_share = audio_share_checkbox_ &&
                       audio_share_checkbox_->visible() &&
                       audio_share_checkbox_->checked();

//  if (source.type == DesktopMediaID::TYPE_WEB_CONTENTS) {
//    // Activate the selected tab and bring the browser window for the selected
//    // tab to the front.
//    content::WebContents* tab = content::WebContents::FromRenderFrameHost(
//        content::RenderFrameHost::FromID(
//            source.web_contents_id.render_process_id,
//            source.web_contents_id.main_render_frame_id));
//    if (tab) {
//      tab->GetDelegate()->ActivateContents(tab);
//      Browser* browser = chrome::FindBrowserWithWebContents(tab);
//      if (browser && browser->window())
//        browser->window()->Activate();
//    }
//  } else if (source.type == DesktopMediaID::TYPE_WINDOW) {
//#if defined(USE_AURA)
//    aura::Window* window = DesktopMediaID::GetNativeWindowById(source);
//   Browser* browser = chrome::FindBrowserWithWindow(window);
//    if (browser && browser->window())
//      browser->window()->Activate();
//#endif
//  }

  if (parent_)
    parent_->NotifyDialogResult(source);

  // Return true to close the window.
  return true;
}

bool DesktopMediaPickerDialogView::ShouldShowCloseButton() const {
  return false;
}

void DesktopMediaPickerDialogView::DeleteDelegate() {
  // If the dialog is being closed then notify the parent about it.
  if (parent_)
    parent_->NotifyDialogResult(DesktopMediaID());
  delete this;
}

void DesktopMediaPickerDialogView::OnSelectionChanged() {
  DialogModelChanged();
}

void DesktopMediaPickerDialogView::AcceptSource() {
  // This will call Accept() and close the dialog.
  GetDialogClientView()->AcceptWindow();
}

void DesktopMediaPickerDialogView::AcceptSpecificSource(DesktopMediaID source) {
  accepted_source_ = base::Optional<DesktopMediaID>(source);
  AcceptSource();
}

void DesktopMediaPickerDialogView::SelectTab(
    content::DesktopMediaID::Type source_type) {
  for (size_t i = 0; i < source_types_.size(); i++) {
    if (source_types_[i] == source_type) {
      pane_->SelectTabAt(i);
      return;
    }
  }
}

void DesktopMediaPickerDialogView::OnSourceListLayoutChanged() {
  PreferredSizeChanged();
  // TODO(pbos): Ideally this would use shared logic similar to
  // BubbleDialogDelegateView::SizeToContents() instead of implementing sizing
  // logic in-place.
  const gfx::Size new_size = GetWidget()->GetRootView()->GetPreferredSize();
  if (modality_ == ui::ModalType::MODAL_TYPE_CHILD) {
    // For the web-modal dialog resize the dialog in place.
    // TODO(pbos): This should ideally use UpdateWebContentsModalDialogPosition
    // to keep the widget centered horizontally. As this dialog is fixed-width
    // we're effectively only changing the height, so reusing the current
    // widget origin should be equivalent.
    GetWidget()->SetSize(new_size);
    return;
  }

  // When not using the web-modal dialog, center the dialog with its new size.
  GetWidget()->CenterWindow(new_size);
}

DesktopMediaPickerViews::DesktopMediaPickerViews() : dialog_(nullptr) {}

DesktopMediaPickerViews::~DesktopMediaPickerViews() {
  if (dialog_) {
    dialog_->DetachParent();
    dialog_->GetWidget()->Close();
  }
}

void DesktopMediaPickerViews::Show(
    const DesktopMediaPicker::Params& params,
    std::vector<std::unique_ptr<DesktopMediaList>> source_lists,
    const DoneCallback& done_callback) {
  DesktopMediaPickerManager::Get()->OnShowDialog();

  callback_ = done_callback;
  dialog_ =
      new DesktopMediaPickerDialogView(params, this, std::move(source_lists));
}

void DesktopMediaPickerViews::NotifyDialogResult(DesktopMediaID source) {
  // Once this method is called the |dialog_| will close and destroy itself.
  dialog_->DetachParent();
  dialog_ = nullptr;

  DesktopMediaPickerManager::Get()->OnHideDialog();

  if (callback_.is_null())
    return;

  // Notify the |callback_| asynchronously because it may need to destroy
  // DesktopMediaPicker.
  base::PostTaskWithTraits(FROM_HERE, {content::BrowserThread::UI},
                           base::BindOnce(callback_, source));
  callback_.Reset();
}

// static
std::unique_ptr<DesktopMediaPicker> DesktopMediaPicker::Create() {
  return std::unique_ptr<DesktopMediaPicker>(new DesktopMediaPickerViews());
}
}//namespace Cef