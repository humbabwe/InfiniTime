#include "displayapp/screens/Tennis.h"
#include <cstdio>

using namespace Pinetime::Applications::Screens;
using Pinetime::Applications::Tennis::Event;
using Pinetime::Applications::Tennis::Match;

namespace {
  // Kept outside the screen object so the score survives leaving the app
  // (or the watch going back to the clock) until a new match is started.
  Match match;
  constexpr uint8_t historySize = 40;
  Match history[historySize];
  uint8_t historyCount = 0;

  constexpr TickType_t resetPromptTimeout = pdMS_TO_TICKS(3000);

  const lv_color_t sideColor[2] = {LV_COLOR_MAKE(0x0b, 0x3d, 0x1f), LV_COLOR_MAKE(0x26, 0x26, 0x30)};
  const lv_color_t sidePressed[2] = {LV_COLOR_MAKE(0x1d, 0x6b, 0x3a), LV_COLOR_MAKE(0x45, 0x45, 0x55)};
  const lv_color_t ballYellow = LV_COLOR_MAKE(0xdf, 0xff, 0x4f);

  void SideEventHandler(lv_obj_t* obj, lv_event_t event) {
    if (event == LV_EVENT_SHORT_CLICKED) {
      auto* screen = static_cast<Tennis*>(obj->user_data);
      // Left button sits at x = 0 (ME), right button at x = 121 (THEM).
      screen->OnPoint(lv_obj_get_x(obj) > 60 ? 1 : 0);
    }
  }

  lv_obj_t* MakeLabel(lv_obj_t* parent, const lv_font_t* font, lv_color_t color, const char* text) {
    lv_obj_t* label = lv_label_create(parent, nullptr);
    lv_obj_set_style_local_text_font(label, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, font);
    lv_obj_set_style_local_text_color(label, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, color);
    lv_label_set_text_static(label, text);
    return label;
  }
}

Tennis::Tennis(Controllers::MotorController& motorController) : motorController {motorController} {
  static const char* names[2] = {"ME", "THEM"};

  for (uint8_t p = 0; p < 2; p++) {
    btnSide[p] = lv_btn_create(lv_scr_act(), nullptr);
    btnSide[p]->user_data = this;
    lv_obj_set_size(btnSide[p], 119, 240);
    lv_obj_set_pos(btnSide[p], p == 0 ? 0 : 121, 0);
    lv_obj_set_style_local_radius(btnSide[p], LV_BTN_PART_MAIN, LV_STATE_DEFAULT, 0);
    lv_obj_set_style_local_border_width(btnSide[p], LV_BTN_PART_MAIN, LV_STATE_DEFAULT, 0);
    lv_obj_set_style_local_bg_color(btnSide[p], LV_BTN_PART_MAIN, LV_STATE_DEFAULT, sideColor[p]);
    lv_obj_set_style_local_bg_color(btnSide[p], LV_BTN_PART_MAIN, LV_STATE_PRESSED, sidePressed[p]);
    lv_btn_set_layout(btnSide[p], LV_LAYOUT_OFF);
    lv_obj_set_event_cb(btnSide[p], SideEventHandler);

    lblName[p] = MakeLabel(btnSide[p], &jetbrains_mono_bold_20, LV_COLOR_WHITE, names[p]);
    lv_obj_align(lblName[p], btnSide[p], LV_ALIGN_IN_TOP_MID, 0, 8);

    serveDot[p] = lv_obj_create(btnSide[p], nullptr);
    lv_obj_set_size(serveDot[p], 10, 10);
    lv_obj_set_click(serveDot[p], false);
    lv_obj_set_style_local_radius(serveDot[p], LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, LV_RADIUS_CIRCLE);
    lv_obj_set_style_local_border_width(serveDot[p], LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, 0);
    lv_obj_set_style_local_bg_color(serveDot[p], LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, ballYellow);
    lv_obj_align(serveDot[p], lblName[p], LV_ALIGN_OUT_RIGHT_MID, 6, 0);

    lblPoints[p] = MakeLabel(btnSide[p], &jetbrains_mono_76, ballYellow, "0");
    lv_label_set_long_mode(lblPoints[p], LV_LABEL_LONG_CROP);
    lv_label_set_align(lblPoints[p], LV_LABEL_ALIGN_CENTER);
    lv_obj_set_width(lblPoints[p], 119);
    lv_obj_set_pos(lblPoints[p], 0, 36);

    lblGames[p] = MakeLabel(btnSide[p], &jetbrains_mono_42, LV_COLOR_WHITE, "0");
    lv_label_set_long_mode(lblGames[p], LV_LABEL_LONG_CROP);
    lv_label_set_align(lblGames[p], LV_LABEL_ALIGN_CENTER);
    lv_obj_set_width(lblGames[p], 119);
    lv_obj_set_pos(lblGames[p], 0, 158);
  }

  lblStatus = MakeLabel(lv_scr_act(), &jetbrains_mono_bold_20, LV_COLOR_WHITE, "");
  lv_label_set_long_mode(lblStatus, LV_LABEL_LONG_CROP);
  lv_label_set_align(lblStatus, LV_LABEL_ALIGN_CENTER);
  lv_obj_set_width(lblStatus, 240);
  lv_obj_set_pos(lblStatus, 0, 128);

  lblSets = MakeLabel(lv_scr_act(), &jetbrains_mono_bold_20, LV_COLOR_SILVER, "");
  lv_label_set_long_mode(lblSets, LV_LABEL_LONG_CROP);
  lv_label_set_align(lblSets, LV_LABEL_ALIGN_CENTER);
  lv_obj_set_width(lblSets, 240);
  lv_obj_set_pos(lblSets, 0, 212);

  UpdateDisplay();
  taskRefresh = lv_task_create(RefreshTaskCallback, 500, LV_TASK_PRIO_LOW, this);
}

Tennis::~Tennis() {
  lv_task_del(taskRefresh);
  lv_obj_clean(lv_scr_act());
}

void Tennis::Refresh() {
  if (resetPrompt && xTaskGetTickCount() - resetPromptTime > resetPromptTimeout) {
    resetPrompt = false;
    UpdateDisplay();
  }
}

void Tennis::OnPoint(uint8_t player) {
  if (match.winner >= 0) {
    return;
  }
  if (historyCount == historySize) {
    for (uint8_t i = 1; i < historySize; i++) {
      history[i - 1] = history[i];
    }
    historyCount--;
  }
  history[historyCount++] = match;

  resetPrompt = false;
  switch (match.AddPoint(player)) {
    case Event::Point:
      motorController.RunForDuration(25);
      break;
    case Event::Game:
      motorController.RunForDuration(120);
      break;
    case Event::Set:
      motorController.RunForDuration(250);
      break;
    case Event::Match:
      motorController.RunForDuration(255);
      break;
  }
  UpdateDisplay();
}

void Tennis::Undo() {
  if (historyCount == 0) {
    return;
  }
  match = history[--historyCount];
  motorController.RunForDuration(15);
  UpdateDisplay();
}

void Tennis::NewMatch() {
  match = Match {};
  historyCount = 0;
  motorController.RunForDuration(80);
  UpdateDisplay();
}

bool Tennis::OnTouchEvent(TouchEvents event) {
  switch (event) {
    case TouchEvents::SwipeDown:
      resetPrompt = false;
      Undo();
      return true;
    case TouchEvents::SwipeUp:
      resetPrompt = false;
      match.ToggleServer();
      UpdateDisplay();
      return true;
    case TouchEvents::LongTap:
      if (resetPrompt && xTaskGetTickCount() - resetPromptTime <= resetPromptTimeout) {
        resetPrompt = false;
        NewMatch();
      } else {
        resetPrompt = true;
        resetPromptTime = xTaskGetTickCount();
        UpdateDisplay();
      }
      return true;
    default:
      return false;
  }
}

void Tennis::UpdateDisplay() {
  for (uint8_t p = 0; p < 2; p++) {
    if (match.tiebreak) {
      lv_label_set_text_fmt(lblPoints[p], "%d", match.points[p]);
    } else {
      lv_label_set_text_static(lblPoints[p], Pinetime::Applications::Tennis::PointText(match, p));
    }
    lv_label_set_text_fmt(lblGames[p], "%d", match.games[p]);
    bool showDot = match.winner < 0 && match.server == p;
    lv_obj_set_hidden(serveDot[p], !showDot);
  }

  if (resetPrompt) {
    lv_label_set_text_static(lblStatus, "HOLD AGAIN: NEW");
  } else if (match.winner == 0) {
    lv_label_set_text_static(lblStatus, "YOU WIN!");
  } else if (match.winner == 1) {
    lv_label_set_text_static(lblStatus, "THEY WIN");
  } else if (match.tiebreak) {
    lv_label_set_text_static(lblStatus, "TIEBREAK");
  } else if (Pinetime::Applications::Tennis::IsDeuce(match)) {
    lv_label_set_text_static(lblStatus, "DEUCE");
  } else {
    lv_label_set_text_fmt(lblStatus, "SET %d", match.setsCompleted + 1);
  }

  char buf[24] = "";
  size_t len = 0;
  uint8_t n = match.setsCompleted < Match::maxSets ? match.setsCompleted : Match::maxSets;
  for (uint8_t i = 0; i < n && len < sizeof(buf); i++) {
    len += snprintf(buf + len, sizeof(buf) - len, i == 0 ? "%d-%d" : "  %d-%d", match.setHistory[i][0], match.setHistory[i][1]);
  }
  lv_label_set_text(lblSets, buf);
}
