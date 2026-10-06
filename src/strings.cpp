#include "app.h"

// {Russian, English}. Lines with %d or %s are filled in by the game that shows them.
// A status line must fit the 240-pixel screen in the small font; the tests check that.
static const char* const TEXT[S_COUNT][2] = {
  /* S_TITLE */          {"ИГРОТЕКА", "GAME SHELF"},
  /* S_PICK */           {"Выберите игру", "Pick a game"},
  /* S_MINES */          {"Сапёр", "Minesweeper"},
  /* S_LINES */          {"Пять в линию", "Five in a Line"},
  /* S_SETTINGS */       {"Настройки", "Settings"},
  /* S_FOOT */           {"Игры написал Claude · идея posoxAI", "Games by Claude · idea by posoxAI"},
  /* S_MENU */           {"Меню", "Menu"},
  /* S_BEST */           {"Рекорд", "Best"},
  /* S_NO_BEST */        {"рекорда пока нет", "no best yet"},

  /* S_M_NEW */          {"Новая", "New"},
  /* S_M_DIG */          {"Копать", "Dig"},
  /* S_M_FLAG */         {"Флаг", "Flag"},
  /* S_M_READY */        {"Откройте любую клетку.", "Open any cell."},
  /* S_M_PLAY */         {"Долгое нажатие ставит флажок.", "A long press plants a flag."},
  /* S_M_PLAY_FLAG */    {"Нажатие ставит флажок.", "A tap plants a flag."},
  /* S_M_WON */          {"Победа! Время %s.", "You won! Time %s."},
  /* S_M_WON_BEST */     {"Победа! %s, новый рекорд.", "You won! %s, a new best."},
  /* S_M_LOST */         {"Мина. Игра окончена.", "A mine. Game over."},
  /* S_M_CONFIRM */      {"Ещё раз: партия сбросится.", "Again: the game will reset."},

  /* S_L_SCORE */        {"Счёт", "Score"},
  /* S_L_NEXT */         {"Далее", "Next"},
  /* S_L_NEW */          {"Новая игра", "New game"},
  /* S_L_PROMPT */       {"Выберите шарик, затем клетку.", "Pick a marble, then a cell."},
  /* S_L_PICKED */       {"Шарик выбран. Укажите клетку.", "Marble picked. Choose a cell."},
  /* S_L_PICK_FIRST */   {"Сначала выберите шарик.", "Pick a marble first."},
  /* S_L_BLOCKED */      {"Туда нет прохода.", "No way through."},
  /* S_L_LINE */         {"Линия из %d: +%d. Ещё ход.", "Line of %d: +%d. Go again."},
  /* S_L_LUCKY */        {"Шарики сами собрали %d: +%d.", "New marbles lined up %d: +%d."},
  /* S_L_OVER */         {"Игра окончена. Счёт %d.", "Game over. Score %d."},
  /* S_L_OVER_BEST */    {"Игра окончена. Рекорд: %d!", "Game over. New best: %d!"},
  /* S_L_CONFIRM */      {"Ещё раз: счёт сбросится.", "Again: the score will reset."},

  /* S_SET_LANG */       {"Язык", "Language"},
  /* S_SET_SOUND */      {"Звук", "Sound"},
  /* S_SET_SCREEN */     {"Экран", "Screen"},
  /* S_SET_COLORS */     {"Цвета", "Colours"},
  /* S_SET_MARKS */      {"Метки", "Marks"},
  /* S_ON */             {"Вкл", "On"},
  /* S_OFF */            {"Выкл", "Off"},
  /* S_FLIP */           {"Перевернуть", "Turn over"},
  /* S_NORMAL */         {"Обычные", "Normal"},
  /* S_INVERTED */       {"Инверсия", "Inverted"},
  /* S_SET_CAL */        {"Калибровка касаний", "Touch calibration"},
  /* S_SET_RESET */      {"Сбросить рекорды", "Reset best results"},
  /* S_SET_RESET_SURE */ {"Точно сбросить?", "Really reset?"},
  /* S_SET_RESET_DONE */ {"Рекорды сброшены", "Best results reset"},
};

const char* T(StrId id) { return TEXT[id][app::lang ? 1 : 0]; }
