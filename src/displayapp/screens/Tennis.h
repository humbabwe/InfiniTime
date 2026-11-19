#pragma once

#include "displayapp/apps/Apps.h"
#include "displayapp/screens/Screen.h"
#include "displayapp/screens/TennisScore.h"
#include "displayapp/Controllers.h"
#include "components/motor/MotorController.h"
#include <FreeRTOS.h>
#include <task.h>
#include <lvgl/lvgl.h>

namespace Pinetime {
  namespace Applications {
    namespace Screens {

      // Tennis score counter.
      //  Tap left half  = point for ME
      //  Tap right half = point for THEM
      //  Swipe down     = undo last point
      //  Swipe up       = switch server
      //  Long press x2  = new match
      //  Button         = leave app (score is kept until next match/reboot)
      class Tennis : public Screen {
      public:
        explicit Tennis(Controllers::MotorController& motorController);
        ~Tennis() override;

        bool OnTouchEvent(TouchEvents event) override;
        void Refresh() override;
        void OnPoint(uint8_t player);

      private:
        void Undo();
        void NewMatch();
        void UpdateDisplay();

        Controllers::MotorController& motorController;

        lv_obj_t* btnSide[2];
        lv_obj_t* lblName[2];
        lv_obj_t* lblPoints[2];
        lv_obj_t* lblGames[2];
        lv_obj_t* serveDot[2];
        lv_obj_t* lblStatus;
        lv_obj_t* lblSets;

        TickType_t resetPromptTime = 0;
        bool resetPrompt = false;
        lv_task_t* taskRefresh;
      };
    }

    template <>
    struct AppTraits<Apps::Tennis> {
      static constexpr Apps app = Apps::Tennis;
      static constexpr const char* icon = "15-0";

      static Screens::Screen* Create(AppControllers& controllers) {
        return new Screens::Tennis(controllers.motorController);
      };

      static bool IsAvailable(Pinetime::Controllers::FS& /*filesystem*/) {
        return true;
      };
    };
  }
}
