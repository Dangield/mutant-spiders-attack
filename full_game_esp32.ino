#include "pitches.h"
#include "Adafruit_GFX.h"
#include "Adafruit_ILI9341.h"

#define QUARTER_NOTE_DURATION (1000/4)
#define SDT_LEN 32
#define SE_LEN 4
#define JUMP_SE 1
#define SHOOT_SE 2
// ESP32-WROOM-32D
#define ACTION_PIN 15
#define MOVE_PIN 2
#define BUZZER_PIN 13
#define TFT_CS 5
#define TFT_RST 0
#define TFT_DC 4
#define TFT_MOSI 23
#define TFT_CLK 18
#define TFT_MISO 19

void tone(byte pin, int freq) {
  ledcSetup(0, 2000, 8); // setup beeper
  ledcAttachPin(pin, 0); // attach beeper
  ledcWriteTone(0, freq); // play tone
}

// notes in the melodies
const int soundtrack[] = {
  NOTE_C4, NOTE_E4, NOTE_G4, NOTE_E4, NOTE_F4, NOTE_D4, NOTE_B3, NOTE_D4,
  NOTE_C4, NOTE_E4, NOTE_G4, NOTE_E4, NOTE_F4, NOTE_D4, NOTE_B3, NOTE_B3,
  NOTE_C4, NOTE_E4, NOTE_G4, NOTE_E4, NOTE_F4, NOTE_D4, NOTE_B3, NOTE_D4,
  NOTE_G4, NOTE_F4, NOTE_E4, NOTE_D4, NOTE_C4, NOTE_C4, NOTE_C4, 0
};

const int jump_se[] = {
  NOTE_E5, NOTE_GS5, NOTE_B5, NOTE_C6
};

const int shoot_se[] = {
  NOTE_F3, NOTE_F3, NOTE_F3, NOTE_F2
};

int sdt_progress = 0, se_progress = 4, current_se = 0, note = 0;
int last_action_state = 520, move_state = 0;
int delay_dec = 0;
const int hero_default_y = 186;
int hero_pos[2] = {160, hero_default_y};
int zombie_pos[2] = {20, 158};
int bullet_pos[2] = {0, 176};
int obstacle_pos[8] = {305, 186, 305, 186, 305, 186, 305, 186};
bool spawn_zombie = false;
bool game_over = false;
bool spawn_bullet = false;
bool spawn_obstacle[4] = {false, false, false, false};
int kills = 0;
int distance = 0;
int score = 0;
int obstacle_amount = 1;
int game_over_counter = 10;

Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_MOSI, TFT_CLK, TFT_RST, TFT_MISO);

void setup() {
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(ACTION_PIN, INPUT);
  pinMode(MOVE_PIN, INPUT);
  tft.begin();
  tft.setRotation(1);
  // read diagnostics (optional but can help debug problems)
  uint8_t x = tft.readcommand8(ILI9341_RDMODE);
  Serial.print("Display Power Mode: 0x"); Serial.println(x, HEX);
  x = tft.readcommand8(ILI9341_RDMADCTL);
  Serial.print("MADCTL Mode: 0x"); Serial.println(x, HEX);
  x = tft.readcommand8(ILI9341_RDPIXFMT);
  Serial.print("Pixel Format: 0x"); Serial.println(x, HEX);
  x = tft.readcommand8(ILI9341_RDIMGFMT);
  Serial.print("Image Format: 0x"); Serial.println(x, HEX);
  x = tft.readcommand8(ILI9341_RDSELFDIAG);
  Serial.print("Self Diagnostic: 0x"); Serial.println(x, HEX);
  tft.fillScreen(ILI9341_BLUE);
  tft.fillRect(0, 200, 320, 40, ILI9341_BLACK);
  for (int i = -sdt_progress*10; i < 320; i += 64) {
    tft.fillRect(i, 215, 40, 1, ILI9341_WHITE);
    tft.fillRect(i+40, 215, 10, 1, ILI9341_BLACK);
  }
  tft.setTextColor(ILI9341_PINK);    tft.setTextSize(4);
  tft.print("SCORE:");
  paint_hero();
  randomSeed(analogRead(2));
  // initialize serial communication at 9600 bits per second:
  Serial.begin(9600);
}

void loop() {
  if (game_over) {
    print_game_over_screen();
    tone(BUZZER_PIN, 0);
    delay(1000);
    game_over_counter--;
    if (game_over_counter == 0) {
      clear_env();
      spawn_zombie = false;
      spawn_bullet = false;
      for (int i = 0; i < obstacle_amount; i++) spawn_obstacle[i] = false;
      sdt_progress++;
      remove_hero();
      hero_pos[0] = 160;
      hero_pos[1] = hero_default_y;
      paint_hero();
      delay_dec = 0;
      sdt_progress = 0;
      se_progress = 4;
      kills = 0;
      distance = 0;
      clear_game_over_screen();
      tft.fillRect(0, 215, 320, 1, ILI9341_BLACK);
      game_over_counter = 10;
      game_over = false;
    }
    return;
  }
  // play soundtrack
  note = soundtrack[sdt_progress];
  sdt_progress++;
  if (sdt_progress == SDT_LEN) {
    sdt_progress = 0;
    delay_dec += 5;
    if (delay_dec > QUARTER_NOTE_DURATION / 2) delay_dec = QUARTER_NOTE_DURATION / 2;
    obstacle_amount = 1 + int(delay_dec / 40);
  }

  // detect jump/shoot
  int action_state = analogRead(ACTION_PIN);
  // shoot
  if (action_state > 3000 && hero_pos[1] == hero_default_y && !spawn_bullet && spawn_zombie) {
    se_progress = 0;
    current_se = SHOOT_SE;
    spawn_bullet = true;
    bullet_pos[0] = hero_pos[0] - 10;
    // jump
  } else if (action_state < 1000 && hero_pos[1] == hero_default_y) {
    se_progress = 0;
    current_se = JUMP_SE;
  }

  // detect movement
  move_state = analogRead(MOVE_PIN);

  // remove all elements
  clear_env();

  // paint hero
  animate_hero();

  // paint zombie
  if (random(100) < max(10, delay_dec / 5) && spawn_zombie == false){
    spawn_zombie = true;
    zombie_pos[0] = 20;
  }
  animate_zombie();

  // paint bullet
  animate_bullet();

  // check for shooting zombie
  if (spawn_bullet && spawn_zombie && bullet_pos[0] - zombie_pos[0] < 30) {
    spawn_bullet = false;
    remove_bullet();
    spawn_zombie = false;
    remove_zombie();
    kills++;
  }

  // paint obstacle
  for (int i = 0; i < obstacle_amount; i++) {
    if (random(100) < max(10, delay_dec / 5) && spawn_obstacle[i] == false) {
      spawn_obstacle[i] = true;
      obstacle_pos[0 + 2 * i] = 305;
      break;
    }
  }
  animate_obstacle();

  // print score
  print_score();

  // check for zombie colision
  if (hero_pos[0] - zombie_pos[0] < 35 && spawn_zombie) game_over = true;

  // check for obstacle collision
  for (int i = 0; i < obstacle_amount; i++) {
    if (obstacle_pos[0 + 2 * i] - hero_pos[0] <= 10 && obstacle_pos[0 + 2 * i] - hero_pos[0] >= -5 && hero_pos[1] > hero_default_y - 10 && spawn_obstacle[i]) game_over = true;
  }

  // play special effect
  if (se_progress < 4) {
    if (current_se == JUMP_SE)
      note = jump_se[se_progress];
    if (current_se == SHOOT_SE)
      note = shoot_se[se_progress];
    se_progress++;
  }

  // play sound
  tone(BUZZER_PIN, note);
  delay(QUARTER_NOTE_DURATION - delay_dec);
  distance++;
}

void print_score() {
  tft.setCursor(140, 0);
  tft.setTextColor(ILI9341_BLUE);    tft.setTextSize(4);
  tft.print(score);
  tft.setCursor(140, 0);
  score = distance + 5 * kills;
  tft.setTextColor(ILI9341_PINK);    tft.setTextSize(4);
  tft.print(score);
}

void clear_env() {
  //  remove_hero();
  if (spawn_zombie) remove_zombie();
  if (spawn_bullet) remove_bullet();
  for (int i = 0; i < obstacle_amount; i++) {
    if (spawn_obstacle[i]) remove_obstacle(i);
  }
  for (int i = -sdt_progress*10; i < 320; i += 64) {
    tft.fillRect(i, 215, 40, 1, ILI9341_WHITE);
    tft.fillRect(i+40, 215, 10, 1, ILI9341_BLACK);
  }
}

void animate_hero() {
  remove_hero();
  if (move_state < 1000 || move_state > 3000 || (se_progress < 4 && current_se == JUMP_SE) || hero_pos[1] < hero_default_y) {
    // move
    if (move_state < 1000) {
      hero_pos[0] -= 10;
      if (hero_pos[0] < 0) hero_pos[0] = 0;
    } else if (move_state > 3000) {
      hero_pos[0] += 10;
      if (hero_pos[0] > 290) hero_pos[0] = 290;
    }
    // jump
    if (se_progress < 4 && current_se == JUMP_SE) hero_pos[1] -= 6;
    else if (hero_pos[1] < hero_default_y) hero_pos[1] += 6;
  }
  // repaint
  paint_hero();
}

void remove_hero() {
  tft.setCursor(hero_pos[0], hero_pos[1]);
  tft.setTextColor(ILI9341_BLUE);    tft.setTextSize(2);
  if (sdt_progress % 2 == 1) tft.print("X");
  else tft.print("Y");
  tft.setCursor(hero_pos[0], hero_pos[1]-15);
  tft.setTextColor(ILI9341_BLUE);    tft.setTextSize(2);
  tft.print("O");
}

void paint_hero() {
  tft.setCursor(hero_pos[0], hero_pos[1]);
  tft.setTextColor(ILI9341_YELLOW);    tft.setTextSize(2);
  if (sdt_progress % 2 == 1) tft.print("Y");
  else tft.print("X");
  tft.setCursor(hero_pos[0], hero_pos[1]-15);
  tft.setTextColor(ILI9341_YELLOW);    tft.setTextSize(2);
  tft.print("O");
}

void animate_zombie() {
  if (spawn_zombie) {
    zombie_pos[0] += 5;
    if (zombie_pos[0] > 280) {
      spawn_zombie = false;
      zombie_pos[0] = 20;
      return;
    }
    paint_zombie();
  }
}

void remove_zombie() {
  tft.setCursor(zombie_pos[0], zombie_pos[1]);
  tft.setTextColor(ILI9341_BLUE);    tft.setTextSize(6);
  tft.print("M");
}

void paint_zombie() {
  tft.setCursor(zombie_pos[0], zombie_pos[1]);
  tft.setTextColor(ILI9341_GREEN);    tft.setTextSize(6);
  tft.print("M");
}

void animate_bullet() {
  if (spawn_bullet) {
    bullet_pos[0] -= 10;
    if (bullet_pos[0] < 0) {
      spawn_bullet = false;
      return;
    }
    paint_bullet();
  }
}

void remove_bullet() {
  tft.setCursor(bullet_pos[0], bullet_pos[1]);
  tft.setTextColor(ILI9341_BLUE);    tft.setTextSize(1);
  tft.print("O");
}

void paint_bullet() {
  tft.setCursor(bullet_pos[0], bullet_pos[1]);
  tft.setTextColor(ILI9341_WHITE);    tft.setTextSize(1);
  tft.print("O");
}

void animate_obstacle() {
  for (int i = 0; i < obstacle_amount; i++) {
    if (spawn_obstacle[i]) {
      obstacle_pos[0 + 2 * i] -= 10;
      if (obstacle_pos[0 + 2 * i] < 0) {
        spawn_obstacle[i] = false;
        return;
      }
      paint_obstacle(i);
    }
  }
}

void remove_obstacle(int i) {
  tft.setCursor(obstacle_pos[0 + 2 * i], obstacle_pos[1 + 2 * i]);
  tft.setTextColor(ILI9341_BLUE);    tft.setTextSize(2);
  tft.print("I");
}

void paint_obstacle(int i) {
  tft.setCursor(obstacle_pos[0 + 2 * i], obstacle_pos[1 + 2 * i]);
  tft.setTextColor(ILI9341_DARKGREY);    tft.setTextSize(2);
  tft.print("I");
}

void print_game_over_screen() {
  tft.setCursor(60, 40);
  tft.setTextColor(ILI9341_RED);    tft.setTextSize(4);
  tft.print("GAME OVER");
  tft.setCursor(40, 80);
  tft.print("RESTART IN: ");
  tft.setCursor(160, 120);
  tft.setTextColor(ILI9341_BLUE);
  tft.print(game_over_counter+1);
  tft.setCursor(160, 120);
  tft.setTextColor(ILI9341_RED);
  tft.print(game_over_counter);
}

void clear_game_over_screen() {
  tft.setCursor(60, 40);
  tft.setTextColor(ILI9341_BLUE);    tft.setTextSize(4);
  tft.print("GAME OVER");
  tft.setCursor(40, 80);
  tft.print("RESTART IN: ");
  tft.setCursor(160, 120);
  tft.print(game_over_counter+1);
}
