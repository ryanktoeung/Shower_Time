// ECE4305
// Final Project
// Cory Kim
// Ryan Toeung
// Edward Hwang

#include <stdlib.h>

#include "chu_init.h"
#include "gpio_cores.h"
#include "xadc_core.h"
#include "sseg_core.h"
#include "spi_core.h"
#include "ps2_core.h"

#include "vga_core.h"
#include "sseg_core.h"

// Calculate accelerometer values
int* tilt_detection(SpiCore *spi_p) {

	const uint8_t RD_CMD = 0x0b; // read command
	const uint8_t PART_ID_REG = 0x02;
	const uint8_t DATA_REG = 0x09;
	const float raw_max = 127.0 / 2.0; // 128 max 8-bit reading for +/-2g
	int* orientation;
	int8_t yraw;
	float y;

	spi_p->set_freq(400000); // 400KHz
	spi_p->set_mode(0, 0); // Polarity 0, Phase 0

	// check part id
	spi_p->assert_ss(0); // activate
	spi_p->transfer(RD_CMD); // for read operation
	spi_p->transfer(PART_ID_REG); // part id address
	spi_p->transfer(0x00);
	spi_p->deassert_ss(0);

	// read 8-bit y g values once
	spi_p->assert_ss(0); // activate
	spi_p->transfer(RD_CMD); // for read operation
	spi_p->transfer(DATA_REG);
	yraw = spi_p->transfer(0x00);
	spi_p->deassert_ss(0);
	y = (float) yraw / raw_max;

	// tilt right 45 degrees
	if(y <= -0.50) {
		orientation[0] = 0;
		orientation[1] = 0;
		orientation[2] = 1;
	}
	// tilt left 45 degrees
	else if(y >= 0.50 ) {
		orientation[0] = 1;
		orientation[1] = 0;
		orientation[2] = 0;
	}
	// center
	else {
		orientation[0] = 0;
		orientation[1] = 1;
		orientation[2] = 0;
	}

	return orientation;
}

// Randomly add obstacles to the top of the 3x3 grid
// 0 0 0    1 0 0
// 0 0 0 -> 0 0 0
// 0 0 1    0 0 1
void addObstacles(int objects[][3]) {

	int randomObj = rand() % 3;
	objects[0][randomObj] = 1;

}

// Moves all rows down 1
// 0 0 0    0 0 0
// 0 1 1 -> 0 0 0
// 0 0 1    0 1 1
void moveRows(int objects[][3], int character[3], int charPos, int &score, bool &hit) {

	// Remove Row 2
	for(int i = 0; i < 3; i++) {
		if(objects[2][i] == 1 && charPos != i)
			score++;
		// Top hit
		else if(objects[2][i] && charPos == i) {
			hit = true;
		}

		if(charPos != i)
			objects[2][i] = 0;
	}

	// Row 1 -> 2
	for(int i = 0; i < 3; i++) {
		objects[2][i] = objects[1][i];
		objects[1][i] = 0;
	}

	// Row 0 -> 1
	for(int i = 0; i < 3; i++) {
		objects[1][i] = objects[0][i];
		objects[0][i] = 0;
	}
}

// Character movement
void moveCharacter(int objects[][3], int character[3], SpiCore *spi_p, int &charPos, bool &hit) {

	int* orientation = tilt_detection(spi_p);

	// move left
	if(orientation[0]) {
		character[charPos] = 0;
		charPos = 0;
		character[charPos] = 1;
	}
	// move right
	else if(orientation[2]) {
		character[charPos] = 0;
		charPos = 2;
		character[charPos] = 1;
	}
	// center
	else {
		character[charPos] = 0;
		charPos = 1;
		character[charPos] = 1;
	}

	// Side hit
	if(character[charPos] == objects[2][charPos]) {
		hit = true;
	}
}

// Print 3x3 grid on VGA
void printMove(int objects[][3], int charPos, SpriteCore *char_p, SpriteCore *objs_p) {

	// Quarter segments of VGA resolution
	const int x = 160;
	const int y = 120;

	for(int i = 0; i < 3; i++) {
		for(int j = 0; j < 3; j++) {
			if(i == 2 && j == charPos && objects[2][charPos] == 0)
				char_p->move_xy((x-12) * (j+1), y * (i+1));
			else if (objects[i][j])
				objs_p->move_xy(x * (j+1), y * (i+1));
		}
	}
}

void printHit(int charPos, SpriteCore *char_p, SpriteCore *objs_p) {

	// Quarter segments of VGA resolution
	const int x = 160;
	const int y = 120;

	char_p->move_xy((x-12) * (charPos+1), y * 3);
	objs_p->wr_ctrl(0x04); // red water
	objs_p->move_xy(x * (charPos+1), (y-2) * 3);
}

// Flash LEDs
void LEDFlash(GpoCore *led_p) {
	for(int i = 0; i < 3; i++) {
		led_p->write(0xffff);
		sleep_ms(100);
		led_p->write(0x0000);
		sleep_ms(100);
	}
}

// get keyboard input
char getKB(Ps2Core *ps2_p) {
	char ch;
	ps2_p->get_kb_ch(&ch);
	return ch;
}

// Start game
int start(SpriteCore *char_p, SpriteCore *objs_p, OsdCore *osd_p, SpiCore *spi_p, TimerCore *timer_p, GpoCore *led_p, Ps2Core *ps2_p, int difficulty, int highscore) {
	// Initialize 3x3 Object & Character Grid
	int objects[3][3] = {
							{0,0,0},
							{0,0,0},
							{0,0,0}
						};
	int character[3] = {0,1,0};
	int charPos = 1;

	// Initialize New Obstacle frequency
	int objsFreq = 0;
	const int objsFreqMax = 2;

	// Initialize Difficulty values (ms)
	int speeds[4] = {100, 500, 350, 200};

	// Initialize Character Hit value
	bool hit = false;

	// Initialize Pause Game value
	bool pause = false;

	// Initialize Score
	int score = 0;
	osd_p->bypass(0);
	osd_p->set_color(0xf00, 0x000);

	// OSD: Score:
	osd_p->wr_char(0, 1, 'S');
	osd_p->wr_char(1, 1, 'c');
	osd_p->wr_char(2, 1, 'o');
	osd_p->wr_char(3, 1, 'r');
	osd_p->wr_char(4, 1, 'e');
	osd_p->wr_char(5, 1, ':');
	osd_p->wr_char(6, 1, ' ');

	// Initialize keyboard value
	char kb;

	// Initialize Sprites
	char_p->bypass(0);
	char_p->move_xy(640, 480);
	objs_p->bypass(0);
	objs_p->move_xy(640, 480);
	objs_p->wr_ctrl(0x1c); // blue water

	// Run game while player !hit
	while(!hit) {

		// Obstacle frequency
		if (objsFreq == objsFreqMax) {
			addObstacles(objects);
			objsFreq = 0;
		}
		else
			objsFreq++;

		// Time for the player to move
		timer_p->clear();
		while(timer_p->read_tick() / 100000.0 < speeds[difficulty]) {

			// Move character and show output
			moveCharacter(objects, character, spi_p, charPos, hit);
			printMove(objects, charPos, char_p, objs_p);

			// Pause
			kb = getKB(ps2_p);
			if(kb == 'p') {
				osd_p->set_color(0xf00, 0x000);

				// OSD: PAUSED
				osd_p->wr_char(38, 14, 'P');
				osd_p->wr_char(39, 14, 'A');
				osd_p->wr_char(40, 14, 'U');
				osd_p->wr_char(41, 14, 'S');
				osd_p->wr_char(42, 14, 'E');
				osd_p->wr_char(43, 14, 'D');
				pause = true;
				sleep_ms(100); // for keyboard press

				// Wait until resume
				while(pause) {
					kb = getKB(ps2_p);
					if (kb == 'p')
						pause = false;
				}

				osd_p->clr_screen();

				// OSD: Score: ##
				osd_p->wr_char(0, 1, 'S');
				osd_p->wr_char(1, 1, 'c');
				osd_p->wr_char(2, 1, 'o');
				osd_p->wr_char(3, 1, 'r');
				osd_p->wr_char(4, 1, 'e');
				osd_p->wr_char(5, 1, ':');
				osd_p->wr_char(6, 1, ' ');
				osd_p->wr_char(7, 1, '0' + score / 100);
				osd_p->wr_char(8, 1, '0' + score / 10);
				osd_p->wr_char(9, 1, '0' + score % 10);

				// OSD: High Score: ##
				osd_p->wr_char(0, 0, 'H');
				osd_p->wr_char(1, 0, 'i');
				osd_p->wr_char(2, 0, 'g');
				osd_p->wr_char(3, 0, 'h');
				osd_p->wr_char(4, 0, ' ');
				osd_p->wr_char(5, 0, 'S');
				osd_p->wr_char(6, 0, 'c');
				osd_p->wr_char(7, 0, 'o');
				osd_p->wr_char(8, 0, 'r');
				osd_p->wr_char(9, 0, 'e');
				osd_p->wr_char(10, 0, ':');
				osd_p->wr_char(11, 0, ' ');
				osd_p->wr_char(12, 0, '0' + highscore / 100);
				osd_p->wr_char(13, 0, '0' + highscore / 10);
				osd_p->wr_char(14, 0, '0' + highscore % 10);
			}

			// Character hits obstacle sideways
			if(hit)
				break;
		}

		// Move all obstacles down 1 row
		moveRows(objects, character, charPos, score, hit);

		// Update OSD Score
		osd_p->wr_char(7, 1, '0' + score / 100);
		osd_p->wr_char(8, 1, '0' + score / 10);
		osd_p->wr_char(9, 1, '0' + score % 10);
	}

	// Game over, player got hit
	printHit(charPos, char_p, objs_p);
	LEDFlash(led_p);
	return score;
}

// Set difficulty
int setDifficulty(GpiCore *sw_p, SsegCore *sseg_p) {
	int difficulty;
	difficulty = sw_p->read() & 0x03;
	sseg_p->write_1ptn(sseg_p->h2s(difficulty), 0);

	return difficulty;
}

// Draws shower background
void drawBackground(FrameCore *frame_p) {
	// Initialize background
    frame_p->bypass(0);
    frame_p->clr_screen(0x000);

    // Draw Bathtub Bounds
    frame_p->plot_line(120,0,120,480,0xfff); // right
    frame_p->plot_line(520,0,520,480, 0xfff); // left
    for(int i = 121; i < 520; i++)
        frame_p->plot_line(i,0,i,480,83); // center

    // Draw Shower Head
    int x = 150;
    for(int y = 80; y <= 115; y++) {
        frame_p->plot_line(x,y,x+340,y,0xfff);
        if(y%3 == 0) {
            x++;
        }

        // Draw Shower Head Holes
        if(y < 110 && y > 85) {
            for(int i = 1; i < 20; i++) {
                int dotXPixl = ((i/20.0)*340) + x;
                frame_p->wr_pix(dotXPixl, y, 0x000);
            }
        }
    }

    // Draw Shower Head Neck
    for(int i = 315; i <= 335; i++)
        frame_p->plot_line(i,0,i,80,0xfff);
}

// external core instantiation
TimerCore timer(get_slot_addr(BRIDGE_BASE, S0_SYS_TIMER));
GpoCore led(get_slot_addr(BRIDGE_BASE, S2_LED));
GpiCore sw(get_slot_addr(BRIDGE_BASE, S3_SW));
XadcCore adc(get_slot_addr(BRIDGE_BASE, S5_XDAC));
SpiCore spi(get_slot_addr(BRIDGE_BASE, S9_SPI));
Ps2Core ps2(get_slot_addr(BRIDGE_BASE, S11_PS2));
SsegCore sseg(get_slot_addr(BRIDGE_BASE, S8_SSEG));

SpriteCore water(get_sprite_addr(BRIDGE_BASE, V3_WATER), 1024);
SpriteCore cat(get_sprite_addr(BRIDGE_BASE, V1_CAT), 4096);
OsdCore osd(get_sprite_addr(BRIDGE_BASE, V2_OSD));
FrameCore frame(FRAME_BASE);

// main driver
int main() {

	// Initialize SSEG
	uint8_t clear[8] = {0xFF, 0xFF, 0xFF, 0xA1, 0x86, 0x86, 0x8C, 0x92};
	sseg.write_8ptn(clear);
	sseg.set_dp(0);

	// Initialize Keyboard
	ps2.init();
	char input;

	// Initialize Display
	cat.bypass(1);
	water.bypass(1);
	osd.bypass(0);
	osd.clr_screen();
	drawBackground(&frame);

	// Initialize Game Settings
	int score = 0;
	int highscore = 0;
	int difficulty;
	bool play = true;

	// OSD: Shower Time
	osd.wr_char(34, 10, 'S');
	osd.wr_char(35, 10, 'h');
	osd.wr_char(36, 10, 'o');
	osd.wr_char(37, 10, 'w');
	osd.wr_char(38, 10, 'e');
	osd.wr_char(39, 10, 'r');
	osd.wr_char(40, 10, ' ');
	osd.wr_char(41, 10, 'T');
	osd.wr_char(42, 10, 'i');
	osd.wr_char(43, 10, 'm');
	osd.wr_char(44, 10, 'e');

	// OSD: Press SPACE To Start!
	osd.wr_char(30, 24, 'P');
	osd.wr_char(31, 24, 'r');
	osd.wr_char(32, 24, 'e');
	osd.wr_char(33, 24, 's');
	osd.wr_char(34, 24, 's');
	osd.wr_char(35, 24, ' ');
	osd.wr_char(36, 24, 'S');
	osd.wr_char(37, 24, 'P');
	osd.wr_char(38, 24, 'A');
	osd.wr_char(39, 24, 'C');
	osd.wr_char(40, 24, 'E');
	osd.wr_char(41, 24, ' ');
	osd.wr_char(42, 24, 'T');
	osd.wr_char(43, 24, 'o');
	osd.wr_char(44, 24, ' ');
	osd.wr_char(45, 24, 'S');
	osd.wr_char(46, 24, 't');
	osd.wr_char(47, 24, 'a');
	osd.wr_char(48, 24, 'r');
	osd.wr_char(49, 24, 't');
	osd.wr_char(50, 24, '!');

	while (play) {

		// Set difficulty, check SPACE
		do {
			difficulty = setDifficulty(&sw, &sseg);
			input = getKB(&ps2);
		} while(input != ' ');

		osd.clr_screen();

		// OSD: High Score: ##
		osd.wr_char(0, 0, 'H');
		osd.wr_char(1, 0, 'i');
		osd.wr_char(2, 0, 'g');
		osd.wr_char(3, 0, 'h');
		osd.wr_char(4, 0, ' ');
		osd.wr_char(5, 0, 'S');
		osd.wr_char(6, 0, 'c');
		osd.wr_char(7, 0, 'o');
		osd.wr_char(8, 0, 'r');
		osd.wr_char(9, 0, 'e');
		osd.wr_char(10, 0, ':');
		osd.wr_char(11, 0, ' ');
		osd.wr_char(12, 0, '0' + highscore / 100);
		osd.wr_char(13, 0, '0' + highscore / 10);
		osd.wr_char(14, 0, '0' + highscore % 10);

		// Randomize Obstacles
		srand(now_ms());

		// Start Game
		score = start(&cat, &water, &osd, &spi, &timer, &led, &ps2, difficulty, highscore);

		// New highscore reached
		if (score > highscore) {
			highscore = score;
			osd.wr_char(12, 0, '0' + highscore / 100);
			osd.wr_char(13, 0, '0' + highscore / 10);
			osd.wr_char(14, 0, '0' + highscore % 10);
		}

		// Press SPACE To Restart
		osd.wr_char(35, 14, 'Y');
		osd.wr_char(36, 14, 'o');
		osd.wr_char(37, 14, 'u');
		osd.wr_char(38, 14, ' ');
		osd.wr_char(39, 14, 'G');
		osd.wr_char(40, 14, 'o');
		osd.wr_char(41, 14, 't');
		osd.wr_char(42, 14, ' ');
		osd.wr_char(43, 14, 'H');
		osd.wr_char(44, 14, 'i');
		osd.wr_char(45, 14, 't');
		osd.wr_char(46, 14, '!');

		// OSD: Press SPACE To Restart
		osd.wr_char(30, 16, 'P');
		osd.wr_char(31, 16, 'r');
		osd.wr_char(32, 16, 'e');
		osd.wr_char(33, 16, 's');
		osd.wr_char(34, 16, 's');
		osd.wr_char(35, 16, ' ');
		osd.wr_char(36, 16, 'S');
		osd.wr_char(37, 16, 'P');
		osd.wr_char(38, 16, 'A');
		osd.wr_char(39, 16, 'C');
		osd.wr_char(40, 16, 'E');
		osd.wr_char(41, 16, ' ');
		osd.wr_char(42, 16, 'T');
		osd.wr_char(43, 16, 'o');
		osd.wr_char(44, 16, ' ');
		osd.wr_char(45, 16, 'R');
		osd.wr_char(46, 16, 'e');
		osd.wr_char(47, 16, 's');
		osd.wr_char(48, 16, 't');
		osd.wr_char(49, 16, 'a');
		osd.wr_char(50, 16, 'r');
		osd.wr_char(51, 16, 't');

	} // while

} //main
