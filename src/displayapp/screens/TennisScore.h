#pragma once

#include <cstdint>

// Pure tennis scoring logic (no LVGL), so it can be tested on a PC.
// Best of 3 sets, advantage scoring, 7-point tiebreak at 6-6.
namespace Pinetime {
  namespace Applications {
    namespace Tennis {

      enum class Event : uint8_t { Point, Game, Set, Match };

      struct Match {
        static constexpr uint8_t setsToWin = 2;
        static constexpr uint8_t maxSets = 3;

        uint8_t points[2] = {0, 0}; // points in current game (or tiebreak)
        uint8_t games[2] = {0, 0};  // games in current set
        uint8_t sets[2] = {0, 0};
        uint8_t setHistory[maxSets][2] = {};
        uint8_t setsCompleted = 0;
        uint8_t server = 0;         // 0 = me, 1 = them
        uint8_t tbFirstServer = 0;  // who served first point of tiebreak
        bool tiebreak = false;
        int8_t winner = -1;         // -1 = in progress

        bool Started() const {
          return points[0] + points[1] + games[0] + games[1] + setsCompleted > 0;
        }

        Event AddPoint(uint8_t p) {
          if (winner >= 0) {
            return Event::Match;
          }
          uint8_t o = 1 - p;
          points[p]++;

          if (tiebreak) {
            if (points[p] >= 7 && points[p] - points[o] >= 2) {
              games[p]++;
              return WinSet(p);
            }
            uint8_t n = points[0] + points[1];
            // Server changes after the first point, then every two points.
            server = (((n + 1) / 2) % 2 == 0) ? tbFirstServer : 1 - tbFirstServer;
            return Event::Point;
          }

          if (points[p] >= 4 && points[p] - points[o] >= 2) {
            return WinGame(p);
          }
          return Event::Point;
        }

        void ToggleServer() {
          server = 1 - server;
          if (tiebreak && points[0] + points[1] == 0) {
            tbFirstServer = server;
          }
        }

      private:
        Event WinGame(uint8_t p) {
          uint8_t o = 1 - p;
          games[p]++;
          points[0] = points[1] = 0;
          server = 1 - server;
          if (games[p] >= 6 && games[p] - games[o] >= 2) {
            return WinSet(p);
          }
          if (games[0] == 6 && games[1] == 6) {
            tiebreak = true;
            tbFirstServer = server;
          }
          return Event::Game;
        }

        Event WinSet(uint8_t p) {
          if (setsCompleted < maxSets) {
            setHistory[setsCompleted][0] = games[0];
            setHistory[setsCompleted][1] = games[1];
          }
          setsCompleted++;
          sets[p]++;
          if (tiebreak) {
            // Player who received first in the tiebreak serves the next set.
            server = 1 - tbFirstServer;
          }
          tiebreak = false;
          points[0] = points[1] = 0;
          games[0] = games[1] = 0;
          if (sets[p] >= setsToWin) {
            winner = p;
            return Event::Match;
          }
          return Event::Set;
        }
      };

      // Text for a player's point score: "0", "15", "30", "40", "AD" or "" (other side has AD).
      inline const char* PointText(const Match& m, uint8_t p) {
        static const char* names[] = {"0", "15", "30", "40"};
        uint8_t me = m.points[p];
        uint8_t them = m.points[1 - p];
        if (me >= 3 && them >= 3) {
          if (me == them) {
            return "40";
          }
          return me > them ? "AD" : "40";
        }
        return names[me > 3 ? 3 : me];
      }

      inline bool IsDeuce(const Match& m) {
        return !m.tiebreak && m.points[0] >= 3 && m.points[0] == m.points[1];
      }
    }
  }
}
